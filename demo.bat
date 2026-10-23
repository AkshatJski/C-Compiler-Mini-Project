@echo off
rem ============================================================
rem  demo.bat - the five-minute demo for a NON-TECHNICAL audience
rem
rem  One command, no flags, nothing to explain in advance.
rem  Every scene answers one question an outsider actually asks:
rem
rem    1. what does it do?              -> it prints a sentence
rem    2. can I see it work?            -> you type your name
rem    3. is it doing anything?         -> same program, half the code
rem    4. did you write it?             -> the binary says so
rem    5. can I trust it?               -> 33 programs, gcc, a bug ledger
rem
rem  Requirements: mycc.exe (run build.bat first) and MinGW gcc.
rem  Or set CC to the full path of your gcc.
rem
rem  Presenter script: DEMO.md.  Fallback transcript: demo\expected.txt.
rem ============================================================
setlocal enabledelayedexpansion
title mycc - a small compiler that actually compiles
mode con cols=84 lines=30 >nul 2>&1
color 0A

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

rem ------------------------------------------------------------
rem Scene 1
rem ------------------------------------------------------------
cls
echo.
echo   ==========================================================
echo    1.  This compiler used to be silent.
echo   ==========================================================
echo.
echo   Until recently a mycc program had no way to say anything:
echo   its entire output was an exit code, a number nobody sees.
echo.
echo   Then I gave it a calling convention and a C runtime:
echo.
mycc.exe hello.c -o build\demo_hello.s >nul 2>&1
%CC% -o build\demo_hello.exe build\demo_hello.s >nul 2>&1
echo      ----------------------------------------------
build\demo_hello.exe
echo      ----------------------------------------------
echo.
echo   That text was written by printf, inside the C library.
echo   My compiler had to obey the same rules Microsoft's
echo   compiler obeys for that to work.
echo.
pause

rem ------------------------------------------------------------
rem Scene 2
rem ------------------------------------------------------------
cls
echo.
echo   ==========================================================
echo    2.  Your turn. Type your name.
echo   ==========================================================
echo.
mycc.exe demo\2_name.c -o build\demo_name.s >nul 2>&1
%CC% -o build\demo_name.exe build\demo_name.s >nul 2>&1
build\demo_name.exe
echo.
echo   A local array, a library call, three string literals and
echo   a real ABI. Nothing in that program knows it was a demo.
echo.
pause

rem ------------------------------------------------------------
rem Scene 3
rem ------------------------------------------------------------
cls
echo.
echo   ==========================================================
echo    3.  Is it doing anything, or just copying the source?
echo   ==========================================================
echo.
echo   One program, compiled twice. Left: with the optimizer.
echo   Right: with it switched off. I have not run either yet.
echo.
mycc.exe demo\3_optimizer.c -o build\demo_opt.s >nul 2>&1
mycc.exe --no-opt demo\3_optimizer.c -o build\demo_plain.s >nul 2>&1
for /f %%L in ('type build\demo_opt.s ^| find /c /v ""') do set /a OPT=%%L
for /f %%L in ('type build\demo_plain.s ^| find /c /v ""') do set /a PLAIN=%%L
echo      ----------------------------------------------
echo       with optimizer :  !OPT! lines of instructions
echo       without        :  !PLAIN! lines of instructions
echo      ----------------------------------------------
echo.
%CC% -o build\demo_opt.exe build\demo_opt.s >nul 2>&1
%CC% -o build\demo_plain.exe build\demo_plain.s >nul 2>&1
build\demo_opt.exe
set /a R1=!errorlevel!
build\demo_plain.exe
set /a R2=!errorlevel!
echo.
echo   Both returned !R1! and !R2!. The optimizer removed work,
echo   not behaviour -- and the test suite checks exactly that on
echo   every program by running both paths.
echo.
pause

rem ------------------------------------------------------------
rem Scene 4
rem ------------------------------------------------------------
cls
echo.
echo   ==========================================================
echo    4.  "This is just gcc in a wrapper, right?"
echo   ==========================================================
echo.
echo   A fair question. So I asked the program itself.
echo   First, what the compiler says it is:
echo.
mycc.exe --version
echo.
echo   Now: I asked it to stamp its name into the program's data.
echo   This searches the finished .exe for that stamp.
echo.
mycc.exe --sign hello.c -o build\demo_sign.s >nul 2>&1
%CC% -o build\demo_sign.exe build\demo_sign.s >nul 2>&1
where strings >nul 2>&1
if errorlevel 1 (
    findstr /c:"generated by mycc" build\demo_sign.exe
) else (
    strings build\demo_sign.exe 2>nul | find "generated by mycc"
)
echo.
echo   That string is inside the executable. gcc assembled it,
echo   but mycc wrote it. My compiler is a few hundred kilobytes
echo   and six and a half thousand lines; gcc is millions. It
echo   cannot secretly be gcc.
echo.
pause

rem ------------------------------------------------------------
rem Scene 5
rem ------------------------------------------------------------
cls
echo.
echo   ==========================================================
echo    5.  Can you trust it?
echo   ==========================================================
echo.
echo   Not because I say so. Because it is checked two ways:
echo.
echo   - 33 programs it must accept, 41 mistakes it must reject
echo   - and every answer compared against the world's most
echo     widely used compiler, on the same source file
echo.
call .\test.bat >build\demo_test.log 2>&1
set /a TR=!errorlevel!
call .\difftest.bat >build\demo_diff.log 2>&1
set /a DR=!errorlevel!
if !TR!==0 (echo       tests        : all passed) else (echo       tests        : FAILURES - see build\demo_test.log)
if !DR!==0 (echo       versus gcc   : all agreed) else (echo       versus gcc   : DISAGREEMENT - see build\demo_diff.log)
echo.
echo   And when it got something wrong, it is written down, with
echo   the commit it was found in: report.md, 23 entries.
echo.
echo   Thank you.
echo.
endlocal
exit /b 0
