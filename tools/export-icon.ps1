$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Add-Type -AssemblyName System.Drawing
$src=[Drawing.Image]::FromFile((Join-Path $root 'assets/icon-source.png'))
$output=[Drawing.Bitmap]::new(256,256,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
 $g=[Drawing.Graphics]::FromImage($output)
 try {
  $g.Clear([Drawing.Color]::Transparent)
  $g.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($src,0,0,256,256)
 } finally {$g.Dispose()}
 $path=Join-Path $root 'mod/icon.png'
 $output.Save($path,[Drawing.Imaging.ImageFormat]::Png)
 if((Get-Item -LiteralPath $path).Length -gt 64KB){throw 'PNG exceeds public API limit'}
 Write-Output $path
} finally {$output.Dispose();$src.Dispose()}
