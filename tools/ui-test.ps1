# End-to-end UI test for RoomHome. Runs the real window off-screen (never activated) with a
# throw-away config, drives it with posted mouse/keyboard messages and captures it with PrintWindow.
# It only ever touches its own process: a RoomHome you are using is left alone.
# Run with Windows PowerShell 5.1:  powershell -File tools\ui-test.ps1 -Arch x64 -OutDir <dir> [-Camera]
# -Camera adds a live camera test: it turns this PC's room camera on for a few seconds. It checks the app's
# log only - no camera picture is captured or saved.
param([string]$Arch = 'x64', [Parameter(Mandatory)][string]$OutDir, [switch]$Camera)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class W {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, string l);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder sb, int n);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    static string Cls(IntPtr h) { var sb = new StringBuilder(64); GetClassName(h, sb, 64); return sb.ToString(); }
    public static IntPtr MainOf(int pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, p) => { uint wp; GetWindowThreadProcessId(h, out wp);
            if (wp == (uint)pid && Cls(h) == "RoomHome.MainWindow") { found = h; return false; } return true; }, IntPtr.Zero);
        return found;
    }
    public static List<IntPtr> Edits(IntPtr parent) {
        var list = new List<IntPtr>();
        EnumChildWindows(parent, (h, p) => { if (Cls(h) == "Edit" && IsWindowVisible(h)) list.Add(h); return true; }, IntPtr.Zero);
        return list;
    }
}
'@
# Posted mouse coordinates and PrintWindow sizes are DPI-virtualised for unaware callers.
[void][W]::SetProcessDpiAwarenessContext([IntPtr](-4))

$exe = Join-Path (Split-Path $PSScriptRoot) "build\$Arch\RoomHome.exe"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$cfg = Join-Path $OutDir "ui-test-$Arch.json"
Remove-Item $cfg -ErrorAction SilentlyContinue
$fails = 0
function Check($name, $ok) { if ($ok) { "PASS $name" } else { "FAIL $name"; $script:fails++ } }

function Shot($h, $name) {
    $r = New-Object W+RECT; [void][W]::GetClientRect($h, [ref]$r)
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [void][W]::PrintWindow($h, $dc, 3)   # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    $g.ReleaseHdc($dc); $g.Dispose()
    $bmp.Save((Join-Path $OutDir "$name.png")); $bmp.Dispose()
}
# window 1280x720 physical px -> scale 2/3, virtual 1920x1080; client px = virtual * 2/3
function Click($h, [double]$vx, [double]$vy) {
    $x = [int]($vx * 2 / 3); $y = [int]($vy * 2 / 3)
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [void][W]::PostMessage($h, 0x0200, [IntPtr]0, $lp); Start-Sleep -Milliseconds 120   # WM_MOUSEMOVE
    [void][W]::PostMessage($h, 0x0201, [IntPtr]1, $lp); Start-Sleep -Milliseconds 120   # WM_LBUTTONDOWN
    [void][W]::PostMessage($h, 0x0202, [IntPtr]0, $lp); Start-Sleep -Milliseconds 450   # WM_LBUTTONUP
}
function Key($h, [int]$vk) {
    [void][W]::PostMessage($h, 0x0100, [IntPtr]$vk, [IntPtr]1); Start-Sleep -Milliseconds 80
    [void][W]::PostMessage($h, 0x0101, [IntPtr]$vk, [IntPtr]0xC0000001); Start-Sleep -Milliseconds 400
}
function ReadCfg { if (Test-Path $cfg) { Get-Content -Raw -Encoding UTF8 $cfg | ConvertFrom-Json } else { $null } }

$left = [W]::GetSystemMetrics(76) - 1500   # left of the whole virtual desktop
function StartApp([string[]]$extra) {
    $a = @('--window', '1280x720', '--pos', "$left,40", '--noactivate', '--dry-run', '--config', $cfg) + $extra
    $p = Start-Process $exe -ArgumentList $a -PassThru
    $h = [IntPtr]::Zero
    for ($i = 0; $i -lt 60 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100; $h = [W]::MainOf($p.Id) }
    Start-Sleep -Milliseconds 1300
    return @($p, $h)
}
function StopApp($p, $h) {
    if ($h -ne [IntPtr]::Zero) { [void][W]::PostMessage($h, 0x8007, [IntPtr]0, [IntPtr]0) }   # WM_APP_QUIT
    if (-not $p.WaitForExit(8000)) { Stop-Process -Id $p.Id -Force; return $false }
    return $true
}

$fgBefore = [W]::GetForegroundWindow()

# ---- run 1: rename, theme switch, autostart toggle, dry-run launch
$p, $h = StartApp @('--open', 'rename')
try {
    Check 'window created' ($h -ne [IntPtr]::Zero)
    Check 'focus not stolen' ([W]::GetForegroundWindow() -eq $fgBefore)
    Shot $h "$Arch-1-rename"
    $edits = [W]::Edits($h)
    Check 'rename sheet shows 2 text fields' ($edits.Count -eq 2)
    if ($edits.Count -ge 1) {
        [void][W]::SendMessage($edits[0], 0x000C, [IntPtr]0, 'Board Room 3')   # WM_SETTEXT -> EN_CHANGE
        Start-Sleep -Milliseconds 200
        [void][W]::PostMessage($edits[0], 0x0100, [IntPtr]0x0D, [IntPtr]1)        # Enter
        Start-Sleep -Milliseconds 900
    }
    Check 'room name saved' ((ReadCfg).roomName -eq 'Board Room 3')
    Check 'text fields gone after save' (([W]::Edits($h)).Count -eq 0)

    $before = (ReadCfg).theme
    Click $h 1671 69
    $after = (ReadCfg).theme
    Check "theme switch ($before -> $after)" ($after -eq 'light' -or $after -eq 'dark')

    $run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
    $had = (Get-ItemProperty $run -Name RoomHome -ErrorAction SilentlyContinue) -ne $null
    Click $h 1751 69
    Start-Sleep -Milliseconds 600
    Click $h 865 574
    Start-Sleep -Milliseconds 400
    $val = (Get-ItemProperty $run -Name RoomHome -ErrorAction SilentlyContinue).RoomHome
    Check 'autostart switched on (HKCU Run)' ($val -like "*$Arch\RoomHome.exe*--autostart*")
    Click $h 865 574
    Start-Sleep -Milliseconds 400
    $gone = (Get-ItemProperty $run -Name RoomHome -ErrorAction SilentlyContinue) -eq $null
    Check 'autostart switched off again' $gone
    if ($had -and $gone) { "NOTE: a RoomHome Run entry existed before the test and was removed" }
    Key $h 0x1B
    Click $h 260 912
    Start-Sleep -Milliseconds 900
    Shot $h "$Arch-2-launch"
} finally { Check 'run 1 closed its own window' (StopApp $p $h) }

# ---- run 2: widgets - add a Note from the gallery, give it a title, save with Enter
$p, $h = StartApp @('--open', 'addwidget')
try {
    Check 'widget gallery open' ($h -ne [IntPtr]::Zero)
    Shot $h "$Arch-3-gallery"
    Click $h 960 651                       # "Note" card (row 2, column 2)
    Start-Sleep -Milliseconds 700
    $edits = [W]::Edits($h)
    Check 'note editor shows title + message fields' ($edits.Count -eq 2)
    if ($edits.Count -ge 1) {
        [void][W]::SendMessage($edits[0], 0x000C, [IntPtr]0, 'Test note')
        Start-Sleep -Milliseconds 200
        Shot $h "$Arch-4-note-editor"
        [void][W]::PostMessage($edits[0], 0x0100, [IntPtr]0x0D, [IntPtr]1)
        Start-Sleep -Milliseconds 900
    }
    $c = ReadCfg
    Check 'four widgets saved' ($c -and $c.widgets.Count -eq 4)
    Check 'note widget saved with its title' ($c -and $c.widgets[3].type -eq 'note' -and $c.widgets[3].title -eq 'Test note')
    Shot $h "$Arch-5-with-note"
} finally { Check 'run 2 closed its own window' (StopApp $p $h) }

# ---- run 3: a photo widget loads through the background decoder
$photo = Get-ChildItem "$env:WINDIR\Web\Wallpaper" -Recurse -Include *.jpg -ErrorAction SilentlyContinue | Select-Object -First 1
if ($photo) {
    $c = ReadCfg
    $c.widgets[3] = [pscustomobject]@{ type = 'photo'; path = $photo.FullName; source = 'photo'; interval = 30; shuffle = $false; shape = 'landscape'; fit = 'fill'; caption = 'Test photo' }
    [IO.File]::WriteAllText($cfg, ($c | ConvertTo-Json -Depth 5))
    $p, $h = StartApp @()
    try {
        Start-Sleep -Milliseconds 1500
        Shot $h "$Arch-6-photo"
        # sample the photo area: a decoded picture is not one flat colour
        $bmp = [System.Drawing.Bitmap]::FromFile((Join-Path $OutDir "$Arch-6-photo.png"))
        $cols = @{}
        foreach ($x in 0..9) { foreach ($y in 0..5) {
            $px = $bmp.GetPixel([int](1000 + $x * 22), [int](300 + $y * 20)); $cols["$([int]($px.R/16))-$([int]($px.G/16))-$([int]($px.B/16))"] = 1 } }
        $bmp.Dispose()
        Check "photo widget shows a picture ($($cols.Count) distinct colours)" ($cols.Count -ge 8)
    } finally { Check 'run 3 closed its own window' (StopApp $p $h) }
}

# ---- run 4: language - Settings > Language: ไทย, then English, then back to Auto
$p, $h = StartApp @('--open', 'settings')
try {
    Check 'settings open' ($h -ne [IntPtr]::Zero)
    Click $h 1478 255                      # "ไทย" (Language row, right column)
    Start-Sleep -Milliseconds 500
    Check 'language switched to Thai' ((ReadCfg).language -eq 'th')
    Shot $h "$Arch-7-settings-th"
    Click $h 1365 255                      # "English"
    Start-Sleep -Milliseconds 500
    Check 'language switched to English' ((ReadCfg).language -eq 'en')
    Click $h 1252 255                      # "Auto"
    Start-Sleep -Milliseconds 500
    Check 'language back to Auto' ((ReadCfg).language -eq 'auto')
    Check 'room name kept through the switches' ((ReadCfg).roomName -eq 'Board Room 3')
} finally { Check 'run 4 closed its own window' (StopApp $p $h) }

# ---- run 5 (-Camera only): the camera test sheet shows the room camera live, and lets it go when closed
if ($Camera) {
    $log = Join-Path $OutDir "$Arch-camera.log"
    Remove-Item $log -ErrorAction SilentlyContinue
    $p, $h = StartApp @('--open', 'camera', '--log', $log)
    try {
        Check 'camera sheet open' ($h -ne [IntPtr]::Zero)
        $t = ''
        $deadline = (Get-Date).AddSeconds(20)
        do {
            Start-Sleep -Milliseconds 500
            if (Test-Path $log) {   # the app is still writing it: share read/write
                $fs = [IO.File]::Open($log, 'Open', 'Read', 'ReadWrite')
                $sr = New-Object IO.StreamReader($fs)
                $t = $sr.ReadToEnd()
                $sr.Close()
            }
        } until ($t -match 'camera: \d+ pictures' -or $t -match 'camera: phase=3' -or (Get-Date) -gt $deadline)
        $first = [regex]::Match($t, 'camera: phase=2 fail=0 hr=00000000 size=(\d+)x(\d+) first=(\d+)ms')
        Check "camera picture arrives ($($first.Groups[1].Value)x$($first.Groups[2].Value) after $($first.Groups[3].Value) ms)" $first.Success
        $rate = [regex]::Match($t, 'camera: (\d+) pictures, ([\d.]+) fps, brightness (\d+)')
        Check "camera keeps streaming ($($rate.Groups[2].Value) fps)" ($rate.Success -and [double]$rate.Groups[2].Value -ge 5)
        $c = ReadCfg
        Check "room camera picked automatically ($($c.camera))" ($c -and $c.cameraId -like 'USB\VID_*')
        Key $h 0x1B                              # Esc closes the sheet, which must release the camera
        $deadline = (Get-Date).AddSeconds(5)
        do {
            Start-Sleep -Milliseconds 300
            $fs = [IO.File]::Open($log, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close()
        } until ($t -match 'camera: released' -or (Get-Date) -gt $deadline)
        Check 'camera released when the sheet closes' ($t -match 'camera: released')
    } finally { Check 'run 5 closed its own window' (StopApp $p $h) }
}
"failures: $fails"
exit $fails
