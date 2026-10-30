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
