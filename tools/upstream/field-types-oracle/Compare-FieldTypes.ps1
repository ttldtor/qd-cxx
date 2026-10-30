#requires -Version 7.4
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Actual,[Parameter(Mandatory)][string]$Expected)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
function Assert([bool]$Value,[string]$Reason) { if (!$Value) { throw $Reason } }
function Integer([string]$Value,[bool]$Wide=$false) {
    Assert ($Value -cmatch '^(0|-?[1-9][0-9]*)$') 'canonical integer'
    if ($Wide) { $null=[long]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
    else { $null=[int]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture) }
}
function Units([string]$Value,[bool]$Nullable=$false) { Assert ($Value -cmatch $(if($Nullable){'^(null|-|(?:[0-9a-f]{4})+)$'}else{'^(-|(?:[0-9a-f]{4})+)$'})) 'UTF-16 grammar' }
function HexBytes([string]$Value) { Assert ($Value -cmatch '^(-|(?:[0-9a-f]{2})+)$') 'byte grammar' }
function Validate([string]$Root) {
    $files=@(Get-ChildItem -LiteralPath $Root -File | Sort-Object Name)
    $names=@('mixed-stream','mixed-raw','profile-stream','profile-raw','composer','missing','repeat-0','repeat-1','repeat-2','mixed-order','unknown-field','unknown-record','malformed-skipped','malformed-unknown-record','oversized-skipped','mismatch-int','mismatch-object') + @(0..10|ForEach-Object{"string-wire-$_"}) + @(foreach($t in @(40,72,88)){foreach($v in @('2147483648','4294967295','-9223372036854775808','9223372036854775807')){"wide-$t-$v"}})
    $expectedFiles=@($names|ForEach-Object{"$_.qtp";"$_.tsv"})+@('primitives.tsv','schema.tsv')
    Assert (($files.Name -join '|') -ceq (($expectedFiles|Sort-Object) -join '|')) 'exact finite artifact set'
    foreach($file in $files){
        Assert ($file.Length -gt 0 -and $file.Length -lt 1048576) 'bounded artifact'
        if($file.Extension -eq '.qtp'){continue}
        $rows=[IO.File]::ReadAllLines($file.FullName);$name=$file.BaseName
        Assert ($rows[0] -ceq "case`t$name" -and $rows[-1] -ceq "complete`t$name") 'header/completion'
        $record=0;$field=0;$required=0;$errors=0;$ended=$false;$keys=[Collections.Generic.HashSet[string]]::new();$logical=0;$schemas=0
        for($n=1;$n -lt $rows.Count-1;$n++){
            $c=$rows[$n].Split("`t");Assert (!$ended -and $c -notcontains '') 'empty/post-end row'
            switch -CaseSensitive ($c[0]) {
                'record' {
                    Assert ($name -notin @('schema','primitives') -and $c.Count -eq 8 -and $field -eq $required -and $c[1] -ceq "$record") 'record/order/previous fields'
                    Assert ($c[2] -cin @('10','15','20') -and $c[6] -ceq '0') 'callback/flags';Integer $c[7] $true;Units $c[4];Units $c[5]
                    $profile=$name.StartsWith('profile-');$required=if($profile){16}else{9};$field=0
                    Assert ($c[3] -ceq $(if($profile){'5'}else{'0'})) 'local record identity';$record++
                }
                'field' {
                    Assert ($c.Count -eq 7 -and $required -gt 0 -and $field -lt $required -and $c[1] -ceq "$($record-1)" -and $c[2] -ceq "$field") 'field arity/order'
                    $tags=if($required -eq 16){@(120,120,120,120,88,120,120,120,120,120,120,56,56,8,10,10)}else{@(10,40,72,88,8,2,56,120,10)}
                    Assert ($c[4] -ceq "$($tags[$field])") 'full tag';Units $c[3];Integer $c[5] ($c[4] -eq '120')
                    if($c[4] -eq '10'){Assert ($c[5] -ceq '0') 'object raw invariant';Units $c[6] $true}else{Assert ($c[6] -ceq '-') 'scalar payload invariant'}
                    $field++
                }
                'error' {Assert ($c.Count -eq 3 -and $c[1] -cin @('message','stream','unknown')) 'error grammar';Integer $c[2];$errors++}
                'consumed' {Assert ($name -ceq 'composer' -and $c.Count -eq 3 -and $c[2] -ceq '0') 'composer consumption';Integer $c[1]}
                'end' {Assert ($c.Count -eq 3 -and $c[1] -ceq "$record" -and $c[2] -ceq "$errors" -and $field -eq $required -and $n -eq $rows.Count-2) 'end counts';$ended=$true}
                'string-write' {Assert ($name -ceq 'primitives' -and $c.Count -eq 4 -and $keys.Add("write/$($c[1])")) 'string writer';Units $c[2] $true;HexBytes $c[3]}
                'string-read' {
                    Assert ($name -ceq 'primitives' -and $c.Count -eq 7 -and $c[2] -cin @('buffered','data-input','message-part') -and $keys.Add("string/$($c[1])/$($c[2])")) 'string reader'
                    HexBytes $c[3];Units $c[4] $true;Assert ($c[5] -cin @('ok','EOFException','IOException','UTFDataFormatException')) 'status';Integer $c[6]
                    Assert ([int]$c[6] -ge 0 -and [int]$c[6] -le $(if($c[3] -eq '-'){0}else{$c[3].Length/2})) 'consumption'
                    if($c[5] -ne 'ok'){Assert ($c[4] -ceq '-') 'no partial string'}
                }
                'int-read' {
                    Assert ($name -ceq 'primitives' -and $c.Count -eq 7 -and $c[1] -cin @('40','72','88') -and $keys.Add("int/$($c[1])/$($c[2])")) 'integer reader'
                    HexBytes $c[3];Assert ($c[5] -cin @('ok','EOFException')) 'integer status';Integer $c[6]
                    if($c[5] -eq 'ok'){Integer $c[4]}else{Assert ($c[4] -ceq '-') 'no failed value'}
                }
                'settings' {Assert ($name -ceq 'schema' -and $rows[$n] -ceq "settings`t331`toverrides=none") 'unchanged settings'}
                'schema' {Assert ($name -ceq 'schema' -and $c.Count -eq 6 -and $c[3] -ceq 'false') 'ordinary schema';Units $c[1];foreach($i in @(2,4,5)){Integer $c[$i]};$schemas++}
                {$_ -cin @('int-slot','obj-slot')} {Assert ($name -ceq 'schema' -and $c.Count -eq 6) 'physical field';Units $c[1];Integer $c[2];Units $c[3];Units $c[4];Integer $c[5];Assert ($c[5] -cin @('0','2','8','10','40','56','72','88','120')) 'profile type gate'}
                'logical' {Assert ($name -ceq 'schema' -and $c.Count -eq 6 -and $c[1] -ceq "$logical" -and $c[4] -cin @('int','obj')) 'logical projection';Units $c[2];Integer $c[3];Integer $c[5];$logical++}
                default {throw "unknown row $($c[0])"}
            }
        }
        if($name -eq 'schema'){Assert ($schemas -eq 2 -and $logical -eq 9) 'complete layouts'}
        elseif($name -eq 'primitives'){Assert ($keys.Count -gt 400) 'primitive coverage'}
        else{Assert $ended 'missing end'}
    }
    return $files
}
$a=@(Validate $Actual);$e=@(Validate $Expected)
for($i=0;$i -lt $a.Count;$i++){Assert ((Get-FileHash $a[$i].FullName).Hash -ceq (Get-FileHash $e[$i].FullName).Hash) "artifact mismatch: $($a[$i].Name)"}
"PASS: strict field-type comparison of $($a.Count) artifacts"

# The scalar child capture extends the oracle without altering the historical root artifacts.
# Called by Invoke-Oracle.ps1; input is a fresh capture and reviewed expectations.
# Returns no data, and throws on malformed grammar, missing files or byte differences.
function CompareScalars([string]$ActualRoot, [string]$ExpectedRoot) {
    $names = @('samples.tsv') + @(foreach ($t in @(0,1,3,4,24,104,152)) { "composer-$t.qtp"; "composer-$t-raw.qtp" }) + @(foreach ($t in @(0,4,24,104,152)) { "file-$t.qtp"; "file-$t-raw.qtp" })
    foreach ($root in @($ActualRoot, $ExpectedRoot)) {
        $files = @(Get-ChildItem -LiteralPath $root -File | Sort-Object Name)
        Assert (($files.Name -join '|') -ceq (($names | Sort-Object) -join '|')) 'exact scalar artifact set'
        $rows = [IO.File]::ReadAllLines((Join-Path $root 'samples.tsv'))
        Assert ($rows[0] -ceq "case`tscalar-samples" -and $rows[-1] -ceq "complete`tscalar-samples") 'scalar header/completion'
        $keys = [Collections.Generic.HashSet[string]]::new()
        $samples = 0
        for ($n = 1; $n -lt $rows.Count - 1; ++$n) {
            $c = $rows[$n].Split("`t")
            Assert ($c -notcontains '') 'scalar empty cell'
            switch -CaseSensitive ($c[0]) {
                'factory' { Assert ($c.Count -eq 3 -and $c[1] -cin @('1','3') -and $c[2] -ceq 'null' -and $keys.Add("factory/$($c[1])")) 'absent production factory' }
                'binding' {
                    Assert ($c.Count -eq 4 -and $c[1] -cin @('0','1','3','4','24','104','152') -and $keys.Add("binding/$($c[1])")) 'scalar binding'
                    $consumer = if ($c[1] -cin @('1','3')) { '8' } else { $c[1] }
                    Assert ($c[2] -ceq $consumer -and $c[3] -ceq $(if ($c[1] -ceq '0') { '1' } else { '2' })) 'consumer identity and VOID elision'
                }
                'composer' { Assert ($c.Count -eq 6 -and $c[2] -cin @('5','15') -and $keys.Add("composer/$($c[1])/$($c[2])")) 'composer row'; Integer $c[3] $true; Assert ($c[4] -ceq '913') 'composer sentinel'; Integer $c[5] }
                'file' { Assert ($c.Count -eq 5 -and $c[2] -cin @('5','15') -and $keys.Add("file/$($c[1])/$($c[2])")) 'file row'; Integer $c[3] $true; Assert ($c[4] -ceq '913') 'file sentinel' }
                'sample' {
                    Assert ($c.Count -eq 10 -and $c[2] -cin @('0','1','3','4','24','104','152') -and $c[3] -cin @('5','15') -and $keys.Add("sample/$($c[1])/$($c[2])/$($c[3])")) 'sample identity'
                    HexBytes $c[4]; Integer $c[5]; Integer $c[6]; Integer $c[9]
                    Assert ([long]$c[9] -ge 0 -and [long]$c[9] -le ($c[4].Length / 2 - 3)) 'production examined field position'
                    if ($c[6] -ceq '0') {
                        Assert ($c[5] -ceq $(if ($c[3] -ceq '5') { '3' } else { '1' }) -and $c[8] -ceq '913') 'production callback fanout and sentinel'
                        Integer $c[7] $true
                    } else { Assert ($c[5] -ceq '0' -and $c[6] -ceq '1' -and $c[7] -ceq '-' -and $c[8] -ceq '-') 'failed record unpublished' }
                    $samples++
                }
                default { throw "unknown scalar row $($c[0])" }
            }
        }
        Assert ($samples -eq 1106 -and $keys.Count -eq 1139) 'finite scalar scenario counts'
    }
    foreach ($name in $names) { Assert ((Get-FileHash (Join-Path $ActualRoot $name)).Hash -ceq (Get-FileHash (Join-Path $ExpectedRoot $name)).Hash) "scalar artifact mismatch: $name" }
    "PASS: strict scalar comparison of $($names.Count) artifacts"
}
CompareScalars (Join-Path $Actual 'scalars') (Join-Path $Expected 'scalars')

# Called by Invoke-Oracle.ps1 after the unchanged root/scalar comparisons.
# Validates the finite owned-payload capture, then requires original byte equality.
function CompareObjects([string]$ActualRoot, [string]$ExpectedRoot) {
    $names = @('samples.tsv') + @(foreach ($kind in @('composer','file')) {
        foreach ($type in @(9,137,233,249)) { foreach ($id in @('null','empty','content')) {
            "$kind-$type-$id.qtp"; "$kind-$type-$id-raw.qtp"
        } }
    })
    foreach ($root in @($ActualRoot, $ExpectedRoot)) {
        $files = @(Get-ChildItem -LiteralPath $root -File | Sort-Object Name)
        Assert (($files.Name -join '|') -ceq (($names | Sort-Object) -join '|')) 'exact object artifact set'
        foreach ($file in $files) { Assert ($file.Length -gt 0 -and $file.Length -lt 1048576) 'bounded object artifact' }
        $rows = [IO.File]::ReadAllLines((Join-Path $root 'samples.tsv'))
        Assert ($rows.Count -eq 803 -and $rows[0] -ceq "case`tobject-samples" -and $rows[-1] -ceq "complete`tobject-samples") 'object scenario count/header/completion'
        $keys = [Collections.Generic.HashSet[string]]::new()
        $counts = @{}
        for ($n = 1; $n -lt $rows.Count - 1; ++$n) {
            $c = $rows[$n].Split("`t")
            Assert ($c -notcontains '' -and $c[1] -cin @('9','137','233','249')) 'object full type and cells'
            $kind = $c[0]
            if (-not $counts.ContainsKey($kind)) { $counts[$kind] = 0 }
            $counts[$kind]++
            $identity = $null
            $observed = $null
            switch -CaseSensitive ($kind) {
                'composer' {
                    Assert ($c.Count -eq 10 -and $c[2] -cin @('null','empty','content') -and $c[3] -cin @('5','15')) 'object composer grammar'
                    Assert ($c[4] -ceq $(if ($c[1] -ceq '233') { '9' } else { $c[1] })) 'requested versus local type'
                    HexBytes $c[5]
                    Assert ($c[6] -ceq $(if ($c[3] -ceq '5') { '3' } else { '1' }) -and $c[7] -ceq '0' -and $c[9] -ceq '77') 'object composer callbacks/sentinel'
                    $observed=$c[8]; $identity="$kind/$($c[1])/$($c[2])/$($c[3])"
                }
                'FileWriter-FileReader' {
                    Assert ($c.Count -eq 10 -and $c[2] -cin @('null','empty','content') -and $c[3] -cin @('5','15') -and $c[4] -ceq 'actual-local-type') 'object file grammar'
                    Assert ($c[5] -ceq $(if ($c[1] -ceq '233') { '9' } else { $c[1] })) 'FileWriter actual local type'
                    Assert ($c[6] -ceq $(if ($c[3] -ceq '5') { '3' } else { '1' }) -and $c[7] -ceq '0' -and $c[9] -ceq '77') 'object file callbacks/sentinel'
                    $observed=$c[8]; $identity="$kind/$($c[1])/$($c[2])/$($c[3])"
                }
                { $_ -cin @('parser','binding-parser') } {
                    Assert ($c.Count -eq 11 -and $c[4] -cin @('5','15')) 'object parser grammar'
                    Assert ($c[3] -cin $(if ($kind -ceq 'parser') { @('known','unknown-field') } else { @('unknown-record','repeated-target') })) 'object binding mode'
                    Assert ($c[5] -cmatch '^(empty|(?:[0-9a-f]{2})+)$') 'object parser bytes'
                    foreach ($i in @(6,7,9,10)) { Integer $c[$i] }
                    Assert ([int]$c[10] -ge 0 -and [int]$c[10] -le $(if($c[5] -ceq 'empty'){2}else{$c[5].Length/2+2})) 'object actual field examination'
                    Assert ($c[6] -cin @('0','1','3') -and $c[7] -cin @('0','1')) 'object callback counts'
                    $observed=$c[8]; $identity="$kind/$($c[1])/$($c[2])/$($c[3])/$($c[4])"
                }
                'FieldReader-message-part' {
                    Assert ($c.Count -eq 9 -and $c[3] -cin @('known','unknown-field')) 'production FieldReader grammar'
                    Assert ($c[4] -cmatch '^(empty|(?:[0-9a-f]{2})+)$' -and $c[5] -cin @('OK','IOException','UTFDataFormatException','EOFException')) 'production FieldReader bytes/status'
                    Integer $c[7]; Integer $c[8]
                    $observed=$c[6]; $identity="$kind/$($c[1])/$($c[2])/$($c[3])"
                }
                'primitive-message-part' {
                    Assert ($c.Count -eq 7 -and $c[3] -cmatch '^(empty|(?:[0-9a-f]{2})+)$' -and $c[4] -cin @('OK','IOException','UTFDataFormatException','EOFException')) 'supporting bounded primitive grammar'
                    Integer $c[6]
                    $observed=$c[5]; $identity="$kind/$($c[1])/$($c[2])"
                }
                default { throw "unknown object row $kind" }
            }
            Assert ($observed -cmatch '^(-|skipped|null|bytes:(empty|(?:[0-9a-f]{2})+)|marshalled:(null|empty|(?:[0-9a-f]{2})+)|units:(?:[0-9a-f]{4})*)$') 'owned object projection grammar'
            Assert ($keys.Add($identity)) 'unique object observation'
        }
        foreach ($entry in @{composer=24;'FileWriter-FileReader'=24;parser=324;'binding-parser'=186;'FieldReader-message-part'=162;'primitive-message-part'=81}.GetEnumerator()) {
            Assert ($counts[$entry.Key] -eq $entry.Value) 'complete finite object surfaces'
        }
    }
    foreach ($name in $names) { Assert ((Get-FileHash (Join-Path $ActualRoot $name)).Hash -ceq (Get-FileHash (Join-Path $ExpectedRoot $name)).Hash) "object artifact mismatch: $name" }
    "PASS: strict object comparison of $($names.Count) artifacts"
}
CompareObjects (Join-Path $Actual 'objects') (Join-Path $Expected 'objects')

# Indexed observations retain distinct source slots, local logical fields and actual parser remaps.
function CompareIndexed([string]$ActualRoot, [string]$ExpectedRoot) {
    $names=@('observations.tsv','TimeAndSale.qtp','reordered-alias.qtp','missing-high.qtp',
        'session-replacements.qtp','session-renamed.qtp','session-partial-failure.qtp',
        'batch-subset-last.qtp','batch-retyped-last.qtp','batch-renamed-last.qtp',
        'batch-renamed-exact-last.qtp','batch-late-invalid.qtp','batch-late-invalid-prelude.qtp',
        'batch-late-invalid-following-data.qtp','batch-middle-invalid.qtp',
        'batch-middle-invalid-following-data.qtp')+
        @(foreach($type in @(4,8,24,40,56,72,88)){"Pair$type.qtp"})+
        @(foreach($type in @(104,120,152)){"Single$type.qtp"})
    $arity=@{schema=5;slot=8;'source-map'=5;'omitted-tail'=4;'source-lookup'=4;exporter=3;
        source=6;delivered=8;bound=6;remap=3;'partial-failure'=3;reset=3;all=2;
        'batch-begin'=4;'batch-remap'=4;'batch-delivered'=8;'batch-delivery-complete'=4;
        'batch-complete'=5;'batch-invalid-complete'=5;'batch-middle-invalid-complete'=5;'batch-all'=2}
    $required=@{schema=11;slot=45;'source-map'=37;'omitted-tail'=8;'source-lookup'=96;exporter=11;
        source=67;delivered=122;bound=71;remap=23;complete=30;'partial-failure'=1;reset=1;all=1;
        'batch-begin'=4;'batch-remap'=20;'batch-delivered'=8;'batch-delivery-complete'=8;
        'batch-complete'=4;'batch-invalid-complete'=1;'batch-middle-invalid-complete'=1;'batch-all'=1}
    foreach($directory in @($ActualRoot,$ExpectedRoot)) {
        $files=@(Get-ChildItem -LiteralPath $directory -File | Sort-Object Name)
        Assert (($files.Name -join '|') -ceq (($names|Sort-Object)-join '|')) 'finite indexed artifact set'
        foreach($file in $files){Assert ($file.Length -gt 0 -and $file.Length -le 1048576) 'indexed artifact bound'}
        $path=Join-Path $directory 'observations.tsv'
        $bytes=[IO.File]::ReadAllBytes($path)
        Assert ($bytes -notcontains 13 -and $bytes[-1] -eq 10) 'producer canonical LF'
        $rows=[IO.File]::ReadAllLines($path)
        Assert ($rows[0] -ceq ('case'+[char]9+'indexed-schemas') -and
            $rows[-1] -ceq ('complete'+[char]9+'indexed-schemas')) 'indexed capture boundaries'
        $counts=@{}
        for($index=1;$index -lt $rows.Count;++$index) {
            $c=$rows[$index].Split([char]9)
            $kind=$c[0]
            Assert ($c -notcontains '') 'nonempty indexed cells'
            Assert ($required.ContainsKey($kind)) 'known indexed observation'
            if(!$counts.ContainsKey($kind)){$counts[$kind]=0}
            $counts[$kind]++
            if($kind -ceq 'complete') {
                Assert (($c.Count -eq 4 -and $c[3] -ceq '0') -or
                    ($index -eq $rows.Count-1 -and $c.Count -eq 2)) 'completed indexed callback'
            } else {
                Assert ($c.Count -eq $arity[$kind]) "indexed row arity: $kind at row $index"
            }
            switch -CaseSensitive ($kind) {
                'schema' {Assert ($c[2] -ceq 'true' -and $c[4] -ceq '0') 'actual indexed schema';Integer $c[3]}
                'slot' {Integer $c[2];Integer $c[3];Assert ($c[4] -cin @('true','false')) 'source storage width'}
                'source-map' {Integer $c[2];Integer $c[3];Integer $c[4]}
                'omitted-tail' {Integer $c[2];Integer $c[3]}
                'source-lookup' {Assert ($c[3] -cmatch '^(field|tail-owner):[0-9]+$') 'source identity projection'}
                'exporter' {Assert ($c[2] -ceq 'UNSUPPORTED_INDEXED') 'unchanged ordinary exporter gate'}
                'bound' {Integer $c[2];Integer $c[4];Integer $c[5]}
                'source' {
                    Integer $c[2] $true;Integer $c[3];Integer $c[4]
                    Assert ($c[5] -ceq '0' -and [long]$c[2] -eq
                        (([long]$c[3] -shl 32) -bor ([long]$c[4] -band 4294967295L))) 'actual source time words'
                }
                'delivered' {
                    Assert ($c[2] -ceq '15' -and $c[7] -ceq '0') 'STREAM indexed time separate from event time'
                    Integer $c[3];Integer $c[4] $true;Integer $c[5];Integer $c[6]
                    Assert ([long]$c[4] -eq (([long]$c[5] -shl 32) -bor
                        ([long]$c[6] -band 4294967295L))) 'actual delivered time words'
                }
                'batch-delivered' {
                    Integer $c[2];Integer $c[4] $true;Integer $c[5];Integer $c[6]
                    Assert ($c[3] -cin @('Pair8','Other') -and $c[7] -ceq '913') 'actual batch record and sentinel'
                    Assert ([long]$c[4] -eq (([long]$c[5] -shl 32) -bor
                        ([long]$c[6] -band 4294967295L))) 'batch time words'
                }
                'batch-begin' {Assert ($c[2] -cin @('3','4') -and $c[3] -ceq '1-description-frame') 'one repeated-sender batch'}
                'batch-remap' {Integer $c[1];Assert ($c[2] -ceq '0') 'same sparse sender identity'}
                'batch-complete' {Integer $c[2];Integer $c[4] $true}
                'batch-delivery-complete' {Assert ($c[2] -ceq '1' -and $c[3] -ceq '0') 'one complete batch delivery'}
                'batch-invalid-complete' {
                    Assert ($c[1] -ceq '1' -and $c[2] -ceq '2-prefix-installs' -and $c[3] -ceq 'Other') 'late error retains Java prefix'
                    Integer $c[4] $true
                }
                'batch-middle-invalid-complete' {
                    Assert ($c[1] -ceq '1' -and $c[2] -ceq '1-prefix-install' -and $c[3] -ceq 'Pair8') 'middle error stops following Java remap'
                    Integer $c[4] $true
                }
                'partial-failure' {Assert ($c[1] -ceq '1' -and $c[2] -ceq 'Other') 'Java valid-prefix mutation'}
                'reset' {Assert ($c[1] -ceq '0' -and $c[2] -ceq '1') 'actual reset loses sender map'}
                'all' {Assert ($c[1] -ceq 'PASS') 'indexed observation completion'}
                'batch-all' {Assert ($c[1] -ceq 'PASS') 'same-message observation completion'}
            }
        }
        foreach($item in $required.GetEnumerator()){
            Assert ($counts[$item.Key] -eq $item.Value) "finite indexed count $($item.Key)"
        }
    }
    foreach($name in $names){
        Assert ((Get-FileHash -LiteralPath (Join-Path $ActualRoot $name)).Hash -ceq
            (Get-FileHash -LiteralPath (Join-Path $ExpectedRoot $name)).Hash) "indexed artifact mismatch: $name"
    }
    "PASS: strict indexed comparison of $($names.Count) artifacts"
}
CompareIndexed (Join-Path $Actual 'indexed') (Join-Path $Expected 'indexed')

# Flags child capture: exact artifact set and canonical grammar, followed by byte identity.
function CompareFlags([string]$ActualRoot, [string]$ExpectedRoot) {
    $names = @(
        'composer-10-default.qtp',
        'composer-10-empty.qtp',
        'composer-10-hs-hs.qtp',
        'composer-10-hs-unknown.qtp',
        'composer-10-hs.qtp',
        'composer-10-unknown.qtp',
        'composer-15-default.qtp',
        'composer-15-empty.qtp',
        'composer-15-hs-hs.qtp',
        'composer-15-hs-unknown.qtp',
        'composer-15-hs.qtp',
        'composer-15-unknown.qtp',
        'composer-20-default.qtp',
        'composer-20-empty.qtp',
        'composer-20-hs-hs.qtp',
        'composer-20-hs-unknown.qtp',
        'composer-20-hs.qtp',
        'composer-20-unknown.qtp',
        'composer-5-default.qtp',
        'composer-5-empty.qtp',
        'composer-5-hs-hs.qtp',
        'composer-5-hs-unknown.qtp',
        'composer-5-hs.qtp',
        'composer-5-unknown.qtp',
        'composer-unknown-10-empty.qtp',
        'composer-unknown-10-hs.qtp',
        'composer-unknown-15-empty.qtp',
        'composer-unknown-15-hs.qtp',
        'composer-unknown-20-empty.qtp',
        'composer-unknown-20-hs.qtp',
        'composer-unknown-5-empty.qtp',
        'composer-unknown-5-hs.qtp',
        'designed-10-all64.qtp',
        'designed-15-all64.qtp',
        'designed-20-all64.qtp',
        'designed-5-all64.qtp',
        'designed-descriptor-_hs.qtp',
        'designed-descriptor-absent.qtp',
        'designed-descriptor-empty.qtp',
        'designed-descriptor-hs-hs.qtp',
        'designed-descriptor-hs-unknown.qtp',
        'designed-descriptor-hs.qtp',
        'designed-descriptor-unknown.qtp',
        'designed-malformed-1-249.qtp',
        'designed-malformed-1-255.qtp',
        'designed-malformed-2-248.qtp',
        'designed-malformed-2-249.qtp',
        'designed-unknown-100.qtp',
        'designed-unknown-20.qtp',
        'designed-unknown-80.qtp',
        'designed-unknown-80000000.qtp',
        'designed-unknown-ffffffff.qtp',
        'designed-wide-flags.qtp',
        'empty-message.qtp',
        'file-pair-absent-setting.qtp',
        'file-pair-empty.qtp',
        'file-pair-hs.qtp',
        'file-time-and-sale-absent-setting.qtp',
        'file-time-and-sale-empty.qtp',
        'file-time-and-sale-hs.qtp',
        'message-mru-reset.qtp',
        'message-repeat-reset.qtp',
        'observations.tsv',
        'remove-Empty-0.qtp',
        'remove-Empty-1234567889abcdef.qtp',
        'remove-Empty-7fffffffffffffff.qtp',
        'remove-Empty-8000000000000000.qtp',
        'remove-Empty-ffffffffffffffff.qtp',
        'remove-followed-sentinel-Empty.qtp',
        'remove-followed-sentinel-Pair.qtp',
        'remove-followed-sentinel-Plain.qtp',
        'remove-layout-104.qtp',
        'remove-layout-120.qtp',
        'remove-layout-152.qtp',
        'remove-layout-24.qtp',
        'remove-layout-4.qtp',
        'remove-layout-40.qtp',
        'remove-layout-56.qtp',
        'remove-layout-72.qtp',
        'remove-layout-8.qtp',
        'remove-layout-88.qtp',
        'remove-Pair-0-unknown.qtp',
        'remove-Pair-0.qtp',
        'remove-Pair-1234567889abcdef-unknown.qtp',
        'remove-Pair-1234567889abcdef.qtp',
        'remove-Pair-7fffffffffffffff-unknown.qtp',
        'remove-Pair-7fffffffffffffff.qtp',
        'remove-Pair-8000000000000000-unknown.qtp',
        'remove-Pair-8000000000000000.qtp',
        'remove-Pair-ffffffffffffffff-unknown.qtp',
        'remove-Pair-ffffffffffffffff.qtp',
        'remove-Plain-0.qtp',
        'remove-Plain-1234567889abcdef.qtp',
        'remove-Plain-7fffffffffffffff.qtp',
        'remove-Plain-8000000000000000.qtp',
        'remove-Plain-ffffffffffffffff.qtp',
        'remove-truncated-0.qtp',
        'remove-truncated-1.qtp',
        'remove-truncated-2.qtp',
        'remove-truncated-3.qtp',
        'remove-truncated-4.qtp',
        'remove-truncated-5.qtp',
        'remove-truncated-6.qtp',
        'remove-truncated-7.qtp',
        'remove-truncated-8.qtp'
    )
    $required = @{all=1; callback=3305; corrupt=15; 'descriptor-option'=13; layout=10; negative=9; option=6; parse=98; 'partial-frame'=1; primitive=6; 'primitive-error'=3; 'production-file'=6; projection=9; 'raw-removal-time'=794; 'requested-composer'=24}
    $arity = @{all=2; callback=4; corrupt=3; 'descriptor-option'=3; layout=5; negative=4; option=3; parse=5; 'partial-frame'=3; primitive=7; 'primitive-error'=5; 'production-file'=4; projection=5; 'raw-removal-time'=6; 'requested-composer'=4}
    foreach ($root in @($ActualRoot, $ExpectedRoot)) {
        $files = @(Get-ChildItem -LiteralPath $root -File | Sort-Object Name)
        Assert (($files.Name -join '|') -ceq ($names -join '|')) 'exact flags artifact set'
        Assert (@(Get-ChildItem -LiteralPath $root -Directory).Count -eq 0) 'no flags child directories'
        $rows = [IO.File]::ReadAllLines((Join-Path $root 'observations.tsv'))
        Assert ($rows.Count -eq 4300 -and $rows[-1] -ceq "all`tPASS") 'flags completion/count'
        $counts = @{}
        foreach ($line in $rows) {
            $c = $line.Split("`t")
            Assert ($arity.ContainsKey($c[0]) -and $c.Count -eq $arity[$c[0]]) 'flags row kind/arity'
            if (!$counts.ContainsKey($c[0])) { $counts[$c[0]] = 0 }; $counts[$c[0]]++
            if ($c[0] -ceq 'callback') {
                Assert ($c[2] -cin @('10','15','20')) 'labelled callback family'
                $v = $c[3].Split('|')
                Assert ($v.Count -ge 4 -and $v.Count -le 19 -and $v[0] -cmatch '^(Pair|Plain|Empty|Layout(4|8|24|40|56|72|88|104|120|152)|TimeAndSale)$') 'finite record shape'
                Integer $v[1]; Integer $v[2] $true; Assert ($v[3] -ceq '0') 'no FIELD metadata'
                for ($i=4; $i -lt $v.Count; $i++) { Integer $v[$i] }
            } elseif ($c[0] -ceq 'raw-removal-time') {
                Integer $c[3] $true; Integer $c[4] $true; Integer $c[5] $true
                Assert ([long]$c[5] -gt [long]$c[4]) 'removal primitive consumes bytes'
            } elseif ($c[0] -ceq 'projection') {
                Assert ($c[2] -ceq 'STREAM') 'explicit STREAM projection'; Integer $c[3]
                Assert ($c[4] -ceq ('callbacks=' + (3 * [int]$c[3]))) 'RAW three callbacks per wire record'
            } elseif ($c[0] -ceq 'requested-composer') {
                Assert ($c[2] -ceq 'input=all64' -and $c[3] -cin @('mask=0','mask=95')) 'composer masks separate from input'
            } elseif ($c[0] -ceq 'primitive') {
                Integer $c[2]; Integer $c[3]; Integer $c[4]; Integer $c[5]; Integer $c[6]
                Assert ([int]$c[6] -gt [int]$c[5]) 'symbol primitive consumption'
            } elseif ($c[0] -ceq 'negative') {
                Assert ($c[2] -ceq 'completed=0' -and $c[3] -ceq 'errors=1') 'Java buffered-prefix error observation'
            }
        }
        foreach ($item in $required.GetEnumerator()) { Assert ($counts[$item.Key] -eq $item.Value) "finite flags count $($item.Key)" }
    }
    foreach ($name in $names) { Assert ((Get-FileHash -LiteralPath (Join-Path $ActualRoot $name)).Hash -ceq (Get-FileHash -LiteralPath (Join-Path $ExpectedRoot $name)).Hash) "flags artifact mismatch: $name" }
    "PASS: strict flags comparison of $($names.Count) artifacts"
}
CompareFlags (Join-Path $Actual 'flags') (Join-Path $Expected 'flags')

# Exact finite production dispatch capture; random synthetic endpoint UIDs remain in raw files.
# Only the recorded serializer UID range is normalized in memory for comparison, never on disk.
function CompareDispatch([string]$ActualRoot,[string]$ExpectedRoot) {
    $arity=@{all=2;descriptor=10;direct=6;file=7;other=4;record=3;writer=4}
    foreach($mode in @('normal','headerless')) {
        $roots=@((Join-Path $ActualRoot $mode),(Join-Path $ExpectedRoot $mode))
        $required=if($mode -ceq 'normal'){@{all=1;descriptor=59;direct=231;file=295;other=3;record=71;writer=8}}else{@{all=1;file=8;record=8;writer=8}}
        $captures=@()
        foreach($root in $roots) {
            Assert (@(Get-ChildItem -LiteralPath $root -Directory).Count -eq 0) 'no dispatch leaf directories'
            $rows=[IO.File]::ReadAllLines((Join-Path $root 'observations.tsv'))
            Assert ($rows.Count -eq $(if($mode -ceq 'normal'){668}else{25}) -and $rows[-1] -ceq "all`tPASS") 'finite dispatch observations'
            $counts=@{}
            $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            $null=$names.Add('observations.tsv')
            foreach($time in @('NONE','MESSAGE')) {foreach($family in @(5,10,15,20)){$null=$names.Add("writer-$time-$family.qtp")}}
            foreach($line in $rows) {
                $cells=$line.Split("`t")
                Assert ($arity.ContainsKey($cells[0]) -and $cells.Count -eq $arity[$cells[0]]) 'dispatch row arity'
                $counts[$cells[0]]=1+$(if($counts.ContainsKey($cells[0])){$counts[$cells[0]]}else{0})
                if($cells[0] -cne 'all') {Assert ($cells[1] -cmatch '^[A-Za-z0-9_-]+$') 'safe synthetic dispatch case'}
                if($cells[0] -ceq 'file') {$null=$names.Add($cells[1]+'.qtp')}
                if($cells[0] -ceq 'record') {Assert ($cells[2] -cmatch '^(5|10|15|20)/[01]/913/(0|1311768464867721216)$') 'dispatch family/value/timing'}
                if($cells[0] -ceq 'descriptor') {Integer $cells[2];Assert ($cells[9] -cin @('absent','present')) 'endpoint presence'}
            }
            foreach($item in $required.GetEnumerator()){Assert ($counts[$item.Key] -eq $item.Value) "finite dispatch row count $($item.Key)"}
            $ranges=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
            if($mode -ceq 'normal') {
                $null=$names.Add('uid-ranges.tsv')
                $rangeRows=[IO.File]::ReadAllLines((Join-Path $root 'uid-ranges.tsv'))
                Assert ($rangeRows.Count -eq 59 -and $rangeRows[0] -ceq "file`tstart`tlength" -and $rangeRows[-1] -ceq "complete`t57") 'complete random UID range map'
                foreach($line in $rangeRows[1..57]) {
                    $cells=$line.Split("`t");Assert ($cells.Count -eq 3 -and $names.Contains($cells[0])) 'UID range file'
                    Integer $cells[1];Integer $cells[2]
                    $offset=[int]$cells[1];$size=[int]$cells[2]
                    Assert ($offset -ge 0 -and $size -gt 0 -and $size -le 16 -and !$ranges.ContainsKey($cells[0])) 'finite UID range'
                    $ranges.Add($cells[0],@($offset,$size))
                }
            }
            $files=@(Get-ChildItem -LiteralPath $root -File|Sort-Object Name)
            Assert ($files.Count -eq $(if($mode -ceq 'normal'){305}else{17}) -and ($files.Name -join '|') -ceq (($names|Sort-Object) -join '|')) 'exact dispatch artifact set'
            foreach($file in $files){Assert ($file.Length -lt 1048576 -and ($file.Length -gt 0 -or $file.Name -cin @('empty-file.qtp','physical-endpoint-cut-0.qtp'))) 'bounded dispatch artifact'}
            $captures+=,[ordered]@{root=$root;rows=$rows;files=$files;ranges=$ranges}
        }
        Assert (($captures[0].rows -join "`n") -ceq ($captures[1].rows -join "`n")) 'exact dispatch canonical rows'
        foreach($file in $captures[0].files) {
            $left=[IO.File]::ReadAllBytes($file.FullName)
            $right=[IO.File]::ReadAllBytes((Join-Path $captures[1].root $file.Name))
            Assert ($left.Length -eq $right.Length) 'dispatch raw length'
            if($captures[0].ranges.ContainsKey($file.Name)) {
                Assert ($captures[1].ranges.ContainsKey($file.Name)) 'matching UID range case'
                $range=$captures[0].ranges[$file.Name];$other=$captures[1].ranges[$file.Name]
                Assert (($range -join '/') -ceq ($other -join '/') -and $range[0] -le $left.Length - $range[1]) 'matching UID range coordinates'
                for($i=0;$i -lt $range[1];$i++){$left[$range[0]+$i]=0;$right[$range[0]+$i]=0}
            }
            Assert ([Convert]::ToHexString($left) -ceq [Convert]::ToHexString($right)) "dispatch bytes outside recorded random UID: $($file.Name)"
        }
        if($mode -ceq 'normal'){"PASS: finite dispatch normal comparison; only57 synthetic UID ranges normalized in memory"}
        else{"PASS: exact17 headerless artifacts including16 raw tapes; no UID normalization"}
    }
}
CompareDispatch (Join-Path $Actual 'dispatch') (Join-Path $Expected 'dispatch')
