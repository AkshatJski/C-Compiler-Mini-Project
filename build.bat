@echo off
rem ============================================================
rem  build.bat - builds mycc.exe on Windows with MinGW-w64 gcc
rem
rem  Uses gcc from your PATH. If gcc is not on PATH, the script
rem  auto-detects a MSYS2 UCRT64 or w64devkit installation.
rem  Or set CC to the full path of your gcc, e.g.:
rem      set CC=C:\mingw64\bin\gcc.exe
rem ============================================================
setlocal
if "%CC%"=="" (
    where gcc >nul 2>&1
    if errorlevel 1 (
        if exist C:\msys64\ucrt64\bin\gcc.exe set "PATH=C:\msys64\ucrt64\bin;%PATH%"
        if exist C:\w64devkit\bin\gcc.exe    set "PATH=C:\w64devkit\bin;%PATH%"
    )
    set CC=gcc
)

rem A glob, not a hand-written file list: a list is exactly what went stale
rem when the toy front end (src\myc.c) and the optimizer pass (src\opt.c)
rem were added, and the failure was a linker error rather than a missing
rem compile. Let gcc name the files (BUG-028).
%CC% -std=c99 -O2 -Wall -Wextra -Wpedantic -Iinclude -o mycc.exe src\*.c
if errorlevel 1 goto :fail

echo.
echo Build OK: mycc.exe
exit /b 0

:fail
echo.
echo Build FAILED
exit /b 1
