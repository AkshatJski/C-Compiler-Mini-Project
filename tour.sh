#!/bin/bash
# ============================================================
#  tour.sh - a guided, narrated demo of the mycc compiler.
#
#  Run this inside WSL (type "wsl" in PowerShell first):
#      bash tour.sh
#  Requires: mycc (run: make CC=/tmp/opencode/gccwrap)
#  Press Enter to move from one step to the next.
# ============================================================

set -u

export PATH=/tmp/opencode/root/usr/bin:$PATH
export LD_LIBRARY_PATH=/tmp/opencode/root/usr/lib/x86_64-linux-gnu
CC=/tmp/opencode/gccwrap

if [ ! -x ./mycc ]; then
    echo "mycc not found. Run: make CC=/tmp/opencode/gccwrap"
    exit 1
fi
mkdir -p build

step() { echo; read -r -p "Press Enter to continue... " ; clear; }

clear
echo "=================================================================="
echo "  mycc  -  a compiler you can actually SEE working"
echo "=================================================================="
echo
echo "  A compiler turns human-readable code into the exact instructions"
echo "  your CPU runs. When you type 'gcc hello.c', all of that happens"
echo "  inside a giant black box. mycc does the same job in about 2,300"
echo "  lines of plain C that you could read in an evening - and it shows"
echo "  you every step along the way."
echo
echo "  This tour has 7 steps. Press Enter to advance."
step

echo "[1/7] THE PROGRAM"
echo "--------------------------------------------------------------"
echo
cat demos/simple.c
echo
echo "  Two tiny functions. 'square' multiplies a number by itself;"
echo "  'main' calls it with 5 and adds 1. Answer: 26."
step

echo "[2/7] mycc TURNS IT INTO ASSEMBLY"
echo "--------------------------------------------------------------"
echo
./mycc demos/simple.c -o build/simple.s
cat build/simple.s
echo
echo "  Every line here is a real x86-64 CPU instruction. Notice how it"
echo "  reads like the source:"
echo
echo "    pushq %rbp / movq %rsp,%rbp   - set up the stack frame"
echo "    movq -8(%rbp), %rax            - load \"x\""
echo "    imulq %rcx, %rax               - multiply:      x * x"
echo "    call square                    - jump into the function"
echo "    addq %rcx, %rax                - add 1:  square(5)+1"
echo "    leave / ret                    - return from main"
echo
echo "  gcc does all of this too - it just never lets you watch."
step

echo "[3/7] ASSEMBLE, LINK, AND RUN IT"
echo "--------------------------------------------------------------"
echo
$CC -no-pie -o build/simple build/simple.s
build/simple
result=$?
echo
echo "  The program returned: $result"
echo "  Expected:               26"
echo
echo "  The answer travelled: C source -> assembly -> executable -> exit code."
echo "  A compiler built from scratch produced a working program."
step

echo "[4/7] THE SAME PROGRAM, COMPILED BY REAL GCC"
echo "--------------------------------------------------------------"
echo
$CC -o build/simple_gcc demos/simple.c
build/simple_gcc
result=$?
echo
echo "  gcc's version also returns: $result"
echo
echo "  So mycc really is a compiler, not a toy. But notice the scale:"
echo "  gcc is roughly 15 million lines of code written by thousands of"
echo "  people over 35 years. mycc is about 2,300 lines you can fully"
echo "  understand."
step

echo "[5/7] WHERE THEY DIFFER - THE ASSEMBLY"
echo "--------------------------------------------------------------"
echo
echo "  In step 2 you saw mycc translate square(5)+1 faithfully."
echo "  Now watch what real gcc does with the same source, at -O2:"
echo
$CC -S -O2 demos/simple.c -o build/simple_gcc.s
cat build/simple_gcc.s
echo
echo "  Look at the end of 'main':  movl \$26, %eax."
echo "  gcc is so clever it worked out square(5)+1 = 26 AT COMPILE TIME"
echo "  and deleted the function call entirely."
echo
echo "  That is the real difference:"
echo "    gcc  - a production optimizer. Final code has nothing in"
echo "           common with what you wrote. Fast, but unreadable."
echo "    mycc - never optimizes. It emits exactly what you wrote, one"
echo "           instruction per operation. Slow, but you can follow it."
echo
echo "  For learning how compilers work, mycc wins. For shipping software,"
echo "  gcc wins."
step

echo "[6/7] THE REAL DEMO - EVERY FEATURE AT ONCE"
echo "--------------------------------------------------------------"
echo
echo "  example.c uses functions, recursion, for loops, if/else, % and"
echo "  && || !, ++/--, compound assignment, globals, and block scoping."
echo "  It computes fact(5) = 120 plus the sum of primes up to 20 = 77."
echo
./mycc example.c -o build/example.s
$CC -no-pie -o build/example build/example.s
build/example
result=$?
echo
echo "  The program returned: $result   (120 + 77 = 197)"
echo
echo "  A glimpse of the generated code:"
echo
grep -nE "call (is_prime|fact)|idivq|imulq" build/example.s
step

echo "[7/7] PROOF IT IS ROBUST - THE TEST SUITE"
echo "--------------------------------------------------------------"
echo
make CC=/tmp/opencode/gccwrap test
echo
echo "  Five independent programs, each compiled by mycc, run, and"
echo "  verified against a hand-computed answer."
echo
echo "  UNDER THE HOOD - mycc has four stages, all hand-written:"
echo
echo "    src/lexer.c      text      -> tokens"
echo "    src/parser.c     tokens    -> syntax tree"
echo "    src/semantic.c   checks    -> catches mistakes, assigns stack slots"
echo "    src/codegen.c    tree      -> assembly"
echo
echo "    src      : compiler source (2,130 lines)"
echo "    include  : compiler headers (225 lines)"
echo
echo "  And one more thing: the SAME mycc produces executables for BOTH"
echo "  Windows and Linux. Run tour.bat in PowerShell to see the identical"
echo "  demo on Windows."
step
echo
echo "Thanks for watching!"
echo
exit 0
