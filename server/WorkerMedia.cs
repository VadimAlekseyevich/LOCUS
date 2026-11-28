using System.Security.Cryptography;
using System.Text.Json;

namespace Locus.Server;

// A separate decoder image sees exactly one immutable, operator-owned file.
// Bind the metadata to the digest returned by package inspection, not to
// a client duration, arbitrary path in a worker response or file extension alone.
internal static class WorkerMedia
{
    internal sealed record Audio(long SampleFrames, int SampleRate, int Channels, long DurationMs);
    internal sealed record Image(int Width, int Height);
    internal sealed record Result(int ProtocolVersion, bool Ok, string Kind, string Format,
        string Sha256, long Bytes, Audio? Audio, Image? Image);
    internal static async Task<int> RunAsync(string input, string expectedHash, CancellationToken ct)
    {
        var result = await InspectAsync(input, expectedHash, ct);
        if (result is null)
        {
            WorkerProcess.Write(new { protocolVersion = 1, ok = false, error = "Media rejected." });
            return 2;
        }
        WorkerProcess.Write(result);
        return 0;
    }

    internal static async Task<Result?> InspectAsync(string input, string expectedHash, CancellationToken ct)
    {
        if ((Environment.GetEnvironmentVariable("LOCUS_WORKER_MODE") ?? "container") != "container" ||
            !OperatingSystem.IsLinux() || expectedHash.Length != 64 ||
            !expectedHash.All(c => c is >= '0' and <= '9' or >= 'a' and <= 'f'))
            throw new InvalidOperationException("Media requires isolated execution and an expected digest.");
        var path = WorkerContainer.SafePath(input, false);
        var format = Path.GetExtension(path).ToLowerInvariant() switch
        {
            ".wav" => "wav", ".mp3" => "mp3", ".png" => "png", ".jpg" or ".jpeg" => "jpeg",
            _ => throw new InvalidOperationException("Unsupported media extension.")
        };
        var audio = format is "wav" or "mp3";
        long limit = (audio ? 256L : 24L) * 1024 * 1024;
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.CancelAfter(TimeSpan.FromSeconds(60));
        var size = await CheckDigestAsync(path, expectedHash, limit, budget.Token);
        var outcome = await WorkerContainer.DecodeMediaAsync(path, format, budget.Token);
        // The private stage must have no concurrent writers. Re-check bytes
        // as an additional consistency check, not as a defense against races.
        if (await CheckDigestAsync(path, expectedHash, limit, budget.Token) != size)
            throw new InvalidOperationException("Media changed.");
        if (outcome.ExitCode == 2)
        {
            using var rejection = JsonDocument.Parse(outcome.Output, new JsonDocumentOptions { MaxDepth = 4 });
            var r = rejection.RootElement;
            Shape(r, "protocolVersion", "ok");
            if (r.GetProperty("protocolVersion").GetInt32() != 1 || r.GetProperty("ok").GetBoolean())
                throw new InvalidOperationException("Invalid media rejection.");
            return null;
        }
        if (outcome.ExitCode != 0) throw new InvalidOperationException("Decoder failed.");
        using var doc = JsonDocument.Parse(outcome.Output, new JsonDocumentOptions { MaxDepth = 4 });
        var root = doc.RootElement;
        if (audio)
            Shape(root, "protocolVersion", "ok", "format", "sampleFrames", "sampleRate", "channels");
        else
            Shape(root, "protocolVersion", "ok", "format", "width", "height");
        if (root.GetProperty("protocolVersion").GetInt32() != 1 ||
            !root.GetProperty("ok").GetBoolean() || root.GetProperty("format").GetString() != format)
            throw new InvalidOperationException("Invalid media response.");
        Audio? audioInfo = null;
        Image? imageInfo = null;
        if (audio)
        {
            var rate = root.GetProperty("sampleRate").GetInt32();
            var channels = root.GetProperty("channels").GetInt32();
            var frames = root.GetProperty("sampleFrames").GetInt64();
            if (rate is < 8000 or > 192000 || channels is < 1 or > 2 ||
                frames <= 0 || frames > 86400L * rate)
                throw new InvalidOperationException("Invalid decoded audio bounds.");
            // Integer nearest millisecond, ties up; preserve the exact sample
            // count too so future timing contracts can avoid quantization.
            var duration = (frames * 1000 + rate / 2) / rate;
            if (duration is < 1 or > 86400000) throw new InvalidOperationException("Invalid audio duration.");
            audioInfo = new(frames, rate, channels, duration);
        }
        else
        {
            var width = root.GetProperty("width").GetInt32();
            var height = root.GetProperty("height").GetInt32();
            if (width is < 1 or > 8192 || height is < 1 or > 8192 || (long)width * height > 33554432)
                throw new InvalidOperationException("Invalid decoded image bounds.");
            imageInfo = new(width, height);
        }
        return new Result(1, true, "mediaInspection", format,
            expectedHash, size, audioInfo, imageInfo);
    }

    internal static async Task<long> CheckDigestAsync(string path, string expected, long maxBytes, CancellationToken ct)
    {
        await using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read,
            65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        var length = stream.Length;
        if (length < 1 || length > maxBytes) throw new InvalidOperationException("Media size out of range.");
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        var buffer = new byte[65536];
        long read = 0;
        int count;
        while ((count = await stream.ReadAsync(buffer.AsMemory(), ct)) != 0)
        {
            read += count;
            if (read > length) throw new InvalidOperationException("Media changed.");
            hash.AppendData(buffer, 0, count);
        }
        if (read != length || !Convert.ToHexStringLower(hash.GetHashAndReset()).Equals(expected, StringComparison.Ordinal))
            throw new InvalidOperationException("Media digest mismatch.");
        return length;
    }

    private static void Shape(JsonElement value, params string[] fields)
    {
        if (value.ValueKind != JsonValueKind.Object) throw new InvalidOperationException("Invalid media object.");
        var remaining = new HashSet<string>(fields, StringComparer.Ordinal);
        foreach (var property in value.EnumerateObject())
            if (!remaining.Remove(property.Name)) throw new InvalidOperationException("Invalid media member.");
        if (remaining.Count != 0) throw new InvalidOperationException("Missing media member.");
    }
}
