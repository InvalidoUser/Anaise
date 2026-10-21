# Headless test suite for Anaise.
#
#   powershell -ExecutionPolicy Bypass -File tools/test.ps1
#   powershell -ExecutionPolicy Bypass -File tools/test.ps1 -NoBuild
#
# Builds the app, renders every screen offscreen, and checks the captures. No
# window is ever created, so the desktop is left alone while this runs.
param(
    [switch]$NoBuild,
    [string]$Size = '1080x740'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = Join-Path $root 'bin\Anaise.exe'
$workRoot = Join-Path $root 'build\test'

# Reads a PNG without needing any third-party image tooling.
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

public struct ShotProbe {
    public int Width;
    public int Height;
    public long Black;
    public long White;
    public long Grey;
    public long Colour;
}

public static class AnaiseShot {
    public static ShotProbe Inspect(string path) {
        using (var bitmap = new Bitmap(path)) {
            var bounds = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            var data = bitmap.LockBits(bounds, ImageLockMode.ReadOnly,
                                       PixelFormat.Format24bppRgb);
            try {
                var bytes = new byte[data.Stride * data.Height];
                Marshal.Copy(data.Scan0, bytes, 0, bytes.Length);
                var probe = new ShotProbe();
                probe.Width = bitmap.Width;
                probe.Height = bitmap.Height;
                for (int y = 0; y < data.Height; y++) {
                    int row = y * data.Stride;
                    for (int x = 0; x < bitmap.Width; x++) {
                        int i = row + x * 3;
                        byte blue = bytes[i];
                        byte green = bytes[i + 1];
                        byte red = bytes[i + 2];
                        if (red != green || green != blue) probe.Colour++;
                        else if (red == 255) probe.White++;
                        else if (red == 0) probe.Black++;
                        else probe.Grey++;
                    }
                }
                return probe;
            } finally {
                bitmap.UnlockBits(data);
            }
        }
    }

    public static string Sha256(string path) {
        using (var sha = SHA256.Create())
        using (var stream = File.OpenRead(path)) {
            return BitConverter.ToString(sha.ComputeHash(stream))
                .Replace("-", "").ToLowerInvariant();
        }
    }
}
"@

$script:checks = 0
$script:failures = New-Object System.Collections.Generic.List[string]

function Check {
    param([string]$Name, [bool]$Condition, [string]$Detail = '')
    $script:checks++
    $suffix = if ($Detail) { "  $Detail" } else { '' }
    if ($Condition) {
        Write-Host ("  ok    {0}{1}" -f $Name, $suffix)
    } else {
        $script:failures.Add($Name)
        Write-Host ("  FAIL  {0}{1}" -f $Name, $suffix) -ForegroundColor Red
    }
}

function Invoke-Render {
    param([string[]]$Arguments)
    $process = Start-Process -FilePath $exe -ArgumentList $Arguments `
        -WorkingDirectory $root -Wait -PassThru
    return $process.ExitCode
}

function Render-Screens {
    param([string]$Target, [string]$Screens, [int]$TimeMs)
    return Invoke-Render @('--headless', '--screen', $Screens, '--size', $Size,
                           '--time', $TimeMs, '--out', "`"$Target`"")
}

$expectedWidth, $expectedHeight = $Size -split 'x' | ForEach-Object { [int]$_ }
$screenNames = @('splash', 'menu', 'library', 'paint', 'browser')

if (-not $NoBuild) {
    & cmd /c (Join-Path $root 'build.bat')
    Check 'build.bat' ($LASTEXITCODE -eq 0) "exit $LASTEXITCODE"
    if ($LASTEXITCODE -ne 0) { exit 1 }
}
Check 'bin\Anaise.exe exists' (Test-Path $exe)

if (Test-Path $workRoot) { Remove-Item $workRoot -Recurse -Force }
New-Item -ItemType Directory -Path $workRoot | Out-Null

# Every screen renders, fits the requested size, and stays in the palette.
$first = 'build\test\first'
$exitCode = Render-Screens -Target $first -Screens 'all' -TimeMs 4200
Check 'headless render exits 0' ($exitCode -eq 0) "exit $exitCode"

foreach ($name in $screenNames) {
    $path = Join-Path $root "$first\$name.png"
    if (-not (Test-Path $path)) {
        Check "$name.png written" $false
        continue
    }
    $probe = [AnaiseShot]::Inspect($path)
    Check "$name.png written" $true
    Check "$name.png size" ($probe.Width -eq $expectedWidth -and
                            $probe.Height -eq $expectedHeight) `
        "$($probe.Width)x$($probe.Height)"
    Check "$name.png is greyscale" ($probe.Colour -eq 0) `
        "$($probe.Colour) colour pixels"
    Check "$name.png has content" ($probe.White + $probe.Grey -gt 1000) `
        "$($probe.White) white, $($probe.Grey) grey"
}

# A fixed --time means the same frame every run.
$second = 'build\test\second'
$exitCode = Render-Screens -Target $second -Screens 'all' -TimeMs 4200
Check 'repeat render exits 0' ($exitCode -eq 0) "exit $exitCode"
foreach ($name in $screenNames) {
    $a = Join-Path $root "$first\$name.png"
    $b = Join-Path $root "$second\$name.png"
    if ((Test-Path $a) -and (Test-Path $b)) {
        Check "$name.png is deterministic" `
            ([AnaiseShot]::Sha256($a) -eq [AnaiseShot]::Sha256($b))
    }
}

# The seraphim starts blank and is fully drawn and titled five seconds in.
$early = 'build\test\splash-early'
$late = 'build\test\splash-late'
[void](Render-Screens -Target $early -Screens 'splash' -TimeMs 0)
[void](Render-Screens -Target $late -Screens 'splash' -TimeMs 5000)
$earlyProbe = [AnaiseShot]::Inspect((Join-Path $root "$early\splash.png"))
$lateProbe = [AnaiseShot]::Inspect((Join-Path $root "$late\splash.png"))
Check 'splash starts empty' ($earlyProbe.White + $earlyProbe.Grey -eq 0)
Check 'splash reveals over time' `
    ($lateProbe.White + $lateProbe.Grey -gt $earlyProbe.White + $earlyProbe.Grey) `
    "$($earlyProbe.White) -> $($lateProbe.White) white"

# A mid-step frame carries the directional smear and halo, and stays in the
# palette. The frozen step is set with --walk/--progress.
$moving = 'build\test\library-moving'
$exitCode = Invoke-Render @('--headless', '--screen', 'library', '--size', $Size,
                            '--time', 4200, '--walk', 'right', '--progress', '50',
                            '--out', "`"$moving`"")
Check 'mid-step render exits 0' ($exitCode -eq 0) "exit $exitCode"
$movingPath = Join-Path $root "$moving\library.png"
if (Test-Path $movingPath) {
    $movingProbe = [AnaiseShot]::Inspect($movingPath)
    $standingProbe = [AnaiseShot]::Inspect((Join-Path $root "$first\library.png"))
    Check 'library-moving.png is greyscale' ($movingProbe.Colour -eq 0) `
        "$($movingProbe.Colour) colour pixels"
    Check 'mid-step smear and glow add light' `
        ($movingProbe.White -gt $standingProbe.White) `
        "$($standingProbe.White) -> $($movingProbe.White) white"
} else {
    Check 'mid-step.png written' $false
}

# Bad options are rejected instead of silently launching the app.
$exitCode = Invoke-Render @('--headless', '--screen', 'nonsense')
Check 'invalid arguments rejected' ($exitCode -ne 0) "exit $exitCode"
$exitCode = Invoke-Render @('--headless', '--walk', 'sideways')
Check 'invalid walk direction rejected' ($exitCode -ne 0) "exit $exitCode"

# Veil pages render on their own, and a page outside 1-15 is rejected.
$page = 'build\test\browser-page'
$exitCode = Invoke-Render @('--headless', '--screen', 'browser', '--size', $Size,
                            '--time', 4200, '--page', '3',
                            '--out', "`"$page`"")
Check 'browser page render exits 0' ($exitCode -eq 0) "exit $exitCode"
$pagePath = Join-Path $root "$page\browser.png"
if (Test-Path $pagePath) {
    $pageProbe = [AnaiseShot]::Inspect($pagePath)
    Check 'browser page is greyscale' ($pageProbe.Colour -eq 0) `
        "$($pageProbe.Colour) colour pixels"
    Check 'browser page has content' ($pageProbe.White + $pageProbe.Grey -gt 1000) `
        "$($pageProbe.White) white, $($pageProbe.Grey) grey"
} else {
    Check 'browser page written' $false
}
$exitCode = Invoke-Render @('--headless', '--screen', 'browser', '--page', '16')
Check 'invalid browser page rejected' ($exitCode -ne 0) "exit $exitCode"

# The three tracks are embedded in the executable and must parse as mono
# 22.05 kHz PCM even though no audio device is touched in a headless run.
$exitCode = Invoke-Render @('--headless', '--check-audio')
Check 'embedded music parses' ($exitCode -eq 0) "exit $exitCode"

Write-Host ''
if ($script:failures.Count -eq 0) {
    Write-Host "All $($script:checks) headless checks passed."
    exit 0
}
Write-Host ("{0} of {1} headless checks failed:" -f `
    $script:failures.Count, $script:checks) -ForegroundColor Red
$script:failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
