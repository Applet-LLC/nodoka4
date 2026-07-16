<#
  nodoka アプリ側バージョン一括更新ツール

  使い方:
    バージョン更新:
      pwsh tools\bump_version.ps1 -Old 4.31 -New 4.32
      pwsh tools\bump_version.ps1 -Old 4.31 -New 4.32 -Year 2027   # Applet LLC copyright の終了年も更新
      pwsh tools\bump_version.ps1 -Old 4.31 -New 4.32 -WhatIf      # 変更内容だけ確認、書き込みしない

    現状チェックのみ (更新後の検証、または更新前の状態確認):
      pwsh tools\bump_version.ps1 -Check

  対象:
    misc.h / distrib.mak            短縮形 (例: 4.31, 4.31_sample, 4.31_debug)
    *.manifest                      4分割形式 (例: version="4.3.1.0")
    nodoka.rc / nodoka_helper.rc / setup.rc
                                     FILEVERSION/PRODUCTVERSION (例: 4,3,1,0) と
                                     文字列版 (例: "4, 3, 1, 0")
                                     -Year 指定時は Applet LLC の Copyright 終了年も更新

  対象外 (現状維持):
    d\nodokad.c, d\nodokad.rc, d2\nodokad2.rc など、デバイスドライバ側のバージョン表記
#>

[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Old,
    [string]$New,
    [string]$Year,
    [switch]$Check,
    [string]$Root = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

# ---- 対象ファイル一覧 (root からの相対パス) ----------------------------------

$ShortVersionFiles = @(
    'nodoka\misc.h'
    'distrib\distrib.mak'
)

$ManifestFiles = @(
    'exe.hil\nodoka_hil.manifest'
    'exe.limit\nodoka.manifest'
    'exe.x64\nodoka64.manifest'
    'exe.x64.hil\nodoka64_hil.manifest'
    'exe.x64.limit\nodoka64.manifest'
    'exe.x64.nua\nodoka64_nua.manifest'
    'nodoka\nodoka.manifest'
    'nodoka_helper\nodoka_helper.manifest'
    's\setup.manifest'
    's.x64\setup64.manifest'
)

$RcFiles = @(
    'nodoka\nodoka.rc'
    'nodoka_helper\nodoka_helper.rc'
    's\setup.rc'
)

# ---- ヘルパ --------------------------------------------------------------

function Get-Utf8Encoding([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    return New-Object System.Text.UTF8Encoding($hasBom)
}

function Read-TextFile([string]$Path) {
    $enc = Get-Utf8Encoding $Path
    return [System.IO.File]::ReadAllText($Path, $enc)
}

function Write-TextFile([string]$Path, [string]$Content) {
    $enc = Get-Utf8Encoding $Path
    [System.IO.File]::WriteAllText($Path, $Content, $enc)
}

function ConvertTo-VersionParts([string]$ShortVersion) {
    if ($ShortVersion -notmatch '^(\d+)\.(\d)(\d)$') {
        throw "バージョン '$ShortVersion' の形式が想定外です (期待形式: 例 4.31)。桁数が変わる場合はスクリプトの改修が必要です。"
    }
    $major, $minor, $patch = $Matches[1], $Matches[2], $Matches[3]
    [pscustomobject]@{
        Short      = $ShortVersion
        Dotted4    = "$major.$minor.$patch.0"
        Comma4     = "$major,$minor,$patch,0"
        CommaSpace = "$major, $minor, $patch, 0"
    }
}

# 実際に書き換えを行う場合は Old/New から派生した各表記を使う
if ($Old -and $New) {
    $OldParts = ConvertTo-VersionParts $Old
    $NewParts = ConvertTo-VersionParts $New
}

$script:HadWarning = $false

function Update-File {
    [CmdletBinding(SupportsShouldProcess)]
    param([string]$RelPath, [scriptblock]$Transform)
    $path = Join-Path $Root $RelPath
    if (-not (Test-Path $path)) {
        Write-Warning "見つかりません: $RelPath"
        $script:HadWarning = $true
        return
    }
    $content = Read-TextFile $path
    $result = & $Transform $content
    $newContent = $result.Content
    $count = $result.Count
    if ($count -eq 0) {
        Write-Warning "置換対象が見つかりませんでした: $RelPath (漏れの可能性、要確認)"
        $script:HadWarning = $true
        return
    }
    if ($PSCmdlet.ShouldProcess($RelPath, "$count 箇所を更新")) {
        Write-TextFile $path $newContent
    }
    Write-Host ("  {0,-45} {1} 箇所更新" -f $RelPath, $count)
}

function Measure-Occurrences([string]$Content, [string]$Literal) {
    ([regex]::Matches($Content, [regex]::Escape($Literal))).Count
}

# ---- 更新モード ------------------------------------------------------------

function Invoke-Bump {
    Write-Host "=== misc.h / distrib.mak ($($OldParts.Short) -> $($NewParts.Short)) ===" -ForegroundColor Cyan
    foreach ($f in $ShortVersionFiles) {
        Update-File $f {
            param($content)
            $c = 0
            foreach ($suffix in @('_sample', '_debug', '')) {
                $old = "$($OldParts.Short)$suffix"
                $new = "$($NewParts.Short)$suffix"
                $c += Measure-Occurrences $content $old
                $content = $content.Replace($old, $new)
            }
            [pscustomobject]@{ Content = $content; Count = $c }
        }
    }

    Write-Host "=== *.manifest ($($OldParts.Dotted4) -> $($NewParts.Dotted4)) ===" -ForegroundColor Cyan
    foreach ($f in $ManifestFiles) {
        Update-File $f {
            param($content)
            $old = "version=`"$($OldParts.Dotted4)`""
            $new = "version=`"$($NewParts.Dotted4)`""
            $cnt = Measure-Occurrences $content $old
            [pscustomobject]@{ Content = $content.Replace($old, $new); Count = $cnt }
        }
    }

    Write-Host "=== *.rc ($($OldParts.Comma4) -> $($NewParts.Comma4)) ===" -ForegroundColor Cyan
    foreach ($f in $RcFiles) {
        Update-File $f {
            param($content)
            $cnt = 0
            $cnt += Measure-Occurrences $content $OldParts.Comma4
            $content = $content.Replace($OldParts.Comma4, $NewParts.Comma4)
            $cnt += Measure-Occurrences $content $OldParts.CommaSpace
            $content = $content.Replace($OldParts.CommaSpace, $NewParts.CommaSpace)
            [pscustomobject]@{ Content = $content; Count = $cnt }
        }
    }

    if ($Year) {
        Write-Host "=== Applet LLC copyright 終了年 (-> $Year) ===" -ForegroundColor Cyan
        foreach ($f in $RcFiles) {
            Update-File $f {
                param($content)
                # VALUE "LegalCopyright", "Copyright (C) YYYY~YYYY"  (開始年は保持)
                $pattern1 = '(VALUE "LegalCopyright", "Copyright \(C\) \d{4}~)\d{4}'
                # IDS_version 文字列中の Applet LLC 分だけをピンポイントで置換 (他の著作権表記には触れない)
                $pattern2 = '(Copyright \(C\) 2008-)2026(\\r\\n  Applet LLC)'
                $cnt = ([regex]::Matches($content, $pattern1)).Count + ([regex]::Matches($content, $pattern2)).Count
                $content = [regex]::Replace($content, $pattern1, "`${1}$Year")
                $content = [regex]::Replace($content, $pattern2, "`${1}$Year`${2}")
                [pscustomobject]@{ Content = $content; Count = $cnt }
            }
        }
    }
}

# ---- チェックモード ---------------------------------------------------------

function Get-DetectedVersions {
    $found = [ordered]@{}

    foreach ($f in $ShortVersionFiles) {
        $path = Join-Path $Root $f
        if (-not (Test-Path $path)) { continue }
        $content = Read-TextFile $path
        $vals = [regex]::Matches($content, 'VERSION\s*=?\s*"?(\d+\.\d+)(?:_sample|_debug)?"?') |
            ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
        $found[$f] = $vals -join ','
    }
    foreach ($f in $ManifestFiles) {
        $path = Join-Path $Root $f
        if (-not (Test-Path $path)) { continue }
        $content = Read-TextFile $path
        # 先頭の <?xml version="1.0" ...?> を除外してから、assemblyIdentity 自身の
        # version="..." (依存アセンブリより前に出現) だけを拾う
        $body = $content -replace '<\?xml[^>]*\?>', ''
        $m = [regex]::Match($body, 'version="([\d.]+)"')
        $found[$f] = if ($m.Success) { $m.Groups[1].Value } else { '(not found)' }
    }
    foreach ($f in $RcFiles) {
        $path = Join-Path $Root $f
        if (-not (Test-Path $path)) { continue }
        $content = Read-TextFile $path
        $fv = @([regex]::Matches($content, 'FILEVERSION\s+([\d,]+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $pv = @([regex]::Matches($content, 'PRODUCTVERSION\s+([\d,]+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $fvs = @([regex]::Matches($content, '"FileVersion",\s*"([\d, ]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $pvs = @([regex]::Matches($content, '"ProductVersion",\s*"([\d, ]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $all = @($fv) + @($pv) + @($fvs | ForEach-Object { $_ -replace ',\s*', ',' }) + @($pvs | ForEach-Object { $_ -replace ',\s*', ',' })
        $uniq = $all | Sort-Object -Unique
        if (@($uniq).Count -ne 1) {
            Write-Warning "$f 内で FILEVERSION/PRODUCTVERSION/FileVersion/ProductVersion が不一致: $($all -join ' | ')"
            $script:HadWarning = $true
        }
        $found[$f] = if (@($fv).Count -gt 0) { $fv[0] } else { '(not found)' }
    }
    return $found
}

function Invoke-Check {
    Write-Host "=== 検出されたバージョン ===" -ForegroundColor Cyan
    $found = Get-DetectedVersions
    foreach ($kv in $found.GetEnumerator()) {
        Write-Host ("  {0,-45} {1}" -f $kv.Key, $kv.Value)
    }

    # 短縮形とmanifest/rcの4分割形式が対応しているか (4.31 <-> 4,3,1,0 <-> 4.3.1.0)
    $shortVals = @($ShortVersionFiles | Where-Object { $found.Contains($_) } | ForEach-Object { $found[$_] } | Sort-Object -Unique)
    $manifestVals = @($ManifestFiles | Where-Object { $found.Contains($_) } | ForEach-Object { $found[$_] } | Sort-Object -Unique)
    $rcVals = @($RcFiles | Where-Object { $found.Contains($_) } | ForEach-Object { $found[$_] } | Sort-Object -Unique)

    $ok = $true
    if ($shortVals.Count -gt 1) { Write-Warning "misc.h / distrib.mak 内で短縮形バージョンが不一致: $($shortVals -join ' / ')"; $ok = $false }
    if ($manifestVals.Count -gt 1) { Write-Warning "manifest ファイル間でバージョンが不一致: $($manifestVals -join ' / ')"; $ok = $false }
    if ($rcVals.Count -gt 1) { Write-Warning "rc ファイル間で FILEVERSION が不一致: $($rcVals -join ' / ')"; $ok = $false }

    if ($shortVals.Count -eq 1 -and $manifestVals.Count -eq 1) {
        $expectedDotted = (ConvertTo-VersionParts $shortVals[0]).Dotted4
        if ($manifestVals[0] -ne $expectedDotted) {
            Write-Warning "短縮形 ($($shortVals[0])) と manifest ($($manifestVals[0])) が対応していません (期待値: $expectedDotted)"
            $ok = $false
        }
    }
    if ($shortVals.Count -eq 1 -and $rcVals.Count -eq 1) {
        $expectedComma = (ConvertTo-VersionParts $shortVals[0]).Comma4
        if ($rcVals[0] -ne $expectedComma) {
            Write-Warning "短縮形 ($($shortVals[0])) と rc FILEVERSION ($($rcVals[0])) が対応していません (期待値: $expectedComma)"
            $ok = $false
        }
    }

    # 安全網: 既知リスト以外に version 表記を含む *.manifest / *.rc が無いか広く走査
    Write-Host "=== 未登録ファイルの走査 (漏れチェック) ===" -ForegroundColor Cyan
    $excludeSegments = @('.svn', '.vs', '.cursor', '.specstory', 'Win32', 'x64', 'Release', 'Debug', 'Sample', 'obj', 'd', 'd2', 'd.rescue', 'd.sign', 'd.x64', 'DriverManager', 'DriverManager.x64', 'files', 'temp', 'opened')
    $known = ($ManifestFiles + $RcFiles) | ForEach-Object { (Join-Path $Root $_).ToLowerInvariant() }
    $extra = Get-ChildItem -Path $Root -Recurse -File -Include '*.manifest','*.rc' -ErrorAction SilentlyContinue |
        Where-Object {
            $rel = $_.FullName.Substring($Root.Length).TrimStart('\','/')
            $segments = $rel -split '[\\/]'
            -not ($segments | Where-Object { $excludeSegments -contains $_ })
        } |
        Where-Object { $known -notcontains $_.FullName.ToLowerInvariant() } |
        Where-Object {
            $c = Read-TextFile $_.FullName
            $c -match 'version="4\.\d' -or $c -match 'FILEVERSION\s+4,'
        }
    if ($extra) {
        foreach ($e in $extra) {
            Write-Warning "既知リストに無いバージョン表記ファイルを検出: $($e.FullName.Substring($Root.Length))"
        }
        $ok = $false
    } else {
        Write-Host "  既知リスト以外に該当ファイルなし"
    }

    if ($ok) {
        Write-Host "OK: 全ファイルでバージョン表記が一致しています ($($shortVals[0]))" -ForegroundColor Green
    } else {
        Write-Host "NG: 不一致または未登録ファイルがあります (上記 warning 参照)" -ForegroundColor Red
        $script:HadWarning = $true
    }
}

# ---- エントリポイント -------------------------------------------------------

Write-Host "Root: $Root"

if ($Check -or (-not $Old -and -not $New)) {
    Invoke-Check
} elseif ($Old -and $New) {
    Invoke-Bump
    Write-Host ""
    Invoke-Check
} else {
    throw "-Old と -New は両方指定してください (または -Check のみで現状確認)。"
}

if ($script:HadWarning) { exit 1 }
