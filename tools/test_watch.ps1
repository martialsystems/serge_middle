# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
#
# Rebuild the headless test tree and run the fast test tier whenever a
# DSP/test source file is saved (Windows PowerShell 5.1 or pwsh 7).
# Headless: no iPlug2 plugin, GUI, or audio device.
#
# Usage (from the repo root or anywhere):
#   powershell -ExecutionPolicy Bypass -File tools\test_watch.ps1            # watch, run -L fast after each save
#   powershell -ExecutionPolicy Bypass -File tools\test_watch.ps1 -Once      # one build + test cycle, then exit
#   ... -Label render | -Filter "py_transfer|test_patch" | -Preset windows-clangcl | -DebounceMs 500
# Uses watchexec when it is on PATH, otherwise a built-in FileSystemWatcher loop.
param(
    [string]$Preset = "windows-dev",
    [string]$Label = "fast",
    [string]$Filter = "",
    [int]$DebounceMs = 300,
    [int]$Jobs = 2,
    [switch]$Once
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Build = Join-Path (Join-Path $Root "build") $Preset

function Enter-VsEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
    if ($Preset -like "*clangcl*" -and (Get-Command clang-cl.exe -ErrorAction SilentlyContinue)) { return }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { throw "Visual Studio Build Tools not found. Install: winget install Microsoft.VisualStudio.2022.BuildTools (select 'Desktop development with C++')." }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio is installed but the 'MSVC v143 C++ x64/x86 build tools' component is missing." }
    $dev = Join-Path $vs "Common7\Tools\Launch-VsDevShell.ps1"
    & $dev -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
    Set-Location $Root
}

function Invoke-Cycle {
    Enter-VsEnvironment
    if (-not (Test-Path (Join-Path $Build "build.ninja"))) {
        & cmake --preset $Preset | Out-Host
        if ($LASTEXITCODE -ne 0) { Write-Host "CONFIGURE FAILED" -ForegroundColor Red; return 1 }
    }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $targets = @("test_wave_middle", "test_patch", "test_ends", "test_second", "test_cable_ends", "test_cable_rope")
    if ($Label -eq "render" -or $Label -eq "slow") { $targets += "playground" }
    # TODO(SLEWFOLD FX): append the FX test executable when that target exists.
    & cmake --build $Build --parallel $Jobs --target @targets | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED" -ForegroundColor Red; return 1 }
    $tBuild = $sw.Elapsed.TotalSeconds; $sw.Restart()
    $ctestArgs = @("--test-dir", $Build, "-L", $Label, "-j", "$Jobs", "--output-on-failure")
    if ($Filter) { $ctestArgs += @("-R", $Filter) }
    & ctest @ctestArgs | Out-Host
    $rc = [int]$LASTEXITCODE
    $colour = "Green"; if ($rc -ne 0) { $colour = "Red" }
    Write-Host ("== build {0:N1} s, tests {1:N1} s, ctest exit {2} ({3})" -f $tBuild, $sw.Elapsed.TotalSeconds, $rc, (Get-Date -Format HH:mm:ss)) -ForegroundColor $colour
    return $rc
}

if ($Once) { exit (Invoke-Cycle) }

$null = Invoke-Cycle

$wx = Get-Command watchexec -ErrorAction SilentlyContinue
if ($wx) {
    Write-Host "watching sources with watchexec (Ctrl-C to stop)"
    $psExe = (Get-Process -Id $PID).Path
    $args2 = @("--watch", "SergeMiddle", "--watch", "SergeMiddleCombined", "--watch", "SlopeAC",
               "--watch", "slope", "--watch", "tests", "--watch", "tools/playground", "--watch", "cmake",
               "--watch", "wave_middle.py", "--watch", "CMakeLists.txt", "--watch", "CMakePresets.json",
               "--exts", "h,hpp,cpp,cmake,txt,json,py,csv,md", "--debounce", "${DebounceMs}ms", "--clear", "-n", "-p", "--",
               $psExe, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath, "-Once", "-Preset", $Preset, "-Label", $Label, "-Jobs", "$Jobs")
    if ($Filter) { $args2 += @("-Filter", $Filter) }
    & watchexec @args2
    exit $LASTEXITCODE
}

Write-Host "watchexec not found, using FileSystemWatcher (Ctrl-C to stop). Tip: winget install watchexec"
$pattern = '(\.(h|hpp|cpp|cmake|py|csv|md|json)$)|(CMakeLists\.txt$)|(CMakePresets\.json$)|(wave_middle\.py$)'
$watchers = @()
foreach ($d in @("SergeMiddle", "SergeMiddleCombined", "SlopeAC", "slope", "tests", "tools/playground", "cmake")) {
    $dir = Join-Path $Root $d
    if (-not (Test-Path $dir)) { continue }
    $w = New-Object IO.FileSystemWatcher $dir
    $w.IncludeSubdirectories = $true
    $w.NotifyFilter = [IO.NotifyFilters]'LastWrite, FileName, Size'
    $w.EnableRaisingEvents = $true
    foreach ($ev in @("Changed", "Created", "Renamed")) { $null = Register-ObjectEvent $w $ev -SourceIdentifier ("slewfoldwatch_" + $d.Replace("\", "_") + "_" + $ev) }
    $watchers += $w
}
$wRoot = New-Object IO.FileSystemWatcher $Root
$wRoot.Filter = "CMake*"
$wRoot.NotifyFilter = [IO.NotifyFilters]'LastWrite, FileName, Size'
$wRoot.EnableRaisingEvents = $true
foreach ($ev in @("Changed", "Created", "Renamed")) { $null = Register-ObjectEvent $wRoot $ev -SourceIdentifier ("slewfoldwatch_root_" + $ev) }
$watchers += $wRoot
try {
    $last = $null
    while ($true) {
        $e = Wait-Event -Timeout 0.1
        if ($e) {
            Remove-Event -EventIdentifier $e.EventIdentifier
            if ($e.SourceEventArgs.FullPath -match $pattern) { $last = Get-Date }
        } elseif ($last -and ((Get-Date) - $last).TotalMilliseconds -ge $DebounceMs) {
            $last = $null
            Get-Event | Remove-Event
            Clear-Host
            $null = Invoke-Cycle
        }
    }
} finally {
    Get-EventSubscriber | Unregister-Event
    $watchers | ForEach-Object { $_.Dispose() }
}
