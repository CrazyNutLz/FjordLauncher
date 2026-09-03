[CmdletBinding()]
param(
    # Quick: only the launcher and updater executables.
    # Full:  complete launcher runtime, including Qt, plugins and JAR files.
    [ValidateSet("Quick", "Full")]
    [string] $PackageMode = "Full"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$releaseDir = Join-Path $repoRoot "build\Release"
$distDir = Join-Path $repoRoot "dist"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$bundleName = "fjordlauncher-update-{0}-{1}" -f $PackageMode.ToLowerInvariant(), $timestamp
$bundleDir = Join-Path $distDir $bundleName
$archivePath = Join-Path $distDir ("{0}.zip" -f $bundleName)

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

function Remove-DistPath {
    param([Parameter(Mandatory)] [string] $Path)

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $distPrefix = [System.IO.Path]::GetFullPath($distDir).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedPath.StartsWith($distPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a path outside dist: $resolvedPath"
    }
    if (Test-Path -LiteralPath $resolvedPath) {
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
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

Write-Host "=== Package mode: $PackageMode ==="
if ($PackageMode -eq "Quick") {
    Write-Host "=== Building launcher update executables ==="
    Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
        "--build", "--preset", "windows_msvc", "--config", "Release", "--target",
        "FjordLauncher", "FjordLauncher_updater"
    )
}
else {
    Write-Host "=== Building complete launcher update runtime ==="
    Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
        "--build", "--preset", "windows_msvc", "--config", "Release", "--target",
        "FjordLauncher", "FjordLauncher_updater", "FjordLauncher_filelink"
    )
}

[System.IO.Directory]::CreateDirectory($distDir) | Out-Null
Remove-DistPath -Path $bundleDir
[System.IO.Directory]::CreateDirectory($bundleDir) | Out-Null

if ($PackageMode -eq "Quick") {
    $launcherExe = Join-Path $releaseDir "fjordlauncher.exe"
    $updaterExe = Join-Path $releaseDir "fjordlauncher_updater.exe"
    foreach ($requiredFile in @($launcherExe, $updaterExe)) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Required update file is missing: $requiredFile"
        }
    }

    Copy-Item -LiteralPath $launcherExe -Destination (Join-Path $bundleDir "fjordlauncher.exe") -Force
    Copy-Item -LiteralPath $updaterExe -Destination (Join-Path $bundleDir "fjordlauncher_updater.exe") -Force
}
else {
    Invoke-ExternalCommand -FilePath "cmake" -ArgumentList @(
        "--install", (Join-Path $repoRoot "build"), "--prefix", $bundleDir, "--config", "Release"
    )

    # CMake currently gives qtlogging.ini an absolute configure-time
    # destination, so --prefix does not redirect it into the update folder.
    Copy-Item -LiteralPath (Join-Path $repoRoot "launcher\qtlogging.ini") -Destination (Join-Path $bundleDir "qtlogging.ini") -Force

    foreach ($requiredRelativePath in @(
        "fjordlauncher.exe",
        "fjordlauncher_updater.exe",
        "fjordlauncher_filelink.exe",
        "Qt6Core.dll",
        "platforms\qwindows.dll",
        "jars\NewLaunch.jar",
        "qtlogging.ini"
    )) {
        $requiredFile = Join-Path $bundleDir $requiredRelativePath
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Required full-update file is missing: $requiredRelativePath"
        }
    }
}

# The current updater requires manifest.txt in the ZIP root. It also uses this
# list to decide which files are replaced and backed up.
$manifestEntries = if ($PackageMode -eq "Quick") {
    @("fjordlauncher.exe", "fjordlauncher_updater.exe", "manifest.txt")
}
else {
    @(
        Get-ChildItem -LiteralPath $bundleDir -Force |
            Sort-Object -Property Name |
            ForEach-Object { $_.Name }
    ) + "manifest.txt"
}
$manifestPath = Join-Path $bundleDir "manifest.txt"
[System.IO.File]::WriteAllLines(
    $manifestPath,
    $manifestEntries,
    (New-Object System.Text.UTF8Encoding($false))
)

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -Path (Join-Path $bundleDir "*") -DestinationPath $archivePath -CompressionLevel Optimal

$sha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToUpperInvariant()

Write-Host ""
Write-Host "=== Launcher update bundle created successfully ===" -ForegroundColor Green
Write-Host "Mode:      $PackageMode"
Write-Host "Directory: $bundleDir"
Write-Host "ZIP:       $archivePath"
Write-Host "SHA-256:   $sha256"
