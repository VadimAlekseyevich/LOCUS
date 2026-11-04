@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem LOCUS Windows launcher. Double-click or run from Command Prompt.
rem Default: F-07 placeholder screens with debug F1-F4 navigation; 'audio' runs A-07.
rem Run a temporary copy so git pull can safely update this BAT file.

if /I "%LOCUS_BAT_WORKER%"=="1" goto :worker
set "LOCUS_BAT_WORKER=1"
set "LOCUS_BAT_ROOT=%~dp0"
set "LOCUS_TEMP_COPY=%TEMP%\locus-build-run-%RANDOM%-%RANDOM%.bat"
copy /y "%~f0" "%LOCUS_TEMP_COPY%" >nul
if errorlevel 1 (
    echo [LOCUS] Failed to create a temporary launcher copy.
    pause
    exit /b 1
)
call "%LOCUS_TEMP_COPY%" %*
set "LOCUS_RETURN_CODE=%ERRORLEVEL%"
del /q "%LOCUS_TEMP_COPY%" >nul 2>&1
exit /b %LOCUS_RETURN_CODE%

:worker
cd /d "%LOCUS_BAT_ROOT%"
if errorlevel 1 (
    echo [LOCUS] Cannot open the project directory.
    exit /b 1
)
set "ROOT=%CD%"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=run"
if /I "%MODE%"=="run" goto :mode_valid
if /I "%MODE%"=="game" goto :mode_valid
if /I "%MODE%"=="baseline" goto :mode_valid
if /I "%MODE%"=="build" goto :mode_valid
if /I "%MODE%"=="audio" goto :mode_valid
if /I "%MODE%"=="test" goto :mode_valid
if /I "%MODE%"=="smoke" goto :mode_valid
echo Usage: build_and_run.bat [run ^| build ^| game ^| baseline ^| audio ^| test ^| smoke]
exit /b 2

:mode_valid
set "STAMP="
for /f "delims=" %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%T"
if not defined STAMP set "STAMP=%RANDOM%"
set "LOG_CATEGORY=a07"
if /I "%MODE%"=="run" set "LOG_CATEGORY=game"
if /I "%MODE%"=="game" set "LOG_CATEGORY=game"
if /I "%MODE%"=="baseline" set "LOG_CATEGORY=game"
if /I "%MODE%"=="build" set "LOG_CATEGORY=game"
set "LOG_ARCHIVE=LOCUS-%LOG_CATEGORY%-logs.zip"
set "LOCUS_LOG_ARCHIVE=%LOG_ARCHIVE%"
set "LOG_DIR=%ROOT%\.locus-runs\%LOG_CATEGORY%-%STAMP%-%RANDOM%"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
if not exist "%LOG_DIR%" (
    echo [LOCUS] Cannot create the log directory.
    exit /b 1
)
set "LOCUS_LOG_DIR=%LOG_DIR%"
set "STATUS=1"
> "%LOG_DIR%\run-info.txt" echo mode=%MODE%
>> "%LOG_DIR%\run-info.txt" echo component=%LOG_CATEGORY%
>> "%LOG_DIR%\run-info.txt" echo launcher=build_and_run.bat
>> "%LOG_DIR%\run-info.txt" echo timestamp=%STAMP%

echo.
echo ============================================================
echo   LOCUS / Windows build and run
echo   Mode: %MODE%
echo   Project: %ROOT%
echo ============================================================
echo.

call :ensure_git
if errorlevel 1 goto :failed

if /I "%LOCUS_SKIP_PULL%"=="1" (
    echo [LOCUS] Skipping git pull by explicit LOCUS_SKIP_PULL=1.
    > "%LOG_DIR%\git-update.log" echo Git update skipped by LOCUS_SKIP_PULL.
) else (
    set "DIRTY="
    for /f "delims=" %%D in ('git -C "%ROOT%" status --porcelain 2^>nul') do set "DIRTY=1"
    if defined DIRTY (
        echo [LOCUS] Local uncommitted changes found. Nothing will be overwritten.
        > "%LOG_DIR%\git-update.log" echo Git update blocked: working tree has changes.
        goto :failed
    )
    echo [LOCUS] Pulling the latest changes - fast-forward only...
    git -C "%ROOT%" pull --ff-only > "%LOG_DIR%\git-update.log" 2>&1
    if errorlevel 1 (
        echo [LOCUS] Git pull failed. Check git-update.log in the archive.
        goto :failed
    )
    type "%LOG_DIR%\git-update.log"
)
for /f "delims=" %%H in ('git -C "%ROOT%" rev-parse HEAD 2^>nul') do >> "%LOG_DIR%\run-info.txt" echo git_commit=%%H

call :ensure_cmake
if errorlevel 1 goto :failed
call :ensure_msvc
if errorlevel 1 goto :failed

if /I "%MODE%"=="run" goto :build_game
if /I "%MODE%"=="game" goto :build_game
if /I "%MODE%"=="baseline" goto :build_game
if /I "%MODE%"=="build" goto :build_game

set "SOURCE=%ROOT%\mvp\spikes\raylib_audio"
set "BUILD=%ROOT%\build-a07"
if not exist "%SOURCE%\CMakeLists.txt" (
    echo [LOCUS] Audio probe source is missing: %SOURCE%
    goto :failed
)

echo.
echo [LOCUS] Configuring raylib 6.0 / Windows x64 Release...
cmake -S "%SOURCE%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 > "%LOG_DIR%\configure.log" 2>&1
if errorlevel 1 (
    echo [LOCUS] CMake configuration FAILED.
    call :show_log "configure.log"
    goto :failed
)

echo [LOCUS] Compiling...
cmake --build "%BUILD%" --config Release --parallel 2 > "%LOG_DIR%\build.log" 2>&1
if errorlevel 1 (
    echo [LOCUS] Compilation FAILED.
    call :show_log "build.log"
    goto :failed
)

set "EXE=%BUILD%\Release\locus_raylib_audio_spike.exe"
if not exist "%EXE%" (
    echo [LOCUS] Compiled EXE was not found: %EXE%
    goto :failed
)
for %%F in ("%EXE%") do >> "%LOG_DIR%\run-info.txt" echo exe_size_bytes=%%~zF
echo [LOCUS] Build succeeded: %EXE%

rem Run in the log folder, so the generated WAV and CSV never clutter the repo.
pushd "%LOG_DIR%"
echo [LOCUS] Running headless WAV self-test...
"%EXE%" --self-test > "%LOG_DIR%\self-test.log" 2>&1
if errorlevel 1 (
    popd
    echo [LOCUS] Self-test FAILED.
    call :show_log "self-test.log"
    goto :failed
)
type "%LOG_DIR%\self-test.log"
popd
if /I "%MODE%"=="test" goto :success

pushd "%LOG_DIR%"
echo [LOCUS] Testing your local audio device: play / pause / seek / resume...
"%EXE%" --audio-smoke > "%LOG_DIR%\audio-smoke.log" 2>&1
if errorlevel 1 (
    popd
    echo [LOCUS] Audio smoke test FAILED. Is a Windows audio output device enabled?
    call :show_log "audio-smoke.log"
    goto :failed
)
type "%LOG_DIR%\audio-smoke.log"
popd
if /I "%MODE%"=="smoke" goto :success

echo.
echo ============================================================
echo   A-07 TEST WINDOW - NOT THE PLAYABLE LOCUS GAME
echo   Listen to clicks: 120 BPM, one click every 0.5 seconds.
echo   A / D = tap on beat and record timestamp
echo   Space / P = pause/resume     J / L = seek -/+2 seconds
echo   R = restart                 Esc = close
echo ============================================================
echo.
pushd "%LOG_DIR%"
"%EXE%" > "%LOG_DIR%\probe-console.log" 2>&1
set "PROBE_EXIT=%ERRORLEVEL%"
popd
if not "%PROBE_EXIT%"=="0" (
    echo [LOCUS] Interactive probe exited with code %PROBE_EXIT%.
    call :show_log "probe-console.log"
    goto :failed
)

if /I not "%LOCUS_NO_PROMPT%"=="1" if not defined GITHUB_ACTIONS (
    echo.
    echo [LOCUS] Optional feedback. Press Enter to skip.
    powershell -NoProfile -Command "$f=Join-Path $env:LOCUS_LOG_DIR 'user-feedback.txt'; @('A-07 local subjective observations','audible_and_even='+(Read-Host 'Were metronome clicks audible and evenly spaced?'),'pause_seek_restart='+(Read-Host 'Did pause, seek and restart work?'),'tap_feel='+(Read-Host 'How did A/D input feel?'),'other='+(Read-Host 'Any glitches or comments?')) | Set-Content -LiteralPath $f -Encoding UTF8"
)
goto :success

:build_game
rem F-05 builds the real production target; the original probe is still available via 'audio'.
set "GAME_BUILD=%ROOT%\build-a07\foundation"
echo.
echo [LOCUS] Configuring the production LOCUS window (Windows x64 Release)...
cmake -S "%ROOT%" -B "%GAME_BUILD%" -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=OFF -DLOCUS_VERIFY_DEPENDENCIES=OFF > "%LOG_DIR%\configure.log" 2>&1
if errorlevel 1 (
    echo [LOCUS] Production CMake configuration FAILED.
    call :show_log "configure.log"
    goto :failed
)

echo [LOCUS] Compiling the production LOCUS window...
cmake --build "%GAME_BUILD%" --config Release --target locus --parallel 2 > "%LOG_DIR%\build.log" 2>&1
if errorlevel 1 (
    echo [LOCUS] Production compilation FAILED.
    call :show_log "build.log"
    goto :failed
)

set "GAME_EXE=%GAME_BUILD%\Release\locus.exe"
if not exist "%GAME_EXE%" (
    echo [LOCUS] Compiled game EXE not found: %GAME_EXE%
    goto :failed
)
for %%F in ("%GAME_EXE%") do >> "%LOG_DIR%\run-info.txt" echo exe_size_bytes=%%~zF
echo [LOCUS] Production build succeeded: %GAME_EXE%
if /I "%MODE%"=="build" goto :success
if /I "%MODE%"=="baseline" (
    echo.
    echo [LOCUS] F-09 local baseline: leave the LOCUS window untouched for 12 seconds.
    echo [LOCUS] Once sampling ends, close the window with Esc or X.
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\tools\measure-baseline.ps1" -Executable "%GAME_EXE%" -OutputDirectory "%LOG_DIR%" -Mode Interactive -SampleSeconds 12
    if errorlevel 1 (
        echo [LOCUS] Local performance baseline FAILED or was interrupted.
        goto :failed
    )
    echo [LOCUS] Baseline complete - see baseline-summary.txt and baseline-idle-samples.csv in the ZIP.
    goto :success
)

echo.
echo ============================================================
echo  LOCUS DEMO GRID - GAMEPLAY IN DEVELOPMENT
echo  F1 = Home   F2 = Play grid   F3 = Editor   F4 = Results
echo  Play currently shows the 4x4 grid and stationary starting marker.
echo  Close with ESC or the X button in the window title bar.
echo ============================================================
echo.
pushd "%LOG_DIR%"
"%GAME_EXE%" --debug-screens > "%LOG_DIR%\game-console.log" 2>&1
set "GAME_EXIT=%ERRORLEVEL%"
popd
if not "%GAME_EXIT%"=="0" (
    echo [LOCUS] Game window exited with code %GAME_EXIT%.
    call :show_log "game-console.log"
    goto :failed
)
type "%LOG_DIR%\game-console.log"
goto :success

:ensure_git
where git >nul 2>&1
if not errorlevel 1 exit /b 0
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>&1
if not errorlevel 1 exit /b 0
echo [LOCUS] Git is missing.
call :install_winget Git.Git
if errorlevel 1 exit /b 1
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>&1
if errorlevel 1 echo [LOCUS] Restart this BAT after installing Git.
where git >nul 2>&1
exit /b %ERRORLEVEL%

:ensure_cmake
where cmake >nul 2>&1
if not errorlevel 1 exit /b 0
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where cmake >nul 2>&1
if not errorlevel 1 exit /b 0
echo [LOCUS] CMake is missing.
call :install_winget Kitware.CMake
if errorlevel 1 exit /b 1
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where cmake >nul 2>&1
if errorlevel 1 echo [LOCUS] Restart this BAT after installing CMake.
where cmake >nul 2>&1
exit /b %ERRORLEVEL%

:ensure_msvc
call :check_msvc
if not errorlevel 1 exit /b 0
echo.
echo [LOCUS] Visual Studio 2022 C++ Build Tools are missing.
echo [LOCUS] This may download several GB and can require administrator approval.
call :install_winget Microsoft.VisualStudio.2022.BuildTools
if errorlevel 1 exit /b 1
call :check_msvc
if errorlevel 1 echo [LOCUS] Restart Windows or the BAT if the C++ workload installation just finished.
exit /b %ERRORLEVEL%

:check_msvc
set "MSVC_FOUND="
for %%E in (BuildTools Community Professional Enterprise) do if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\MSVC" set "MSVC_FOUND=1"
if defined MSVC_FOUND exit /b 0
exit /b 1

:install_winget
where winget >nul 2>&1
if errorlevel 1 (
    echo [LOCUS] Cannot auto-install %~1: winget is unavailable.
    echo [LOCUS] See README.md for manual installation instructions.
    exit /b 1
)
echo.
choice /C YN /M "Install %~1 with Windows Package Manager?"
if errorlevel 2 exit /b 1
echo [LOCUS] Installing %~1... see dependencies.log
if /I "%~1"=="Microsoft.VisualStudio.2022.BuildTools" (
    winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --accept-source-agreements --accept-package-agreements --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" >> "%LOG_DIR%\dependencies.log" 2>&1
) else (
    winget install --id "%~1" --exact --accept-source-agreements --accept-package-agreements >> "%LOG_DIR%\dependencies.log" 2>&1
)
if errorlevel 1 (
    echo [LOCUS] Dependency install failed. Review dependencies.log.
    call :show_log "dependencies.log"
    exit /b 1
)
exit /b 0

:show_log
if exist "%LOG_DIR%\%~1" (
    echo [LOCUS] Last 40 lines from %~1:
    powershell -NoProfile -Command "Get-Content -LiteralPath (Join-Path $env:LOCUS_LOG_DIR '%~1') -Tail 40"
)
exit /b 0

:success
set "STATUS=0"
echo.
echo [LOCUS] SUCCESS.
goto :finish

:failed
set "STATUS=1"
echo.
echo [LOCUS] FAILED. See the logs below.

:finish
>> "%LOG_DIR%\run-info.txt" echo exit_code=%STATUS%
echo [LOCUS] Creating a ZIP with compiler, test and input timing logs...
powershell -NoProfile -Command "$dir=$env:LOCUS_LOG_DIR; $files=@(Get-ChildItem -LiteralPath $dir -File | Where-Object { $_.Extension -in '.txt','.log','.csv' }); if($files.Count -gt 0) { Compress-Archive -LiteralPath $files.FullName -DestinationPath (Join-Path $dir $env:LOCUS_LOG_ARCHIVE) -Force }"
echo.
echo ============================================================
echo  REPORT FOR CHATGPT
echo  %LOG_DIR%\%LOG_ARCHIVE%
echo ============================================================
echo  This archive contains NO audio and NO executable.
echo  Review paths in the logs before sharing them.
echo.
if /I not "%LOCUS_NO_PAUSE%"=="1" if not defined GITHUB_ACTIONS pause
exit /b %STATUS%
