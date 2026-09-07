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

%CC% -std=c99 -O2 -Wall -Wextra -Wpedantic -Iinclude -o mycc.exe src\main.c src\lexer.c src\parser.c src\symtab.c src\semantic.c src\codegen.c src\util.c src\dump.c
if errorlevel 1 goto :fail

echo.
echo Build OK: mycc.exe
exit /b 0

:fail
echo.
echo Build FAILED
exit /b 1
