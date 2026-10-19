# Development helper: activate a window by title and save a PNG of it.
#   powershell -ExecutionPolicy Bypass -File tools/capture.ps1 -Title "The Hollow Spire" -Out shot.png
param(
    [string]$Title = "The Hollow Spire",
    [string]$Out = "shot.png",
    [int]$WaitMs = 400
)

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class Win32Shot {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
}
"@

$process = Get-Process | Where-Object { $_.MainWindowTitle -like "*$Title*" } | Select-Object -First 1
if (-not $process) {
    Write-Error "No window matching '$Title'"
    exit 1
}

[void][Win32Shot]::ShowWindow($process.MainWindowHandle, 9)   # SW_RESTORE
[void][Win32Shot]::SetForegroundWindow($process.MainWindowHandle)
Start-Sleep -Milliseconds $WaitMs

$rect = New-Object Win32Shot+RECT
[void][Win32Shot]::GetClientRect($process.MainWindowHandle, [ref]$rect)
$origin = New-Object Win32Shot+POINT
[void][Win32Shot]::ClientToScreen($process.MainWindowHandle, [ref]$origin)
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    Write-Error "Window has no size"
    exit 1
}

$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
$bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
Write-Host "Saved $Out ($width x $height)"
