#Requires -Version 5.1
<#
.SYNOPSIS
    Build and run all tests for the Validate! project.
.PARAMETER Config
    Build configuration: Debug (default) or Release.
.PARAMETER Reconfigure
    Force CMake reconfigure even if a build directory already exists.
.PARAMETER QtDir
    Path to the Qt installation directory (e.g. C:\Qt\6.9.2\msvc2022_64).
    Overrides the QTDIR environment variable.  When either QtDir or QTDIR is
    set, the script enables VD_EXTENSION_QT_BASE and adds Qt DLLs to PATH.
    If neither is set, Qt extension tests are skipped (not built).
.PARAMETER Verbose
    Print full test output. Without this flag only per-suite status lines
    and the final summary are shown.
.EXAMPLE
    .\scripts\build_and_test.ps1
    .\scripts\build_and_test.ps1 -Config Release
    .\scripts\build_and_test.ps1 -Reconfigure
    .\scripts\build_and_test.ps1 -QtDir "C:\Qt\6.9.2\msvc2022_64"
    .\scripts\build_and_test.ps1 -Verbose
#>
param(
    [string]$Config = "Debug",
    [switch]$Reconfigure,
    [string]$QtDir = "",
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# ── Paths ──────────────────────────────────────────────────────────────────────
$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $projectRoot

$buildDir = "build"
$binDir   = Join-Path $buildDir "tests\$Config"

# ── Resolve Qt directory ───────────────────────────────────────────────────────
# Priority: -QtDir argument > $env:QTDIR > (not set)
$effectiveQtDir = if ($QtDir) { $QtDir } elseif ($env:QTDIR) { $env:QTDIR } else { "" }

# ── Helpers ────────────────────────────────────────────────────────────────────
function Write-Banner([string]$text) {
    Write-Host ""
    Write-Host "  ============================================================" -ForegroundColor Cyan
    Write-Host "    $text" -ForegroundColor Cyan
    Write-Host "  ============================================================" -ForegroundColor Cyan
}

function Write-Step([string]$label, [string]$text) {
    Write-Host "[$label] " -ForegroundColor Yellow -NoNewline
    Write-Host $text
}

# ── Header ─────────────────────────────────────────────────────────────────────
$qtLabel = if ($effectiveQtDir) { "  Qt: $effectiveQtDir" } else { "" }
Write-Banner "Validate! - Build & Test Runner   [Config: $Config]$qtLabel"
Write-Host ""

# ── Step 1: CMake configure ────────────────────────────────────────────────────
$cacheFile = Join-Path $buildDir "CMakeCache.txt"
if ($Reconfigure -or -not (Test-Path $cacheFile)) {
    Write-Step "1/3" "Running CMake configure..."

    # Wipe the build tree to avoid generator-mismatch errors (FetchContent
    # subbuilds also cache the generator and must be regenerated from scratch).
    if (Test-Path $buildDir) {
        Write-Host "        Cleaning build directory..." -ForegroundColor DarkGray
        Remove-Item $buildDir -Recurse -Force
    }

    # Ninja Multi-Config: supports --config Debug/Release like the VS generator
    # AND honours CMAKE_EXPORT_COMPILE_COMMANDS (VS generator ignores it).
    $cmakeArgs = @("-B", $buildDir, "-G", "Ninja Multi-Config")
    if ($effectiveQtDir) {
        $cmakeArgs += "-DVD_EXTENSION_QT_BASE=ON"
        # Pass VD_QT_DIR only when -QtDir was given explicitly;
        # otherwise cmake will pick up QTDIR from the environment on its own.
        if ($QtDir) {
            $cmakeArgs += "-DVD_QT_DIR=$QtDir"
        }
        Write-Host "        Qt Base extension: ON" -ForegroundColor DarkGray
    }

    cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "[ERROR] CMake configure failed." -ForegroundColor Red
        exit 1
    }
} else {
    $hint = if ($Reconfigure.IsPresent -eq $false) { "Use -Reconfigure to force" } else { "" }
    Write-Step "1/3" "Build directory found - skipping configure.  ($hint)"
}
Write-Host ""

# ── Step 2: Build ──────────────────────────────────────────────────────────────
Write-Step "2/3" "Building all targets (config: $Config)..."
cmake --build $buildDir --config $Config
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "[ERROR] Build failed." -ForegroundColor Red
    exit 1
}
Write-Host ""

# ── Resolve Qt DLL path for the test runner ───────────────────────────────────
# Priority: -QtDir / QTDIR > VD_QT_DIR in CMakeCache.txt
$qtBinDir = ""
if ($effectiveQtDir) {
    $qtBinDir = Join-Path $effectiveQtDir "bin"
} elseif (Test-Path $cacheFile) {
    $match = Select-String -Path $cacheFile -Pattern "^VD_QT_DIR:PATH=(.+)" | Select-Object -First 1
    if ($match -and $match.Matches[0].Groups[1].Value.Trim()) {
        $qtBinDir = Join-Path $match.Matches[0].Groups[1].Value.Trim() "bin"
    }
}

if ($qtBinDir -and (Test-Path $qtBinDir)) {
    Write-Host "  Qt DLLs: $qtBinDir" -ForegroundColor DarkGray
    $env:PATH = "$qtBinDir;$env:PATH"
    Write-Host ""
}

# ── Discover test suites ───────────────────────────────────────────────────────
# All test_*.exe in the output directory are run automatically — no hardcoded list.
$suites = @()
if (Test-Path $binDir) {
    $suites = Get-ChildItem -Path $binDir -Filter "test_*.exe" `
              | Select-Object -ExpandProperty BaseName `
              | Sort-Object
}

if ($suites.Count -eq 0) {
    Write-Host "[WARN] No test binaries found in: $binDir" -ForegroundColor Yellow
    Write-Host "       Ensure the build succeeded for config '$Config'."
    exit 1
}

# ── Step 3: Run tests ──────────────────────────────────────────────────────────
$verboseLabel = if ($Verbose) { " [verbose]" } else { "" }
Write-Step "3/3" "Running test suites ($($suites.Count) found)...$verboseLabel"
Write-Host ""

$results = [System.Collections.Generic.List[hashtable]]::new()

foreach ($suite in $suites) {
    $exe = Join-Path $binDir "$suite.exe"

    if ($Verbose) {
        $pad = "-" * [Math]::Max(2, 52 - $suite.Length)
        Write-Host "  " -NoNewline
        Write-Host "-- Suite: $suite $pad" -ForegroundColor Cyan
        Write-Host ""
    }

    if (-not (Test-Path $exe)) {
        if ($Verbose) {
            Write-Host "  [ERROR] Binary not found: $exe" -ForegroundColor Red
            Write-Host "          Check that config '$Config' was built successfully." -ForegroundColor DarkGray
            Write-Host ""
        } else {
            Write-Host ("  {0,-44}  " -f $suite) -NoNewline
            Write-Host "[MISSING]" -ForegroundColor Yellow
        }
        $results.Add(@{ Suite = $suite; Passed = 0; Failed = 0; Status = "MISSING"; Elapsed = 0; FailedTests = @() })
        continue
    }

    $outputLines = [System.Collections.Generic.List[string]]::new()
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    if ($Verbose) {
        & $exe --gtest_color=yes 2>&1 | ForEach-Object {
            $text = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.ToString() } else { [string]$_ }
            Write-Host "  $text"
            $outputLines.Add($text)
        }
        $exeExit = $LASTEXITCODE
        Write-Host ""
    } else {
        Write-Host ("  {0,-44}  " -f $suite) -NoNewline
        $rawOutput = & $exe --gtest_color=no 2>&1
        $exeExit = $LASTEXITCODE
        foreach ($line in $rawOutput) {
            $text = if ($line -is [System.Management.Automation.ErrorRecord]) { $line.ToString() } else { [string]$line }
            $outputLines.Add($text)
        }
    }

    $sw.Stop()
    $elapsed = [Math]::Round($sw.Elapsed.TotalSeconds, 2)

    $passed      = 0
    $failed      = 0
    $failedTests = [System.Collections.Generic.List[string]]::new()

    foreach ($line in $outputLines) {
        if ($line -match '\[\s+PASSED\s+\]\s+(\d+) tests?') {
            $passed = [int]$Matches[1]
        }
        if ($line -match '\[\s+FAILED\s+\]\s+(\d+) tests?,') {
            $failed = [int]$Matches[1]
        }
        if ($line -match '\[\s+FAILED\s+\]\s+([\w./\\-]+\.[\w./\\-]+)\s*$') {
            $failedTests.Add($Matches[1])
        }
    }

    $status = if ($failed -eq 0 -and $exeExit -eq 0) { "PASS" } else { "FAIL" }
    $statusColor = if ($status -eq "PASS") { "Green" } else { "Red" }

    if ($Verbose) {
        $detail = if ($status -eq "PASS") { "($passed passed)" } else { "($passed passed, $failed failed)" }
        Write-Host "  " -NoNewline
        Write-Host "[$status]" -ForegroundColor $statusColor -NoNewline
        Write-Host "  ${elapsed}s  $detail"
        Write-Host ""
    } else {
        $detail = if ($status -eq "PASS") { "($passed passed)" } else { "($passed passed, $failed failed)" }
        Write-Host "[$status]" -ForegroundColor $statusColor -NoNewline
        Write-Host "  ${elapsed}s  $detail"
    }

    $results.Add(@{
        Suite       = $suite
        Passed      = $passed
        Failed      = $failed
        Status      = $status
        Elapsed     = $elapsed
        FailedTests = $failedTests.ToArray()
    })
}

# ── Final summary ──────────────────────────────────────────────────────────────
Write-Banner "FINAL SUMMARY"
Write-Host ""

$totalPassed    = 0
$totalFailed    = 0
$allFailedTests = [System.Collections.Generic.List[string]]::new()

$hdr = "  {0,-26} {1,8}   {2,8}   {3,8}   {4}"
Write-Host ($hdr -f "Suite", "Passed", "Failed", "Time", "Status") -ForegroundColor White
Write-Host ("  " + ("-" * 64))

foreach ($r in $results) {
    $totalPassed += $r.Passed
    $totalFailed += $r.Failed
    foreach ($t in $r.FailedTests) {
        $allFailedTests.Add("$($r.Suite) :: $t")
    }

    $color = switch ($r.Status) {
        "PASS"    { "Green"  }
        "MISSING" { "Yellow" }
        default   { "Red"    }
    }
    Write-Host ($hdr -f $r.Suite, $r.Passed, $r.Failed, "$($r.Elapsed)s", $r.Status) -ForegroundColor $color
}

Write-Host ("  " + ("-" * 64))
$totalColor = if ($totalFailed -eq 0) { "Green" } else { "Red" }
Write-Host ($hdr -f "TOTAL", $totalPassed, $totalFailed, "", "") -ForegroundColor $totalColor

if ($allFailedTests.Count -gt 0) {
    Write-Host ""
    Write-Host "  Failed tests:" -ForegroundColor Red
    foreach ($t in $allFailedTests) {
        Write-Host "    [FAIL] $t" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "  ============================================================" -ForegroundColor Cyan
Write-Host ""

if ($totalFailed -gt 0) { exit 1 }
exit 0
