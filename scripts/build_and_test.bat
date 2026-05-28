@echo off
SETLOCAL ENABLEEXTENSIONS DISABLEDELAYEDEXPANSION

:: ============================================================
::   Validate! - Build and Test Runner  (Windows Batch)
::   Usage: scripts\build_and_test.bat [Debug|Release] [QtDir] [-verbose]
::
::   -verbose  Print full test output. Without this flag only
::             per-suite status lines and the final summary are
::             shown.
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

:: Scan all arguments for -verbose flag
SET VERBOSE=0
FOR %%A IN (%*) DO (
    IF /I "%%A"=="-verbose"  SET VERBOSE=1
    IF /I "%%A"=="--verbose" SET VERBOSE=1
)

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
    :: Ninja Multi-Config: supports --config Debug/Release at build time
    :: AND honours CMAKE_EXPORT_COMPILE_COMMANDS (VS generator ignores it).
    IF NOT "%EFFECTIVE_QT_DIR%"=="" (
        IF NOT "%~2"=="" (
            cmake -B "%BUILD_DIR%" -G "Ninja Multi-Config" -DVD_EXTENSION_QT_BASE=ON -DVD_QT_DIR="%EFFECTIVE_QT_DIR%"
        ) ELSE (
            cmake -B "%BUILD_DIR%" -G "Ninja Multi-Config" -DVD_EXTENSION_QT_BASE=ON
        )
    ) ELSE (
        cmake -B "%BUILD_DIR%" -G "Ninja Multi-Config"
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
IF %VERBOSE%==1 (
    echo [3/3] Running test suites... [verbose]
) ELSE (
    echo [3/3] Running test suites...
)
echo.

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

IF NOT EXIST "%EXE%" (
    IF %VERBOSE%==1 (
        echo.
        echo  [Suite %SUITE_COUNT%] %SUITE%
        echo  . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
        echo  [ERROR] Binary not found: %EXE%
    ) ELSE (
        echo   %SUITE%  [MISSING]
    )
    GOTO :EOF
)

IF %VERBOSE%==1 (
    echo.
    echo  [Suite %SUITE_COUNT%] %SUITE%
    echo  . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
    echo.
)

:: Record start time in centiseconds since midnight
CALL :TIME_CS T_START

"%EXE%" --gtest_color=no > "%OUT%" 2>&1
SET EXE_EXIT=%ERRORLEVEL%

:: Record end time and compute elapsed
CALL :TIME_CS T_END
SET /A ELAPSED_CS=T_END - T_START
IF %ELAPSED_CS% LSS 0 SET /A ELAPSED_CS+=8640000
SET /A ELAPSED_S=ELAPSED_CS / 100

IF %VERBOSE%==1 (
    TYPE "%OUT%"
    echo.
)

:: Extract suite-level pass/fail counts
SET "SUITE_P=0"
FOR /F "tokens=4" %%N IN ('FINDSTR /C:"  PASSED  ] " "%OUT%" 2^>NUL') DO SET "SUITE_P=%%N"

SET "SUITE_F=0"
FOR /F "tokens=4" %%N IN ('FINDSTR /C:"  FAILED  ] " "%OUT%" 2^>NUL ^| FINDSTR /C:" listed below"') DO SET "SUITE_F=%%N"

SET /A TOTAL_PASSED+=SUITE_P
SET /A TOTAL_FAILED+=SUITE_F

:: Collect individual failed test names
FINDSTR /C:"  FAILED  ] " "%OUT%" 2>NUL ^
    | FINDSTR /V /C:" listed below" ^
    | FINDSTR /V /C:" ms)" ^
    >> "%FAILURES_FILE%" 2>NUL

:: Print result line
IF %EXE_EXIT%==0 (
    IF %VERBOSE%==1 (
        echo   [PASS]  %ELAPSED_S%s  (%SUITE_P% passed^)
        echo.
    ) ELSE (
        echo   %SUITE%  [PASS]  %ELAPSED_S%s  (%SUITE_P% passed^)
    )
) ELSE (
    IF %VERBOSE%==1 (
        echo   [FAIL]  %ELAPSED_S%s  (%SUITE_P% passed, %SUITE_F% failed^)
        echo.
    ) ELSE (
        echo   %SUITE%  [FAIL]  %ELAPSED_S%s  (%SUITE_P% passed, %SUITE_F% failed^)
    )
)

IF EXIST "%OUT%" DEL /Q "%OUT%"
GOTO :EOF

:: ============================================================
:: Parse %time% into centiseconds since midnight.
:: Usage: CALL :TIME_CS <varname>
:TIME_CS
SET "_T=%time: =0%"
SET "_HH=%_T:~0,2%"
SET "_MM=%_T:~3,2%"
SET "_SS=%_T:~6,2%"
SET "_CC=%_T:~9,2%"
SET /A "%1=(%_HH%*360000)+(%_MM%*6000)+(%_SS%*100)+%_CC%"
GOTO :EOF
