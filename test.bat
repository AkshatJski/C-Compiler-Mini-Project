@echo off
rem ============================================================
rem  test.bat - runs the mycc test suite on Windows
rem
rem  Positive tests (tests\*.c):
rem     1. compile with mycc.exe             (mycc.exe --target=windows)
rem     2. assemble+link with MinGW gcc      (build\<name>.exe)
rem     3. run and compare the exit code with the EXPECT: marker
rem
rem  Error tests (tests\errors\*.c):
rem     mycc must exit 1 and print the diagnostic named by EXPECT-ERROR:
rem
rem  Requirements: mycc.exe (run build.bat first) and MinGW gcc.
rem  Uses gcc from your PATH; auto-detects MSYS2 UCRT64 / w64devkit.
rem  Or set CC to the full path of your gcc.
rem ============================================================
setlocal enabledelayedexpansion
if "%CC%"=="" (
    where gcc >nul 2>&1
    if errorlevel 1 (
        if exist C:\msys64\ucrt64\bin\gcc.exe set "PATH=C:\msys64\ucrt64\bin;%PATH%"
        if exist C:\w64devkit\bin\gcc.exe    set "PATH=C:\w64devkit\bin;%PATH%"
    )
    set CC=gcc
)

if not exist mycc.exe (
    echo mycc.exe not found. Run build.bat first.
    exit /b 1
)

if not exist build mkdir build
set FAILED=0

for %%t in (tests\*.c) do call :positive "%%t"
for %%t in (tests\errors\*.c) do call :error "%%t"

if "%FAILED%"=="1" (
    echo.
    echo Some tests FAILED
    exit /b 1
)
echo.
echo All tests passed
exit /b 0

rem ------------------------------------------------------------
:positive
set "T=%~1"
set "EXPECTED="
for /f "tokens=1,* delims=:" %%a in ('findstr /c:"EXPECT:" "%T%"') do (
    if not defined EXPECTED set "EXPECTED=%%b"
    goto :positive_expect_done
)
:positive_expect_done
set "EXPECTED=%EXPECTED: =%"
if "%EXPECTED%"=="" (
    echo SKIP: %T% ^(no EXPECT marker^)
    goto :eof
)

mycc.exe --target=windows "%T%" -o "build\%~n1.s"
if errorlevel 1 (
    echo FAIL: compile %T%
    set FAILED=1
    goto :eof
)

%CC% -o "build\%~n1.exe" "build\%~n1.s"
if errorlevel 1 (
    echo FAIL: assemble %T%
    set FAILED=1
    goto :eof
)

"build\%~n1.exe"
set "ACTUAL=!errorlevel!"
if not "!ACTUAL!"=="%EXPECTED%" (
    echo FAIL: %T% expected exit %EXPECTED%, got !ACTUAL!
    set FAILED=1
) else (
    echo PASS: %T% ^(exit !ACTUAL!^)
)
goto :eof

rem ------------------------------------------------------------
:error
set "T=%~1"
set "EXPECTED="
for /f "tokens=1,* delims=:" %%a in ('findstr /c:"EXPECT-ERROR:" "%T%"') do (
    if not defined EXPECTED set "EXPECTED=%%b"
    goto :error_expect_done
)
:error_expect_done
:error_trim
if "!EXPECTED:~0,1!"==" " (
    set "EXPECTED=!EXPECTED:~1!"
    goto :error_trim
)

mycc.exe "%T%" -o "build\err_tmp.s" > "build\err_out.txt" 2>&1
set "CODE=!errorlevel!"
if not "!CODE!"=="1" (
    echo FAIL: %T% should be rejected ^(exit 1^), got !CODE!
    set FAILED=1
    goto :eof
)
if defined EXPECTED (
    findstr /c:"!EXPECTED!" "build\err_out.txt" >nul 2>&1
    if errorlevel 1 (
        echo FAIL: %T% missing diagnostic "!EXPECTED!"
        set FAILED=1
        goto :eof
    )
)
echo PASS: %T% ^(rejected^)
goto :eof
