[CmdletBinding()]
param(
    [string]$PackageName = "大雕GTNH客户端_Java25",
    [string]$SourceDirectory = "",
    [string]$OutputDirectory = "",
    [switch]$RemoveServerList
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($SourceDirectory)) {
    $SourceDirectory = Join-Path $repoRoot "install"
}
$sourceDirectory = [System.IO.Path]::GetFullPath($SourceDirectory)

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "dist"
}
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

if ($PackageName.IndexOfAny([System.IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
    $PackageName.Contains([string][System.IO.Path]::DirectorySeparatorChar) -or
    $PackageName.Contains([string][System.IO.Path]::AltDirectorySeparatorChar)) {
    throw "PackageName contains invalid file-name characters."
}

$sourcePrefix = $sourceDirectory.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$outputPrefix = $OutputDirectory.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($outputPrefix.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must not be inside the install directory."
}

if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory "fjordlauncher.exe") -PathType Leaf)) {
    throw "fjordlauncher.exe was not found in SourceDirectory: $sourceDirectory"
}
if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory "portable.txt") -PathType Leaf)) {
    throw "portable.txt was not found. Refusing to build a non-portable distribution package."
}
if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory "instances") -PathType Container)) {
    throw "The instances directory was not found."
}

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

$dateStamp = Get-Date -Format "yyyyMMdd"
$datedPackageName = "{0}_{1}" -f $PackageName, $dateStamp
$archivePath = Join-Path $OutputDirectory ("{0}.zip" -f $datedPackageName)
$legacyHashPath = "$archivePath.sha256"
$stagingDirectory = Join-Path $OutputDirectory (".fjord-package-{0}" -f [guid]::NewGuid().ToString("N"))
$packageDirectory = Join-Path $stagingDirectory $datedPackageName
$shortcutName = "启动大雕GTNH客户端.lnk"
$shortcutPath = Join-Path $stagingDirectory $shortcutName

function Remove-StagingDirectory {
    param([Parameter(Mandatory)][string]$Path)

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $expectedPrefix = $OutputDirectory.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar + ".fjord-package-"
    if (-not $resolvedPath.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove an unexpected staging directory: $resolvedPath"
    }
    if (Test-Path -LiteralPath $resolvedPath) {
        try {
            Remove-Item -LiteralPath $resolvedPath -Recurse -Force -ErrorAction Stop
        }
        catch {
            # Windows PowerShell 5.1 cannot reliably remove some long GTNH paths.
            # Mirroring an empty directory lets Robocopy handle those paths first.
            $emptyDirectory = Join-Path $OutputDirectory (".fjord-empty-{0}" -f [guid]::NewGuid().ToString("N"))
            try {
                [System.IO.Directory]::CreateDirectory($emptyDirectory) | Out-Null
                & robocopy.exe $emptyDirectory $resolvedPath /MIR /R:2 /W:1 /XJ /NFL /NDL /NJH /NJS /NP | Out-Null
                $cleanupExitCode = $LASTEXITCODE
                if ($cleanupExitCode -ge 8) {
                    throw "Robocopy cleanup failed with exit code $cleanupExitCode."
                }
                if (Test-Path -LiteralPath $resolvedPath) {
                    [System.IO.Directory]::Delete($resolvedPath, $false)
                }
            }
            finally {
                if (Test-Path -LiteralPath $emptyDirectory) {
                    [System.IO.Directory]::Delete($emptyDirectory, $true)
                }
            }
        }
    }
}

function Remove-ConfigKeys {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string[]]$Keys
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return
    }

    $keyPattern = "^(?:{0})=" -f (($Keys | ForEach-Object { [regex]::Escape($_) }) -join "|")
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    $cleanLines = @([System.IO.File]::ReadAllLines($Path, $utf8WithoutBom) | Where-Object { $_ -notmatch $keyPattern })
    [System.IO.File]::WriteAllLines($Path, $cleanLines, $utf8WithoutBom)
}

function Set-MinecraftLanguage {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return
    }

    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.AddRange([System.IO.File]::ReadAllLines($Path, $utf8WithoutBom))
    $languageFound = $false
    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -match '^lang:') {
            $lines[$index] = 'lang:zh_CN'
            $languageFound = $true
            break
        }
    }
    if (-not $languageFound) {
        $lines.Add('lang:zh_CN')
    }
    [System.IO.File]::WriteAllLines($Path, $lines, $utf8WithoutBom)
}

try {
    Write-Host "Preparing a clean distribution package..." -ForegroundColor Cyan
    [System.IO.Directory]::CreateDirectory($packageDirectory) | Out-Null

    $excludedFiles = @(
        "accounts.json",
        "launcher_accounts.json",
        "usercache.json",
        "usernamecache.json",
        "*.log",
        ".nutmod-session.lock",
        ".nutmod-game.json",
        "hs_err_pid*.log",
        "package-release.bat",
        "package-client-release.bat",
        (Split-Path -Leaf $PSCommandPath)
    )
    if ($RemoveServerList) {
        $excludedFiles += "servers.dat"
    }

    $excludedDirectories = @(
        (Join-Path $sourceDirectory "logs")
        (Join-Path $sourceDirectory ".nutmod-update")
    )

    Get-ChildItem -LiteralPath (Join-Path $sourceDirectory "instances") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $excludedDirectories += Join-Path $_.FullName '.nutmod-update'
        foreach ($gameFolder in @('.minecraft', 'minecraft')) {
            $excludedDirectories += Join-Path (Join-Path $_.FullName $gameFolder) '.nutmod-update'
        }
        $minecraftDirectory = Join-Path $_.FullName ".minecraft"
        if (Test-Path -LiteralPath $minecraftDirectory -PathType Container) {
            foreach ($relativeDirectory in @("logs", "crash-reports", "screenshots", "saves", "backups", "journeymap")) {
                $excludedDirectories += Join-Path $minecraftDirectory $relativeDirectory
            }
        }
    }

    $robocopyArguments = @(
        $sourceDirectory,
        $packageDirectory,
        "/E",
        "/COPY:DAT",
        "/DCOPY:DAT",
        "/R:2",
        "/W:1",
        "/XJ",
        "/NFL",
        "/NDL",
        "/NJH",
        "/NJS",
        "/NP",
        "/XF"
    ) + $excludedFiles + @("/XD") + $excludedDirectories

    & robocopy.exe @robocopyArguments
    $robocopyExitCode = $LASTEXITCODE
    if ($robocopyExitCode -ge 8) {
        throw "Robocopy failed with exit code $robocopyExitCode."
    }

    Remove-ConfigKeys -Path (Join-Path $packageDirectory "fjordlauncher.cfg") -Keys @(
        "DownloadsDir",
        "JavaPath",
        "JavaArchitecture",
        "JavaRealArchitecture",
        "JavaSignature",
        "JavaVendor",
        "JavaVersion",
        "JProfilerPath",
        "JVisualVMPath",
        "MCEditPath",
        "SelectedAccount",
        "LastUsedAccount",
        "DefaultAccount",
        "ActiveAccount"
    )

    $clientConfigs = @(Get-ChildItem -LiteralPath (Join-Path $packageDirectory "instances") -Directory | ForEach-Object {
        $configPath = Join-Path $_.FullName 'instance.cfg'
        if (Test-Path -LiteralPath $configPath -PathType Leaf) { Get-Item -LiteralPath $configPath }
    })
    $clientConfigs | ForEach-Object {
        Remove-ConfigKeys -Path $_.FullName -Keys @(
            "NutModClientUpdateEnabled",
            "shortcuts",
            "lastLaunchTime",
            "lastTimePlayed",
            "totalTimePlayed"
        )

    }

    Get-ChildItem -LiteralPath (Join-Path $packageDirectory "instances") -Filter "options.txt" -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        Set-MinecraftLanguage -Path $_.FullName
    }

    # Keep the large client in its own directory while providing an obvious
    # entry point at the root of the extracted package. Windows shortcuts keep
    # a relative fallback, so the link continues to work after extraction.
    $launcherPath = Join-Path $packageDirectory "fjordlauncher.exe"
    $shell = New-Object -ComObject WScript.Shell
    try {
        $shortcut = $shell.CreateShortcut($shortcutPath)
        try {
            $shortcut.TargetPath = $launcherPath
            $shortcut.Description = "启动大雕 GTNH 客户端"
            $shortcut.Save()
        }
        finally {
            [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut)
        }
    }
    finally {
        [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    }
    if (-not (Test-Path -LiteralPath $shortcutPath -PathType Leaf)) {
        throw "Failed to create the launcher shortcut: $shortcutPath"
    }

    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    if (Test-Path -LiteralPath $legacyHashPath) {
        Remove-Item -LiteralPath $legacyHashPath -Force
    }

    $sevenZip = Get-Command 7z.exe, 7zz.exe, 7za.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $sevenZip) {
        Write-Host "Creating ZIP with 7-Zip..." -ForegroundColor Cyan
        Push-Location $stagingDirectory
        try {
            & $sevenZip.Source a -tzip -mx=7 $archivePath $datedPackageName $shortcutName
            if ($LASTEXITCODE -ne 0) {
                throw "7-Zip failed with exit code $LASTEXITCODE."
            }
        }
        finally {
            Pop-Location
        }
    }
    elseif ($null -ne (Get-Command tar.exe -ErrorAction SilentlyContinue)) {
        Write-Host "Creating ZIP with Windows tar..." -ForegroundColor Cyan
        & tar.exe -a -c -f $archivePath -C $stagingDirectory $datedPackageName $shortcutName
        if ($LASTEXITCODE -ne 0) {
            throw "tar.exe failed with exit code $LASTEXITCODE."
        }
    }
    else {
        Write-Host "Creating ZIP with Compress-Archive..." -ForegroundColor Cyan
        Compress-Archive -LiteralPath @($packageDirectory, $shortcutPath) -DestinationPath $archivePath -CompressionLevel Optimal
    }

    $archiveSizeGiB = [math]::Round((Get-Item -LiteralPath $archivePath).Length / 1GB, 2)
    Write-Host "Package created successfully." -ForegroundColor Green
    Write-Host "ZIP:    $archivePath ($archiveSizeGiB GiB)"
    if (-not $RemoveServerList) {
        Write-Host "servers.dat was kept. Use -RemoveServerList if you do not want to distribute it." -ForegroundColor Yellow
    }
}
finally {
    try {
        Remove-StagingDirectory -Path $stagingDirectory
    }
    catch {
        Write-Warning "The package result is unaffected, but the temporary directory could not be removed: $stagingDirectory"
        Write-Warning $_.Exception.Message
    }
}
