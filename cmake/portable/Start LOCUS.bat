@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
if errorlevel 1 exit /b 1
set "LOG=%TEMP%\LOCUS-startup-%RANDOM%-%RANDOM%.log"
> "%LOG%" echo LOCUS portable startup
>> "%LOG%" echo directory=%CD%
echo Starting LOCUS...
if /I "%~1"=="--window-smoke" (
    "%~dp0locus.exe" --window-smoke >> "%LOG%" 2>&1
) else (
    "%~dp0locus.exe" >> "%LOG%" 2>&1
)
set "RESULT=%ERRORLEVEL%"
>> "%LOG%" echo exit_code=%RESULT%
if "%RESULT%"=="0" (
    if /I not "%~1"=="--window-smoke" exit /b 0
)
type "%LOG%"
echo.
echo Diagnostic log: "%LOG%"
if /I not "%LOCUS_NO_PAUSE%"=="1" pause
exit /b %RESULT%
