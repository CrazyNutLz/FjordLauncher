$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $repoRoot "build"
$distDir = Join-Path $repoRoot "dist"
$packageDir = Join-Path $distDir "FjordLauncher"

function Invoke-ExternalCommand {
    param(
        [Parameter(Mandatory)] [string] $FilePath,
        [Parameter()] [string[]] $ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed: $FilePath $($ArgumentList -join ' ') (exit code $LASTEXITCODE)"
    }
}

function Remove-DistDirectory {
    param([Parameter(Mandatory)] [string] $Path)

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $distPrefix = [System.IO.Path]::GetFullPath($distDir).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedPath.StartsWith($distPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a directory outside dist: $resolvedPath"
    }
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
    }
}

function Assert-PackageFile {
    param([Parameter(Mandatory)] [string] $RelativePath)

    $path = Join-Path $packageDir $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "The full launcher package is incomplete. Missing: $RelativePath"
    }
}

Write-Host "=== Loading development environment ==="
. (Join-Path $PSScriptRoot "dev-env.ps1")

Write-Host "=== Configuring CMake ==="
Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
    "--preset", "windows_msvc",
    "-DLauncher_BUILD_ARTIFACT=windows-x64",
    "-DLauncher_BUILD_PLATFORM=windows-x64"
)

Write-Host "=== Building complete FjordLauncher Release runtime ==="
Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
    "--build", "--preset", "windows_msvc", "--config", "Release", "--target",
    "FjordLauncher", "FjordLauncher_updater", "FjordLauncher_filelink"
)

[System.IO.Directory]::CreateDirectory($distDir) | Out-Null
Remove-DistDirectory -Path $packageDir

# Remove artifacts created by the older version of this script. Keep the
# launcher-update folder and ZIP produced by scripts/build-update-bundle.ps1.
Get-ChildItem -LiteralPath $distDir -Filter "FjordLauncher-Full-*.zip" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Remove-DistDirectory -Path $_.FullName
}
Get-ChildItem -LiteralPath $distDir -Filter ".fjord-full-*" -Directory -Force -ErrorAction SilentlyContinue | ForEach-Object {
    Remove-DistDirectory -Path $_.FullName
}

Write-Host "=== Installing complete launcher package to dist ==="
Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
    "--install", $buildDir, "--prefix", $packageDir, "--config", "Release"
)
Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
    "--install", $buildDir, "--prefix", $packageDir, "--config", "Release", "--component", "portable"
)

# CMake currently gives qtlogging.ini an absolute configure-time destination,
# so --prefix does not redirect it into dist. Copy it explicitly.
Copy-Item -LiteralPath (Join-Path $repoRoot "launcher\qtlogging.ini") -Destination (Join-Path $packageDir "qtlogging.ini") -Force

foreach ($requiredFile in @(
    "fjordlauncher.exe",
    "fjordlauncher_updater.exe",
    "fjordlauncher_filelink.exe",
    "Qt6Core.dll",
    "platforms\qwindows.dll",
    "jars\NewLaunch.jar",
    "qtlogging.ini",
    "portable.txt"
)) {
    Assert-PackageFile -RelativePath $requiredFile
}

Write-Host ""
Write-Host "=== Full launcher package created successfully ===" -ForegroundColor Green
Write-Host "Directory: $packageDir"
