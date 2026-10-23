# Development helper: render Anaise screens offscreen and save PNGs.
#
#   powershell -ExecutionPolicy Bypass -File tools/headless.ps1
#   powershell -ExecutionPolicy Bypass -File tools/headless.ps1 -Screen menu -Size 900x620
#
# No window is created, so this never flashes on the desktop or steals focus.
param(
    [ValidateSet('splash', 'menu', 'library', 'paint', 'browser', 'crash', 'search', 'corridor', 'gods-ritual', 'gods-arena', 'gods-tower', 'snake', 'tentacle', 'all')]
    [string]$Screen = 'all',
    [string]$Size = '1080x740',
    [int]$TimeMs = 4200,
    [ValidateRange(0, 10)]
    [int]$GodsView = 0,
    [ValidateRange(0, 1)]
    [int]$SnakeView = 0,
    [int]$Dpi = 96,
    [ValidateRange(0, 4)]
    [int]$SchoolView = 0,
    [ValidateRange(1, 4)]
    [int]$SchoolGirl = 1,
    [ValidateSet('neutral', 'happy', 'shy', 'hurt', 'afraid', 'dead')]
    [string]$SchoolExpression = 'neutral',
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
                    '--time', $TimeMs, '--gods-view', $GodsView, '--snake-view', $SnakeView, '--dpi', $Dpi, '--school-view', $SchoolView,
                    '--school-girl', $SchoolGirl, '--school-expression', $SchoolExpression, '--out', "`"$OutDir`"") `
    -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) {
    throw "headless render failed (exit $($process.ExitCode))"
}

$resolved = Join-Path $root $OutDir
$shots = Get-ChildItem -Path $resolved -Filter '*.png' -ErrorAction SilentlyContinue
if (-not $shots) { throw "no PNGs were written to $resolved" }
$shots | ForEach-Object {
    Write-Host ("{0}  {1} bytes" -f $_.FullName, $_.Length)
}
