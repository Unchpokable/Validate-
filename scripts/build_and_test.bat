@echo off
SETLOCAL ENABLEEXTENSIONS DISABLEDELAYEDEXPANSION

:: ============================================================
::   Validate! - Build and Test Runner  (Windows Batch)
::   Usage: scripts\build_and_test.bat [Debug|Release]
:: ============================================================

SET "BUILD_CONFIG=%~1"
IF "%BUILD_CONFIG%"=="" SET "BUILD_CONFIG=Debug"

SET "SCRIPT_DIR=%~dp0"
PUSHD "%SCRIPT_DIR%.."
IF ERRORLEVEL 1 (
    echo [ERROR] Cannot navigate to project root.
    EXIT /B 1
)

SET "BUILD_DIR=build"
SET "BIN_DIR=%BUILD_DIR%\tests\%BUILD_CONFIG%"
SET "FAILURES_FILE=%TEMP%\vd_test_failures.txt"

IF EXIST "%FAILURES_FILE%" DEL /Q "%FAILURES_FILE%"

echo.
echo  ============================================================
echo    Validate! - Build ^& Test Runner
echo    Config: %BUILD_CONFIG%
echo  ============================================================
echo.

:: ── Step 1: CMake configure ──────────────────────────────────
IF NOT EXIST "%BUILD_DIR%\CMakeCache.txt" (
    echo [1/3] Running CMake configure...
    cmake -B "%BUILD_DIR%"
    IF ERRORLEVEL 1 (
        echo.
        echo [ERROR] CMake configure failed.
        POPD & EXIT /B 1
    )
) ELSE (
    echo [1/3] Build directory found - skipping configure.
    echo       Delete '%BUILD_DIR%' to force reconfigure.
)
echo.

:: ── Step 2: Build ────────────────────────────────────────────
echo [2/3] Building all targets (config: %BUILD_CONFIG%)...
cmake --build "%BUILD_DIR%" --config "%BUILD_CONFIG%"
IF ERRORLEVEL 1 (
    echo.
    echo [ERROR] Build failed.
    POPD & EXIT /B 1
)
echo.

:: ── Step 3: Run tests ────────────────────────────────────────
echo [3/3] Running tests...
echo.
echo  ------------------------------------------------------------
echo    Test Suites
echo  ------------------------------------------------------------

SET TOTAL_PASSED=0
SET TOTAL_FAILED=0
SET SUITE_COUNT=0

FOR %%T IN (test_assert test_models test_string_rules) DO CALL :RUN_SUITE "%%T"

:: ── Final summary ────────────────────────────────────────────
echo.
echo  ============================================================
echo    FINAL SUMMARY
echo  ============================================================
echo    Suites run  : %SUITE_COUNT%
echo    Tests passed: %TOTAL_PASSED%
echo    Tests failed: %TOTAL_FAILED%

IF %TOTAL_FAILED% GTR 0 (
    echo.
    echo    Failed tests:
    IF EXIST "%FAILURES_FILE%" (
        FOR /F "usebackq delims=" %%L IN ("%FAILURES_FILE%") DO echo      %%L
    )
)
echo  ============================================================
echo.

IF EXIST "%FAILURES_FILE%" DEL /Q "%FAILURES_FILE%"
POPD

IF %TOTAL_FAILED% GTR 0 EXIT /B 1
EXIT /B 0

:: ============================================================
:RUN_SUITE
SET "SUITE=%~1"
SET "EXE=%BIN_DIR%\%SUITE%.exe"
SET "OUT=%TEMP%\vd_%SUITE%.txt"
SET /A SUITE_COUNT+=1

echo.
echo  [Suite %SUITE_COUNT%] %SUITE%
echo  . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

IF NOT EXIST "%EXE%" (
    echo  [ERROR] Binary not found: %EXE%
    echo  Hint: check that config '%BUILD_CONFIG%' was built successfully.
    GOTO :EOF
)

"%EXE%" --gtest_color=no > "%OUT%" 2>&1
TYPE "%OUT%"

:: Extract passed count from summary line: "[  PASSED  ] N tests."
:: Token layout after splitting on spaces: [  PASSED  ] 5 tests.
::   1:[    2:PASSED    3:]    4:5    5:tests.
FOR /F "tokens=4" %%N IN ('FINDSTR /C:"  PASSED  ] " "%OUT%" 2^>NUL') DO SET /A TOTAL_PASSED+=%%N

:: Extract failed count from line: "[  FAILED  ] N tests, listed below:"
FOR /F "tokens=4" %%N IN ('FINDSTR /C:"  FAILED  ] " "%OUT%" 2^>NUL ^| FINDSTR /C:" listed below"') DO SET /A TOTAL_FAILED+=%%N

:: Collect individual failed test names (summary lines have no " ms)" timing)
FINDSTR /C:"  FAILED  ] " "%OUT%" 2>NUL ^
    | FINDSTR /V /C:" listed below" ^
    | FINDSTR /V /C:" ms)" ^
    >> "%FAILURES_FILE%" 2>NUL

IF EXIST "%OUT%" DEL /Q "%OUT%"
GOTO :EOF
