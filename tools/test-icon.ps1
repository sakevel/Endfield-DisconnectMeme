$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$path=Join-Path $root 'mod/icon.png'
if((Get-Item -LiteralPath $path).Length -gt 64KB){throw 'Icon exceeds 64 KiB'}
$bytes=[IO.File]::ReadAllBytes($path)
if([Convert]::ToBase64String($bytes[0..7]) -ne 'iVBORw0KGgo='){throw 'Not a PNG'}
Add-Type -AssemblyName System.Drawing
$image=[Drawing.Bitmap]::new($path)
try {
 if($image.Width -ne 256 -or $image.Height -ne 256){throw 'Expected 256x256'}
 if($image.GetPixel(0,0).A -ne 0 -or $image.GetPixel(255,255).A -ne 0){throw 'Exterior must be transparent'}
 $solid=0; $white=0; $yellow=0
 for($y=0;$y -lt 256;$y++){for($x=0;$x -lt 256;$x++){
  $c=$image.GetPixel($x,$y)
  # Generated artwork uses 253/254 alpha for otherwise solid interior pixels.
  if($c.A -ge 250){$solid++}
  if($c.A -gt 240 -and $c.R -gt 230 -and $c.G -gt 230 -and $c.B -gt 230){$white++}
  if($c.A -gt 240 -and $c.R -gt 220 -and $c.G -gt 200 -and $c.B -lt 60){$yellow++}
 }}
 if($solid -lt 10000 -or $white -lt 1000 -or $yellow -lt 300){throw 'Missing opaque badge, white silhouette or yellow accents'}
} finally {$image.Dispose()}
Write-Output 'PNG decoding, dimensions, transparency, solid badge, white and yellow passed'
