@echo off
setlocal enabledelayedexpansion
set "PATH=C:\Windows\System32;C:\Windows;%PATH%"

if "%CC%"=="" (
    if exist C:\msys64\ucrt64\bin\gcc.exe (
        set "CC=C:\msys64\ucrt64\bin\gcc.exe"
    ) else if exist C:\msys64\mingw64\bin\gcc.exe (
        set "CC=C:\msys64\mingw64\bin\gcc.exe"
    ) else if exist C:\w64devkit\bin\gcc.exe (
        set "CC=C:\w64devkit\bin\gcc.exe"
    ) else (
        set "CC=gcc"
    )
)

if not exist mycc.exe (
    echo mycc.exe not found. Run build.bat first.
    exit /b 1
)

if not exist build mkdir build
set FAILED=0

for %%t in (tests\*.c) do call :positive "%%t"
for %%t in (tests\errors\*.c) do call :error "%%t"
call :output_file_contract
call :silence_contract
call :sign_contract

if "%FAILED%"=="1" (
    echo.
    echo Some tests FAILED
    exit /b 1
)

echo.
echo All tests passed
exit /b 0

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

mycc.exe --target=windows "%T%" -o "build\%~n1.s" > "build\%~n1.warn" 2>&1
if errorlevel 1 (
    echo FAIL: compile %T%
    set FAILED=1
    goto :eof
)

rem Optional `// WARN: <text>` marker: the compile must succeed *and* emit that
rem warning text (BUG-016). Warnings never change the exit status.
set "WANTWARN="
for /f "tokens=1,* delims=:" %%a in ('findstr /c:"WARN:" "%T%"') do (
    if not defined WANTWARN set "WANTWARN=%%b"
    goto :positive_warn_trim
)
:positive_warn_trim
if "!WANTWARN:~0,1!"==" " (
    set "WANTWARN=!WANTWARN:~1!"
    goto :positive_warn_trim
)
if defined WANTWARN (
    findstr /c:"!WANTWARN!" "build\%~n1.warn" >nul 2>&1
    if errorlevel 1 (
        echo FAIL: %T% missing warning "!WANTWARN!"
        set FAILED=1
        goto :eof
    )
)

%CC% -o "build\%~n1.exe" "build\%~n1.s"
if errorlevel 1 (
    echo FAIL: assemble %T%
    set FAILED=1
    goto :eof
)

rem Optional `// EXPECT-OUT: <text>` marker: the program must print that text
rem (ROADMAP Phase 4). Matched as a substring so tests stay readable.
set "WANTOUT="
for /f "tokens=1,* delims=:" %%a in ('findstr /c:"EXPECT-OUT:" "%T%"') do (
    if not defined WANTOUT set "WANTOUT=%%b"
    goto :positive_out_trim
)
:positive_out_trim
if "!WANTOUT:~0,1!"==" " (
    set "WANTOUT=!WANTOUT:~1!"
    goto :positive_out_trim
)

"build\%~n1.exe" > "build\%~n1.stdout" 2>&1
set "ACTUAL=!errorlevel!"
if defined WANTOUT (
    findstr /c:"!WANTOUT!" "build\%~n1.stdout" >nul 2>&1
    if errorlevel 1 (
        echo FAIL: %T% missing output "!WANTOUT!"
        set FAILED=1
        goto :eof
    )
)
if not "!ACTUAL!"=="%EXPECTED%" (
    echo FAIL: %T% expected exit %EXPECTED%, got !ACTUAL!
    set FAILED=1
) else if defined WANTOUT (
    echo PASS: %T% ^(exit !ACTUAL!, printed "!WANTOUT!"^)
) else (
    echo PASS: %T% ^(exit !ACTUAL!^)
)
goto :eof

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

rem BUG-017: a compile that fails must not leave an output file behind, and an
rem existing .s from a previous successful build must survive a failed rebuild.
:output_file_contract
if exist "build\contract.s" del "build\contract.s" >nul 2>&1
mycc.exe tests\errors\undeclared.c -o "build\contract.s" >nul 2>&1
if exist "build\contract.s" (
    echo FAIL: BUG-017 a failed compile left build\contract.s behind
    set FAILED=1
    goto :eof
)
echo PASS: failed compile leaves no output file
goto :eof

rem -w silences the warning channel only: the compile still succeeds and the
rem diagnostic text disappears (BUG-016).
:silence_contract
mycc.exe -w tests\25_fall_off_end.c -o "build\silent.s" > "build\silent.out" 2>&1
if errorlevel 1 (
    echo FAIL: -w turned a valid compile into a failure
    set FAILED=1
    goto :eof
)
findstr /c:"warning:" "build\silent.out" >nul 2>&1
if not errorlevel 1 (
    echo FAIL: -w did not silence the warning channel
    set FAILED=1
    goto :eof
)
echo PASS: -w silences warnings without failing the build
goto :eof

rem --sign stamps exactly one provenance string into the emitted assembly, and
rem a build without it stamps nothing. The length argument to intern_string()
rem is a byte count, and passing 0 once interned an empty string -- valid
rem assembly, no warning, no string: a silent-wrong-output bug (report.md
rem BUG-024). This check reads the .s, because the failure mode is invisible
rem everywhere else.
:sign_contract
mycc.exe --sign tests\26_hello_world.c -o "build\signed.s" >nul 2>&1
if errorlevel 1 (
    echo FAIL: --sign broke a valid compile
    set FAILED=1
    goto :eof
)
findstr /c:"generated by mycc" "build\signed.s" >nul 2>&1
if errorlevel 1 (
    echo FAIL: --sign did not embed the provenance string
    set FAILED=1
    goto :eof
)
mycc.exe tests\26_hello_world.c -o "build\unsigned.s" >nul 2>&1
if errorlevel 1 (
    echo FAIL: plain compile failed after --sign
    set FAILED=1
    goto :eof
)
findstr /c:"generated by mycc" "build\unsigned.s" >nul 2>&1
if not errorlevel 1 (
    echo FAIL: provenance string leaked into a default build
    set FAILED=1
    goto :eof
)
echo PASS: --sign embeds the provenance string, and only on request
goto :eof
