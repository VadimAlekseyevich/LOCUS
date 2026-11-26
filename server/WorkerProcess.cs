using System.Diagnostics;
using System.Text;
using System.Text.Json;

namespace Locus.Server;

// Only an operator can invoke this adapter today. Do not expose arbitrary
// worker arguments as HTTP parameters. A future scheduler resolves trusted IDs.
internal static class WorkerProcess
{
    internal static async Task<int> RunAsync(string[] args, CancellationToken ct)
    {
        var executable = Environment.GetEnvironmentVariable("LOCUS_WORKER_PATH");
        if (string.IsNullOrWhiteSpace(executable) || !Path.IsPathFullyQualified(executable))
            throw new InvalidOperationException("LOCUS_WORKER_PATH must be an absolute executable path.");
        if (!(args.Length == 3 && args[0] == "inspect") &&
            !(args.Length == 9 && args[0] == "verify"))
            throw new InvalidOperationException("Invalid worker arguments.");
        var start = new ProcessStartInfo(executable)
        {
            UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8,
            CreateNoWindow = true
        };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        using var process = new Process { StartInfo = start };
        using var budget = CancellationTokenSource.CreateLinkedTokenSource(ct);
        budget.CancelAfter(TimeSpan.FromSeconds(60));
        if (!process.Start()) throw new InvalidOperationException("Cannot start worker.");
        using var cancellation = budget.Token.Register(() =>
        {
            try { if (!process.HasExited) process.Kill(entireProcessTree: true); }
            catch (InvalidOperationException) { }
            catch (System.ComponentModel.Win32Exception) { }
        });
        var stdout = ReadBoundedAsync(process.StandardOutput, 1024 * 1024, budget);
        var stderr = ReadBoundedAsync(process.StandardError, 64 * 1024, budget);
        try
        {
            await process.WaitForExitAsync(budget.Token);
            await Task.WhenAll(stdout, stderr);
            using var json = JsonDocument.Parse(await stdout);
            var root = json.RootElement;
            if (root.GetProperty("protocolVersion").GetInt32() != 1)
                throw new InvalidOperationException("Unsupported worker protocol.");
            var ok = root.GetProperty("ok").GetBoolean();
            if ((ok && process.ExitCode != 0) || (!ok && process.ExitCode != 2))
                throw new InvalidOperationException("Unexpected worker outcome.");
            if (ok && (args[0] == "inspect"
                ? root.GetProperty("kind").GetString() != "packageInspection" || root.GetProperty("mediaValidated").GetBoolean()
                : root.GetProperty("kind").GetString() != "scoreEvaluation" || root.GetProperty("rankApproved").GetBoolean()))
                throw new InvalidOperationException("Unexpected worker response kind.");
            Console.WriteLine(root.GetRawText());
            return process.ExitCode;
        }
        finally
        {
            // Unblock both pipes even if the other stream or protocol fails.
            budget.Cancel();
            try { await Task.WhenAll(stdout, stderr); } catch (Exception) { }
        }
    }

    private static async Task<string> ReadBoundedAsync(StreamReader reader, int limit,
        CancellationTokenSource budget)
    {
        try
        {
            var text = new StringBuilder();
            var buffer = new char[4096];
            int count;
            while ((count = await reader.ReadAsync(buffer.AsMemory(), budget.Token)) != 0)
            {
                if (count > limit - text.Length)
                    throw new InvalidOperationException("Worker output exceeds limit.");
                text.Append(buffer, 0, count);
            }
            return text.ToString();
        }
        catch
        {
            budget.Cancel();
            throw;
        }
    }
}
