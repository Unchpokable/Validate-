#Requires -Version 5.1
<#
.SYNOPSIS
    Build and run all tests for the Validate! project.
.PARAMETER Config
    Build configuration: Debug (default) or Release.
.PARAMETER Reconfigure
    Force CMake reconfigure even if a build directory already exists.
.EXAMPLE
    .\scripts\build_and_test.ps1
    .\scripts\build_and_test.ps1 -Config Release
    .\scripts\build_and_test.ps1 -Reconfigure
#>
param(
    [string]$Config = "Debug",
    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"

# ── Paths ──────────────────────────────────────────────────────────────────────
$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $projectRoot

$buildDir  = "build"
$binDir    = Join-Path $buildDir "tests\$Config"
$suites    = @("test_assert", "test_models", "test_string_rules")

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
Write-Banner "Validate! - Build & Test Runner   [Config: $Config]"
Write-Host ""

# ── Step 1: CMake configure ────────────────────────────────────────────────────
$cacheFile = Join-Path $buildDir "CMakeCache.txt"
if ($Reconfigure -or -not (Test-Path $cacheFile)) {
    Write-Step "1/3" "Running CMake configure..."
    cmake -B $buildDir
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "[ERROR] CMake configure failed." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Step "1/3" "Build directory found - skipping configure.  (Use -Reconfigure to force)"
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

# ── Step 3: Run tests ──────────────────────────────────────────────────────────
Write-Step "3/3" "Running test suites..."
Write-Host ""

$results = [System.Collections.Generic.List[hashtable]]::new()

foreach ($suite in $suites) {
    $exe = Join-Path $binDir "$suite.exe"

    # Suite header
    $pad = "-" * [Math]::Max(2, 52 - $suite.Length)
    Write-Host "  " -NoNewline
    Write-Host "-- Suite: $suite $pad" -ForegroundColor Cyan
    Write-Host ""

    if (-not (Test-Path $exe)) {
        Write-Host "  [ERROR] Binary not found: $exe" -ForegroundColor Red
        Write-Host "          Check that config '$Config' was built successfully." -ForegroundColor DarkGray
        $results.Add(@{ Suite = $suite; Passed = 0; Failed = 0; Status = "MISSING"; FailedTests = @() })
        Write-Host ""
        continue
    }

    # Stream output line-by-line, capture for parsing
    $outputLines = [System.Collections.Generic.List[string]]::new()

    & $exe --gtest_color=yes 2>&1 | ForEach-Object {
        $text = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.ToString() } else { [string]$_ }
        Write-Host "  $text"
        $outputLines.Add($text)
    }
    $exeExit = $LASTEXITCODE

    Write-Host ""

    # Parse GTest summary
    $passed     = 0
    $failed     = 0
    $failedTests = [System.Collections.Generic.List[string]]::new()

    foreach ($line in $outputLines) {
        # "[  PASSED  ] N tests."
        if ($line -match '\[\s+PASSED\s+\]\s+(\d+) tests?') {
            $passed = [int]$Matches[1]
        }
        # "[  FAILED  ] N tests, listed below:"
        if ($line -match '\[\s+FAILED\s+\]\s+(\d+) tests?,') {
            $failed = [int]$Matches[1]
        }
        # "[  FAILED  ] Suite.TestName"  — summary section (no timing "(N ms)")
        if ($line -match '\[\s+FAILED\s+\]\s+([\w./\\-]+\.[\w./\\-]+)\s*$') {
            $failedTests.Add($Matches[1])
        }
    }

    # Fall back to exit code if GTest output was not parseable (e.g. crash)
    $status = if ($failed -eq 0 -and $exeExit -eq 0) { "PASS" } else { "FAIL" }

    $results.Add(@{
        Suite       = $suite
        Passed      = $passed
        Failed      = $failed
        Status      = $status
        FailedTests = $failedTests.ToArray()
    })
}

# ── Final summary ──────────────────────────────────────────────────────────────
Write-Banner "FINAL SUMMARY"
Write-Host ""

$totalPassed   = 0
$totalFailed   = 0
$allFailedTests = [System.Collections.Generic.List[string]]::new()

$hdr = "  {0,-26} {1,8}   {2,8}   {3}"
Write-Host ($hdr -f "Suite", "Passed", "Failed", "Status") -ForegroundColor White
Write-Host ("  " + ("-" * 56))

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
    Write-Host ($hdr -f $r.Suite, $r.Passed, $r.Failed, $r.Status) -ForegroundColor $color
}

Write-Host ("  " + ("-" * 56))
$totalColor = if ($totalFailed -eq 0) { "Green" } else { "Red" }
Write-Host ($hdr -f "TOTAL", $totalPassed, $totalFailed, "") -ForegroundColor $totalColor

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
