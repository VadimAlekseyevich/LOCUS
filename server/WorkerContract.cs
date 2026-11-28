using System.Globalization;
using System.Text;
using System.Text.Json;

namespace Locus.Server;

// Protocol 1 is deliberately strict. These are transport acceptance limits,
// not a second difficulty/PP calculator. Bump the contract when a worker
// changes fields, rules or calculation versions.
internal static class WorkerContract
{
    internal const int ProtocolVersion = 1;
    internal const int JudgementVersion = 1;
    internal const int DifficultyVersion = 4;
    internal const int PerformanceVersion = 1;
    private const int MaxKeys = 2_000_000;
    private const long MaxPackageBytes = 320L * 1024 * 1024;
    private const long MaxAssetBytes = 256L * 1024 * 1024;
    private const int MaxChartBytes = 8 * 1024 * 1024;
    private static readonly UTF8Encoding Utf8 = new(false, true);

    internal abstract record Response
    {
        public int ProtocolVersion => WorkerContract.ProtocolVersion;
        public abstract bool Ok { get; }
    }

    internal sealed record Rejection : Response
    {
        public override bool Ok => false;
        public string Error => "Worker rejected input.";
    }
    internal sealed record Digest(long Bytes, string Sha256);
    internal sealed record Difficulty(string DifficultyId, string Title, Digest Content,
        string GameplaySha256, double Stars, int KeyPresses);
    internal sealed record Inspection(string SetId, string Title, string Artist, Digest Manifest,
        Digest Audio, Digest? Background, int DifficultyVersion,
        IReadOnlyList<Difficulty> Difficulties) : Response
    {
        public override bool Ok => true;
        public string Kind => "packageInspection";
        public bool MediaValidated => false;
    }
    internal sealed record Grades(int Perfect, int Good, int Bad, int Miss);
    internal sealed record Score(string DifficultyId, string GameplaySha256, int RatePercent,
        uint ModFlags, uint ModSeed, int JudgementVersion, int DifficultyVersion,
        int PerformanceVersion, double Stars, int KeyPresses, double Pp, Grades Grades,
        double AccuracyPercent, int MaxCombo, int TimedHits, double MeanErrorMs,
        double UnstableRate, IReadOnlyList<int> Histogram) : Response
    {
        public override bool Ok => true;
        public string Kind => "scoreEvaluation";
        public bool RankApproved => false;
    }

    internal sealed record ScoreRequest(string DifficultyId, string GameplaySha256,
        long AudioDurationMs, int RatePercent, uint ModFlags, uint ModSeed)
    {
        internal static ScoreRequest Parse(string[] args)
        {
            Require(args.Length == 9 && args[0] == "verify");
            Require(Hex(args[2], 32) && Hex(args[3], 64));
            var duration = long.Parse(args[4], NumberStyles.None, CultureInfo.InvariantCulture);
            var rate = int.Parse(args[5], NumberStyles.None, CultureInfo.InvariantCulture);
            var flags = uint.Parse(args[6], NumberStyles.None, CultureInfo.InvariantCulture);
            var seed = uint.Parse(args[7], NumberStyles.None, CultureInfo.InvariantCulture);
            Require(duration is >= 1 and <= 86400000 && rate is >= 50 and <= 150 &&
                flags <= 511 && (flags & 64) == 0 &&
                (flags & 3) != 3 && (flags & 12) != 12 &&
                (flags & 48) != 48 && (flags & 384) != 384 &&
                ((flags & 32) != 0 || seed == 0));
            return new(args[2], args[3], duration, rate, flags, seed);
        }
    }

    internal static Response Parse(string output, int exitCode, ScoreRequest? request)
    {
        using var doc = JsonDocument.Parse(output, new JsonDocumentOptions { MaxDepth = 16 });
        var root = doc.RootElement;
        Require(root.ValueKind == JsonValueKind.Object);
        Require(Int(root, "protocolVersion", ProtocolVersion, ProtocolVersion) == ProtocolVersion);
        var ok = root.GetProperty("ok").GetBoolean();
        Require(exitCode == (ok ? 0 : 2));
        if (!ok)
        {
            Shape(root, "protocolVersion", "ok", "error");
            // Never propagate worker-supplied paths, content or diagnostics.
            _ = Text(root, "error", 4096);
            return new Rejection();
        }
        return request is null ? ParseInspection(root) : ParseScore(root, request);
    }

    private static Inspection ParseInspection(JsonElement root)
    {
        Shape(root, "protocolVersion", "ok", "kind", "mediaValidated", "setId", "title",
            "artist", "manifest", "audio", "background", "difficultyVersion", "difficulties");
        Require(root.GetProperty("kind").GetString() == "packageInspection" &&
            !root.GetProperty("mediaValidated").GetBoolean());
        var set = Text(root, "setId", 128);
        var title = Text(root, "title", 1024, maxScalars: 256);
        var artist = Text(root, "artist", 1024, allowEmpty: true, maxScalars: 256);
        var version = Int(root, "difficultyVersion", DifficultyVersion, DifficultyVersion);
        var manifest = ReadDigest(root.GetProperty("manifest"), 64 * 1024);
        var audio = ReadDigest(root.GetProperty("audio"), MaxAssetBytes);
        var art = root.GetProperty("background");
        var background = art.ValueKind == JsonValueKind.Null ? null : ReadDigest(art, 24 * 1024 * 1024);
        var entries = root.GetProperty("difficulties");
        Require(entries.ValueKind == JsonValueKind.Array && entries.GetArrayLength() >= 1 &&
            entries.GetArrayLength() <= 254 - (background is null ? 0 : 1));
        var ids = new HashSet<string>(StringComparer.Ordinal);
        var charts = new List<Difficulty>(entries.GetArrayLength());
        var total = manifest.Bytes + audio.Bytes + (background?.Bytes ?? 0);
        foreach (var entry in entries.EnumerateArray())
        {
            Shape(entry, "difficultyId", "title", "content", "gameplaySha256", "stars", "keyPresses");
            var id = Hash(entry, "difficultyId", 32);
            Require(ids.Add(id));
            var content = ReadDigest(entry.GetProperty("content"), MaxChartBytes);
            total += content.Bytes;
            Require(total <= MaxPackageBytes);
            charts.Add(new(id, Text(entry, "title", 1024), content,
                Hash(entry, "gameplaySha256", 64), Number(entry, "stars", 0, 100000),
                Int(entry, "keyPresses", 1, MaxKeys)));
        }
        return new(set, title, artist, manifest, audio, background, version, charts.AsReadOnly());
    }

    private static Score ParseScore(JsonElement root, ScoreRequest request)
    {
        Shape(root, "protocolVersion", "ok", "kind", "rankApproved", "difficultyId", "gameplaySha256",
            "ratePercent", "modFlags", "modSeed", "judgementVersion", "difficultyVersion",
            "performanceVersion", "stars", "keyPresses", "pp", "grades", "accuracyPercent",
            "maxCombo", "timedHits", "meanErrorMs", "unstableRate", "histogram");
        Require(root.GetProperty("kind").GetString() == "scoreEvaluation" &&
            !root.GetProperty("rankApproved").GetBoolean());
        var id = Hash(root, "difficultyId", 32);
        var hash = Hash(root, "gameplaySha256", 64);
        var rate = Int(root, "ratePercent", 50, 150);
        var flags = root.GetProperty("modFlags").GetUInt32();
        var seed = root.GetProperty("modSeed").GetUInt32();
        Require(id == request.DifficultyId && hash == request.GameplaySha256 &&
            rate == request.RatePercent && flags == request.ModFlags && seed == request.ModSeed);
        var judgement = Int(root, "judgementVersion", JudgementVersion, JudgementVersion);
        var difficulty = Int(root, "difficultyVersion", DifficultyVersion, DifficultyVersion);
        var performance = Int(root, "performanceVersion", PerformanceVersion, PerformanceVersion);
        var stars = Number(root, "stars", 0, 100000);
        var keys = Int(root, "keyPresses", 1, MaxKeys);
        var pp = Number(root, "pp", 0, double.MaxValue);
        var g = root.GetProperty("grades");
        Shape(g, "perfect", "good", "bad", "miss");
        var grades = new Grades(Int(g, "perfect", 0, keys), Int(g, "good", 0, keys),
            Int(g, "bad", 0, keys), Int(g, "miss", 0, keys));
        var hits = grades.Perfect + grades.Good + grades.Bad;
        Require(hits + grades.Miss == keys);
        Require(((flags & 128) == 0 || grades.Miss == 0) &&
            ((flags & 256) == 0 || grades.Good + grades.Bad + grades.Miss == 0));
        var accuracy = Number(root, "accuracyPercent", 0, 100);
        var expected = (100.0 * grades.Perfect + 75.0 * grades.Good + 50.0 * grades.Bad) / keys;
        Require(Math.Abs(accuracy - expected) <= 1e-9);
        var combo = Int(root, "maxCombo", 0, hits);
        Require(hits == 0 ? combo == 0 : combo >= (hits + grades.Miss) / (grades.Miss + 1));
        var timed = Int(root, "timedHits", hits, hits);
        // Judgement V1: 180 ms Bad window, EZ x1.5 / HR x0.7.
        var window = 180.0 * ((flags & 1) != 0 ? 1.5 : (flags & 2) != 0 ? 0.7 : 1.0);
        var mean = Number(root, "meanErrorMs", -window - 1e-6, window + 1e-6);
        var ur = Number(root, "unstableRate", 0, window * 10 + 1e-5);
        Require((hits != 0 || (mean == 0 && pp == 0)) && (hits > 1 || ur == 0));
        Require(stars != 0 || pp == 0);
        var histogram = root.GetProperty("histogram");
        Require(histogram.ValueKind == JsonValueKind.Array && histogram.GetArrayLength() == 36);
        var bins = new int[36];
        var i = 0;
        long sum = 0;
        foreach (var bin in histogram.EnumerateArray())
        {
            var count = bin.GetInt32();
            Require(count >= 0 && count <= hits);
            bins[i++] = count;
            sum += count;
        }
        Require(sum == hits);
        return new(id, hash, rate, flags, seed, judgement, difficulty, performance,
            stars, keys, pp, grades, accuracy, combo, timed, mean, ur, Array.AsReadOnly(bins));
    }

    private static Digest ReadDigest(JsonElement element, long maximum)
    {
        Shape(element, "bytes", "sha256");
        var size = element.GetProperty("bytes").GetInt64();
        Require(size >= 1 && size <= maximum);
        return new(size, Hash(element, "sha256", 64));
    }

    private static void Shape(JsonElement element, params string[] names)
    {
        Require(element.ValueKind == JsonValueKind.Object);
        var remaining = new HashSet<string>(names, StringComparer.Ordinal);
        foreach (var member in element.EnumerateObject())
            Require(remaining.Remove(member.Name)); // unknown or duplicate decoded key
        Require(remaining.Count == 0);
    }
    private static bool Hex(string value, int length) =>
        value.Length == length && value.All(c => c is >= '0' and <= '9' or >= 'a' and <= 'f');
    private static string Hash(JsonElement element, string name, int length)
    {
        var value = element.GetProperty(name).GetString() ?? "";
        Require(Hex(value, length));
        return value;
    }
    private static string Text(JsonElement element, string name, int maxBytes,
        bool allowEmpty = false, int maxScalars = 1024)
    {
        var value = element.GetProperty(name).GetString() ?? throw new InvalidOperationException("Invalid worker text.");
        Require(Utf8.GetByteCount(value) <= maxBytes);
        if (allowEmpty && value.Length == 0) return value;
        var scalars = 0;
        foreach (var rune in value.EnumerateRunes()) { _ = rune; ++scalars; }
        Require(!string.IsNullOrWhiteSpace(value) && scalars <= maxScalars && !value.Any(char.IsControl));
        return value;
    }
    private static int Int(JsonElement element, string name, int min, int max)
    {
        var value = element.GetProperty(name).GetInt32();
        Require(value >= min && value <= max);
        return value;
    }
    private static double Number(JsonElement element, string name, double min, double max)
    {
        var value = element.GetProperty(name).GetDouble();
        Require(double.IsFinite(value) && value >= min && value <= max);
        return value;
    }
    private static void Require(bool condition)
    {
        if (!condition) throw new InvalidOperationException("Invalid worker protocol response or request.");
    }
}
