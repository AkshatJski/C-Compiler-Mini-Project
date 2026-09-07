@echo off
rem ============================================================
rem  test.bat - runs the mycc test suite on Windows
rem
rem  For each tests\*.c it:
rem     1. compiles it with mycc.exe          (mycc.exe --target=windows)
rem     2. assembles+links it with MinGW gcc  (build\<name>.exe)
rem     3. runs it and compares the exit code with the EXPECT: marker
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

for %%t in (tests\*.c) do (
    set EXPECTED=
    for /f "tokens=3" %%e in ('findstr /c:"EXPECT:" "%%t"') do set EXPECTED=%%e
    if "!EXPECTED!"=="" (
        echo SKIP: %%t ^(no EXPECT marker^)
        goto :next
    )

    mycc.exe --target=windows "%%t" -o "build\%%~nt.s"
    if errorlevel 1 (
        echo FAIL: compile %%t
        set FAILED=1
        goto :next
    )

    %CC% -o "build\%%~nt.exe" "build\%%~nt.s"
    if errorlevel 1 (
        echo FAIL: assemble %%t
        set FAILED=1
        goto :next
    )

    "build\%%~nt.exe"
    set ACTUAL=!errorlevel!
    if not "!ACTUAL!"=="!EXPECTED!" (
        echo FAIL: %%t expected exit !EXPECTED!, got !ACTUAL!
        set FAILED=1
    ) else (
        echo PASS: %%t ^(exit !ACTUAL!^)
    )

:next
    set EXPECTED=
)

if "%FAILED%"=="1" (
    echo.
    echo Some tests FAILED
    exit /b 1
)
echo.
echo All tests passed
exit /b 0
