# Verify the complete QDCXX-026 manifest in disposable Git repositories, never the owner's index.
param([Parameter(Mandatory)][string]$Report)
$ErrorActionPreference = 'Stop'
if ($env:GIT_DIR -or $env:GIT_WORK_TREE -or $env:GIT_INDEX_FILE) { throw 'Run without Git location overrides' }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location $root
$indexPath = [IO.Path]::GetFullPath((git rev-parse --git-path index), $root)
$indexHash = (Get-FileHash -LiteralPath $indexPath).Hash
$manifestPath = 'tests/conformance/refactor/changed-files.txt'
$manifest = @(Get-Content $manifestPath)
$actual = @((git diff --name-only), (git ls-files --others --exclude-standard)) | ForEach-Object { $_ } | Sort-Object -Unique
if (@(Compare-Object $manifest $actual).Count -or @($manifest | Sort-Object -Unique).Count -ne $manifest.Count) {
    throw 'Manifest must list every changed/untracked path exactly once'
}
$raw = @($manifest | Where-Object { $_ -match '^tests/conformance/refactor/evidence/.*\.(log|json|sha256|txt|patch)$' })
$allowedChanges = @('.gitattributes', $manifestPath, 'tests/conformance/refactor/VALIDATION.md', 'tests/conformance/refactor/VALIDATION.ru.md')
$preserved = 0
foreach ($line in Get-Content tests/conformance/refactor/evidence/git-preservation/scope-before.sha256) {
    $hash, $path = $line -split '  ', 2
    if ((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -cne $hash) {
        if ($path -notin $allowedChanges) { throw "Pre-existing file changed: $path" }
    } else { $preserved++ }
}
git diff --check
if ($LASTEXITCODE) { throw 'Working-tree whitespace failed' }
git diff --cached --check
if ($LASTEXITCODE) { throw 'Owner index whitespace failed' }
$documents = @($manifest | Where-Object { $_ -match '\.md$' })
$links = 0
foreach ($path in $documents) {
    $text = [IO.File]::ReadAllText((Join-Path $root $path))
    if ($text -match '(?m)[ \t]+\r?$') { throw "Document trailing whitespace: $path" }
    $pair = if ($path.EndsWith('.ru.md')) { $path.Replace('.ru.md', '.md') } else { $path.Replace('.md', '.ru.md') }
    if (!(Test-Path -LiteralPath $pair) -or !$text.Contains([IO.Path]::GetFileName($pair))) {
        throw "Missing language pair/link: $path"
    }
    foreach ($match in [regex]::Matches($text, '\[[^\]]*\]\(([^)]+)\)')) {
        $target = $match.Groups[1].Value
        if ($target -match '^(https?://|mailto:|#)') { continue }
        $target = ($target -split '#', 2)[0]
        if (!(Test-Path -LiteralPath (Join-Path (Split-Path -Parent $path) $target))) { throw "Broken link: $path -> $target" }
        $links++
    }
}

$runs = @()
try {
    foreach ($autocrlf in @('true', 'false')) {
        $isolation = Join-Path $root ('.work/raw-evidence-' + [Guid]::NewGuid().ToString('N'))
        $checkout = Join-Path $isolation 'extracted'
        New-Item -ItemType Directory -Path $isolation | Out-Null
        foreach ($path in $manifest) {
            $destination = Join-Path $isolation $path
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
            Copy-Item -LiteralPath (Join-Path $root $path) -Destination $destination
        }
        git -C $isolation init -q
        if ($LASTEXITCODE) { throw 'Isolated init failed' }
        git -C $isolation config core.autocrlf $autocrlf
        if ($LASTEXITCODE) { throw 'Isolated configuration failed' }
        git -C $isolation add --pathspec-from-file=$manifestPath
        if ($LASTEXITCODE) { throw 'Isolated manifest add failed' }
        $staged = @(git -C $isolation diff --cached --name-only)
        if (@(Compare-Object $manifest $staged).Count) { throw 'Isolated index differs from manifest' }
        git -C $isolation diff --cached --check
        if ($LASTEXITCODE) { throw 'Isolated full-manifest whitespace failed' }
        $attributes = @(git -C $isolation check-attr --cached text whitespace -- $manifest)
        if ($LASTEXITCODE) { throw 'Attribute inspection failed' }
        foreach ($path in $raw) {
            if ("${path}: text: unset" -notin $attributes) { throw "Raw text conversion remains enabled: $path" }
            if ("${path}: whitespace: unset" -in $attributes) {
                $text = [IO.File]::ReadAllText((Join-Path $root $path))
                if ($text -notmatch '(?m)[ \t]+\r?$|(?:\r?\n){2,}$|(?m)^ * +\t') {
                    throw "Unnecessary raw whitespace exception: $path"
                }
            }
            $blob = (git -C $isolation ls-files --stage -- $path) -split '\s+', 4
            $originalBlob = git hash-object --no-filters -- $path
            if ($LASTEXITCODE -or $blob[1] -cne $originalBlob) { throw "Git blob changed bytes: $path" }
        }
        foreach ($path in $manifest | Where-Object { $_ -match '\.(ps1|md|cpp|hpp)$' }) {
            if ("${path}: whitespace: unset" -in $attributes) { throw "Source whitespace disabled: $path" }
        }
        git -C $isolation checkout-index --all --prefix="$($checkout.Replace('\', '/'))/"
        if ($LASTEXITCODE) { throw 'Isolated extraction failed' }
        foreach ($path in $raw) {
            if ((Get-FileHash -LiteralPath (Join-Path $root $path)).Hash -cne
                (Get-FileHash -LiteralPath (Join-Path $checkout $path)).Hash) { throw "Extracted bytes differ: $path" }
        }
        # Deliberate whitespace defects exist only in disposable copies, including a PS1 in evidence/.
        $negativePaths = @('tests/conformance/refactor/evidence/finding-1/Verify-Evidence.ps1',
            'tests/conformance/refactor/VALIDATION.md', 'src/encoding/compact.cpp',
            'tests/conformance/refactor/evidence/finding-1/coverage.json')
        foreach ($path in $negativePaths) {
            [IO.File]::AppendAllText((Join-Path $isolation $path), "probe `n")
            git -C $isolation add -- $path
            if ($LASTEXITCODE) { throw 'Isolated negative add failed' }
            $diagnostic = @(git -C $isolation diff --cached --check -- $path)
            if ($LASTEXITCODE -eq 0 -or !($diagnostic -match 'trailing whitespace')) {
                throw "Whitespace negative was not rejected: $path"
            }
            Copy-Item -LiteralPath (Join-Path $root $path) -Destination (Join-Path $isolation $path)
            git -C $isolation add -- $path
            if ($LASTEXITCODE) { throw 'Isolated restoration failed' }
        }
        git -C $isolation diff --cached --check
        if ($LASTEXITCODE) { throw 'Restored whitespace failed' }
        $runs += [ordered]@{ autocrlf = $autocrlf; isolation = $isolation; manifestFiles = $manifest.Count
            rawBlobsAndExtractionsEqual = $raw.Count; whitespaceExit = 0; negativePaths = $negativePaths
            attributes = $attributes }
    }
} finally {
    if ((Get-FileHash -LiteralPath $indexPath).Hash -cne $indexHash) { throw 'Owner index changed' }
}
[ordered]@{ utc = [DateTime]::UtcNow.ToString('o'); ownerIndexSha256 = $indexHash; ownerIndexUnchanged = $true
    preservedPreExistingFiles = $preserved; documents = $documents; relativeLinks = $links; runs = $runs } | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath ([IO.Path]::GetFullPath($Report, $root))
Write-Output "PASS: $($manifest.Count) manifest files; $($raw.Count) exact raw blobs/extractions under autocrlf=true/false; four whitespace negatives each; owner index unchanged."
