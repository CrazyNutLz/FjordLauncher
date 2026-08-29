$ErrorActionPreference = "Stop"

Write-Host "=== Loading development environment ==="
. .\dev-env.ps1

Write-Host "=== Building FjordLauncher Release ==="
cmake --build --preset windows_msvc --config Release --target FjordLauncher

Write-Host "=== Cleaning install directory ==="
Remove-Item -Recurse -Force .\install -ErrorAction SilentlyContinue

Write-Host "=== Installing Release ==="
cmake --install build --config Release

Write-Host "=== Enabling portable mode ==="
cmake --install build --config Release --component portable

Write-Host ""
Write-Host "=== Repack completed ==="
Write-Host "Output: D:\PCL\Fjord\install"