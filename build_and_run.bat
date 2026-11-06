@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem LOCUS - Windows game launcher. Modes: run, build, test.
rem Run from a temporary copy so git pull may update this BAT safely.

if /I "%LOCUS_BAT_WORKER%"=="1" goto :worker
set "LOCUS_BAT_WORKER=1"
set "LOCUS_BAT_ROOT=%~dp0"
set "LOCUS_TEMP_COPY=%TEMP%\locus-run-%RANDOM%-%RANDOM%.bat"
copy /y "%~f0" "%LOCUS_TEMP_COPY%" >nul
if errorlevel 1 (
    echo [LOCUS] Cannot create a temporary launcher copy.
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
if /I "%MODE%"=="run" goto :valid
if /I "%MODE%"=="build" goto :valid
if /I "%MODE%"=="test" goto :valid
echo Usage: build_and_run.bat [run ^| build ^| test]
exit /b 2

:valid
set "STAMP="
for /f "delims=" %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%T"
if not defined STAMP set "STAMP=%RANDOM%"
set "LOG_DIR=%ROOT%\.locus-runs\game-%STAMP%-%RANDOM%"
set "LOG_ARCHIVE=LOCUS-game-logs.zip"
mkdir "%LOG_DIR%" >nul 2>&1
if not exist "%LOG_DIR%" (
    echo [LOCUS] Cannot create log directory.
    exit /b 1
)
set "LOCUS_LOG_DIR=%LOG_DIR%"
set "LOCUS_LOG_ARCHIVE=%LOG_ARCHIVE%"
rem Restore the established local incremental build tree. GitHub CI keeps
rem its separate build/windows tree.
set "BUILD=%ROOT%\build-a07\foundation"
set "STATUS=1"
> "%LOG_DIR%\run-info.txt" echo mode=%MODE%
>> "%LOG_DIR%\run-info.txt" echo launcher=build_and_run.bat
>> "%LOG_DIR%\run-info.txt" echo timestamp=%STAMP%
>> "%LOG_DIR%\run-info.txt" echo build_dir=%BUILD%
echo.
echo ============================================================
echo   LOCUS / Windows build and run
echo   Mode: %MODE%
echo   Project: %ROOT%
echo   Build directory: %BUILD%
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
        echo [LOCUS] Local changes detected. Git update canceled.
        > "%LOG_DIR%\git-update.log" echo Git update blocked by local changes.
        goto :failed
    )
    echo [LOCUS] Pulling latest changes - fast-forward only...
    git -C "%ROOT%" pull --ff-only > "%LOG_DIR%\git-update.log" 2>&1
    if errorlevel 1 (
        echo [LOCUS] Git pull failed.
        call :show_log "git-update.log"
        goto :failed
    )
    type "%LOG_DIR%\git-update.log"
)
for /f "delims=" %%H in ('git -C "%ROOT%" rev-parse HEAD 2^>nul') do >> "%LOG_DIR%\run-info.txt" echo git_commit=%%H
call :ensure_cmake
if errorlevel 1 goto :failed
call :ensure_msvc
if errorlevel 1 goto :failed

set "TESTS=OFF"
if /I "%MODE%"=="test" set "TESTS=ON"
echo [LOCUS] Configuring LOCUS (Windows x64 Release)...
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=%TESTS% > "%LOG_DIR%\configure.log" 2>&1
if errorlevel 1 (
    echo [LOCUS] CMake configuration FAILED.
    call :show_log "configure.log"
    findstr /C:"Failed to remove directory" "%LOG_DIR%\configure.log" >nul 2>&1
    if not errorlevel 1 (
        echo [LOCUS] A dependency source directory could not be removed.
        echo [LOCUS] Close Visual Studio, other build processes and file explorers.
        echo [LOCUS] Check file locks before removing only the build directory.
        echo [LOCUS] The build directory is: %BUILD%
    )
    goto :failed
)

echo [LOCUS] Compiling LOCUS (Release)...
if /I "%MODE%"=="test" (
    cmake --build "%BUILD%" --config Release --parallel 2 > "%LOG_DIR%\build.log" 2>&1
) else (
    cmake --build "%BUILD%" --config Release --target locus --parallel 2 > "%LOG_DIR%\build.log" 2>&1
)
if errorlevel 1 (
    echo [LOCUS] Compilation FAILED.
    call :show_log "build.log"
    goto :failed
)

if /I "%MODE%"=="test" (
    echo [LOCUS] Running tests...
    ctest --test-dir "%BUILD%" -C Release --output-on-failure > "%LOG_DIR%\tests.log" 2>&1
    if errorlevel 1 (
        call :show_log "tests.log"
        goto :failed
    )
    type "%LOG_DIR%\tests.log"
    goto :success
)

set "GAME_EXE=%BUILD%\Release\locus.exe"
if not exist "%GAME_EXE%" (
    echo [LOCUS] Executable not found: %GAME_EXE%
    goto :failed
)
for %%F in ("%GAME_EXE%") do >> "%LOG_DIR%\run-info.txt" echo exe_size_bytes=%%~zF
echo [LOCUS] Ready: %GAME_EXE%
if /I "%MODE%"=="build" goto :success
pushd "%LOG_DIR%"
"%GAME_EXE%" > "%LOG_DIR%\game-console.log" 2>&1
set "GAME_EXIT=%ERRORLEVEL%"
popd
if not "%GAME_EXIT%"=="0" (
    echo [LOCUS] Game exited with error code %GAME_EXIT%.
    call :show_log "game-console.log"
    goto :failed
)
if exist "%LOG_DIR%\game-console.log" type "%LOG_DIR%\game-console.log"
goto :success

:ensure_git
where git >nul 2>&1
if not errorlevel 1 exit /b 0
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>&1
if not errorlevel 1 exit /b 0
call :install_winget Git.Git
if errorlevel 1 exit /b 1
if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>&1
exit /b %ERRORLEVEL%

:ensure_cmake
where cmake >nul 2>&1
if not errorlevel 1 exit /b 0
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where cmake >nul 2>&1
if not errorlevel 1 exit /b 0
call :install_winget Kitware.CMake
if errorlevel 1 exit /b 1
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "PATH=%ProgramFiles%\CMake\bin;%PATH%"
where cmake >nul 2>&1
exit /b %ERRORLEVEL%

:ensure_msvc
call :check_msvc
if not errorlevel 1 exit /b 0
echo [LOCUS] Visual Studio 2022 C++ Build Tools missing.
call :install_winget Microsoft.VisualStudio.2022.BuildTools
if errorlevel 1 exit /b 1
call :check_msvc
exit /b %ERRORLEVEL%

:check_msvc
set "MSVC_FOUND="
for %%E in (BuildTools Community Professional Enterprise) do if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Tools\MSVC" set "MSVC_FOUND=1"
if defined MSVC_FOUND exit /b 0
exit /b 1

:install_winget
where winget >nul 2>&1
if errorlevel 1 (
    echo [LOCUS] Please install %~1 manually; winget is unavailable.
    exit /b 1
)
choice /C YN /M "Install %~1 using winget?"
if errorlevel 2 exit /b 1
if /I "%~1"=="Microsoft.VisualStudio.2022.BuildTools" (
    winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --accept-source-agreements --accept-package-agreements --override "--passive --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" > "%LOG_DIR%\dependencies.log" 2>&1
) else (
    winget install --id "%~1" --exact --accept-source-agreements --accept-package-agreements > "%LOG_DIR%\dependencies.log" 2>&1
)
if errorlevel 1 (
    call :show_log "dependencies.log"
    exit /b 1
)
exit /b 0

:show_log
if exist "%LOG_DIR%\%~1" (
    echo [LOCUS] Last 40 lines of %~1:
    rem MSBuild / CMake output is usually UTF-8; decode it explicitly.
    powershell -NoProfile -Command "Get-Content -LiteralPath (Join-Path $env:LOCUS_LOG_DIR '%~1') -Encoding UTF8 -Tail 40"
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
echo [LOCUS] FAILED. Logs: %LOG_DIR%
:finish
>> "%LOG_DIR%\run-info.txt" echo exit_code=%STATUS%
echo [LOCUS] Creating diagnostics archive...
powershell -NoProfile -Command "$dir=$env:LOCUS_LOG_DIR; $files=@(Get-ChildItem -LiteralPath $dir -File | Where-Object { $_.Extension -in '.txt','.log','.csv' }); if($files.Count -gt 0) { Compress-Archive -LiteralPath $files.FullName -DestinationPath (Join-Path $dir $env:LOCUS_LOG_ARCHIVE) -Force }"
echo.
echo ============================================================
echo  REPORT FOR CHATGPT
echo  %LOG_DIR%\%LOG_ARCHIVE%
echo ============================================================
echo  Contains build and diagnostic logs, no audio or executables.
echo  Review local paths and other private details before sharing.
echo.
if /I not "%LOCUS_NO_PAUSE%"=="1" if not defined GITHUB_ACTIONS pause
exit /b %STATUS%
