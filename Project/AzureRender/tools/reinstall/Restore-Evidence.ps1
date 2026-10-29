param([switch]$CheckOnly, [ValidateSet('Runs','All')][string]$Scope='Runs')
$ErrorActionPreference='Stop'
$engineRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ([IO.Path]::GetPathRoot($engineRoot) -ne 'D:\') { throw 'Restore the project on D: first.' }
$archive=Join-Path $engineRoot 'build/archives/development-evidence-20261029.tar.zst'
$receiptPath=$archive+'.receipt.json'
if (!(Test-Path -LiteralPath $receiptPath)) { throw 'Missing verified archive receipt.' }
$receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
if ($receipt.status -ne 'verified') { throw 'Archive verification is incomplete.' }
if ((Get-Item -LiteralPath $archive).Length -ne $receipt.archiveBytes) { throw 'Archive length mismatch.' }
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $receipt.archiveSha256) { throw 'Archive SHA256 mismatch.' }
Write-Output 'Archive SHA256 verified.'
if ($CheckOnly) { return }
$inputs=if($Scope -eq 'All'){@($receipt.inputs)}else{@('build/runs')}
foreach ($relative in $inputs) {
    $target=[IO.Path]::GetFullPath((Join-Path $engineRoot $relative))
    if (!$target.StartsWith($engineRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid restore boundary.' }
    if (Test-Path -LiteralPath $target) { throw "Destination exists; preserve it and inspect before restoring: $target" }
}
$tarArgs=@('-xf',$archive,'-C',$engineRoot)+$inputs
& tar @tarArgs
if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $LASTEXITCODE. Retain the archive and inspect partial outputs." }
Write-Output 'Evidence restored at its original project-relative paths.'
