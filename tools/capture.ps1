# Development helper: save a PNG of a running window without focusing it.
#   powershell -ExecutionPolicy Bypass -File tools/capture.ps1 -Title "Anaise" -Out shot.png
#
# Prefer tools/headless.ps1 for app screens: it renders offscreen and needs no
# running window at all. This one exists for capturing the live app as it is.
param(
    [string]$Title = "Anaise",
    [string]$Out = "shot.png"
)

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class Win32Shot {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
}
"@

# Match the target app's per-monitor DPI awareness so sizes line up.
[void][Win32Shot]::SetProcessDpiAwarenessContext([IntPtr](-4))

$process = Get-Process | Where-Object { $_.MainWindowTitle -like "*$Title*" } | Select-Object -First 1
if (-not $process) {
    Write-Error "No window matching '$Title'"
    exit 1
}

$rect = New-Object Win32Shot+RECT
[void][Win32Shot]::GetClientRect($process.MainWindowHandle, [ref]$rect)
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    Write-Error "Window has no size"
    exit 1
}

$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$hdc = $graphics.GetHdc()
# PW_CLIENTONLY | PW_RENDERFULLCONTENT; fall back for older systems.
$captured = [Win32Shot]::PrintWindow($process.MainWindowHandle, $hdc, 3)
if (-not $captured) {
    $captured = [Win32Shot]::PrintWindow($process.MainWindowHandle, $hdc, 1)
}
$graphics.ReleaseHdc($hdc)
$graphics.Dispose()
if (-not $captured) {
    $bitmap.Dispose()
    Write-Error "PrintWindow failed for '$Title'"
    exit 1
}

$bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
Write-Host "Saved $Out ($width x $height)"
