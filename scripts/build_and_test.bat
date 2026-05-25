@echo off
SETLOCAL ENABLEEXTENSIONS DISABLEDELAYEDEXPANSION

:: ============================================================
::   Validate! - Build and Test Runner  (Windows Batch)
::   Usage: scripts\build_and_test.bat [Debug|Release] [QtDir]
::
::   QtDir  Optional path to Qt installation directory.
::          Overrides the QTDIR environment variable.
::          When QtDir or QTDIR is set, VD_EXTENSION_QT_BASE
::          is enabled and Qt DLLs are added to PATH.
::          Example: scripts\build_and_test.bat Debug C:\Qt\6.9.2\msvc2022_64
:: ============================================================

SET "BUILD_CONFIG=%~1"
IF "%BUILD_CONFIG%"=="" SET "BUILD_CONFIG=Debug"

:: Resolve Qt directory: explicit arg > QTDIR env var
SET "EFFECTIVE_QT_DIR=%~2"
IF "%EFFECTIVE_QT_DIR%"=="" IF NOT "%QTDIR%"=="" SET "EFFECTIVE_QT_DIR=%QTDIR%"

SET "SCRIPT_DIR=%~dp0"
PUSHD "%SCRIPT_DIR%.."
IF ERRORLEVEL 1 (
    echo [ERROR] Cannot navigate to project root.
    EXIT /B 1
)

SET "BUILD_DIR=build"
SET "BIN_DIR=%BUILD_DIR%\tests\%BUILD_CONFIG%"
SET "FAILURES_FILE=%TEMP%\vd_test_failures.txt"
SET "SUITE_COUNT=0"

IF EXIST "%FAILURES_FILE%" DEL /Q "%FAILURES_FILE%"

echo.
echo  ============================================================
echo    Validate! - Build ^& Test Runner
echo    Config: %BUILD_CONFIG%
IF NOT "%EFFECTIVE_QT_DIR%"=="" echo    Qt:     %EFFECTIVE_QT_DIR%
echo  ============================================================
echo.

:: ── Step 1: CMake configure ──────────────────────────────────
IF NOT EXIST "%BUILD_DIR%\CMakeCache.txt" (
    echo [1/3] Running CMake configure...
    IF NOT "%EFFECTIVE_QT_DIR%"=="" (
        IF NOT "%~2"=="" (
            cmake -B "%BUILD_DIR%" -DVD_EXTENSION_QT_BASE=ON -DVD_QT_DIR="%EFFECTIVE_QT_DIR%"
        ) ELSE (
            cmake -B "%BUILD_DIR%" -DVD_EXTENSION_QT_BASE=ON
        )
    ) ELSE (
        cmake -B "%BUILD_DIR%"
    )
    IF ERRORLEVEL 1 (
        echo.
        echo [ERROR] CMake configure failed.
        POPD & EXIT /B 1
    )
) ELSE (
    echo [1/3] Build directory found - skipping configure.
    IF NOT "%EFFECTIVE_QT_DIR%"=="" echo       Hint: delete '%BUILD_DIR%' and re-run to apply Qt flags.
    IF "%EFFECTIVE_QT_DIR%"=="" echo       Delete '%BUILD_DIR%' to force reconfigure.
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

:: ── Qt DLL path ───────────────────────────────────────────────
:: Priority: arg/QTDIR > VD_QT_DIR in CMakeCache.txt
SET "QT_BIN_DIR="
IF NOT "%EFFECTIVE_QT_DIR%"=="" (
    SET "QT_BIN_DIR=%EFFECTIVE_QT_DIR%\bin"
) ELSE IF EXIST "%BUILD_DIR%\CMakeCache.txt" (
    FOR /F "tokens=2 delims==" %%V IN (
        'FINDSTR /B "VD_QT_DIR:PATH=" "%BUILD_DIR%\CMakeCache.txt" 2^>NUL'
    ) DO (
        IF NOT "%%V"=="" SET "QT_BIN_DIR=%%V\bin"
    )
)

IF NOT "%QT_BIN_DIR%"=="" IF EXIST "%QT_BIN_DIR%" (
    echo   Qt DLLs: %QT_BIN_DIR%
    SET "PATH=%QT_BIN_DIR%;%PATH%"
    echo.
)

:: ── Step 3: Run tests ─────────────────────────────────────────
echo [3/3] Running test suites...
echo.
echo  ------------------------------------------------------------
echo    Test Suites
echo  ------------------------------------------------------------

IF NOT EXIST "%BIN_DIR%" (
    echo  [WARN] Binary directory not found: %BIN_DIR%
    echo         Ensure the build succeeded for config '%BUILD_CONFIG%'.
    POPD & EXIT /B 1
)

SET TOTAL_PASSED=0
SET TOTAL_FAILED=0

:: Discover all test_*.exe automatically — no hardcoded list.
FOR %%F IN ("%BIN_DIR%\test_*.exe") DO CALL :RUN_SUITE "%%~nF"

:: ── Final summary ─────────────────────────────────────────────
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
