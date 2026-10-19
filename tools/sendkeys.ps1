# Development helper: activate a window and type a sequence of commands.
#   powershell -ExecutionPolicy Bypass -File tools/sendkeys.ps1 -Keys "take lantern|north"
param(
    [string]$Title = "The Hollow Spire",
    [string]$Keys = "",
    [int]$DelayMs = 240
)

$shell = New-Object -ComObject WScript.Shell
if (-not $shell.AppActivate($Title)) {
    Write-Error "No window matching '$Title'"
    exit 1
}
Start-Sleep -Milliseconds 350

if ($Keys -ne "") {
    foreach ($line in ($Keys -split '\|')) {
        if ($line -eq '') { continue }
        $shell.SendKeys($line)
        Start-Sleep -Milliseconds 60
        $shell.SendKeys('{ENTER}')
        Start-Sleep -Milliseconds $DelayMs
    }
}
Write-Host "Sent: $Keys"
