# SJIS (CP932) -> UTF-8 BOM converter for nodoka solution
# Converts .cpp, .c, .h, .rc, .nodoka, .mayu without corrupting Japanese text.
# For .rc files, also changes #pragma code_page(932) to code_page(65001).
# .nodoka and .mayu are converted everywhere (no path exclude).

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot + "\.."
$extensions = @("*.cpp", "*.c", "*.h", "*.rc", "*.nodoka", "*.mayu")
# Paths to exclude for source files only (not for .nodoka / .mayu)
$excludePathParts = @(
    "\.vs\", "_UpgradeReport", "\r\", "\doc\", "\contrib\", "\test\", "\tools\",
    "\sxs_trace_log\", "\distrib\files\", "\distrib\temp\", "\distrib\Release\", "\distrib\Sample\",
    "\Release\", "\Debug\", "\Sample\", "\nodoka\exe\", "\nodoka\Debug\", "\nodoka\Release\", "\nodoka\Sample\",
    "\x64\Debug\", "\x64\Release\", "\exe.x64\x64\", "\ats4nodoka_debug\"
)

function ShouldExclude($path, [string]$ext) {
    # .nodoka and .mayu: convert all (no exclude)
    if ($ext -match '\.(nodoka|mayu)$') { return $false }
    $norm = $path.Replace($root, "").Replace("/", "\")
    foreach ($d in $excludePathParts) {
        if ($norm.IndexOf($d, [StringComparison]::OrdinalIgnoreCase) -ge 0) { return $true }
    }
    return $false
}

$encSJIS = [System.Text.Encoding]::GetEncoding(932)
$encUTF8BOM = New-Object System.Text.UTF8Encoding $true
$utf8BOM = [byte[]]@(0xEF, 0xBB, 0xBF)

$count = 0
$skipped = 0
$errors = @()

foreach ($ext in $extensions) {
    Get-ChildItem -Path $root -Recurse -Filter $ext -File | ForEach-Object {
        $full = $_.FullName
        if (ShouldExclude $full $_.Extension) { return }
        # For source files only, skip build output dirs
        if ($_.Extension -notmatch '\.(nodoka|mayu)$') {
            if ($full -match "\\(Debug|Release|x64\\Debug|x64\\Release|Sample)\\.*$") { return }
        }

        try {
            $bytes = [System.IO.File]::ReadAllBytes($full)
            if ($bytes.Length -eq 0) { $skipped++; return }

            # Already UTF-8 BOM
            if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
                $skipped++
                return
            }
            # UTF-16 BOM - skip (do not convert)
            if ($bytes.Length -ge 2 -and (($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) -or ($bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF))) {
                $skipped++
                return
            }

            $content = $encSJIS.GetString($bytes)
            # For .rc: tell RC to interpret as UTF-8
            if ($_.Extension -eq ".rc") {
                $content = $content -replace "#pragma code_page\(932\)", "#pragma code_page(65001)"
            }
            [System.IO.File]::WriteAllText($full, $content, $encUTF8BOM)
            $script:count++
            Write-Host "OK: $($_.FullName.Replace($root, '.'))"
        }
        catch {
            $script:errors += "$full : $_"
            Write-Host "ERR: $full - $_" -ForegroundColor Red
        }
    }
}

Write-Host "`nConverted: $count  Skipped (already UTF-8/empty): $skipped"
if ($errors.Count -gt 0) {
    Write-Host "Errors: $($errors.Count)" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host $_ -ForegroundColor Red }
}
