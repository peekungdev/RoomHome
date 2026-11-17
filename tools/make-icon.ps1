# Renders res\RoomHome.ico (PNG-compressed entries): an aurora-gradient tile with a clock face
# and a small dock. Run with Windows PowerShell 5.1 (System.Drawing).
param([string]$Out = (Join-Path (Join-Path $PSScriptRoot '..') (Join-Path 'res' 'RoomHome.ico')))
Add-Type -AssemblyName System.Drawing

function RoundRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90); $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

$sizes = 16, 20, 24, 32, 40, 48, 64, 96, 128, 256
$pngs = foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)

    $m = [Math]::Max(0.5, $s * 0.03)
    $tile = RoundRect $m $m ($s - 2 * $m) ($s - 2 * $m) ([Math]::Max(2, $s * 0.22))
    $rect = New-Object System.Drawing.RectangleF 0, 0, $s, $s
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb(255, 40, 92, 255)), ([System.Drawing.Color]::FromArgb(255, 132, 52, 230)), 45
    $g.FillPath($grad, $tile)
    # soft highlight
    $hl = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb(70, 255, 255, 255)), ([System.Drawing.Color]::FromArgb(0, 255, 255, 255)), 90
    $g.FillPath($hl, $tile)

    $white = [System.Drawing.Color]::White
    $small = $s -lt 32
    $cx = $s / 2; $cy = if ($small) { $s / 2 } else { $s * 0.43 }
    $r = if ($small) { $s * 0.33 } else { $s * 0.27 }
    $pen = New-Object System.Drawing.Pen $white, ([Math]::Max(1.2, $s * 0.055))
    $g.DrawEllipse($pen, $cx - $r, $cy - $r, 2 * $r, 2 * $r)
    $hand = New-Object System.Drawing.Pen $white, ([Math]::Max(1.2, $s * 0.06))
    $hand.StartCap = 'Round'; $hand.EndCap = 'Round'
    $g.DrawLine($hand, $cx, $cy, $cx, $cy - $r * 0.62)
    $g.DrawLine($hand, $cx, $cy, $cx + $r * 0.48, $cy + $r * 0.18)
    if (-not $small) {
        $sec = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 159, 10)), ([Math]::Max(1, $s * 0.022))
        $sec.StartCap = 'Round'; $sec.EndCap = 'Round'
        $g.DrawLine($sec, $cx, $cy, $cx - $r * 0.55, $cy + $r * 0.62)
        $dot = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 159, 10))
        $g.FillEllipse($dot, $cx - $s * 0.03, $cy - $s * 0.03, $s * 0.06, $s * 0.06)
        # dock
        $db = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(150, 255, 255, 255))
        $dw = $s * 0.17; $dh = $s * 0.10; $gap = $s * 0.045
        $dx = $cx - (3 * $dw + 2 * $gap) / 2; $dy = $s * 0.78
        for ($i = 0; $i -lt 3; $i++) {
            $p = RoundRect ($dx + $i * ($dw + $gap)) $dy $dw $dh ($dh * 0.3)
            $g.FillPath($db, $p)
        }
    }
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    , $ms.ToArray()
}

$fs = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $len = $pngs[$i].Length
    $bw.Write([byte]($s % 256)); $bw.Write([byte]($s % 256)); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32); $bw.Write([uint32]$len); $bw.Write([uint32]$offset)
    $offset += $len
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Flush()
$dir = Split-Path $Out
New-Item -ItemType Directory -Force $dir | Out-Null
[System.IO.File]::WriteAllBytes((Join-Path (Resolve-Path $dir).Path (Split-Path $Out -Leaf)), $fs.ToArray())
"wrote $Out"
