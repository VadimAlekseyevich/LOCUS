@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem LOCUS - Windows game launcher. Modes: run, build, test.
rem Work from a temporary copy so git pull can safely update the BAT itself.

if /I "%LOCUS_BAT_WORKER%"=="1" goto :worker
set "LOCUS_BAT_WORKER=1"
set "LOCUS_BAT_ROOT=%~dp0"
set "LOCUS_TEMP_COPY=%TEMP%\locus-run-%RANDOM%-%RANDOM%.bat"
copy /y "%~f0" "%LOCUS_TEMP_COPY%" >nul
if errorlevel 1 exit /b 1
call "%LOCUS_TEMP_COPY%" %*
set "LOCUS_RETURN_CODE=%ERRORLEVEL%"
del /q "%LOCUS_TEMP_COPY%" >nul 2>&1
exit /b %LOCUS_RETURN_CODE%

:worker
cd /d "%LOCUS_BAT_ROOT%"
if errorlevel 1 exit /b 1
set "ROOT=%CD%"
set "MODE=%~1"
if "%MODE%"=="" set "MODE=run"
if /I "%MODE%"=="run" goto :valid
if /I "%MODE%"=="build" goto :valid
if /I "%MODE%"=="test" goto :valid
echo Usage: build_and_run.bat [run ^| build ^| test]
exit /b 2

:valid
for /f "delims=" %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%T"
if not defined STAMP set "STAMP=%RANDOM%"
set "LOG_DIR=%ROOT%\.locus-runs\%STAMP%-%RANDOM%"
mkdir "%LOG_DIR%" >nul 2>&1
if not exist "%LOG_DIR%" exit /b 1
set "LOCUS_LOG_DIR=%LOG_DIR%"
set "BUILD=%ROOT%\build\windows"
set "STATUS=1"
echo [LOCUS] %MODE%

call :ensure_git
if errorlevel 1 goto :failed
if /I "%LOCUS_SKIP_PULL%"=="1" (
    echo [LOCUS] Skipping git update.
) else (
    set "DIRTY="
    for /f "delims=" %%D in ('git -C "%ROOT%" status --porcelain 2^>nul') do set "DIRTY=1"
    if defined DIRTY (
        echo [LOCUS] Local changes detected. Git update canceled.
        goto :failed
    )
    git -C "%ROOT%" pull --ff-only > "%LOG_DIR%\git-update.log" 2>&1
    if errorlevel 1 (
        echo [LOCUS] Git update failed.
        call :show_log "git-update.log"
        goto :failed
    )
)
call :ensure_cmake
if errorlevel 1 goto :failed
call :ensure_msvc
if errorlevel 1 goto :failed

set "TESTS=OFF"
if /I "%MODE%"=="test" set "TESTS=ON"
echo [LOCUS] Configuring...
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=%TESTS% > "%LOG_DIR%\configure.log" 2>&1
if errorlevel 1 (
    call :show_log "configure.log"
    goto :failed
)

echo [LOCUS] Building...
if /I "%MODE%"=="test" (
    cmake --build "%BUILD%" --config Release --parallel 2 > "%LOG_DIR%\build.log" 2>&1
) else (
    cmake --build "%BUILD%" --config Release --target locus --parallel 2 > "%LOG_DIR%\build.log" 2>&1
)
if errorlevel 1 (
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
if not exist "%GAME_EXE%" goto :failed
echo [LOCUS] Ready: %GAME_EXE%
if /I "%MODE%"=="build" goto :success
pushd "%LOG_DIR%"
"%GAME_EXE%" > "%LOG_DIR%\game-console.log" 2>&1
set "GAME_EXIT=%ERRORLEVEL%"
popd
if not "%GAME_EXIT%"=="0" (
    call :show_log "game-console.log"
    goto :failed
)
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
if exist "%LOG_DIR%\%~1" powershell -NoProfile -Command "Get-Content -LiteralPath (Join-Path $env:LOCUS_LOG_DIR '%~1') -Tail 40"
exit /b 0

:success
set "STATUS=0"
echo [LOCUS] Done.
goto :finish
:failed
set "STATUS=1"
echo [LOCUS] Failed. Logs: %LOG_DIR%
:finish
if /I not "%LOCUS_NO_PAUSE%"=="1" if not defined GITHUB_ACTIONS pause
exit /b %STATUS%
