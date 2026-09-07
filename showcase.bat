@echo off
rem ============================================================
rem  showcase.bat - end-to-end demo on Windows
rem
rem  Compiles example.c with mycc, assembles it into an .exe with
rem  MinGW gcc, runs it, and shows the exit code.
rem  Expected result: 197  (fact(5)=120 + sum of primes up to 20=77)
rem
rem  Requirements: mycc.exe (run build.bat first) and MinGW gcc.
rem  Uses gcc from your PATH; auto-detects MSYS2 UCRT64 / w64devkit.
rem  Or set CC to the full path of your gcc.
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

if not exist mycc.exe (
    echo mycc.exe not found. Run build.bat first.
    exit /b 1
)

if not exist build mkdir build

echo [1/3] Compiling example.c to assembly...
mycc.exe --target=windows example.c -o build\example.s
if errorlevel 1 exit /b 1

echo [2/3] Assembling and linking...
%CC% -o build\example.exe build\example.s
if errorlevel 1 exit /b 1

echo [3/3] Running...
build\example.exe
set RESULT=%errorlevel%
echo.
echo The program returned: %RESULT%
echo Expected:               197
exit /b 0
