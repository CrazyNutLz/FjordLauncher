$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
. (Join-Path $PSScriptRoot "dev-env.ps1")

cmake -E touch (Join-Path $repoRoot "launcher\ui\dialogs\AboutDialog.cpp")
cmake --build --preset windows_msvc --config Debug --target FjordLauncher

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $repoRoot "build\Debug\fjordlauncher.exe")
