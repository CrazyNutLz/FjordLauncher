$ErrorActionPreference = "Stop"

. .\dev-env.ps1

cmake --build --preset windows_msvc --config Debug --target FjordLauncher

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

.\build\Debug\fjordlauncher.exe