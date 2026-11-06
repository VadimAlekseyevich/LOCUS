# LOCUS F-09 performance-baseline collection for Windows PowerShell 5.1+.
# Measures release EXE metadata and launch time in both modes.
# Interactive mode additionally observes the actual raylib GUI at idle.
# Do not use these measurements to infer audio DAC/input latency.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [ValidateSet("Headless", "Interactive")]
    [string]$Mode = "Headless",

    [ValidateRange(3, 120)]
    [int]$SampleSeconds = 12
)

$ErrorActionPreference = "Stop"
$culture = [System.Globalization.CultureInfo]::InvariantCulture

function Format-Number([double]$value) {
    return $value.ToString("F2", $culture)
}

function Add-Metric([string]$key, [object]$value) {
    $script:report.Add("$key=$value") | Out-Null
}

$exe = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$info = Get-Item -LiteralPath $exe
$report = New-Object "System.Collections.Generic.List[string]"
$logicalProcessors = [Environment]::ProcessorCount

$os = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
$processor = Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1
$computer = Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue
# Actions supplies a definitive checkout revision even when Git lookup is blocked.
$gitRevision = $env:GITHUB_SHA
if ([string]::IsNullOrWhiteSpace($gitRevision)) {
    $repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        $gitRevision = (& git -C $repositoryRoot rev-parse HEAD 2>$null | Select-Object -First 1)
        if ($LASTEXITCODE -ne 0) { $gitRevision = $null }
    }
}
if ([string]::IsNullOrWhiteSpace($gitRevision)) { $gitRevision = "unknown" }

Add-Metric "baseline_version" "F-09-v1"
Add-Metric "measurement_mode" $Mode
Add-Metric "utc_timestamp" ([DateTimeOffset]::UtcNow.ToString("o"))
Add-Metric "git_commit" $gitRevision
Add-Metric "windows_version" $(if ($os) { $os.Caption + " " + $os.Version } else { [Environment]::OSVersion.VersionString })
Add-Metric "windows_build" $(if ($os) { $os.BuildNumber } else { "unknown" })
Add-Metric "cpu_model" $(if ($processor) { $processor.Name.Trim() } else { "unknown" })
Add-Metric "logical_processors" $logicalProcessors
Add-Metric "system_ram_gib" $(if ($computer) { Format-Number ([double]$computer.TotalPhysicalMemory / 1GB) } else { "unknown" })
Add-Metric "powershell_version" $PSVersionTable.PSVersion
Add-Metric "executable_bytes" $info.Length
Add-Metric "executable_sha256" (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
Add-Metric "executable_path" $exe

# The first run after starting this script is a startup proxy, NOT a true
# boot-cold or disk-cache-flushed startup. It includes Windows process startup,
# initialization, and natural exit of the headless foundation check.
$headlessLog = Join-Path $output "baseline-foundation-output.txt"
$watch = [System.Diagnostics.Stopwatch]::StartNew()
& $exe --foundation-check | Set-Content -LiteralPath $headlessLog -Encoding UTF8
$exitCode = $LASTEXITCODE
$watch.Stop()
if ($exitCode -ne 0 -or -not ((Get-Content -LiteralPath $headlessLog -Raw) -match "LOCUS foundation built")) {
    throw "The production headless startup check failed (exit code $exitCode)."
}
Add-Metric "first_headless_launch_exit_ms" (Format-Number $watch.Elapsed.TotalMilliseconds)
$warm = New-Object "System.Collections.Generic.List[double]"
for ($n = 0; $n -lt 5; $n++) {
    $watch.Restart()
    & $exe --foundation-check | Out-Null
    $exitCode = $LASTEXITCODE
    $watch.Stop()
    if ($exitCode -ne 0) { throw "Repeated foundation check failed (exit code $exitCode)." }
    $warm.Add($watch.Elapsed.TotalMilliseconds) | Out-Null
}
$sorted = @($warm | Sort-Object)
Add-Metric "warm_headless_launch_exit_median_ms" (Format-Number $sorted[2])
Add-Metric "warm_headless_launch_repeats" "5"
Add-Metric "headless_startup_definition" "stopwatch before invocation to process exit; no GUI/window"
Add-Metric "true_cold_boot_startup_measured" "false"
Add-Metric "audio_latency_measured" "false"
Add-Metric "input_latency_measured" "false"

if ($Mode -eq "Interactive") {
    Write-Host ""
    Write-Host "[LOCUS] A blank LOCUS window will open. Do not interact with it for $SampleSeconds seconds."
    Write-Host "[LOCUS] After sampling, press Esc on Home or X, then Y to confirm exit."
    $guiStdout = Join-Path $output "baseline-gui-output.txt"
    $guiStderr = Join-Path $output "baseline-gui-error.txt"
    $guiWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $gui = Start-Process -FilePath $exe -ArgumentList "--debug-screens" -PassThru -RedirectStandardOutput $guiStdout -RedirectStandardError $guiStderr
    $guiHandleFound = $false
    while ($guiWatch.Elapsed.TotalSeconds -lt 10) {
        Start-Sleep -Milliseconds 40
        $gui.Refresh()
        if ($gui.HasExited) { break }
        if ($gui.MainWindowHandle -ne [IntPtr]::Zero) {
            $guiHandleFound = $true
            break
        }
    }
    if (-not $guiHandleFound) {
        Add-Metric "gui_window_detection" "failed"
        $report | Set-Content -LiteralPath (Join-Path $output "baseline-summary.txt") -Encoding UTF8
        throw "No responsive Windows window handle detected within 10 seconds. Check the GUI logs."
    }
    Add-Metric "gui_window_handle_ready_ms" (Format-Number $guiWatch.Elapsed.TotalMilliseconds)
    Add-Metric "gui_window_detection" "success"
    Add-Metric "gui_startup_definition" "process start to a nonzero MainWindowHandle; not first displayed frame"

    $samples = New-Object "System.Collections.Generic.List[object]"
    $idleWatch = [System.Diagnostics.Stopwatch]::StartNew()
    $gui.Refresh()
    $previousCpuSeconds = $gui.TotalProcessorTime.TotalSeconds
    $previousWallSeconds = $idleWatch.Elapsed.TotalSeconds
    for ($n = 0; $n -lt $SampleSeconds; $n++) {
        Start-Sleep -Seconds 1
        $gui.Refresh()
        if ($gui.HasExited) {
            Add-Metric "gui_exited_during_idle_sample" "true"
            break
        }
        $cpuSeconds = $gui.TotalProcessorTime.TotalSeconds
        $wallSeconds = $idleWatch.Elapsed.TotalSeconds
        $deltaSeconds = $wallSeconds - $previousWallSeconds
        $coreCpuPercent = if ($deltaSeconds -gt 0) {
            100.0 * ($cpuSeconds - $previousCpuSeconds) / $deltaSeconds
        } else { 0.0 }
        $samples.Add([PSCustomObject]@{
            elapsed_seconds = [Math]::Round($wallSeconds, 2)
            working_set_mib = [Math]::Round($gui.WorkingSet64 / 1MB, 2)
            private_bytes_mib = [Math]::Round($gui.PrivateMemorySize64 / 1MB, 2)
            cpu_one_core_percent = [Math]::Round($coreCpuPercent, 2)
        }) | Out-Null
        $previousCpuSeconds = $cpuSeconds
        $previousWallSeconds = $wallSeconds
    }
    $csvPath = Join-Path $output "baseline-idle-samples.csv"
    if ($samples.Count -gt 0) {
        $samples | Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8
    }
    Add-Metric "gui_idle_samples" $samples.Count
    Add-Metric "gui_idle_requested_seconds" $SampleSeconds
    Add-Metric "cpu_percent_definition" "CPU seconds per wall second, 100% means one logical core"
    if ($samples.Count -ge 3) {
        $working = $samples | Measure-Object working_set_mib -Average -Maximum
        $private = $samples | Measure-Object private_bytes_mib -Average -Maximum
        $cpu = $samples | Measure-Object cpu_one_core_percent -Average -Maximum
        Add-Metric "idle_working_set_mean_mib" (Format-Number $working.Average)
        Add-Metric "idle_working_set_peak_mib" (Format-Number $working.Maximum)
        Add-Metric "idle_private_mean_mib" (Format-Number $private.Average)
        Add-Metric "idle_cpu_one_core_mean_percent" (Format-Number $cpu.Average)
        Add-Metric "idle_cpu_one_core_peak_percent" (Format-Number $cpu.Maximum)
        Add-Metric "gui_idle_observation" "valid_samples_recorded"
    } else {
        Add-Metric "gui_idle_observation" "insufficient_samples"
    }
    if (-not $gui.HasExited) {
        Write-Host "[LOCUS] Sampling complete. Press Esc on Home or X, then Y / QUIT in the confirmation."
        $gui.WaitForExit()
    }
    $gui.Refresh()
    Add-Metric "gui_exit_code" $gui.ExitCode
    if ($gui.ExitCode -ne 0) {
        Add-Metric "gui_shutdown" "error"
    } else {
        Add-Metric "gui_shutdown" "normal"
    }
} else {
    Add-Metric "gui_window_detection" "not_measured_headless"
    Add-Metric "gui_idle_observation" "NOT_MEASURED_hosted_runner_has_no_interactive_desktop"
}

$summaryPath = Join-Path $output "baseline-summary.txt"
$report | Set-Content -LiteralPath $summaryPath -Encoding UTF8
$report | ForEach-Object { Write-Output $_ }
if ($Mode -eq "Interactive" -and $gui.ExitCode -ne 0) {
    throw "LOCUS window exited with error code $($gui.ExitCode)."
}
Write-Output "F09_BASELINE_COLLECTION_PASS"
