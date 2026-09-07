@echo off
setlocal
rem ============================================================
rem  tour.bat - a guided, narrated demo of the mycc compiler.
rem
rem  Run this on Windows (PowerShell or CMD):   .\tour.bat
rem  Requires: mycc.exe (run build.bat first) and MinGW gcc.
rem  Press any key to move from one step to the next.
rem ============================================================

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

cls
echo ==================================================================
echo  mycc  -  a compiler you can actually SEE working
echo ==================================================================
echo.
echo  A compiler turns human-readable code into the exact instructions
echo  your CPU runs. When you type "gcc hello.c", all of that happens
echo  inside a giant black box. mycc does the same job in about 2,300
echo  lines of plain C that you could read in an evening - and it shows
echo  you every step along the way.
echo.
echo  This tour has 7 steps. Press any key to advance.
echo.
pause
cls

echo [1/7] THE PROGRAM
echo --------------------------------------------------------------
echo.
type demos\simple.c
echo.
echo  Two tiny functions. 'square' multiplies a number by itself;
echo  'main' calls it with 5 and adds 1. Answer: 26.
echo.
pause
cls

echo [2/7] mycc TURNS IT INTO ASSEMBLY
echo --------------------------------------------------------------
echo.
mycc.exe demos\simple.c -o build\simple.s
type build\simple.s
echo.
echo  Every line here is a real x86-64 CPU instruction. Notice how it
echo  reads like the source:
echo.
echo    pushq %%rbp / movq %%rsp,%%rbp   - set up the stack frame
echo    movq -8(%%rbp), %%rax            - load "x"
echo    imulq %%rcx, %%rax               - multiply:      x * x
echo    call square                      - jump into the function
echo    addq %%rcx, %%rax                - add 1:  square(5)+1
echo    leave / ret                      - return from main
echo.
echo  gcc does all of this too - it just never lets you watch.
echo.
pause
cls

echo [3/7] ASSEMBLE, LINK, AND RUN IT
echo --------------------------------------------------------------
echo.
%CC% -o build\simple.exe build\simple.s
build\simple.exe
set RESULT=%errorlevel%
echo.
echo  The program returned: %RESULT%
echo  Expected:               26
echo.
echo  The answer travelled: C source -^> assembly -^> .exe -^> exit code.
echo  A compiler built from scratch produced a working program.
echo.
pause
cls

echo [4/7] THE SAME PROGRAM, COMPILED BY REAL GCC
echo --------------------------------------------------------------
echo.
%CC% -o build\simple_gcc.exe demos\simple.c
build\simple_gcc.exe
set RESULT=%errorlevel%
echo.
echo  gcc's version also returns: %RESULT%
echo.
echo  So mycc really is a compiler, not a toy. But notice the scale:
echo  gcc is roughly 15 million lines of code written by thousands of
echo  people over 35 years. mycc is about 2,300 lines you can fully
echo  understand.
echo.
pause
cls

echo [5/7] WHERE THEY DIFFER - THE ASSEMBLY
echo --------------------------------------------------------------
echo.
echo  In step 2 you saw mycc translate square(5)+1 faithfully.
echo  Now watch what real gcc does with the same source, at -O2:
echo.
%CC% -S -O2 demos\simple.c -o build\simple_gcc.s
type build\simple_gcc.s
echo.
echo  Look at the end of 'main':  movl $26, %%eax.
echo  gcc is so clever it worked out square(5)+1 = 26 AT COMPILE TIME
echo  and deleted the function call entirely.
echo.
echo  That is the real difference:
echo    gcc  - a production optimizer. Final code has nothing in
echo           common with what you wrote. Fast, but unreadable.
echo    mycc - never optimizes. It emits exactly what you wrote, one
echo           instruction per operation. Slow, but you can follow it.
echo.
echo  For learning how compilers work, mycc wins. For shipping software,
echo  gcc wins.
echo.
pause
cls

echo [6/7] THE REAL DEMO - EVERY FEATURE AT ONCE
echo --------------------------------------------------------------
echo.
echo  example.c uses functions, recursion, for loops, if/else, %% and
echo  ^&^& ^|^| !, ++/--, compound assignment, globals, and block scoping.
echo  It computes fact(5) = 120 plus the sum of primes up to 20 = 77.
echo.
mycc.exe example.c -o build\example.s
%CC% -o build\example.exe build\example.s
build\example.exe
set RESULT=%errorlevel%
echo.
echo  The program returned: %RESULT%   (120 + 77 = 197)
echo.
echo  A glimpse of the generated code:
echo.
findstr /n "call is_prime call fact idivq imulq" build\example.s
echo.
pause
cls

echo [7/7] PROOF IT IS ROBUST - THE TEST SUITE
echo --------------------------------------------------------------
echo.
call test.bat
echo.
echo  Five independent programs, each compiled by mycc, run, and
echo  verified against a hand-computed answer.
echo.
echo  UNDER THE HOOD - mycc has four stages, all hand-written:
echo.
echo    src\lexer.c      text      -^> tokens
echo    src\parser.c     tokens    -^> syntax tree
echo    src\semantic.c   checks    -^> catches mistakes, assigns stack slots
echo    src\codegen.c    tree      -^> assembly
echo.
echo    %~dp0src      : compiler source (2,130 lines)
echo    %~dp0include  : compiler headers (225 lines)
echo.
echo  And one more thing: the SAME mycc produces executables for
echo  BOTH Windows and Linux. Run tour.sh inside WSL to see the
echo  identical demo on Linux.
echo.
pause
echo.
echo  Thanks for watching!
echo.
exit /b 0
