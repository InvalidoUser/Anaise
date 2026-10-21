# Development helper: render Anaise screens offscreen and save PNGs.
#
#   powershell -ExecutionPolicy Bypass -File tools/headless.ps1
#   powershell -ExecutionPolicy Bypass -File tools/headless.ps1 -Screen menu -Size 900x620
#
# No window is created, so this never flashes on the desktop or steals focus.
param(
    [ValidateSet('splash', 'menu', 'library', 'paint', 'all')]
    [string]$Screen = 'all',
    [string]$Size = '1080x740',
    [int]$TimeMs = 4200,
    [int]$Dpi = 96,
    [string]$OutDir = 'build\headless',
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = Join-Path $root 'bin\Anaise.exe'

if ($Build -or -not (Test-Path $exe)) {
    & cmd /c (Join-Path $root 'build.bat')
    if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
}

$process = Start-Process -FilePath $exe `
    -ArgumentList @('--headless', '--screen', $Screen, '--size', $Size,
                    '--time', $TimeMs, '--dpi', $Dpi, '--out', "`"$OutDir`"") `
    -WorkingDirectory $root -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "headless render failed (exit $($process.ExitCode))"
}

$resolved = Join-Path $root $OutDir
$shots = Get-ChildItem -Path $resolved -Filter '*.png' -ErrorAction SilentlyContinue
if (-not $shots) { throw "no PNGs were written to $resolved" }
$shots | ForEach-Object {
    Write-Host ("{0}  {1} bytes" -f $_.FullName, $_.Length)
}
