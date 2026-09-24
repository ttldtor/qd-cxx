#Requires -Version 7.4
# Private implementation and offline test seam. Setup-Cpp.ps1 always supplies the repository lock.
Set-StrictMode -Version Latest

function Assert-CppSetupPlainPath([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        $item = Get-Item -LiteralPath $cursor -Force -ErrorAction SilentlyContinue
        if ($null -ne $item -and ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse point is not allowed: $cursor"
        }
        $cursor = [IO.Path]::GetDirectoryName($cursor)
    }
}

function Remove-CppSetupTemporary([string]$Path, [string]$Cache) {
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($Cache).TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
    if (!$full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        !([IO.Path]::GetFileName($full).StartsWith('.setup-', [StringComparison]::Ordinal))) {
        throw "Unsafe setup cleanup path: $full"
    }
    Assert-CppSetupPlainPath $full
    if (Test-Path -LiteralPath $full) {
        # Never follow a filesystem link, including one inserted into an old installation.
        $links = @(Get-ChildItem -LiteralPath $full -Recurse -Force |
            Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 })
        if ($links.Count) { throw "Reparse point in cleanup tree: $full" }
        Remove-Item -LiteralPath $full -Recurse -Force
    }
}

function Expand-CppSetupArchive([string]$Archive, [string]$Stage) {
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        $paths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $entries = [Collections.Generic.List[object]]::new()
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            $parts = $name.TrimEnd('/').Split('/')
            if (!$name -or $name.StartsWith('/') -or $name.Contains(':') -or
                $parts.Where({ !$_ -or $_ -in @('.', '..') -or $_ -match '[. ]$|[<>"|?*\x00-\x1f]' -or
                    $_ -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)' }).Count) {
                throw "Unsafe archive entry: $($entry.FullName)"
            }
            $attributes = [uint32]([int64]$entry.ExternalAttributes -band 0xffffffffL)
            $unixType = ($attributes -shr 16) -band 0xf000
            if (($attributes -band 0x400) -ne 0 -or $unixType -notin @(0, 0x4000, 0x8000)) {
                throw "Archive link or special entry: $($entry.FullName)"
            }
            $relative = $parts -join [IO.Path]::DirectorySeparatorChar
            if (!$paths.Add($relative)) { throw "Duplicate archive entry: $($entry.FullName)" }
            $target = [IO.Path]::GetFullPath((Join-Path $Stage $relative))
            $prefix = [IO.Path]::GetFullPath($Stage).TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
            if (!$target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Unsafe archive entry: $($entry.FullName)"
            }
            $entries.Add(@{ Entry = $entry; Target = $target; Directory = $name.EndsWith('/') })
        }
        # All entry names/attributes are checked before the first extraction write.
        foreach ($item in $entries) {
            Assert-CppSetupPlainPath $item.Target
            if ($item.Directory) {
                [IO.Directory]::CreateDirectory($item.Target) | Out-Null
            } else {
                [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.Target)) | Out-Null
                $inputStream = $item.Entry.Open()
                try {
                    $outputStream = [IO.File]::Open($item.Target, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
                    try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose() }
                } finally { $inputStream.Dispose() }
            }
        }
    } finally { $zip.Dispose() }
}

function Install-LockedCppTool {
    param(
        [Parameter(Mandatory)]$Tool,
        [Parameter(Mandatory)][string]$Cache,
        [scriptblock]$Download = {
            param($Url, $Output)
            Invoke-WebRequest -Uri $Url -OutFile $Output -ErrorAction Stop
        }
    )
    $ErrorActionPreference = 'Stop'
    $cacheRoot = [IO.Path]::GetFullPath($Cache)
    Assert-CppSetupPlainPath $cacheRoot
    [IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
    $installName = "$($Tool.name)-$($Tool.version)"
    if ($installName -notmatch '^[a-zA-Z0-9.-]+$' -or $Tool.sha256 -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'Invalid locked tool identity or SHA-256'
    }
    $archiveName = [IO.Path]::GetFileName(([uri]$Tool.url).AbsolutePath)
    if (!$archiveName -or $archiveName -match '[:\\/]' -or $archiveName.StartsWith('.')) {
        throw 'Invalid locked archive name'
    }
    $archive = Join-Path $cacheRoot $archiveName
    $destination = Join-Path $cacheRoot $installName
    $executable = [IO.Path]::GetFullPath((Join-Path $cacheRoot $Tool.executable))
    if (!$executable.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Expected executable is outside the locked installation'
    }
    Assert-CppSetupPlainPath $archive
    Assert-CppSetupPlainPath $destination
    Assert-CppSetupPlainPath $executable
    if (Test-Path -LiteralPath $destination -PathType Container) {
        $links = @(Get-ChildItem -LiteralPath $destination -Recurse -Force |
            Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 })
        if ($links.Count) { throw "Reparse point in existing installation: $destination" }
    }
    $id = [Guid]::NewGuid().ToString('N')
    $downloadPath = Join-Path $cacheRoot ".setup-$id.download"
    $stage = Join-Path $cacheRoot ".setup-$id.stage"
    $backup = Join-Path $cacheRoot ".setup-$id.backup"
    try {
        if (!(Test-Path -LiteralPath $archive -PathType Leaf)) {
            & $Download $Tool.url $downloadPath | Out-Null
            if ((Get-FileHash -LiteralPath $downloadPath -Algorithm SHA256).Hash -ine $Tool.sha256) {
                throw "Integrity mismatch: $archive"
            }
            [IO.File]::Move($downloadPath, $archive)
        }
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $Tool.sha256) {
            throw "Integrity mismatch: $archive"
        }
        [IO.Directory]::CreateDirectory($stage) | Out-Null
        Expand-CppSetupArchive -Archive $archive -Stage $stage
        $stagedExecutable = Join-Path $stage ([IO.Path]::GetRelativePath($destination, $executable))
        if (!(Test-Path -LiteralPath $stagedExecutable -PathType Leaf)) {
            throw "Missing executable: $executable"
        }
        if (Test-Path -LiteralPath $destination) { [IO.Directory]::Move($destination, $backup) }
        try { [IO.Directory]::Move($stage, $destination) } catch {
            if (Test-Path -LiteralPath $backup) { [IO.Directory]::Move($backup, $destination) }
            throw
        }
        Remove-CppSetupTemporary -Path $backup -Cache $cacheRoot
        Write-Output $executable
    } finally {
        Remove-CppSetupTemporary -Path $downloadPath -Cache $cacheRoot
        Remove-CppSetupTemporary -Path $stage -Cache $cacheRoot
    }
}
