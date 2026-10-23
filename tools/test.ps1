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
        -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru
    return $process.ExitCode
}

function Render-Screens {
    param([string]$Target, [string]$Screens, [int]$TimeMs)
    return Invoke-Render @('--headless', '--screen', $Screens, '--size', $Size,
                           '--time', $TimeMs, '--out', "`"$Target`"")
}

$expectedWidth, $expectedHeight = $Size -split 'x' | ForEach-Object { [int]$_ }
$screenNames = @('splash', 'menu', 'library', 'paint', 'browser', 'snake', 'tentacle', 'gods-ritual', 'gods-arena', 'gods-tower')

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

$exitCode = Invoke-Render @('--headless', '--screen', 'snake', '--snake-view', '1', '--time', '800', '--out', 'build\test\snake-hit')
Check 'Snake hit reaction renders' ($exitCode -eq 0)
$hitPath = Join-Path $root 'build\test\snake-hit\snake.png'
$hitProbe = [AnaiseShot]::Inspect($hitPath)
Check 'Snake reaction stays binary' ($hitProbe.Grey -eq 0 -and $hitProbe.Colour -eq 0)
Check 'Snake hit differs from ready board' ([AnaiseShot]::Sha256($hitPath) -ne [AnaiseShot]::Sha256((Join-Path $root "$first\snake.png")))
$exitCode = Invoke-Render @('--headless', '--screen', 'snake', '--snake-view', '1', '--time', '100', '--out', 'build\test\snake-roll')
Check 'Snake eye-roll starts rendering' ($exitCode -eq 0)
Check 'Snake eye-roll and drool animate' ([AnaiseShot]::Sha256($hitPath) -ne [AnaiseShot]::Sha256((Join-Path $root 'build\test\snake-roll\snake.png')))
$exitCode = Invoke-Render @('--headless', '--screen', 'snake', '--snake-view', '2')
Check 'invalid Snake preview rejected' ($exitCode -ne 0)

foreach ($age in @(1800, 3200, 4000)) {
    $out = "build\test\snake-cinema-$age"
    $exitCode = Invoke-Render @('--headless', '--screen', 'snake', '--snake-view', '1', '--time', $age, '--out', $out)
    Check "Snake cinematic stage $age renders" ($exitCode -eq 0)
    $path = Join-Path $root "$out\snake.png"
    $probe = [AnaiseShot]::Inspect($path)
    Check "Snake cinematic stage $age stays binary" ($probe.Grey -eq 0 -and $probe.Colour -eq 0 -and $probe.White -gt 1000)
    Check "Snake cinematic stage $age advances" ([AnaiseShot]::Sha256($path) -ne [AnaiseShot]::Sha256($hitPath))
}
$repeatOut = 'build\test\snake-cinema-repeat'
$exitCode = Invoke-Render @('--headless', '--screen', 'snake', '--snake-view', '1', '--time', '800', '--out', $repeatOut)
Check 'Snake cinematic is deterministic' ($exitCode -eq 0 -and [AnaiseShot]::Sha256($hitPath) -eq [AnaiseShot]::Sha256((Join-Path $root "$repeatOut\snake.png")))

$exitCode = Invoke-Render @('--headless', '--check-snake')
Check 'Snake scoring, collisions, cinematic freeze, voice timing, next-person progression and desktop routing' ($exitCode -eq 0)
$exitCode = Invoke-Render @('--headless', '--check-tentacle')
Check 'Tentacle flow field, unit pathing, economy, splitters, puddle merging, 3000-blob swarm, defeat and desktop routing' ($exitCode -eq 0)
$tentacleProbe = [AnaiseShot]::Inspect((Join-Path $root "$first\tentacle.png"))
Check 'Tentacle battle renders in black and white' ($tentacleProbe.Grey -eq 0 -and $tentacleProbe.Colour -eq 0 -and $tentacleProbe.White -gt 1000)
$snakeProbe = [AnaiseShot]::Inspect((Join-Path $root "$first\snake.png"))
Check 'Snake palette is strictly binary' ($snakeProbe.Grey -eq 0 -and $snakeProbe.Colour -eq 0)

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

# Browser behavior is exercised through the same handlers used by live input.
$exitCode = Invoke-Render @('--headless', '--check-browser')
Check 'browser navigation, search, bookmarks, all six CC0 bitmaps and deterministic moving lens work' ($exitCode -eq 0) "exit $exitCode"
foreach ($view in @('archive', 'directory', 'history', 'bookmarks')) {
    $viewOut = "build\test\browser-$view"
    $viewArgs = @('--headless', '--screen', 'browser', '--size', $Size,
                  '--time', '4200', '--out', "`"$viewOut`"")
    if ($view -eq 'archive') { $viewArgs += @('--browser-scroll', '320') }
    else { $viewArgs += @('--browser-panel', $view) }
    $exitCode = Invoke-Render $viewArgs
    Check "browser $view renders" ($exitCode -eq 0) "exit $exitCode"
    $viewPath = Join-Path $root "$viewOut\browser.png"
    if (Test-Path $viewPath) {
        $viewProbe = [AnaiseShot]::Inspect($viewPath)
        Check "browser $view is greyscale with content" ($viewProbe.Colour -eq 0 -and $viewProbe.White -gt 1000)
    } else { Check "browser $view written" $false }
}
$exitCode = Invoke-Render @('--headless', '--browser-scroll', '321')
Check 'invalid browser scroll rejected' ($exitCode -ne 0) "exit $exitCode"
$exitCode = Invoke-Render @('--headless', '--browser-panel', 'nonsense')
Check 'invalid browser panel rejected' ($exitCode -ne 0) "exit $exitCode"

# The three tracks are embedded in the executable and must parse as mono
# 22.05 kHz PCM even though no audio device is touched in a headless run.
$exitCode = Invoke-Render @('--headless', '--check-audio')
Check 'embedded music parses' ($exitCode -eq 0) "exit $exitCode"

$exitCode = Invoke-Render @('--headless', '--check-gods')
Check 'New Gods rolling trigger, combat, collision, pause, retries, companion oaths, seals, floors and both endings' ($exitCode -eq 0) "exit $exitCode"
foreach ($name in @('gods-ritual', 'gods-arena', 'gods-tower')) {
    $probe = [AnaiseShot]::Inspect((Join-Path $root "$first\$name.png"))
    Check "$name is pure black and white" ($probe.Grey -eq 0 -and $probe.Colour -eq 0)
}
# Every important state uses the same renderer as the live game.
foreach ($view in 1..10) {
    $target = "build\test\gods-$view"
    $exitCode = Invoke-Render @('--headless', '--screen', 'gods-arena', '--gods-view', $view,
        '--time', '1200', '--out', "`"$target`"")
    Check "New Gods scene $view renders" ($exitCode -eq 0)
    $path = Join-Path $root "$target\gods-arena.png"
    if (Test-Path $path) {
        $probe = [AnaiseShot]::Inspect($path)
        Check "New Gods scene $view has binary content" ($probe.White -gt 1000 -and $probe.Grey -eq 0 -and $probe.Colour -eq 0)
    } else { Check "New Gods scene $view written" $false }
}
# Caption entrance, oversized impact and settled hold must be different frames.
$captionHashes = @()
foreach ($time in @(3500,3600,3700,4200)) {
    $target="build\test\gods-caption-$time"
    $exitCode=Invoke-Render @('--headless','--screen','gods-ritual','--time',$time,'--out',"`"$target`"")
    Check "dramatic reveal at $time ms renders" ($exitCode -eq 0)
    $captionHashes += [AnaiseShot]::Sha256((Join-Path $root "$target\gods-ritual.png"))
}
Check 'reveal changes through caption impact and settle' (@($captionHashes | Select-Object -Unique).Count -eq 4)
$exitCode=Invoke-Render @('--headless','--gods-view','11')
Check 'invalid New Gods scene rejected' ($exitCode -ne 0)

# In-process, offscreen input regression: no native window is created.
$exitCode = Invoke-Render @('--headless', '--check-game')
Check '2D puzzles lead to school; all girls reachable; movement, dialogue, hearts, rejection, dates and restart work' ($exitCode -eq 0) "exit $exitCode"
foreach ($view in 0..4) {
    $schoolOut = "build\test\school-$view"
    $schoolRepeat = "build\test\school-$view-repeat"
    foreach ($target in @($schoolOut, $schoolRepeat)) {
        $exitCode = Invoke-Render @('--headless', '--screen', 'corridor', '--school-view', $view,
            '--size', $Size, '--time', '4200', '--out', "`"$target`"")
        Check "school view $view renders to $target" ($exitCode -eq 0)
    }
    $schoolPath = Join-Path $root "$schoolOut\corridor.png"
    if (Test-Path $schoolPath) {
        $probe = [AnaiseShot]::Inspect($schoolPath)
        Check "school view $view stays pure black and white" ($probe.Grey -eq 0 -and $probe.Colour -eq 0)
        Check "school view $view has content" ($probe.White -gt 1000)
        $repeatPath = Join-Path $root "$schoolRepeat\corridor.png"
        Check "school view $view is deterministic" ((Test-Path $repeatPath) -and
            [AnaiseShot]::Sha256($schoolPath) -eq [AnaiseShot]::Sha256($repeatPath))
    } else { Check "school view $view written" $false }
}
$exitCode = Invoke-Render @('--headless', '--school-view', '5')
Check 'invalid school view rejected' ($exitCode -ne 0)

# All four identities and expressions must make it through the actual renderer.
# --check-game additionally checks all 24 atlas cells, reactions and horror AI.
$portraitHashes = @()
$expressions = @('happy', 'shy', 'hurt', 'neutral')
foreach ($girl in 1..4) {
    $expression = $expressions[$girl - 1]
    $portraitOut = "build\test\portrait-$girl-$expression"
    $exitCode = Invoke-Render @('--headless', '--screen', 'corridor', '--school-view', '1',
        '--school-girl', $girl, '--school-expression', $expression,
        '--size', $Size, '--time', '4200', '--out', "`"$portraitOut`"")
    Check "girl $girl $expression portrait renders" ($exitCode -eq 0)
    $portraitPath = Join-Path $root "$portraitOut\corridor.png"
    if (Test-Path $portraitPath) {
        $probe = [AnaiseShot]::Inspect($portraitPath)
        Check "girl $girl portrait is binary with content" ($probe.Grey -eq 0 -and $probe.Colour -eq 0 -and $probe.White -gt 1000)
        $portraitHashes += [AnaiseShot]::Sha256($portraitPath)
    } else { Check "girl $girl portrait written" $false }
}
Check 'all four rendered portraits differ' (@($portraitHashes | Select-Object -Unique).Count -eq 4)
if ($portraitHashes.Count -eq 4) {
    $neutralPath = Join-Path $root 'build\test\school-1\corridor.png'
    Check 'happy expression changes Hana render' ($portraitHashes[0] -ne [AnaiseShot]::Sha256($neutralPath))
}
$exitCode = Invoke-Render @('--headless', '--school-girl', '5')
Check 'invalid school girl rejected' ($exitCode -ne 0)
$exitCode = Invoke-Render @('--headless', '--school-expression', 'nonsense')
Check 'invalid school expression rejected' ($exitCode -ne 0)

# The additional horror expressions render without literal emotion labels.
foreach ($expression in @('afraid', 'dead')) {
    $target = "build\test\school-expression-$expression"
    $exitCode = Invoke-Render @('--headless', '--screen', 'corridor', '--school-view', '1',
        '--school-girl', '4', '--school-expression', $expression, '--size', $Size,
        '--time', '4200', '--out', "`"$target`"")
    Check "$expression fictional face renders" ($exitCode -eq 0)
    $path = Join-Path $root "$target\corridor.png"
    if (Test-Path $path) {
        $probe = [AnaiseShot]::Inspect($path)
        Check "$expression face stays binary" ($probe.Grey -eq 0 -and $probe.Colour -eq 0)
    } else { Check "$expression face written" $false }
}

$exitCode = Invoke-Render @('--headless', '--check-windows')
Check 'desktop icons move without launching; app windows and content input work' ($exitCode -eq 0) "exit $exitCode"

Write-Host ''
if ($script:failures.Count -eq 0) {
    Write-Host "All $($script:checks) headless checks passed."
    exit 0
}
Write-Host ("{0} of {1} headless checks failed:" -f `
    $script:failures.Count, $script:checks) -ForegroundColor Red
$script:failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
