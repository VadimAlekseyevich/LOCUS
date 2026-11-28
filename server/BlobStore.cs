using System.Buffers;
using System.Security.Cryptography;

namespace Locus.Server;

internal sealed record BlobReference(string Key, long Bytes, string Sha256)
{
    internal const long MaxBytes = 320L * 1024 * 1024;

    internal static BlobReference FromDigest(WorkerContract.Digest digest)
    {
        if (digest.Bytes is < 1 or > MaxBytes || digest.Sha256.Length != 64 ||
            !digest.Sha256.All(c => c is >= '0' and <= '9' or >= 'a' and <= 'f'))
            throw new ArgumentException("Invalid blob digest.");
        return new("sha256/" + digest.Sha256, digest.Bytes, digest.Sha256);
    }
}

// Content identity is independent of the backend. Put never replaces a key,
// and only succeeds after checking the exact bytes and durable commit.
// A verified read returns a stream at offset zero, owned by the caller.
internal interface IBlobStore
{
    Task<BlobReference> PutAsync(Stream source, WorkerContract.Digest expected, CancellationToken ct);
    Task<Stream> OpenVerifiedAsync(WorkerContract.Digest expected, CancellationToken ct);
}

// Private single-host Linux implementation. No worker may mount this root.
// Operator provisions a durable local filesystem and exclusive service UID.
internal sealed class FileBlobStore : IBlobStore
{
    private readonly string root;
    private FileBlobStore(string root) => this.root = root;

    internal static FileBlobStore Open(string? path)
    {
        if (!OperatingSystem.IsLinux() || string.IsNullOrWhiteSpace(path))
            throw new InvalidOperationException("A private Linux object root is required.");
        var root = WorkerContainer.SafePath(path, true);
        LinuxPrivateFiles.RequirePrivateDirectory(root);
        return new(root);
    }

    // Store and staging must not overlap: the extraction container may
    // write its stage, but must never see the store or one of its ancestors.
    internal void RequireSeparate(string path)
    {
        if (path == root || path.StartsWith(root + "/", StringComparison.Ordinal) ||
            root.StartsWith(path + "/", StringComparison.Ordinal))
            throw new InvalidOperationException("Object store and job input must not overlap.");
    }

    private string Shard(BlobReference reference) => Path.Combine(root, reference.Sha256[..2]);

    public async Task<BlobReference> PutAsync(Stream source, WorkerContract.Digest expected, CancellationToken ct)
    {
        if (!OperatingSystem.IsLinux()) throw new PlatformNotSupportedException();
        var reference = BlobReference.FromDigest(expected);
        ct.ThrowIfCancellationRequested();
        var shard = Shard(reference);
        Directory.CreateDirectory(shard, LinuxPrivateFiles.DirectoryMode);
        LinuxPrivateFiles.RequirePrivateDirectory(shard);
        // Persist the shard entry even if another concurrent writer created it.
        LinuxPrivateFiles.SyncDirectory(root);
        var target = Path.Combine(shard, reference.Sha256);
        var pending = Path.Combine(shard, ".pending-" + Guid.NewGuid().ToString("N"));
        try
        {
            await using (var file = new FileStream(pending, new FileStreamOptions
            {
                Mode = FileMode.CreateNew, Access = FileAccess.Write, Share = FileShare.None,
                Options = FileOptions.Asynchronous | FileOptions.SequentialScan,
                BufferSize = 65536, UnixCreateMode = LinuxPrivateFiles.PendingMode
            }))
            {
                await CopyAndCheckAsync(source, file, reference, ct);
                await file.FlushAsync(ct);
                File.SetUnixFileMode(pending, LinuxPrivateFiles.ObjectMode);
                file.Flush(flushToDisk: true);
            }
            ct.ThrowIfCancellationRequested();
            if (!LinuxPrivateFiles.Publish(pending, target))
            {
                // A competing commit or an existing object. Never trust its
                // mere presence, and never repair corruption by overwriting it.
                await using var existing = await OpenVerifiedAsync(expected, ct);
            }
            LinuxPrivateFiles.SyncDirectory(shard);
            return reference;
        }
        finally
        {
            // Crash leftovers have only private .pending-* names. No
            // automatic reaper may race live writers.
            File.Delete(pending);
        }
    }

    public async Task<Stream> OpenVerifiedAsync(WorkerContract.Digest expected, CancellationToken ct)
    {
        var reference = BlobReference.FromDigest(expected);
        ct.ThrowIfCancellationRequested();
        var shard = Shard(reference);
        LinuxPrivateFiles.RequirePrivateDirectory(shard);
        var path = Path.Combine(shard, reference.Sha256);
        if (LinuxPrivateFiles.RegularSize(path, reference.Bytes, stored: true) != reference.Bytes)
            throw new IOException("Stored object length mismatch.");
        var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read,
            65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        try
        {
            await CopyAndCheckAsync(file, null, reference, ct);
            file.Position = 0;
            return file;
        }
        catch
        {
            await file.DisposeAsync();
            throw;
        }
    }

    private static async Task CopyAndCheckAsync(Stream source, Stream? destination,
        BlobReference expected, CancellationToken ct)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        var buffer = ArrayPool<byte>.Shared.Rent(65536);
        try
        {
            long total = 0;
            int count;
            // Read at most one byte beyond the expected size, including for
            // non-seekable request streams. Never allocate the whole object.
            while ((count = await source.ReadAsync(buffer.AsMemory(0,
                (int)Math.Min(65536, expected.Bytes - total + 1)), ct)) != 0)
            {
                total += count;
                if (total > expected.Bytes) throw new IOException("Object exceeds expected size.");
                hash.AppendData(buffer, 0, count);
                if (destination is not null)
                    await destination.WriteAsync(buffer.AsMemory(0, count), ct);
            }
            if (total != expected.Bytes || Convert.ToHexStringLower(hash.GetHashAndReset()) != expected.Sha256)
                throw new IOException("Object digest mismatch.");
        }
        finally { ArrayPool<byte>.Shared.Return(buffer, clearArray: true); }
    }
}
