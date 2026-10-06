$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$path=Join-Path $root 'third_party/minhook'
if(-not(Test-Path -LiteralPath $path)){
    gh repo clone TsudaKageyu/minhook $path -- --branch v1.3.4 --depth 1
    if($LASTEXITCODE){throw 'Authenticated upstream clone failed'}
}
if((git -C $path rev-parse HEAD) -ne 'c3fcafdc10146beb5919319d0683e44e3c30d537'){throw 'Unexpected upstream revision'}
if(git -C $path status --porcelain){throw 'Preserving dirty third-party checkout; resolve manually'}
