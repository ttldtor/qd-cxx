$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location $root
$evidence = Join-Path $PSScriptRoot 'evidence'
$manifest = 'tests/conformance/refactor/changed-files.txt'
# These outputs are linked by the report and are populated below in this same audit.
foreach ($output in @($manifest, (Join-Path $evidence 'document-audit.json'), (Join-Path $evidence 'source-hashes.sha256'))) {
    if (!(Test-Path -LiteralPath $output)) { New-Item -ItemType File -Path $output | Out-Null }
}
$protected = 0
$changedSources = [Collections.Generic.List[string]]::new()
foreach ($line in Get-Content (Join-Path $evidence 'protected-before.sha256')) {
    $hash, $path = $line -split '  ', 2
    # Initial inventory also records authored checker sources that this task may refactor.
    $editableChecker = $path -match '^tests/conformance/.*\.(cpp|hpp|cmake)$'
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $hash) {
        if ($editableChecker) { $changedSources.Add($path) }
        else { throw "Protected file changed: $path" }
    }
    if (!$editableChecker) { $protected++ }
}
[ordered]@{
    utc = [DateTime]::UtcNow.ToString('o'); protectedFiles = $protected; changedProtectedFiles = 0
    authoredCheckerChanges = @($changedSources)
} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'protected-audit.json')
git diff --check
if ($LASTEXITCODE -ne 0) { throw 'Working diff whitespace failed' }
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'Index whitespace failed' }
if (@(git diff --cached --name-only).Count -ne 0) { throw 'Unexpected staged paths' }
$changed = @((git diff --name-only), (git ls-files --others --exclude-standard)) | ForEach-Object { $_ }
$documents = @($changed | Where-Object { $_ -match '\.md$' } | Sort-Object -Unique)
$links = 0
foreach ($document in $documents) {
    $text = [IO.File]::ReadAllText((Join-Path $root $document))
    if ($text -match '(?m)[ \t]+\r?$') { throw "Document trailing whitespace: $document" }
    foreach ($match in [regex]::Matches($text, '\[[^\]]*\]\(([^)]+)\)')) {
        $target = $match.Groups[1].Value
        if ($target -match '^(https?://|mailto:|#)') { continue }
        $target = ($target -split '#', 2)[0]
        $resolved = Join-Path (Split-Path -Parent (Join-Path $root $document)) $target
        if (!(Test-Path -LiteralPath $resolved)) { throw "Broken link in ${document}: $target" }
        $links++
    }
    $pair = if ($document.EndsWith('.ru.md')) { $document.Replace('.ru.md', '.md') } else { $document.Replace('.md', '.ru.md') }
    if (!(Test-Path -LiteralPath $pair)) { throw "Missing language pair: $document" }
    if (!$text.Contains([IO.Path]::GetFileName($pair))) { throw "Missing reciprocal language link: $document" }
}
[ordered]@{ utc = [DateTime]::UtcNow.ToString('o'); documents = $documents; relativeLinks = $links
    trailingWhitespace = 0; stagedFiles = 0 } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'document-audit.json')
$changed | Sort-Object -Unique | Where-Object { $_ -match '(\.(cpp|hpp|ps1|cmake)$|CMakeLists\.txt$)' } |
    ForEach-Object { '{0}  {1}' -f (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant(), $_ } |
    Set-Content (Join-Path $evidence 'source-hashes.sha256')
$all = @((git diff --name-only), (git ls-files --others --exclude-standard), $manifest) | ForEach-Object { $_ }
$all | Sort-Object -Unique | Set-Content $manifest
Write-Output "Protected files unchanged: $protected; documents: $($documents.Count); local links: $links; manifest paths: $((Get-Content $manifest).Count)"
