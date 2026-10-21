# Puts a shortcut to the built app on the desktop.
# The shortcut runs bin\Anaise.exe and takes its icon from the executable, so
# nothing has to be installed or registered.
# Run from anywhere:  powershell -ExecutionPolicy Bypass -File tools/make_shortcut.ps1

$root = Split-Path -Parent $PSScriptRoot
$exePath = Join-Path $root 'bin\Anaise.exe'

if (-not (Test-Path $exePath)) {
    Write-Error "$exePath is missing. Run build.bat first."
    exit 1
}

$desktop = [Environment]::GetFolderPath('Desktop')
$linkPath = Join-Path $desktop 'Anaise.lnk'

$shell = New-Object -ComObject WScript.Shell
$link = $shell.CreateShortcut($linkPath)
$link.TargetPath = $exePath
$link.WorkingDirectory = $root
$link.IconLocation = "$exePath,0"
$link.Description = 'Anaise game library'
$link.Save()

Write-Host "Wrote $linkPath"
