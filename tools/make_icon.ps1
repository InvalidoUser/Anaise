# Draws the application icon (assets/tower.ico) from scratch.
# Run from anywhere:  powershell -ExecutionPolicy Bypass -File tools/make_icon.ps1

Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'assets'
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null
$icoPath = Join-Path $assetDir 'tower.ico'

function New-IconPng([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.ScaleTransform($size / 256.0, $size / 256.0)

    # Rounded dark plate.
    $plate = New-Object System.Drawing.Drawing2D.GraphicsPath
    $plate.AddArc(10, 10, 60, 60, 180, 90)
    $plate.AddArc(186, 10, 60, 60, 270, 90)
    $plate.AddArc(186, 186, 60, 60, 0, 90)
    $plate.AddArc(10, 186, 60, 60, 90, 90)
    $plate.CloseFigure()
    $plateBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 12, 16, 22))
    $g.FillPath($plateBrush, $plate)

    # Stars.
    $starBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(230, 220, 214, 190))
    foreach ($star in @(@(56, 54, 3), @(198, 46, 2), @(44, 118, 2), @(210, 138, 3), @(78, 30, 2))) {
        $g.FillEllipse($starBrush, $star[0], $star[1], $star[2], $star[2])
    }

    # Spire roof.
    $roof = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF(128, 26)),
        (New-Object System.Drawing.PointF(96, 88)),
        (New-Object System.Drawing.PointF(160, 88)))
    $gold = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 233, 184, 110))
    $g.FillPolygon($gold, $roof)

    # Tower body.
    $body = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF(100, 86)),
        (New-Object System.Drawing.PointF(156, 86)),
        (New-Object System.Drawing.PointF(168, 214)),
        (New-Object System.Drawing.PointF(88, 214)))
    $stone = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 205, 158, 84))
    $g.FillPolygon($stone, $body)

    # Battlements.
    $battlementBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 233, 184, 110))
    foreach ($x in @(100, 122, 144)) {
        $g.FillRectangle($battlementBrush, $x, 72, 12, 18)
    }

    # Lit window.
    $windows = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 242, 190))
    $g.FillRectangle($windows, 116, 116, 24, 30)
    $glow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(90, 255, 226, 150))
    $g.FillRectangle($glow, 110, 110, 36, 42)

    # Door.
    $door = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 58, 44, 24))
    $g.FillRectangle($door, 117, 180, 22, 34)

    # Ground.
    $ground = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 110, 86, 48))
    $g.FillRectangle($ground, 74, 214, 108, 10)

    $glow.Dispose(); $windows.Dispose(); $door.Dispose(); $ground.Dispose()
    $stone.Dispose(); $gold.Dispose(); $battlementBrush.Dispose(); $starBrush.Dispose()
    $plateBrush.Dispose(); $plate.Dispose()
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
