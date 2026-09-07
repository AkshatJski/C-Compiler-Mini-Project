#!/bin/bash
# ============================================================
#  tour2.sh - INSIDE THE COMPILER: the four stages, explained.
#
#  tour.sh  shows WHAT mycc does (watch it compile + run).
#  tour2.sh shows HOW it does it (open the hood: lexer,
#            parser, semantic analysis, code generation).
#
#  Run inside WSL:   bash tour2.sh
#  Requires: mycc (run: make CC=/tmp/opencode/gccwrap)
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
echo "  INSIDE mycc - how a compiler actually works (4 stages)"
echo "=================================================================="
echo
echo "  gcc is a black box. mycc is a glass box. Every program mycc"
echo "  compiles passes through four hand-written stages, and you can"
echo "  watch each one:"
echo
echo "    1. LEXER      source text     -> tokens"
echo "    2. PARSER     tokens          -> syntax tree"
echo "    3. SEMANTIC   checks the tree -> catches mistakes"
echo "    4. CODEGEN    tree            -> x86-64 assembly"
echo
echo "  We will trace one small program through all four. Press Enter."
step

echo "[1/6] THE PROGRAM WE WILL TRACE"
echo "--------------------------------------------------------------"
echo
cat demos/simple.c
echo
echo "  To the computer this file is just a string of characters:"
echo "  'i', 'n', 't', ' ', 'm', 'a', 'i', 'n' ...  garbage to the CPU."
echo "  The first job is to chop it into meaningful pieces."
step

echo "[2/6] STAGE 1 - THE LEXER (text -> tokens)"
echo "--------------------------------------------------------------"
echo
echo "  Run:  ./mycc --dump-tokens demos/simple.c"
echo
./mycc --dump-tokens demos/simple.c
echo
echo "  Every line is one TOKEN: a number, an identifier, a keyword,"
echo "  an operator, or a bracket - with its line:column recorded."
echo "  The lexer also threw away whitespace and comments."
echo "  (If you mis-type 'retrun', THIS is where the typo becomes a"
echo "  different token and the whole program gets misread.)"
step

echo "[3/6] STAGE 2 - THE PARSER (tokens -> syntax tree)"
echo "--------------------------------------------------------------"
echo
echo "  Tokens in a flat list have no structure. '1 + 2 * 3' could"
echo "  mean two things. The parser imposes meaning by BUILDING A TREE."
echo "  Run:  ./mycc --dump-ast demos/simple.c"
echo
./mycc --dump-ast demos/simple.c
echo
echo "  The tree says: square is a FUNCTION with one PARAM x; its body"
echo "  is a BLOCK containing RETURN of (x * x). main RETURNs"
echo "  (square(5) + 1). That is the whole program, as structure."
echo
echo "  Precedence is baked into the tree: '*' sits BELOW '+', so it is"
echo "  evaluated first. The parser is \"recursive descent\" - one"
echo "  function per rule (parse_multiplicative, parse_additive, ...)."
echo
echo "  The parser also REWRITES fancy syntax into a smaller core:"
step

echo "[3b] PARSER DESUGARING - fancy syntax gets simplified"
echo "--------------------------------------------------------------"
echo
./mycc --dump-ast demos/desugar.c
echo
echo "  See how the source's sugar was turned into plain operations:"
echo "    a += 2      ->  ASSIGN a = (a + 2)"
echo "    b = a++     ->  ASSIGN b = POSTFIX ++ (a)"
echo "    if (!a)     ->  IF (a == 0)"
echo "    return -b   ->  RETURN (0 - b)"
echo
echo "  A shorter language means a smaller compiler: every later stage"
echo "  only has to understand a tiny core."
step

echo "[4/6] STAGE 3 - SEMANTIC ANALYSIS (catches mistakes)"
echo "--------------------------------------------------------------"
echo
echo "  Now the compiler KNOWS your program's structure. Before going"
echo "  further it verifies it makes sense: every variable exists,"
echo "  functions get the right number of arguments, nothing is"
echo "  declared twice. It also assigns every local a home on the"
echo "  stack - those '@ -8(%rbp)' offsets and '(frame N bytes)'"
echo "  sizes you saw in the AST came from this stage."
echo
echo "  Watch it catch mistakes. Run:  ./mycc demos/broken.c"
echo
./mycc demos/broken.c
echo
echo "  The error reports point to the EXACT line and column."
echo "  gcc gives you errors too - but here you can read the ~250"
echo "  lines of src/semantic.c that produce them."
step

echo "[5/6] STAGE 4 - CODE GENERATION (tree -> assembly)"
echo "--------------------------------------------------------------"
echo
echo "  The tree is walked top to bottom and every node becomes an"
echo "  x86-64 instruction. Run:  ./mycc demos/simple.c"
echo
./mycc demos/simple.c
echo
echo "  How the generated code works:"
echo
echo "    pushq %rbp / movq %rsp,%rbp   - open a stack frame"
echo "    subq \$16, %rsp                  - reserve local space"
echo "    movq %rdi, -8(%rbp)            - save argument x"
echo "    movq -8(%rbp), %rax            - load x"
echo "    pushq %rax ... popq %rcx       - operand stacking"
echo "    imulq %rcx, %rax               - x * x"
echo "    call square                    - jump, remember return addr"
echo "    leave / ret                    - close frame, go back"
echo
echo "  Expressions work like a pocket calculator: push, push, pop,"
echo "  compute. That is why nested calls like f(g(x)) are safe."
step

echo "[6/6] THE WHOLE MACHINE - one run, end to end"
echo "--------------------------------------------------------------"
echo
echo "  Now the real program: example.c - functions, recursion, for"
echo "  loops, if/else, globals, scoping.  fact(5)+primes = 197."
echo
./mycc example.c -o build/example.s
$CC -no-pie -o build/example build/example.s
build/example
result=$?
echo
echo "  The program returned: $result   (expected 197)"
echo
echo "  That one number travelled through all four stages - and a"
echo "  full test suite proves it is reliable:"
echo
make CC=/tmp/opencode/gccwrap test
echo
echo "  UNDER THE HOOD - the four stages, and how big each one is:"
echo
echo "    src/lexer.c      ~330 lines   text      -> tokens"
echo "    src/parser.c     ~810 lines   tokens    -> tree"
echo "    src/semantic.c   ~250 lines   checks + stack slots"
echo "    src/codegen.c    ~420 lines   tree      -> assembly"
echo "    src/symtab.c     ~90 lines    the scope stack that backs it"
echo "    src/dump.c       ~210 lines   powers --dump-tokens / --dump-ast"
echo
echo "  Total: about 2,400 lines of C99. No Lex, no Yacc, no LLVM."
echo "  Everything is hand-written, and you just watched all of it."
step
echo
echo "That is mycc - a whole compiler you can see working."
echo
exit 0
