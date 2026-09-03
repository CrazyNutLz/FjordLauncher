[CmdletBinding()]
param(
    [string] $VisualStudioPath = "",
    [string] $QtDirectory = "",
    [string] $VcpkgDirectory = "",
    [string] $JavaDirectory = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$localConfig = Join-Path $PSScriptRoot "dev-env.local.ps1"
if (Test-Path -LiteralPath $localConfig -PathType Leaf) {
    . $localConfig
}

function Find-VisualStudio {
    if (-not [string]::IsNullOrWhiteSpace($VisualStudioPath)) {
        return [System.IO.Path]::GetFullPath($VisualStudioPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:FJORD_VS_INSTALL_PATH)) {
        return [System.IO.Path]::GetFullPath($env:FJORD_VS_INSTALL_PATH)
    }

    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vsWhere -PathType Leaf) {
        $detected = & $vsWhere -latest -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not [string]::IsNullOrWhiteSpace($detected)) {
            return [System.IO.Path]::GetFullPath(($detected | Select-Object -First 1))
        }
    }
    throw "Visual Studio 2022 with the Desktop development with C++ workload was not found. Set FJORD_VS_INSTALL_PATH."
}

function Find-Qt {
    if (-not [string]::IsNullOrWhiteSpace($QtDirectory)) {
        return [System.IO.Path]::GetFullPath($QtDirectory)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:FJORD_QT_DIR)) {
        return [System.IO.Path]::GetFullPath($env:FJORD_QT_DIR)
    }

    $qtRoots = @(
        (Join-Path (Split-Path -Parent $repoRoot) "Qt"),
        (Join-Path $env:SystemDrive "Qt"),
        "D:\Qt",
        "F:\Qt"
    ) | Select-Object -Unique
    $candidates = foreach ($root in $qtRoots) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }
        Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $candidate = Join-Path $_.FullName "msvc2022_64"
            if (Test-Path -LiteralPath (Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake") -PathType Leaf) {
                $version = [version]"0.0"
                [void][version]::TryParse($_.Name, [ref]$version)
                [pscustomobject]@{ Path = $candidate; Version = $version }
            }
        }
    }
    $selected = $candidates | Sort-Object -Property Version -Descending | Select-Object -First 1
    if ($null -eq $selected) {
        throw "Qt 6 for MSVC 2022 x64 was not found. Set FJORD_QT_DIR to a directory such as C:\Qt\6.11.2\msvc2022_64."
    }
    return $selected.Path
}

function Find-Vcpkg {
    if (-not [string]::IsNullOrWhiteSpace($VcpkgDirectory)) {
        return [System.IO.Path]::GetFullPath($VcpkgDirectory)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
    }

    foreach ($candidate in @((Join-Path (Split-Path -Parent $repoRoot) "vcpkg"), "C:\vcpkg", "D:\vcpkg")) {
        if (Test-Path -LiteralPath (Join-Path $candidate "scripts\buildsystems\vcpkg.cmake") -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($env:VCPKG_INSTALLATION_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:VCPKG_INSTALLATION_ROOT)
    }
    throw "vcpkg was not found. Set VCPKG_ROOT or pass -VcpkgDirectory."
}

function Find-Java17 {
    if (-not [string]::IsNullOrWhiteSpace($JavaDirectory)) {
        return [System.IO.Path]::GetFullPath($JavaDirectory)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:JAVA_HOME) -and
        (Test-Path -LiteralPath (Join-Path $env:JAVA_HOME "bin\java.exe") -PathType Leaf)) {
        return [System.IO.Path]::GetFullPath($env:JAVA_HOME)
    }

    $javaCandidates = @()
    foreach ($root in @(
        (Join-Path $env:ProgramFiles "Eclipse Adoptium"),
        (Join-Path $env:ProgramFiles "Microsoft"),
        (Join-Path $env:ProgramFiles "Zulu")
    )) {
        if (Test-Path -LiteralPath $root -PathType Container) {
            $javaCandidates += Get-ChildItem -LiteralPath $root -Directory -Filter "*17*" -ErrorAction SilentlyContinue |
                Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "bin\java.exe") -PathType Leaf }
        }
    }
    $selected = $javaCandidates | Sort-Object -Property LastWriteTime -Descending | Select-Object -First 1
    if ($null -ne $selected) {
        return $selected.FullName
    }
    throw "JDK 17 was not found. Set JAVA_HOME or pass -JavaDirectory."
}

$VisualStudioPath = Find-VisualStudio
$QtDirectory = Find-Qt
$VcpkgDirectory = Find-Vcpkg
$JavaDirectory = Find-Java17

$devShellModule = Join-Path $VisualStudioPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
    throw "Visual Studio developer shell module was not found: $devShellModule"
}
Import-Module $devShellModule
Enter-VsDevShell -VsInstallPath $VisualStudioPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"

foreach ($requiredPath in @(
    (Join-Path $QtDirectory "bin\Qt6Core.dll"),
    (Join-Path $VcpkgDirectory "scripts\buildsystems\vcpkg.cmake"),
    (Join-Path $JavaDirectory "bin\java.exe")
)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required development file was not found: $requiredPath"
    }
}

$env:JAVA_HOME = $JavaDirectory
$env:VCPKG_ROOT = $VcpkgDirectory
$env:FJORD_VS_INSTALL_PATH = $VisualStudioPath
$env:FJORD_QT_DIR = $QtDirectory
$env:ARTIFACT_NAME = "windows-x64"
$env:BUILD_PLATFORM = "windows-x64"
$env:CMAKE_PREFIX_PATH = if ([string]::IsNullOrWhiteSpace($env:CMAKE_PREFIX_PATH)) {
    $QtDirectory
} else {
    "$QtDirectory;$env:CMAKE_PREFIX_PATH"
}
$env:Path = "$JavaDirectory\bin;$QtDirectory\bin;$env:Path"

foreach ($command in @("cmake.exe", "ninja.exe", "git.exe")) {
    if ($null -eq (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "$command was not found in PATH. Install it before building Fjord Launcher."
    }
}

Set-Location $repoRoot

Write-Host "Fjord development environment loaded."
Write-Host "Repository:    $repoRoot"
Write-Host "Visual Studio: $VisualStudioPath"
Write-Host "Qt:            $QtDirectory"
Write-Host "vcpkg:         $VcpkgDirectory"
Write-Host "JDK 17:        $JavaDirectory"
