using System.Security.Cryptography;
using System.Text.Json;

namespace Locus.Server;

// Private preparation only. No DB write, durable object or rank approval.
// The operator owns a dedicated bounded stage and immutable input archive.
internal static class PackagePreparation
{
    private const long MaxPackageBytes = 320L * 1024 * 1024;
    private static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web);
    internal sealed record FileEntry(string Name, WorkerContract.Digest Content);
    internal sealed record Prepared(int ProtocolVersion, bool Ok, string Kind,
        WorkerContract.Digest Archive, WorkerContract.Inspection Inspection,
        WorkerMedia.Result Audio, WorkerMedia.Result? Background, IReadOnlyList<FileEntry> Files);

    private static async Task<WorkerContract.Digest> HashAsync(string path, long max, CancellationToken ct)
    {
        var length = LinuxPrivateFiles.RegularSize(path, max);
        await using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read,
            65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        if (file.Length != length) throw new InvalidOperationException("File changed.");
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        var buffer = new byte[65536];
        long total = 0;
        int count;
        while ((count = await file.ReadAsync(buffer.AsMemory(), ct)) != 0)
        {
            total += count;
            if (total > length) throw new InvalidOperationException("File changed.");
            hash.AppendData(buffer, 0, count);
        }
        if (total != length) throw new InvalidOperationException("File changed.");
        return new(total, Convert.ToHexStringLower(hash.GetHashAndReset()));
    }

    internal static async Task<int> RunAsync(string archivePath, string stagePath, CancellationToken ct)
    {
        var result = await PrepareAsync(archivePath, stagePath, ct);
        if (result is null)
        {
            WorkerProcess.Write(new WorkerContract.Rejection());
            return 2;
        }
        WorkerProcess.Write(result);
        return 0;
    }

    internal static async Task<Prepared?> PrepareAsync(string archivePath, string stagePath, CancellationToken ct)
    {
        if (!OperatingSystem.IsLinux() ||
            (Environment.GetEnvironmentVariable("LOCUS_WORKER_MODE") ?? "container") != "container")
            throw new InvalidOperationException("Preparation requires isolated Linux workers.");
        var archive = WorkerContainer.SafePath(archivePath, false);
        var stage = WorkerContainer.SafePath(stagePath, true);
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.CancelAfter(TimeSpan.FromMinutes(5));
        var token = budget.Token;
        var archiveDigest = await HashAsync(archive, MaxPackageBytes, token);
        var (response, _) = await WorkerProcess.EvaluateAsync(["inspect", archive, stage], token);
        if (response is WorkerContract.Rejection) return null;
        var inspection = (WorkerContract.Inspection)response;

        var files = await InventoryAsync(stage, inspection, token);
        var audioFile = files.Single(f => IsAudio(f.Name));
        var audio = await WorkerMedia.InspectAsync(Path.Combine(stage, audioFile.Name),
            inspection.Audio.Sha256, token);
        if (audio is null) return null;
        if (audio.Audio is null || audio.Bytes != inspection.Audio.Bytes)
            throw new InvalidOperationException("Audio result mismatch.");
        WorkerMedia.Result? background = null;
        if (inspection.Background is not null)
        {
            var art = files.Single(f => IsImage(f.Name));
            background = await WorkerMedia.InspectAsync(Path.Combine(stage, art.Name),
                inspection.Background.Sha256, token);
            if (background is null) return null;
            if (background.Image is null || background.Bytes != inspection.Background.Bytes)
                throw new InvalidOperationException("Background result mismatch.");
        }

        using var timingBudget = CancellationTokenSource.CreateLinkedTokenSource(token);
        timingBudget.CancelAfter(TimeSpan.FromSeconds(60));
        var timing = await WorkerContainer.ValidateTimingAsync(stage, audio.Audio.SampleFrames,
            audio.Audio.SampleRate, timingBudget.Token);
        if (timing.ExitCode == 2)
        {
            if (WorkerContract.Parse(timing.Output, 2, null) is not WorkerContract.Rejection)
                throw new InvalidOperationException("Invalid timing rejection.");
            return null;
        }
        using var doc = JsonDocument.Parse(timing.Output, new JsonDocumentOptions { MaxDepth = 16 });
        var root = doc.RootElement;
        WorkerContract.Shape(root, "protocolVersion", "ok", "kind", "sampleFrames", "sampleRate", "inspection");
        WorkerContract.Require(timing.ExitCode == 0 && root.GetProperty("protocolVersion").GetInt32() == 1 &&
            root.GetProperty("ok").GetBoolean() && root.GetProperty("kind").GetString() == "timingValidation" &&
            root.GetProperty("sampleFrames").GetInt64() == audio.Audio.SampleFrames &&
            root.GetProperty("sampleRate").GetInt32() == audio.Audio.SampleRate);
        var nested = root.GetProperty("inspection");
        // Parse the complete nested envelope, including its protocol version.
        var validated = WorkerContract.Parse(nested.GetRawText(), 0, null);
        if (validated is not WorkerContract.Inspection verified ||
            JsonSerializer.Serialize(verified, Json) != JsonSerializer.Serialize(inspection, Json))
            throw new InvalidOperationException("Timing result refers to different package.");

        // Re-check source and entire stage after the read-only jobs. This is
        // a consistency check under exclusive ownership, not a race defense.
        var finalFiles = await InventoryAsync(stage, inspection, token);
        if (!files.SequenceEqual(finalFiles) ||
            await HashAsync(archive, MaxPackageBytes, token) != archiveDigest)
            throw new InvalidOperationException("Preparation input changed.");
        return new(1, true, "preparedPackage", archiveDigest, inspection,
            audio, background, files.AsReadOnly());
    }

    private static bool IsAudio(string name) =>
        Path.GetExtension(name).ToLowerInvariant() is ".mp3" or ".wav";
    private static bool IsImage(string name) =>
        Path.GetExtension(name).ToLowerInvariant() is ".png" or ".jpg" or ".jpeg";

    private static async Task<List<FileEntry>> InventoryAsync(string stage,
        WorkerContract.Inspection inspection, CancellationToken ct)
    {
        var entries = new List<FileEntry>();
        var charts = inspection.Difficulties.Select(x => x.Content).ToList();
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var haveManifest = false;
        var haveAudio = false;
        var haveArt = false;
        long total = 0;
        foreach (var path in Directory.EnumerateFileSystemEntries(stage))
        {
            ct.ThrowIfCancellationRequested();
            var name = Path.GetFileName(path);
            // Only flat regular files, no paths supplied by worker JSON.
            if (entries.Count >= 256 || !names.Add(name) ||
                name.Any(c => c < 32 || c == 127 || c == ',' || c == '"' || c == '\\' || c == ':'))
                throw new InvalidOperationException("Invalid stage inventory.");
            var digest = await HashAsync(path, 256L * 1024 * 1024, ct);
            total += digest.Bytes;
            if (total > MaxPackageBytes) throw new InvalidOperationException("Stage budget exceeded.");
            if (name == "set.locus.json")
            {
                if (haveManifest || digest != inspection.Manifest) throw new InvalidOperationException("Manifest mismatch.");
                haveManifest = true;
            }
            else if (IsAudio(name))
            {
                if (haveAudio || digest != inspection.Audio) throw new InvalidOperationException("Audio mismatch.");
                haveAudio = true;
            }
            else if (IsImage(name))
            {
                if (haveArt || digest != inspection.Background) throw new InvalidOperationException("Background mismatch.");
                haveArt = true;
            }
            else if (Path.GetExtension(name) == ".json")
            {
                if (!charts.Remove(digest)) throw new InvalidOperationException("Chart mismatch.");
            }
            else throw new InvalidOperationException("Unexpected stage file.");
            entries.Add(new(name, digest));
        }
        if (!haveManifest || !haveAudio || haveArt != (inspection.Background is not null) || charts.Count != 0)
            throw new InvalidOperationException("Incomplete stage inventory.");
        entries.Sort((a, b) => StringComparer.Ordinal.Compare(a.Name, b.Name));
        return entries;
    }
}
