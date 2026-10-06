$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$source=Join-Path $root 'build/package/Release/disconnect-meme'
$bundle=Get-Content -LiteralPath (Join-Path $source 'zml-package.json') -Raw -Encoding utf8 | ConvertFrom-Json
$expected=@('mod.ini','config.ini','icon.png','DisconnectMeme.dll','MINHOOK-LICENSE.txt')
if($bundle.id -ne 'disconnect-meme' -or (Compare-Object @($bundle.files) $expected)){throw 'Unexpected package whitelist'}
$files=$expected+@('zml-package.json')
foreach($f in $files){if(-not(Test-Path -LiteralPath (Join-Path $source $f) -PathType Leaf)){throw "Missing $f"}}
$dist=Join-Path $root 'dist';New-Item -ItemType Directory -Force $dist | Out-Null
$ini=Get-Content -LiteralPath (Join-Path $source 'mod.ini') -Raw -Encoding utf8
if($ini -notmatch '(?m)^version=(\d+\.\d+\.\d+)\r?$'){throw 'Invalid version'}
$zip=Join-Path $dist ('EndfieldDisconnectMeme-'+$Matches[1]+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.zip')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
try {foreach($f in $files){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $source $f),$f,[IO.Compression.CompressionLevel]::Optimal)|Out-Null}}
finally {$archive.Dispose()}
(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash+'  '+(Split-Path $zip -Leaf)|Set-Content -Encoding ascii ($zip+'.sha256')
Write-Output $zip
