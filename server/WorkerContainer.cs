using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;

namespace Locus.Server;

// Private Linux operator adapter. Docker daemon access is privileged;
// never grant it to the public API process or expose it inside a worker.
internal static class WorkerContainer
{
    private const long StageLimit = 384L * 1024 * 1024;
    private const long InputLimit = 320L * 1024 * 1024;

    [DllImport("libc", EntryPoint = "geteuid")]
    private static extern uint EffectiveUid();
    [DllImport("libc", EntryPoint = "getegid")]
    private static extern uint EffectiveGid();

    internal static async Task<WorkerProcess.Outcome> RunAsync(string[] args, CancellationToken ct)
    {
        EnsureOperator();
        var image = Image("LOCUS_WORKER_IMAGE");
        var inspect = args[0] == "inspect";
        var stage = SafePath(inspect ? args[2] : args[1], true);
        var input = SafePath(inspect ? args[1] : args[8], false);
        if (new FileInfo(input).Length > InputLimit ||
            input.StartsWith(stage + "/", StringComparison.Ordinal))
            throw new InvalidOperationException("Invalid worker input.");
        if (inspect) ValidateStage(stage);
        else RejectNestedMounts(stage);

        var mounts = new[] { Mount(input, "/input", true), Mount(stage, "/stage", !inspect) };
        var command = inspect ? new List<string> { "inspect", "/input", "/stage" } :
            new List<string> { "verify", "/stage" };
        if (!inspect)
        {
            command.AddRange(args[2..8]);
            command.Add("/input");
        }
        return await RunIsolatedAsync(image, "/usr/local/bin/locus_worker", mounts, command, ct);
    }

    internal static Task<WorkerProcess.Outcome> ValidateTimingAsync(string stage, long frames,
        int rate, CancellationToken ct)
    {
        EnsureOperator();
        stage = SafePath(stage, true);
        RejectNestedMounts(stage);
        return RunIsolatedAsync(Image("LOCUS_WORKER_IMAGE"), "/usr/local/bin/locus_worker",
            [Mount(stage, "/stage", true)],
            ["validate-timing", "/stage", frames.ToString(CultureInfo.InvariantCulture),
                rate.ToString(CultureInfo.InvariantCulture)], ct);
    }

    internal static Task<WorkerProcess.Outcome> DecodeMediaAsync(string input, string format, CancellationToken ct)
    {
        EnsureOperator();
        return RunIsolatedAsync(Image("LOCUS_MEDIA_IMAGE"), "/usr/local/bin/locus_media",
            [Mount(SafePath(input, false), "/input", true)], [format], ct);
    }

    private static void EnsureOperator()
    {
        if (!OperatingSystem.IsLinux() || EffectiveUid() == 0)
            throw new InvalidOperationException("Container worker requires a non-root Linux operator.");
    }
    private static string Image(string variable)
    {
        var image = Environment.GetEnvironmentVariable(variable);
        if (image is null || !image.StartsWith("sha256:", StringComparison.Ordinal) ||
            image.Length != 71 || !image.AsSpan(7).ToString().All(c => c is >= '0' and <= '9' or >= 'a' and <= 'f'))
            throw new InvalidOperationException("A locally built immutable image ID is required.");
        return image;
    }

    private static async Task<WorkerProcess.Outcome> RunIsolatedAsync(string image, string entrypoint,
        IEnumerable<string> mounts, IEnumerable<string> command, CancellationToken ct)
    {
        // An unpredictable name also lets cleanup address a container after
        // the attached Docker client has been killed or disconnected.
        var name = "locus-job-" + Guid.NewGuid().ToString("N");
        var user = FormattableString.Invariant($"{EffectiveUid()}:{EffectiveGid()}");
        var create = new List<string>
        {
            "create", "--pull", "never", "--name", name,
            "--label", "org.locus.worker=1",
            "--network", "none", "--ipc", "none", "--read-only",
            "--cap-drop", "ALL", "--security-opt", "no-new-privileges=true",
            "--user", user, "--pids-limit", "32",
            "--cpus", "1", "--memory", "768m", "--memory-swap", "768m",
            "--ulimit", "core=0:0", "--ulimit", "nofile=64:64",
            "--ulimit", "fsize=268435456:268435456",
            "--log-driver", "none", "--stop-timeout", "1",
            "--workdir", "/", "--env", "HOME=/nonexistent"
        };
        foreach (var mount in mounts) create.AddRange(["--mount", mount]);
        create.AddRange(["--entrypoint", entrypoint, image]);
        create.AddRange(command);

        try
        {
            var created = await WorkerProcess.ExecuteAsync(Docker(create), ct);
            if (created.ExitCode != 0)
                throw new InvalidOperationException("Cannot create isolated worker.");
            return await WorkerProcess.ExecuteAsync(Docker(["start", "--attach", name]), ct);
        }
        finally
        {
            // Independent bounded cleanup even if the job deadline elapsed.
            // A daemon failure is not a successful verification. Operators
            // must reap labeled orphan containers after daemon recovery.
            using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            var removed = await WorkerProcess.ExecuteAsync(Docker(["rm", "--force", name]), cleanup.Token);
            if (removed.ExitCode != 0)
                throw new InvalidOperationException("Worker container cleanup unconfirmed.");
        }
    }

    private static ProcessStartInfo Docker(IEnumerable<string> args)
    {
        // Fixed local daemon; no inherited database credentials, Docker
        // context, proxy settings or credential helpers from server env.
        var start = WorkerProcess.Start("/usr/bin/docker",
            new[] { "--host", "unix:///var/run/docker.sock", "--config", "/nonexistent" }.Concat(args));
        start.Environment["PATH"] = "/usr/bin:/bin";
        start.Environment["HOME"] = "/nonexistent";
        return start;
    }

    private static string Mount(string source, string target, bool readOnly) =>
        $"type=bind,source={source},target={target},bind-propagation=rprivate,bind-recursive=disabled" +
        (readOnly ? ",readonly" : "");

    internal static string SafePath(string input, bool directory)
    {
        if (!Path.IsPathFullyQualified(input) || input.Any(c => c < 32 || c == ',' || c == '"' || c == '\\'))
            throw new InvalidOperationException("Worker path is not an allowed absolute path.");
        var path = Path.TrimEndingDirectorySeparator(Path.GetFullPath(input));
        if (path == "/" || (directory ? !Directory.Exists(path) : !File.Exists(path)))
            throw new InvalidOperationException("Missing worker path.");
        for (var part = path; part != "/"; part = Path.GetDirectoryName(part)!)
            if ((File.GetAttributes(part) & FileAttributes.ReparsePoint) != 0)
                throw new InvalidOperationException("Symlinked worker path.");
        return path;
    }

    private sealed record MountInfo(string Target, string Root, string Options, string Type, string SuperOptions);

    private static IEnumerable<MountInfo> Mounts()
    {
        foreach (var line in File.ReadLines("/proc/self/mountinfo"))
        {
            var separator = line.IndexOf(" - ", StringComparison.Ordinal);
            if (separator < 0) throw new InvalidOperationException("Invalid mount information.");
            var left = line[..separator].Split(' ');
            var right = line[(separator + 3)..].Split(' ');
            if (left.Length < 6 || right.Length < 3)
                throw new InvalidOperationException("Invalid mount information.");
            // Linux mountinfo escapes spaces, tabs, newlines and backslashes.
            var target = left[4].Replace("\\040", " ").Replace("\\011", "\t")
                .Replace("\\012", "\n").Replace("\\134", "\\");
            yield return new MountInfo(target, left[3], left[5], right[0], right[2]);
        }
    }

    private static void RejectNestedMounts(string stage)
    {
        if (Mounts().Any(m => m.Target.StartsWith(stage + "/", StringComparison.Ordinal)))
            throw new InvalidOperationException("Nested mounts in worker stage.");
    }

    private static void ValidateStage(string stage)
    {
        var mounts = Mounts().ToArray();
        var exact = mounts.Where(m => m.Target == stage).ToArray();
        if (exact.Length != 1 || exact[0].Type != "tmpfs" || exact[0].Root != "/" ||
            mounts.Any(m => m.Target.StartsWith(stage + "/", StringComparison.Ordinal)))
            throw new InvalidOperationException("Inspection requires a dedicated tmpfs stage.");
        var flags = exact[0].Options.Split(',');
        if (!flags.Contains("nodev") || !flags.Contains("nosuid") || !flags.Contains("noexec"))
            throw new InvalidOperationException("Unsafe inspection stage mount options.");
        var inodeLimit = exact[0].SuperOptions.Split(',')
            .SingleOrDefault(option => option.StartsWith("nr_inodes=", StringComparison.Ordinal));
        if (inodeLimit is null || !int.TryParse(inodeLimit.AsSpan(10), NumberStyles.None,
                CultureInfo.InvariantCulture, out var inodes) || inodes is < 1 or > 1024)
            throw new InvalidOperationException("Inspection stage inode budget missing or excessive.");
        var capacity = new DriveInfo(stage).TotalSize;
        if (capacity <= 0 || capacity > StageLimit || Directory.EnumerateFileSystemEntries(stage).Any())
            throw new InvalidOperationException("Inspection stage is not empty or exceeds its disk budget.");
    }
}
