using System.Globalization;

namespace Locus.Server;

// No externally supplied Prepared JSON: only the in-process trusted
// preparation result can flow into durable storage.
internal static class PackageStorage
{
    internal sealed record StoredFile(string Name, BlobReference Blob);
    internal sealed record StoredPackage(int ProtocolVersion, bool Ok, string Kind,
        BlobReference Archive, PackagePreparation.Prepared Preparation, IReadOnlyList<StoredFile> Files);

    internal static async Task<int> RunAsync(string archivePath, string stagePath, CancellationToken ct)
    {
        var store = FileBlobStore.Open(Environment.GetEnvironmentVariable("LOCUS_OBJECT_ROOT"));
        var archive = WorkerContainer.SafePath(archivePath, false);
        var stage = WorkerContainer.SafePath(stagePath, true);
        store.RequireSeparate(archive);
        store.RequireSeparate(stage);
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.CancelAfter(TimeSpan.FromMinutes(6));
        var prepared = await PackagePreparation.PrepareAsync(archive, stage, budget.Token);
        if (prepared is null)
        {
            WorkerProcess.Write(new WorkerContract.Rejection());
            return 2;
        }
        var result = await PersistAsync(store, archive, stage, prepared, budget.Token);
        WorkerProcess.Write(result);
        return 0;
    }

    internal static async Task<StoredPackage> PersistAsync(IBlobStore store, string archive, string stage,
        PackagePreparation.Prepared prepared, CancellationToken ct)
    {
        var archiveRef = await PutFileAsync(store, archive, prepared.Archive, ct);
        var assets = new List<StoredFile>(prepared.Files.Count);
        foreach (var entry in prepared.Files)
            assets.Add(new(entry.Name, await PutFileAsync(store, Path.Combine(stage, entry.Name), entry.Content, ct)));
        // A failure can leave unreferenced complete objects, never a successful
        // receipt with missing objects. SQL publication is a later transaction.
        return new(1, true, "storedPackage", archiveRef, prepared, assets.AsReadOnly());
    }

    internal static async Task<BlobReference> PutFileAsync(IBlobStore store, string path,
        WorkerContract.Digest digest, CancellationToken ct)
    {
        if (LinuxPrivateFiles.RegularSize(path, digest.Bytes) != digest.Bytes)
            throw new IOException("Object source size mismatch.");
        await using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read,
            65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        return await store.PutAsync(file, digest, ct);
    }

    // Operator diagnostic/import primitives for trusted private files.
    // A raw blob is NOT a validated map, replay, or publication.
    internal static async Task<int> ObjectCommandAsync(string[] args, CancellationToken ct)
    {
        var put = args.Length == 4 && args[0] == "put";
        if (!put && !(args.Length == 3 && args[0] == "verify"))
            throw new ArgumentException("Invalid object command.");
        var hash = args[put ? 2 : 1];
        var length = long.Parse(args[put ? 3 : 2], NumberStyles.None, CultureInfo.InvariantCulture);
        var digest = new WorkerContract.Digest(length, hash);
        var reference = BlobReference.FromDigest(digest);
        var store = FileBlobStore.Open(Environment.GetEnvironmentVariable("LOCUS_OBJECT_ROOT"));
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.CancelAfter(TimeSpan.FromSeconds(60));
        if (put)
        {
            var path = WorkerContainer.SafePath(args[1], false);
            store.RequireSeparate(path);
            reference = await PutFileAsync(store, path, digest, budget.Token);
        }
        else
        {
            await using var file = await store.OpenVerifiedAsync(digest, budget.Token);
        }
        WorkerProcess.Write(reference);
        return 0;
    }
}
