# Builds the per-user TEST variant of the installer and checks install -> reinstall with options ->
# uninstall. No UAC: it installs to %LOCALAPPDATA%\Programs\RoomHome-test and writes HKCU only.
# Run with PowerShell (not Git Bash: it rewrites /S and /ARCH= switches).
param([Parameter(Mandatory)][string]$OutDir)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$setup = Join-Path $OutDir 'RoomHome-test-setup.exe'
New-Item -ItemType Directory -Force $OutDir | Out-Null
& 'C:\Program Files (x86)\NSIS\makensis.exe' /V1 /DVERSION=1.2.0 /DTESTBUILD "/DOUTFILE=$setup" (Join-Path $root 'installer\RoomHome.nsi')
if ($LASTEXITCODE -ne 0) { throw "makensis failed" }

$dir = Join-Path $env:LOCALAPPDATA 'Programs\RoomHome-test'
$exe = Join-Path $dir 'RoomHome.exe'
$run = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$unin = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\RoomHome'
$startLnk = Join-Path ([Environment]::GetFolderPath('Programs')) 'RoomHome.lnk'
$deskLnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'RoomHome.lnk'
$appKey = 'HKCU:\Software\RoomHome'
# the test build writes HKCU\Software\RoomHome (language) and its uninstaller deletes it: keep a copy of
# whatever is there now so the PC's own RoomHome settings come back afterwards
$savedKey = if (Test-Path $appKey) { Get-ItemProperty $appKey } else { $null }
function AppLang { (Get-ItemProperty $appKey -ErrorAction SilentlyContinue).Language }
$fails = 0
function Check($name, $ok) { if ($ok) { "PASS $name" } else { "FAIL $name"; $script:fails++ } }
function Machine($path) {
    $b = [IO.File]::ReadAllBytes($path); $pe = [BitConverter]::ToInt32($b, 0x3C)
    switch ([BitConverter]::ToUInt16($b, $pe + 4)) { 0x8664 { 'x64' } 0x14C { 'x86' } 0xAA64 { 'arm64' } default { 'unknown' } }
}
$native = if ($env:PROCESSOR_IDENTIFIER -match 'ARMv8|ARM64') { 'arm64' } elseif ([Environment]::Is64BitOperatingSystem) { 'x64' } else { 'x86' }

# 1. default install: native build, autostart on, no desktop shortcut
$p = Start-Process $setup -ArgumentList '/S' -Wait -PassThru
Check "install exit 0" ($p.ExitCode -eq 0)
Check "exe installed" (Test-Path $exe)
Check "native build ($native)" ((Machine $exe) -eq $native)
Check "README + sample config" ((Test-Path "$dir\README.txt") -and (Test-Path "$dir\RoomHome.defaults.sample.json"))
Check "Thai README" (Test-Path "$dir\README.th.txt")
$uiLang = if ((Get-UICulture).Name -like 'th*') { 'th' } else { 'en' }
Check "silent install language = Windows language ($uiLang)" ((AppLang) -eq $uiLang)
Check "uninstaller written" (Test-Path "$dir\Uninstall.exe")
$u = Get-ItemProperty $unin -ErrorAction SilentlyContinue
Check "Apps & features entry" ($u -and $u.DisplayVersion -eq '1.2.0' -and $u.InstallLocation -eq $dir)
Check "start with Windows on by default" ((Get-ItemProperty $run -Name RoomHome -ErrorAction SilentlyContinue).RoomHome -like "*RoomHome-test\RoomHome.exe*--autostart")
Check "start menu shortcut" (Test-Path $startLnk)
Check "no desktop shortcut by default" (-not (Test-Path $deskLnk))
Check "app not launched by /S" (-not (Get-Process RoomHome -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }))

# 2. reinstall: force x64, no autostart, desktop shortcut
$p = Start-Process $setup -ArgumentList '/S', '/ARCH=x64', '/NOAUTOSTART', '/DESKTOP' -Wait -PassThru
Check "reinstall exit 0" ($p.ExitCode -eq 0)
Check "/ARCH=x64 honoured" ((Machine $exe) -eq 'x64')
Check "/NOAUTOSTART honoured" (-not (Get-ItemProperty $run -Name RoomHome -ErrorAction SilentlyContinue))
Check "/DESKTOP honoured" (Test-Path $deskLnk)
Check "entry shows x64" ((Get-ItemProperty $unin).DisplayName -eq 'RoomHome (x64)')
$p = Start-Process $setup -ArgumentList '/S', '/ARCH=x86', '/LANG=th' -Wait -PassThru
Check "/ARCH=x86 honoured" ((Machine $exe) -eq 'x86')
Check "/LANG=th honoured" ((AppLang) -eq 'th' -and (Get-ItemProperty $appKey).InstallerLanguage -eq '1054')
$p = Start-Process $setup -ArgumentList '/S', '/ARCH=x86', '/LANG=en' -Wait -PassThru
Check "/LANG=en honoured" ((AppLang) -eq 'en' -and (Get-ItemProperty $appKey).InstallerLanguage -eq '1033')

# 3. uninstall (synchronous with _?=)
$p = Start-Process "$dir\Uninstall.exe" -ArgumentList '/S', "_?=$dir" -Wait -PassThru
Check "uninstall exit 0" ($p.ExitCode -eq 0)
Check "exe removed" (-not (Test-Path $exe))
Check "entry removed" (-not (Test-Path $unin))
Check "autostart removed" (-not (Get-ItemProperty $run -Name RoomHome -ErrorAction SilentlyContinue))
Check "shortcuts removed" (-not (Test-Path $startLnk) -and -not (Test-Path $deskLnk))
Check "language key removed" (-not (Test-Path $appKey))
if ($savedKey) {
    New-Item $appKey -Force | Out-Null
    foreach ($pr in $savedKey.PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' }) {
        Set-ItemProperty $appKey -Name $pr.Name -Value $pr.Value
    }
}
Remove-Item -Recurse -Force $dir -ErrorAction SilentlyContinue   # _?= leaves Uninstall.exe behind
"failures: $fails"
exit $fails
