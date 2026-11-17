# Records bursts of frames from a live (off-screen, never activated) RoomHome window into contact
# sheets, to find animation glitches that single screenshots miss.
#   powershell -File tools\frame-burst.ps1 -Arch x64 -OutDir <dir> [-Config <json>]
param([string]$Arch = 'x64', [Parameter(Mandatory)][string]$OutDir, [string]$Config = '')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class FB {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    public static IntPtr MainOf(int pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, p) => { uint wp; GetWindowThreadProcessId(h, out wp); var sb = new StringBuilder(64); GetClassName(h, sb, 64);
            if (wp == (uint)pid && sb.ToString() == "RoomHome.MainWindow") { found = h; return false; } return true; }, IntPtr.Zero);
        return found;
    }
}
'@
[void][FB]::SetProcessDpiAwarenessContext([IntPtr](-4))
New-Item -ItemType Directory -Force $OutDir | Out-Null
$exe = Join-Path (Split-Path $PSScriptRoot) "build\$Arch\RoomHome.exe"
$cfg = Join-Path $OutDir 'burst.json'
if ($Config) { Copy-Item $Config $cfg -Force } else { Remove-Item $cfg -ErrorAction SilentlyContinue }
$WinW = 1440; $WinH = 900   # 16:10 like the user's screen; virtual 1728 x 1080, scale 0.8333
$k = $WinH / 1080.0
$left = [FB]::GetSystemMetrics(76) - $WinW - 200
$p = Start-Process $exe -ArgumentList @('--window', "${WinW}x${WinH}", '--pos', "$left,40", '--noactivate', '--dry-run', '--config', $cfg) -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 60 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100; $h = [FB]::MainOf($p.Id) }
Start-Sleep -Milliseconds 1500

function Grab() {
    $bmp = New-Object System.Drawing.Bitmap $WinW, $WinH
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [void][FB]::PrintWindow($h, $dc, 3); $g.ReleaseHdc($dc); $g.Dispose()
    return $bmp
}
function Burst($name, [int]$count, [int]$gapMs, [scriptblock]$during) {
    $frames = @()
    for ($i = 0; $i -lt $count; $i++) {
        if ($during) { & $during $i }
        $frames += ,(Grab)
        Start-Sleep -Milliseconds $gapMs
    }
    $cols = 4; $tw = 720; $th = 450
    $rows = [Math]::Ceiling($frames.Count / $cols)
    $sheet = New-Object System.Drawing.Bitmap ($cols * $tw), ($rows * $th)
    $g = [System.Drawing.Graphics]::FromImage($sheet)
    $g.InterpolationMode = 'HighQualityBicubic'
    for ($i = 0; $i -lt $frames.Count; $i++) {
        $g.DrawImage($frames[$i], ($i % $cols) * $tw, [Math]::Floor($i / $cols) * $th, $tw, $th)
        $g.DrawString("$i", (New-Object System.Drawing.Font 'Consolas', 14), [System.Drawing.Brushes]::Red, ($i % $cols) * $tw + 4, [Math]::Floor($i / $cols) * $th + 4)
        $frames[$i].Save((Join-Path $OutDir ("$name-{0:D2}.png" -f $i)))
        $frames[$i].Dispose()
    }
    $g.Dispose(); $sheet.Save((Join-Path $OutDir "$name-sheet.png")); $sheet.Dispose()
    "$name : $($frames.Count) frames"
}
function Post($msg, $wp, $vx, $vy) {
    $x = [int]($vx * $k); $y = [int]($vy * $k)
    [void][FB]::PostMessage($h, $msg, [IntPtr]$wp, [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF)))
}

try {
    # 1. second boundary: clock tick + seconds slide
    $ms = (Get-Date).Millisecond; Start-Sleep -Milliseconds ([Math]::Max(0, 950 - $ms))
    Burst 'tick' 12 25 $null
    # 2. hover a dock tile (tilt), moving across it
    Burst 'hover' 12 30 { param($i) Post 0x0200 0 (110 + $i * 15) (790 + $i * 6) }
    Post 0x0200 0 864 600; Start-Sleep -Milliseconds 400
    # 3. enter edit mode (right-click the first tile), then leave with Esc
    Burst 'edit' 12 40 { param($i) if ($i -eq 1) { Post 0x0204 2 230 900; Start-Sleep -Milliseconds 30; Post 0x0205 0 230 900 } }
    [void][FB]::PostMessage($h, 0x0100, [IntPtr]0x1B, [IntPtr]1); Start-Sleep -Milliseconds 800
    # 4. open and close the power sheet
    Burst 'sheet' 12 40 { param($i) if ($i -eq 1) { Post 0x0201 1 1666 69; Start-Sleep -Milliseconds 30; Post 0x0202 0 1666 69 } elseif ($i -eq 7) { [void][FB]::PostMessage($h, 0x0100, [IntPtr]0x1B, [IntPtr]1) } }
    Start-Sleep -Milliseconds 600
    # 5. light/dark switch (sun/moon button)
    Burst 'theme' 12 60 { param($i) if ($i -eq 1) { Post 0x0201 1 1474 69; Start-Sleep -Milliseconds 30; Post 0x0202 0 1474 69 } }
} finally {
    if ($h -ne [IntPtr]::Zero) { [void][FB]::PostMessage($h, 0x8007, [IntPtr]0, [IntPtr]0) }
    if (-not $p.WaitForExit(8000)) { Stop-Process -Id $p.Id -Force }
}
