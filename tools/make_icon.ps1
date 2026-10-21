# Draws the application icon (assets/library.ico) from 10x10 pixel art.
# The artwork uses only pure black and pure white, nearest-neighbour scaled.
# Run from anywhere:  powershell -ExecutionPolicy Bypass -File tools/make_icon.ps1

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'assets'
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null
$icoPath = Join-Path $assetDir 'library.ico'

# 10x10 logo: three books on a shelf.
$art = @(
    '..........',
    '.##..##..#',
    '.##..##..#',
    '.##..##..#',
    '.##..##..#',
    '.##..##..#',
    '.##..##..#',
    '##########',
    '..........',
    '..........'
)

function New-IconPng([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

    $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
    for ($row = 0; $row -lt 10; $row++) {
        for ($col = 0; $col -lt 10; $col++) {
            if ($art[$row][$col] -ne '#') { continue }
            $x0 = [int][Math]::Floor($col * $size / 10.0)
            $x1 = [int][Math]::Floor(($col + 1) * $size / 10.0)
            $y0 = [int][Math]::Floor($row * $size / 10.0)
            $y1 = [int][Math]::Floor(($row + 1) * $size / 10.0)
            $g.FillRectangle($white, $x0, $y0, $x1 - $x0, $y1 - $y0)
        }
    }

    $white.Dispose()
    $g.Dispose()

    $stream = New-Object System.IO.MemoryStream
    $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $stream.ToArray()
    $stream.Dispose()
    $bmp.Dispose()
    return $bytes
}

$images = @()
foreach ($size in @(16, 24, 32, 48, 64, 128, 256)) {
    $bytes = [byte[]](New-IconPng $size)
    $images += [pscustomobject]@{ Size = $size; Bytes = $bytes }
}

$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($stream)
$writer.Write([UInt16]0)                 # reserved
$writer.Write([UInt16]1)                 # type: icon
$writer.Write([UInt16]$images.Count)     # image count

$offset = 6 + 16 * $images.Count
foreach ($image in $images) {
    $dimension = if ($image.Size -ge 256) { 0 } else { $image.Size }
    $writer.Write([byte]$dimension)      # width
    $writer.Write([byte]$dimension)      # height
    $writer.Write([byte]0)               # palette colours
    $writer.Write([byte]0)               # reserved
    $writer.Write([UInt16]1)             # colour planes
    $writer.Write([UInt16]32)            # bits per pixel
    $writer.Write([UInt32]$image.Bytes.Length)
    $writer.Write([UInt32]$offset)
    $offset += $image.Bytes.Length
}
foreach ($image in $images) {
    $writer.Write([byte[]]$image.Bytes)
}
$writer.Flush()
[System.IO.File]::WriteAllBytes($icoPath, $stream.ToArray())
$writer.Dispose()
$stream.Dispose()

Write-Host "Wrote $icoPath"
