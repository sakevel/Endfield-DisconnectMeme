param([string]$KeybindsDll='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'bootstrap.ps1')
$opts=@()
if($KeybindsDll){$opts+="-DZML_KEYBINDS_DLL=$KeybindsDll"}
cmake -S $root -B (Join-Path $root 'build') -A x64 @opts
if($LASTEXITCODE){throw 'CMake configure failed'}
cmake --build (Join-Path $root 'build') --config Release --parallel 6
if($LASTEXITCODE){throw 'Build failed'}
ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
if($LASTEXITCODE){throw 'Tests failed'}
