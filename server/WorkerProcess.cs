using System.Diagnostics;
using System.Text;
using System.Text.Json;

namespace Locus.Server;

// Operator only. An HTTP request must never choose executable, image,
// host paths or worker arguments. The public API has no Docker dependency.
internal static class WorkerProcess
{
    internal sealed record Outcome(int ExitCode, string Output);

    internal static async Task<int> RunAsync(string[] args, CancellationToken ct)
    {
        if (args.Length == 3 && args[0] == "media")
            return await WorkerMedia.RunAsync(args[1], args[2], ct);
        if (args.Length == 3 && args[0] == "prepare")
            return await PackagePreparation.RunAsync(args[1], args[2], ct);
        if (args.Length == 3 && args[0] == "store")
            return await PackageStorage.RunAsync(args[1], args[2], ct);
        var (response, exitCode) = await EvaluateAsync(args, ct);
        Write(response);
        return exitCode;
    }

    internal static void Write(object response) =>
        Console.WriteLine(JsonSerializer.Serialize(response, response.GetType(),
            new JsonSerializerOptions(JsonSerializerDefaults.Web)));

    internal static async Task<(WorkerContract.Response Response, int ExitCode)> EvaluateAsync(
        string[] args, CancellationToken ct)
    {
        if (!(args.Length == 3 && args[0] == "inspect") &&
            !(args.Length == 9 && args[0] == "verify"))
            throw new InvalidOperationException("Invalid worker arguments.");

        var request = args[0] == "verify" ? WorkerContract.ScoreRequest.Parse(args) : null;
        var mode = Environment.GetEnvironmentVariable("LOCUS_WORKER_MODE") ?? "container";
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.CancelAfter(TimeSpan.FromSeconds(60));
        Outcome outcome;
        if (mode == "container")
            outcome = await WorkerContainer.RunAsync(args, budget.Token);
        else if (mode == "trusted-local")
        {
            var executable = Environment.GetEnvironmentVariable("LOCUS_WORKER_PATH");
            if (string.IsNullOrWhiteSpace(executable) || !Path.IsPathFullyQualified(executable))
                throw new InvalidOperationException("LOCUS_WORKER_PATH must be an absolute executable path.");
            var start = Start(executable, args);
            if (OperatingSystem.IsWindows())
            {
                var systemRoot = Environment.GetEnvironmentVariable("SystemRoot");
                if (systemRoot is not null) start.Environment["SystemRoot"] = systemRoot;
            }
            outcome = await ExecuteAsync(start, budget.Token);
        }
        else throw new InvalidOperationException("Unknown worker mode.");

        var response = WorkerContract.Parse(outcome.Output, outcome.ExitCode, request);
        return (response, outcome.ExitCode);
    }

    internal static ProcessStartInfo Start(string executable, IEnumerable<string> args)
    {
        var start = new ProcessStartInfo(executable)
        {
            UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true,
            RedirectStandardInput = true, CreateNoWindow = true
        };
        start.Environment.Clear();
        foreach (var arg in args) start.ArgumentList.Add(arg);
        return start;
    }

    internal static async Task<Outcome> ExecuteAsync(ProcessStartInfo start, CancellationToken ct)
    {
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.Token.ThrowIfCancellationRequested();
        using var process = new Process { StartInfo = start };
        if (!process.Start()) throw new InvalidOperationException("Cannot start worker process.");
        using var cancellation = budget.Token.Register(() =>
        {
            try { if (!process.HasExited) process.Kill(entireProcessTree: true); }
            catch (InvalidOperationException) { }
            catch (System.ComponentModel.Win32Exception) { }
        });
        process.StandardInput.Close();
        var stdout = ReadBoundedAsync(process.StandardOutput.BaseStream, 1024 * 1024, budget);
        var stderr = ReadBoundedAsync(process.StandardError.BaseStream, 64 * 1024, budget);
        try
        {
            await process.WaitForExitAsync(budget.Token);
            await Task.WhenAll(stdout, stderr);
            return new Outcome(process.ExitCode, new UTF8Encoding(false, true).GetString(await stdout));
        }
        finally
        {
            budget.Cancel();
            try { await Task.WhenAll(stdout, stderr); } catch (Exception) { }
        }
    }

    private static async Task<byte[]> ReadBoundedAsync(Stream reader, int limit,
        CancellationTokenSource budget)
    {
        try
        {
            using var result = new MemoryStream();
            var buffer = new byte[4096];
            int count;
            while ((count = await reader.ReadAsync(buffer.AsMemory(), budget.Token)) != 0)
            {
                if (count > limit - result.Length)
                    throw new InvalidOperationException("Worker output exceeds limit.");
                result.Write(buffer, 0, count);
            }
            return result.ToArray();
        }
        catch
        {
            budget.Cancel();
            throw;
        }
    }
}
