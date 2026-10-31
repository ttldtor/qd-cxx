<#
.SYNOPSIS
Validate subscription-oracle artifacts and optionally compare an immutable baseline.
.DESCRIPTION
Reads Actual and, when supplied, Expected without changing either tree. Requires the wire,
collectors and session component artifacts and root provenance.tsv/manifest.tsv. Validates TSV schemas,
typed observations, case references, completion counts and provenance digests. Comparison uses
the exact relative file set and SHA-256 of the original bytes; nothing is normalized or copied.
Empty .qtp/.bin files are valid. Reparse points and unexpected paths are rejected.
.PARAMETER Actual
Required for validation or comparison. Existing artifact directory; no default.
.PARAMETER Expected
Optional existing baseline directory. If omitted, Actual is only validated.
.PARAMETER Help
Print usage without reading artifact files. No arguments have the same behavior.
.EXAMPLE
pwsh -NoProfile -File tools/upstream/subscription-oracle/Compare-Oracle.ps1 -Actual run/artifacts
.EXAMPLE
pwsh -NoProfile -File tools/upstream/subscription-oracle/Compare-Oracle.ps1 -Actual run/artifacts -Expected fixtures/expected
.OUTPUTS
A short validation/comparison result on stdout, or a diagnostic on stderr.
.NOTES
Requires PowerShell 7.4 or newer. Limits per tree: 128 files, 32 directories, 4 MiB per file,
16 MiB total, 100000 TSV data rows per file, 32768 characters per TSV line.
Exit codes: 0 help or success; 2 validation, difference or I/O failure; 3 invalid argument
combinations. PowerShell parameter-binding errors can exit 1. No writes or other side effects.
#>
[CmdletBinding()]
param(
    [string]$Actual,
    [string]$Expected,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$usage = @(
    'Usage: Compare-Oracle.ps1 [-Help | -Actual DIRECTORY [-Expected DIRECTORY]]'
    'Validate offline subscription-oracle artifacts; optionally compare exact baseline bytes.'
    'No arguments and -Help print this help without reading artifacts.'
    '-Actual is required for work. -Expected is optional; omission means self-validation.'
    'Requires wire, collectors, session, provenance.tsv and manifest.tsv; UTF-8 without BOM, LF TSV.'
    'Limits per tree: 128 files, 32 directories, 4 MiB/file, 16 MiB total,'
    '100000 TSV data rows/file, 32768 characters/line. Empty binaries are valid.'
    'Read-only: no normalization, copies, updates or network access. Reparse points rejected.'
    'Exit: 0 help/success; 2 validation/difference/I/O; 3 argument combinations; binding errors 1.'
) -join "`n"

function Get-ArtifactBytes([IO.FileInfo]$File) {
    $stream = [IO.File]::Open($File.FullName, 'Open', 'Read', 'Read')
    try {
        if ($stream.Length -gt 4MB) {
            throw "File exceeds 4 MiB: $($File.Name)"
        }
        $bytes = [byte[]]::new([int]$stream.Length)
        $offset = 0
        while ($offset -lt $bytes.Length) {
            $count = $stream.Read($bytes, $offset, $bytes.Length - $offset)
            if ($count -eq 0) {
                throw "File changed while reading: $($File.Name)"
            }
            $offset += $count
        }
        if ($stream.ReadByte() -ne -1) {
            throw "File grew while reading: $($File.Name)"
        }
        return ,$bytes
    } finally {
        $stream.Dispose()
    }
}

function Get-ArtifactTree([string]$Path) {
    $root = Get-Item -LiteralPath $Path -Force
    if ($root -isnot [IO.DirectoryInfo] -or
        ($root.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw 'Artifact root must be an ordinary filesystem directory.'
    }
    $files = [Collections.Generic.SortedDictionary[string, object]]::new([StringComparer]::Ordinal)
    $pending = [Collections.Generic.Queue[IO.DirectoryInfo]]::new()
    $pending.Enqueue($root)
    $directories = 1
    [long]$total = 0
    while ($pending.Count -ne 0) {
        $directory = $pending.Dequeue()
        foreach ($entry in $directory.EnumerateFileSystemInfos()) {
            $relative = $entry.FullName.Substring($root.FullName.TrimEnd('\', '/').Length + 1)
            $relative = $relative.Replace('\', '/')
            if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Reparse point is not an artifact: $relative"
            }
            if ($entry -is [IO.DirectoryInfo]) {
                if (++$directories -gt 32 -or
                    $relative -cnotmatch '^(wire|collectors|session|session/wire)$') {
                    throw "Unexpected directory or directory budget exceeded: $relative"
                }
                $pending.Enqueue($entry)
                continue
            }
            $allowed = $relative -cmatch '^(provenance|manifest)\.tsv$' -or
                $relative -cmatch '^wire/(cases\.tsv|observations\.tsv|COMPLETE|[A-Za-z0-9][A-Za-z0-9._-]*\.qtp)$' -or
                $relative -cmatch '^(collectors|session)/(cases\.tsv|observations\.tsv)$' -or
                $relative -cmatch '^session/wire/[A-Za-z0-9][A-Za-z0-9._-]*\.bin$'
            if (-not $allowed -or $files.Count -ge 128) {
                throw "Unexpected file or file budget exceeded: $relative"
            }
            if ($entry.Length -gt 4MB -or $total + $entry.Length -gt 16MB) {
                throw "Artifact byte budget exceeded: $relative"
            }
            $bytes = Get-ArtifactBytes $entry
            $total += $bytes.Length
            if ($total -gt 16MB) {
                throw 'Artifact total byte budget exceeded.'
            }
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $digest = [Convert]::ToHexString($sha.ComputeHash($bytes))
            } finally {
                $sha.Dispose()
            }
            $files.Add($relative, [pscustomobject]@{ Bytes = $bytes; Digest = $digest })
        }
    }
    foreach ($required in @(
        'provenance.tsv', 'manifest.tsv', 'wire/cases.tsv', 'wire/observations.tsv', 'wire/COMPLETE',
        'collectors/cases.tsv', 'collectors/observations.tsv',
        'session/cases.tsv', 'session/observations.tsv'
    )) {
        if (-not $files.ContainsKey($required)) {
            throw "Missing required artifact: $required"
        }
    }
    if (@($files.Keys | Where-Object { $_ -cmatch '^wire/.+\.qtp$' }).Count -eq 0 -or
        @($files.Keys | Where-Object { $_ -cmatch '^session/wire/.+\.bin$' }).Count -eq 0) {
        throw 'Wire and session binary captures are required.'
    }
    return [pscustomobject]@{ Files = $files; Bytes = $total }
}

function Get-ArtifactText([byte[]]$Bytes, [string]$Name) {
    $encoding = [Text.UTF8Encoding]::new($false, $true)
    $text = $encoding.GetString($Bytes)
    if ($text.Length -eq 0 -or $text[0] -eq [char]0xFEFF -or
        -not $text.EndsWith("`n", [StringComparison]::Ordinal) -or
        $text -match '[\x00-\x08\x0b-\x1f\x7f]') {
        throw "Expected nonempty UTF-8 without BOM, LF lines and no control cells: $Name"
    }
    return $text
}

function Get-ArtifactTable($Tree, [string]$Name, [string]$Header) {
    $text = Get-ArtifactText $Tree.Files[$Name].Bytes $Name
    $lines = $text.Split([char]"`n")
    if ($lines[0] -cne $Header -or $lines.Length -gt 100002) {
        throw "Wrong header or row budget exceeded: $Name"
    }
    $arity = $Header.Split([char]"`t").Length
    $rows = [Collections.Generic.List[string[]]]::new()
    for ($i = 1; $i -lt $lines.Length - 1; $i++) {
        $cells = $lines[$i].Split([char]"`t")
        if ($lines[$i].Length -gt 32768 -or $cells.Length -ne $arity -or
            @($cells | Where-Object { $_.Length -eq 0 }).Count -ne 0) {
            throw "Wrong TSV arity, empty cell or line budget exceeded: ${Name}:$($i + 1)"
        }
        $rows.Add($cells)
    }
    if ($rows.Count -eq 0) {
        throw "Empty table: $Name"
    }
    return [pscustomobject]@{ Rows = $rows }
}

function Test-SignedLong([string]$Value) {
    [long]$number = 0
    return $Value -cmatch '^(0|-?[1-9][0-9]*)$' -and
        [long]::TryParse($Value, [Globalization.NumberStyles]::AllowLeadingSign,
            [Globalization.CultureInfo]::InvariantCulture, [ref]$number)
}

function Test-TypedValue([string]$Value) {
    if ($Value.StartsWith('i:', [StringComparison]::Ordinal)) {
        return Test-SignedLong $Value.Substring(2)
    }
    return $Value -cmatch '^(b:[01]|u:([0-9a-fA-F]{4})*|x:([0-9a-fA-F]{2})*|t:[\x21-\x7e]+|n:null)$'
}

function Test-ArtifactTables($Tree) {
    $manifest = Get-ArtifactTable $Tree 'manifest.tsv' "path`tbytes`tsha256"
    $manifestPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $previous = $null
    foreach ($cells in $manifest.Rows) {
        $name, $length, $digest = $cells
        if ($name -ceq 'manifest.tsv' -or -not $Tree.Files.ContainsKey($name) -or
            -not $manifestPaths.Add($name) -or
            ($null -ne $previous -and [StringComparer]::Ordinal.Compare($previous, $name) -ge 0) -or
            $length -cnotmatch '^(0|[1-9][0-9]*)$' -or -not (Test-SignedLong $length) -or
            $digest -cnotmatch '^[0-9a-f]{64}$') {
            throw 'Invalid, unsorted, duplicate or unexpected manifest entry.'
        }
        if ([long]$length -ne $Tree.Files[$name].Bytes.Length -or
            $digest -cne $Tree.Files[$name].Digest.ToLowerInvariant()) {
            throw "Manifest disagrees with artifact bytes: $name"
        }
        $previous = $name
    }
    if ($manifestPaths.Count -ne $Tree.Files.Count - 1) {
        throw 'Manifest does not cover the exact artifact file set.'
    }
    foreach ($component in @('wire', 'collectors', 'session')) {
        $cases = Get-ArtifactTable $Tree "$component/cases.tsv" "case`tevidence_kind`tproduction_path`toutcome"
        $caseIds = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($cells in $cases.Rows) {
            if ($cells[0] -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$' -or
                -not $caseIds.Add($cells[0])) {
                throw "Invalid or duplicate case ID: $component/$($cells[0])"
            }
        }
        $observations = Get-ArtifactTable $Tree "$component/observations.tsv" "case`tphase`tindex`tkey`tvalue"
        $keys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $covered = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($cells in $observations.Rows) {
            $identity = $cells[0..3] -join "`t"
            if (-not $caseIds.Contains($cells[0]) -or -not $keys.Add($identity)) {
                throw "Unknown case reference or duplicate observation key: $component/$identity"
            }
            if ($cells[1] -cnotmatch '^[\x21-\x7e]+$' -or
                $cells[3] -cnotmatch '^[\x21-\x7e]+$' -or
                $cells[2] -cnotmatch '^(-1|0|[1-9][0-9]*)$' -or
                -not (Test-SignedLong $cells[2]) -or -not (Test-TypedValue $cells[4])) {
                throw "Invalid observation cells: $component/$identity"
            }
            [void]$covered.Add($cells[0])
        }
        if (-not $covered.SetEquals($caseIds)) {
            throw "Cases without observations: $component"
        }
        if ($component -ceq 'wire') {
            $marker = Get-ArtifactText $Tree.Files['wire/COMPLETE'].Bytes 'wire/COMPLETE'
            $expected = "wire-oracle-v1`ncases=$($cases.Rows.Count)`nrows=$($observations.Rows.Count)`n"
            if ($marker -cne $expected) {
                throw 'Wire completion marker disagrees with its tables.'
            }
        }
    }
    $provenance = Get-ArtifactTable $Tree 'provenance.tsv' "kind`tpath`tdigest"
    $paths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $counts = @{ pin = 0; tool = 0; source = 0 }
    foreach ($cells in $provenance.Rows) {
        $kind, $path, $digest = $cells
        if (@('pin', 'tool', 'source') -cnotcontains $kind -or
            $path -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._/-]*$' -or
            @($path.Split('/') | Where-Object { $_ -in @('', '.', '..') }).Count -ne 0 -or
            -not $paths.Add("$kind`t$path")) {
            throw 'Invalid or duplicate provenance entry.'
        }
        if (($kind -ceq 'pin' -and ($path -cne 'upstream.lock' -or $digest -cnotmatch '^[0-9a-fA-F]{40}$')) -or
            ($kind -cne 'pin' -and $digest -cnotmatch '^[0-9a-fA-F]{64}$')) {
            throw "Invalid provenance digest: $kind/$path"
        }
        $counts[$kind]++
    }
    if ($counts.pin -ne 1 -or $counts.tool -eq 0 -or $counts.source -eq 0) {
        throw 'Provenance requires one upstream pin and nonempty tool/source inventories.'
    }
}

try {
    if ($PSBoundParameters.Count -eq 0 -or ($Help -and $PSBoundParameters.Count -eq 1)) {
        [Console]::Out.Write($usage + "`n")
        exit 0
    }
    if ($Help -or -not $PSBoundParameters.ContainsKey('Actual') -or
        [string]::IsNullOrWhiteSpace($Actual) -or
        ($PSBoundParameters.ContainsKey('Expected') -and [string]::IsNullOrWhiteSpace($Expected))) {
        [Console]::Error.Write("Compare-Oracle: invalid arguments; use -Help.`n")
        exit 3
    }
    $actualTree = Get-ArtifactTree $Actual
    Test-ArtifactTables $actualTree
    if ($PSBoundParameters.ContainsKey('Expected')) {
        $expectedTree = Get-ArtifactTree $Expected
        Test-ArtifactTables $expectedTree
        foreach ($name in $actualTree.Files.Keys) {
            if (-not $expectedTree.Files.ContainsKey($name)) {
                throw "Extra artifact: $name"
            }
        }
        foreach ($name in $expectedTree.Files.Keys) {
            if (-not $actualTree.Files.ContainsKey($name)) {
                throw "Missing artifact: $name"
            }
        }
        foreach ($name in $actualTree.Files.Keys) {
            if ($actualTree.Files[$name].Digest -cne $expectedTree.Files[$name].Digest) {
                throw "Changed artifact bytes: $name"
            }
        }
        [Console]::Out.Write("Compared $($actualTree.Files.Count) files: identical.`n")
    } else {
        [Console]::Out.Write("Validated $($actualTree.Files.Count) files ($($actualTree.Bytes) bytes).`n")
    }
    exit 0
} catch {
    [Console]::Error.Write("Compare-Oracle: $($_.Exception.Message)`n")
    exit 2
}
