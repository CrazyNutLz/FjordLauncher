[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputManifest,
    [Parameter(Mandatory)][string]$LauncherDirectory,
    [Parameter(Mandatory)][string]$OutputManifest,
    [hashtable]$ArchiveFiles = @{}
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$gameRoot = (Resolve-Path -LiteralPath $LauncherDirectory).Path.TrimEnd('\', '/')
$manifest = Get-Content -LiteralPath $InputManifest -Raw -Encoding UTF8 | ConvertFrom-Json

function Get-ManagedFile([string]$RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or
        $RelativePath -match '(^|/)\.\.?(/|$)|[\\:*?"<>|]|(^|/)\.nutmod-|(^|/)(con|prn|aux|nul|com[0-9]|lpt[0-9])(\.|/|$)|[. ](/|$)|//|^/|/$') {
        throw "Invalid managed file path: $RelativePath"
    }
    $resolved = [IO.Path]::GetFullPath((Join-Path $gameRoot $RelativePath))
    if (-not $resolved.StartsWith($gameRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "File escapes launcher directory: $RelativePath"
    }
    $item = Get-Item -LiteralPath $resolved
    if ($item.PSIsContainer) { throw "Not a file: $RelativePath" }
    $cursor = $item
    while ($null -ne $cursor) {
        if ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked path: $resolved" }
        if ($cursor -is [IO.FileInfo]) { $cursor = $cursor.Directory } else { $cursor = $cursor.Parent }
    }
    return $resolved
}
function Set-Hash($Entry, [string]$LocalPath) {
    $uri = [uri]$Entry.url
    if (-not $uri.IsAbsoluteUri -or $uri.Scheme -notin @('http', 'https')) { throw 'Download URLs must use HTTP or HTTPS.' }
    $hash = (Get-FileHash -LiteralPath $LocalPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $Entry | Add-Member -NotePropertyName sha256 -NotePropertyValue $hash -Force
}

$allowed = @('silent', 'force', 'notice', 'files', 'remove', 'archives')
foreach ($property in $manifest.PSObject.Properties.Name) {
    if ($property -notin $allowed) { throw "Unsupported manifest field: $property" }
}
if (-not (@($manifest.PSObject.Properties.Name | Where-Object { $_ -in @('files','remove','archives') }).Count)) {
    throw 'Manifest must contain at least one operation array.'
}
if ($manifest.PSObject.Properties['silent'] -and $manifest.silent -isnot [bool]) { throw 'silent must be a JSON boolean.' }
if ($manifest.PSObject.Properties['force'] -and $manifest.force -isnot [bool]) { throw 'force must be a JSON boolean.' }
foreach ($collection in @('files', 'remove', 'archives')) {
    if ($manifest.PSObject.Properties[$collection]) {
        foreach ($entry in $manifest.$collection) {
            if ($entry.PSObject.Properties['silent'] -and $entry.silent -isnot [bool]) { throw 'Entry silent must be a JSON boolean.' }
            if ($entry.PSObject.Properties['force'] -and $entry.force -isnot [bool]) { throw 'Entry force must be a JSON boolean.' }
            if ($entry.PSObject.Properties['notice'] -and $entry.notice -isnot [string]) { throw 'Entry notice must be a string.' }
        }
    }
}
$seen = @{}
if ($manifest.PSObject.Properties['files']) {
    foreach ($entry in $manifest.files) {
        if ($seen.ContainsKey($entry.path)) { throw "Duplicate file: $($entry.path)" }
        $seen[$entry.path] = $true
        Set-Hash $entry (Get-ManagedFile $entry.path)
    }
}
if ($manifest.PSObject.Properties['archives']) {
    foreach ($entry in $manifest.archives) {
        if (-not $ArchiveFiles.ContainsKey($entry.url)) {
            throw 'Supply -ArchiveFiles mapping each ZIP URL to its local ZIP file.'
        }
        Set-Hash $entry (Resolve-Path -LiteralPath $ArchiveFiles[$entry.url]).Path
    }
}
$output = [IO.Path]::GetFullPath($OutputManifest)
$parent = Split-Path -Parent $output
[void][IO.Directory]::CreateDirectory($parent)
[IO.File]::WriteAllText($output, ($manifest | ConvertTo-Json -Depth 20) + "`n", (New-Object Text.UTF8Encoding($false)))
Write-Output "Generated SHA-256 manifest: $output"
Write-Output 'Upload and verify the referenced files before publishing this manifest. The launcher validates the complete protocol before applying it.'
