@echo off
rem ============================================================
rem  fuzz.bat - random-program differential testing (Windows)
rem
rem  tools\fuzz.c writes N programs nobody wrote by hand; each
rem  one is compiled twice (mycc, then gcc directly) and both
rem  executables must return the same exit code. This is the
rem  same check difftest.bat runs on tests\diff - the only
rem  difference is where the programs come from.
rem
rem  Usage:  fuzz.bat            (1000 programs, seed 1)
rem          fuzz.bat 200        (200 programs, seed 1)
rem          fuzz.bat 200 7      (200 programs, seed 7)
rem
rem  Deterministic in the seed: a failing case can be regenerated
rem  exactly, by number, on any machine.
rem ============================================================
setlocal enabledelayedexpansion

if "%CC%"=="" (
    where gcc >nul 2>&1
    if errorlevel 1 (
        if exist C:\msys64\ucrt64\bin\gcc.exe set "PATH=C:\msys64\ucrt64\bin;%PATH%"
        if exist C:\w64devkit\bin\gcc.exe    set "PATH=C:\w64devkit\bin;%PATH%"
    )
    set "CC=gcc"
)

if not exist mycc.exe (
    echo mycc.exe not found. Run build.bat first.
    exit /b 1
)

set "N=%~1"
if "%N%"=="" set "N=1000"
set "SEED=%~2"
if "%SEED%"=="" set "SEED=1"

if not exist build mkdir build
%CC% -std=c99 -O2 -Wall -Wextra -Wpedantic -o "build\fuzzgen.exe" "tools\fuzz.c"
if errorlevel 1 (
    echo.
    echo Building the generator FAILED
    exit /b 1
)

if exist "build\fuzz" rmdir /s /q "build\fuzz"
"build\fuzzgen.exe" %N% "build\fuzz" %SEED%
if errorlevel 1 exit /b 1

rem QUIET keeps the transcript to failures plus one summary line: with a
rem thousand cases, one line per case is noise, not evidence.
set "SRCDIR=build\fuzz"
set "QUIET=1"
call difftest.bat
exit /b %errorlevel%
