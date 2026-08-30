@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package-release.ps1"
set "PACKAGE_EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%PACKAGE_EXIT_CODE%"=="0" (
    echo Packaging failed. Exit code: %PACKAGE_EXIT_CODE%
) else (
    echo Packaging finished.
)
pause
exit /b %PACKAGE_EXIT_CODE%
