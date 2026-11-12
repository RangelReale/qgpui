# Draws the temporary QGPUI icon: PNG files in several sizes and a Windows .ico that holds them all.
# Run with Windows PowerShell from this folder: powershell -ExecutionPolicy Bypass -File make-icon.ps1
# The outputs are committed; run this again only to change the icon.

Add-Type -AssemblyName System.Drawing

$sizes = 16, 24, 32, 48, 64, 128, 256
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function New-IconBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.Clear([System.Drawing.Color]::Transparent)

    # Rounded square with a diagonal gradient.
    $inset = [Math]::Max(1, $size / 32)
    $rect = New-Object System.Drawing.RectangleF $inset, $inset, ($size - 2 * $inset), ($size - 2 * $inset)
    $radius = $rect.Width * 0.22
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $path.AddArc($rect.X, $rect.Y, $d, $d, 180, 90)
    $path.AddArc($rect.Right - $d, $rect.Y, $d, $d, 270, 90)
    $path.AddArc($rect.Right - $d, $rect.Bottom - $d, $d, $d, 0, 90)
    $path.AddArc($rect.X, $rect.Bottom - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect,
        ([System.Drawing.Color]::FromArgb(255, 32, 120, 220)),
        ([System.Drawing.Color]::FromArgb(255, 120, 50, 200)), 45.0
    $g.FillPath($brush, $path)

    # A white "Q" in the middle.
    $font = New-Object System.Drawing.Font 'Segoe UI', ($size * 0.62), ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center
    $textRect = New-Object System.Drawing.RectangleF 0, (-$size * 0.03), $size, $size
    $g.DrawString('Q', $font, [System.Drawing.Brushes]::White, $textRect, $format)

    $g.Dispose()
    return $bmp
}

$pngs = @()
foreach ($size in $sizes) {
    $bmp = New-IconBitmap $size
    $file = Join-Path $here "qgpui-$size.png"
    $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $pngs += , @($size, [System.IO.File]::ReadAllBytes($file))
}

# ICO container with PNG-compressed images (supported since Windows Vista).
$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $stream
$writer.Write([UInt16]0)            # reserved
$writer.Write([UInt16]1)            # type: icon
$writer.Write([UInt16]$pngs.Count)
$offset = 6 + 16 * $pngs.Count
foreach ($entry in $pngs) {
    $size = $entry[0]; $bytes = $entry[1]
    $dim = if ($size -ge 256) { 0 } else { $size }
    $writer.Write([Byte]$dim)        # width (0 means 256)
    $writer.Write([Byte]$dim)        # height
    $writer.Write([Byte]0)           # palette colours
    $writer.Write([Byte]0)           # reserved
    $writer.Write([UInt16]1)         # colour planes
    $writer.Write([UInt16]32)        # bits per pixel
    $writer.Write([UInt32]$bytes.Length)
    $writer.Write([UInt32]$offset)
    $offset += $bytes.Length
}
foreach ($entry in $pngs) { $writer.Write([Byte[]]$entry[1]) }
$writer.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $here 'qgpui.ico'), $stream.ToArray())
Write-Output "Wrote qgpui.ico and $($pngs.Count) PNG files to $here"
