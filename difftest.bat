@echo off
rem difftest.bat - differential testing against the host gcc (Windows).
rem
rem For every program in tests\diff\ this compiles it twice:
rem   * with mycc, then assembles the result with gcc;
rem   * with gcc directly.
rem Both executables must return the same exit code.
rem
rem Why this exists: the exit-code suite cannot tell "correct" from "wrong but
rem plausible". BUG-001 (double evaluation of `a[i++] += 5`), BUG-002, BUG-003
rem (8-byte struct copy) and BUG-020 (double index scaling) all returned a
rem number and all passed 48 green tests. A reference implementation catches
rem that class of bug for free.
rem
rem Usage:  difftest.bat          (expects mycc.exe and tests\diff\*.c)
rem Exit:   0 = all programs agreed, 1 = a mismatch or a build failure.
setlocal enabledelayedexpansion

if "%CC%"=="" (
    if exist C:\msys64\ucrt64\bin\gcc.exe (
        set "CC=C:\msys64\ucrt64\bin\gcc.exe"
    ) else if exist C:\msys64\mingw64\bin\gcc.exe (
        set "CC=C:\msys64\mingw64\bin\gcc.exe"
    ) else (
        set "CC=gcc"
    )
)

if not exist mycc.exe (
    echo mycc.exe not found. Run build.bat first.
    exit /b 1
)

if not exist build mkdir build
set "FAILED=0"
set "COUNT=0"
rem SRCDIR lets the fuzz run reuse this harness: fuzz.bat generates random
rem programs into build\fuzz and points SRCDIR at it.
if "%SRCDIR%"=="" set "SRCDIR=tests\diff"
if not exist "%SRCDIR%\*.c" (
    echo no programs found in %SRCDIR%
    exit /b 1
)

for %%t in (%SRCDIR%\*.c) do call :compare "%%t"

if "%FAILED%"=="1" (
    echo.
    echo Some differential tests FAILED
    exit /b 1
)
echo.
if defined QUIET (
    echo PASS: all %COUNT% programs agreed with the reference compiler
) else (
    echo All differential tests passed
)
exit /b 0

:compare
set "T=%~1"
set "BASE=%~n1"
set "MYCC_EXIT="
set "REF_EXIT="
set /a COUNT+=1

mycc.exe --target=windows "%T%" -o "build\diff_%BASE%.s" > "build\diff_%BASE%.mycc.log" 2>&1
if errorlevel 1 (
    echo FAIL: %T% - mycc rejected it
    type "build\diff_%BASE%.mycc.log"
    set "FAILED=1"
    goto :eof
)

%CC% -o "build\diff_%BASE%.mycc.exe" "build\diff_%BASE%.s" > "build\diff_%BASE%.asm.log" 2>&1
if errorlevel 1 (
    echo FAIL: %T% - assembling mycc's output failed
    type "build\diff_%BASE%.asm.log"
    set "FAILED=1"
    goto :eof
)

%CC% -std=c99 -w -o "build\diff_%BASE%.ref.exe" "%T%" > "build\diff_%BASE%.ref.log" 2>&1
if errorlevel 1 (
    echo FAIL: %T% - the reference compiler rejected it ^(the test itself is wrong^)
    type "build\diff_%BASE%.ref.log"
    set "FAILED=1"
    goto :eof
)

"build\diff_%BASE%.mycc.exe"
set "MYCC_EXIT=!errorlevel!"
"build\diff_%BASE%.ref.exe"
set "REF_EXIT=!errorlevel!"

if not "!MYCC_EXIT!"=="!REF_EXIT!" (
    echo FAIL: %T% - mycc returned !MYCC_EXIT!, the reference compiler returned !REF_EXIT!
    set "FAILED=1"
    goto :eof
)

if not defined QUIET echo PASS: %T% ^(both returned !MYCC_EXIT!^)
goto :eof
