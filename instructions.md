# mycc — A C-Subset Compiler for x86-64

This document explains what `mycc` is, what it can do, how it works, and —
most importantly — how to build, test, and showcase it, step by step.

---

## 1. What this project is

`mycc` is a small but complete compiler written in C99. It reads a source
file written in a subset of the C language and produces **x86-64 assembly**
in **AT&T syntax**. The assembly is position-independent and follows both
major x86-64 conventions, so it assembles and runs on **Linux** (ELF,
System V AMD64) and **Windows** (PE/COFF, Microsoft x64) with plain gcc /
MinGW-w64.

The overall pipeline looks like this:

```
your_program.c  ──> mycc ──> your_program.s (assembly)
your_program.s  ──> gcc ──> your_program    (executable)
your_program    ──> run ──> exit code with the result
```

The compiler is deliberately self-contained: a hand-written lexer, a
hand-written recursive-descent parser, a symbol table, a semantic-analysis
pass, and an instruction generator. It has no dependencies on tools like
Lex, Yacc, or LLVM — everything is plain C.

### Project layout

```
my_c_compiler/
├── Makefile            Build + test automation (Linux / WSL)
├── build.bat           Build script (Windows: compiles mycc.exe)
├── test.bat            Test script (Windows: runs the 5 tests)
├── showcase.bat        Demo script (Windows: example.c -> run, exit 197)
├── tour.bat            Guided narrated demo (Windows, press any key)
├── tour.sh             Guided narrated demo (Linux / WSL)
├── tour2.bat           DEEP-DIVE tour: the four stages, with --dump-tokens
│                       and --dump-ast (Windows)
├── tour2.sh            DEEP-DIVE tour: the four stages (Linux / WSL)
├── example.c           Demo program (returns 197)
├── demos/              Tiny showcase programs (whole assembly fits on screen)
│   ├── simple.c          square(5)+1 -> exit 26
│   ├── loop.c            1+2+...+10 -> exit 55
│   ├── desugar.c         shows parser rewrites (a += 2, a++, !a, -b)
│   └── broken.c          intentionally wrong; shows semantic error reports
├── instructions.md     This file
├── include/            Public headers for the compiler's own source
│   ├── tokens.h        Token types used by the lexer/parser
│   ├── lexer.h         Lexer interface
│   ├── ast.h           Abstract Syntax Tree node definitions
│   ├── parser.h        Parser interface
│   ├── symtab.h        Symbol table (scopes) interface
│   ├── semantic.h      Semantic analysis interface
│   ├── codegen.h       Code generation interface
│   ├── dump.h          --dump-tokens / --dump-ast printer interface
│   └── util.h          Shared helpers and error reporting
├── src/                The compiler's own source code
│   ├── main.c          Driver: reads file, runs each stage, writes output
│   ├── lexer.c         Scanner: source text -> tokens
│   ├── parser.c        Recursive-descent parser: tokens -> AST
│   ├── symtab.c        Scoped symbol table
│   ├── semantic.c      Checks + variable storage assignment (single pass)
│   ├── codegen.c       AST -> x86-64 AT&T assembly
│   ├── dump.c          Pretty-printers for --dump-tokens and --dump-ast
│   └── util.c          Helpers (memory allocation, error reporting)
├── tests/              Self-checking test programs
│   ├── 01_arithmetic.c   + - * / %, precedence, unary minus, comments
│   ├── 02_conditionals.c if/else, comparisons
│   ├── 03_loops.c        while and for loops
│   ├── 04_functions.c    functions, params, recursion, globals
│   └── 05_advanced.c     && || !, ++/--, compound assignment, scoping
├── mycc                The compiled compiler binary (Linux build result)
└── mycc.exe            The compiled compiler binary (Windows build result)
```

### The four stages of the compiler

1. **Lexer** (`src/lexer.c`) — reads the characters of the source file and
   groups them into *tokens*: keywords (`int`, `if`, `while`, ...), numbers,
   identifiers, and operators (`+`, `==`, `&&`, `++`, ...). It also skips
   whitespace and `//` and `/* */` comments, and remembers each token's
   line/column for error messages.

2. **Parser** (`src/parser.c`) — consumes the token stream and builds an
   **Abstract Syntax Tree (AST)**: a tree of `ASTNode` structs describing
   the program's structure. It is a *recursive-descent* parser with one
   function per precedence level, so `2 + 3 * 4` parses as `2 + (3 * 4)`.

3. **Semantic analysis** (`src/semantic.c` + `src/symtab.c`) — walks the AST
   and checks it is well-formed: variables are declared, functions exist and
   receive the right number of arguments, nothing is declared twice in the
   same scope, and so on. At the same time it assigns every local variable a
   stack slot (`-8(%rbp)`, `-16(%rbp)`, ...) so the code generator knows
   where to put them. Symbol lookups go through a stack of *scopes*, which
   is what makes nested blocks and variable shadowing work.

4. **Code generation** (`src/codegen.c`) — walks the AST again and emits
   assembly. Arguments go in six fixed registers (calls into a function use
   the same six registers on both targets, so caller and callee agree), the
   stack stays 16-byte aligned at every `call` (required by both the System V
   and Microsoft x64 ABIs), and function results are returned in `%rax`.
   No target-specific ELF/COFF details leak into the instruction stream, so
   the same `.s` output (modulo a few section directives) builds on both
   Linux and Windows.

---

## 2. Language capabilities

Everything below is supported by the compiler.

### Data
- A single integer type: `int` (all values are 64-bit signed).
- Integer literals, e.g. `0`, `42`, `1000`.

### Operators (with correct precedence)
| Group                | Operators                          |
|----------------------|------------------------------------|
| Assignment           | `=  +=  -=  *=  /=  %=`            |
| Logical OR           | `\|\|`                             |
| Logical AND          | `&&`                               |
| Equality             | `==  !=`                           |
| Relational           | `<  <=  >  >=`                     |
| Additive             | `+  -`                             |
| Multiplicative       | `*  /  %`                          |
| Unary                | `-  +  !`                          |
| Postfix              | `x++  x--`                         |

- `&&` and `||` use **short-circuit evaluation**: the right operand is only
  evaluated if the left operand does not already decide the result. This
  means `0 && (1 / 0)` is safe (no division by zero).
- `%` follows C semantics: the result has the sign of the dividend
  (truncated toward zero).
- `!x`, `-x`, `x += y`, and `++x` are implemented by *desugaring*: the
  parser rewrites them into simpler forms the code generator already knows
  (see section 3).

### Statements
- Blocks `{ ... }`
- Local variable declarations with or without initializers, e.g.
  `int a;` and `int a = 5;`
- Multiple declarators: `int a, b = 3, c;`
- `if (cond) stmt` and `if (cond) stmt else stmt`
- `while (cond) stmt`
- `for (init; cond; step) stmt` — `init` may be a declaration
  (`for (int i = 0; ...)`), an expression, or empty
- `return expr;` and `return;`

### Functions
- Global functions returning `int`, with `void` or empty parameter lists.
- Up to **6 parameters** (the x86-64 register-argument limit we support).
- **Recursion** (functions call themselves safely).
- **Forward references**: a function may call another function that is
  defined later in the file.
- Nested function calls, and calls inside expressions:
  `add(mul(a, b), divq(a, b))`.

### Variables and scope
- Global variables (zero-initialized, placed in `.data`).
- Local variables (stack-allocated in the function's own frame).
- Proper **block scoping**: an inner block may declare a variable with the
  same name as one outside it (shadowing), and the outer variable is
  untouched after the block ends.

### Comments
- `// line comment` and `/* block comment */`

### Error reporting
The compiler reports problems with file, line, and column, for example:

```
prog.c:3:9: error: undeclared variable 'y'
prog.c: compilation aborted during semantic analysis
```

---

## 3. How the features were implemented (highlights)

- **Parser desugaring.** Several "convenience" features never reach the
  code generator. The parser rewrites them into a small core the later
  stages already understand: `a += 2` becomes `a = a + 2`, `b = a++`
  becomes `b = (a = a + 1, old_value)` via a `POSTFIX` node, `!a` becomes
  `a == 0`, and `-b` becomes `0 - b`. A shorter language means a smaller
  compiler. You can *see* the rewrites: `mycc --dump-ast demos\desugar.c`
  prints the tree after desugaring.

- **Stack-machine expression codegen.** Binary operators evaluate the left
  operand, push it, evaluate the right operand, pop the left back into a
  scratch register, and apply the operation. This is simple and handles
  arbitrarily deep expressions.

- **Comparison operators** compile to `cmpq` + `setcc` + `movzbq`, which
  produces a normal `0`/`1` value.

- **Division and modulo** use the signed divide instruction: `cqto` sign
  extends the dividend into `%rdx:%rax`, `idivq %rbx` computes quotient in
  `%rax` and remainder in `%rdx`. `%` simply moves `%rdx` into `%rax`.

- **Short-circuit `&&` / `||`** are compiled with branch labels so the right
  operand is skipped when the left decides the result.

- **Calls and the ABI.** Arguments are evaluated *right to left*, each one
  pushed onto the stack, then popped into the argument registers in order.
  Pushing instead of filling registers directly is what makes nested calls
  safe (`add(add(a,b), add(1,2))`) — an inner call can never clobber an
  argument register that is still needed. The stack is padded to stay
  16-byte aligned at the `call` instruction — a requirement shared by both
  the System V AMD64 and Microsoft x64 ABIs, so the same scheme works for
  Linux and Windows.

- **Recursion safety.** Parameters are spilled into the *callee's own* stack
  frame at negative offsets (`-8(%rbp)`, `-16(%rbp)`, ...). An earlier
  design spilled them just below the caller's return address (`+16(%rbp)`),
  which corrupted callers that had no frame — the current layout makes each
  call independent and recursive calls correct.

- **Scoping.** The symbol table is a chain of scopes. Both semantic analysis
  and offset assignment happen in a single pass, so scope push/pop keeps
  variable lifetime correct: a block's variables exist only while that block
  is being checked, and shadowing works naturally.

- **Globals** live in the `.data` section and are accessed with RIP-relative
  addressing (`movq g_count(%rip), %rax`).

---

## 4. Which platform? (choose one)

The compiler itself is portable, and its assembly output works with gcc on
**both** Windows and Linux. Pick whichever you have:

| Option | Where you run it | What you need |
|--------|------------------|---------------|
| **A. Windows (native)** | PowerShell / CMD on Windows | MinGW-w64 gcc (free, see below) |
| **B. Linux via WSL** | A `wsl` shell inside Windows | the private toolchain already set up at `/tmp/opencode/root` |

This project was developed in WSL, so on this particular machine the WSL
toolchain is already prepared. But if your main device is Windows, **use
Option A** — the compiler builds to a real `mycc.exe` and the whole
workflow runs in PowerShell with no WSL involved.

### Option A requirement: MinGW-w64

To build and run on Windows you need **MinGW-w64**, a free Windows port of
gcc. Two easy ways to get it:

- **MSYS2** (recommended): install from https://www.msys2.org, then open
  "MSYS2 UCRT64" and run:
  ```
  pacman -S --needed mingw-w64-ucrt-x86_64-gcc
  ```
  This installs gcc into `C:\msys64\ucrt64\bin`.
- **Standalone toolchain**: download the "winlibs" build from
  https://winlibs.com (choose the "UCRT runtime" 64-bit zip), extract it,
  and add its `bin` folder to your `PATH`.

> **Good news: this machine already has MSYS2 installed**, so you probably
> have nothing to do. The three `.bat` scripts **auto-detect** MSYS2
> (`C:\msys64\ucrt64\bin`) or a `C:\w64devkit` installation when `gcc` is
> not on your PATH. You can just run them (section 5).

Check it worked — in a **new** PowerShell window run:
```
gcc --version
```
You should see a version line (e.g. `gcc (MinGW-W64 ...)`). If it says
`gcc is not recognized`, the scripts will still work if MSYS2 is installed
at the default location; otherwise fix the PATH or set `CC` to the full
path of your gcc before running the scripts:

### Option B requirement: the WSL toolchain

On this machine, the Linux toolchain is unpacked in a private folder
because there is no system gcc:

- Tools live in: `/tmp/opencode/root/usr/bin`
- Libraries live in: `/tmp/opencode/root/usr/lib/x86_64-linux-gnu`
- A helper wrapper exists at: `/tmp/opencode/gccwrap`

Because of that, every new WSL terminal must run two `export` lines before
building (see section 6B).

---

## 5. Option A — Build, test, and showcase on Windows (recommended)

Open **PowerShell** in the project folder:
```
cd "C:\Folder D\C Compiler Test THROUGH OPENCODE\my_c_compiler"
```

(You can also open the folder in File Explorer and type `powershell` in the
address bar.)

### 5.1 Build the compiler (`mycc.exe`)

Run:
```
.\build.bat
```

This compiles all of `src\*.c` into `mycc.exe`. **Expected output:**

```
Build OK: mycc.exe
```

If it fails, gcc was not found anywhere — see section 4, Option A, or set
`CC` to the full path of your gcc before re-running:

```
set CC=C:\msys64\ucrt64\bin\gcc.exe
.\build.bat
```

### 5.2 Run the automated test suite

```
.\test.bat
```

This compiles every `tests\*.c`, assembles it into an `.exe`, runs it, and
compares the exit code with the `EXPECT:` marker in the file. **Expected
output:**

```
PASS: tests\01_arithmetic.c (exit 0)
PASS: tests\02_conditionals.c (exit 0)
PASS: tests\03_loops.c (exit 0)
PASS: tests\04_functions.c (exit 0)
PASS: tests\05_advanced.c (exit 0)
All tests passed
```

### 5.3 Run the end-to-end showcase

```
.\showcase.bat
```

This compiles `example.c`, assembles it, runs it, and prints the result.
**Expected output:**

```
[1/3] Compiling example.c to assembly...
[2/3] Assembling and linking...
[3/3] Running...

The program returned: 197
Expected:               197
```

Why 197? `example.c` computes `fact(5) = 120` plus the sum of primes up to
20 (`2+3+5+7+11+13+17+19 = 77`), so `120 + 77 = 197`. The exit code being
exactly 197 proves the compiler works end to end on Windows.

### 5.4 Manual usage (the commands behind the scripts)

```
mycc.exe example.c -o build\example.s      # C source -> assembly
gcc      -o build\example.exe build\example.s   # assembly -> exe
build\example.exe                           # run it
echo %errorlevel%                          # shows 197
```

Notes:
- `mycc.exe` on Windows automatically emits Windows-compatible assembly.
- `--target=windows` is implied for a MinGW-built `mycc.exe`; you can also
  pass it explicitly: `mycc.exe --target=windows example.c -o out.s`.
- Two diagnostic flags show the compiler's inner workings:
  - `mycc.exe --dump-tokens demos\simple.c` — print the lexer output: every
    token (number, identifier, keyword, operator, bracket) with its
    line:column.
  - `mycc.exe --dump-ast demos\simple.c` — print the syntax tree after
    semantic analysis, annotated with stack offsets, frame sizes, and
    desugared forms. This is tour2's main tool (section 10).

### 5.5 Write and run your own program

Create `hello.c` in the project folder:

```
int triple(int x) {
    return x * 3;
}
int main(void) {
    int n = triple(7);
    return n + 1;
}
```

Then:

```
mycc.exe hello.c -o build\hello.s
gcc -o build\hello.exe build\hello.s
build\hello.exe
echo %errorlevel%
```

`7 * 3 + 1 = 22`, so the last line prints `22`.

---

## 6. Option B — Build, test, and showcase on Linux (WSL)

### 6.1 Enter WSL and set up the toolchain

Run these in **PowerShell** first, then in the Linux shell:

```
wsl
```

Your prompt should now end in `$` (not `PS`). The Windows folder
`C:\Folder D\...` is visible inside WSL at `/mnt/c/Folder D/...`. Now run:

```
cd "/mnt/c/Folder D/C Compiler Test THROUGH OPENCODE/my_c_compiler"
export PATH=/tmp/opencode/root/usr/bin:$PATH
export LD_LIBRARY_PATH=/tmp/opencode/root/usr/lib/x86_64-linux-gnu
```

The two `export` lines tell the shell where the private gcc/make live; they
must be re-run in every new terminal.

Sanity check:
```
ls
```
Expected: `Makefile  example.c  include  instructions.md  mycc  src  tests`

### 6.2 Build the compiler

```
make clean
make CC=/tmp/opencode/gccwrap
```

- `make clean` removes old build artifacts.
- `make CC=/tmp/opencode/gccwrap` compiles `src/*.c` into the `mycc` binary.

Expected: compile lines and no error messages.

### 6.3 Run the automated test suite

```
make CC=/tmp/opencode/gccwrap test
```

Expected output:

```
PASS: tests/01_arithmetic.c (exit 0)
PASS: tests/02_conditionals.c (exit 0)
PASS: tests/03_loops.c (exit 0)
PASS: tests/04_functions.c (exit 0)
PASS: tests/05_advanced.c (exit 0)
All tests passed
```

### 6.4 Watch a program become assembly

```
cat example.c
./mycc example.c
```

The second command prints the generated x86-64 assembly: `pushq`, `movq`,
`call`, `jmp`, and labels like `.L_for_start_1:`.

### 6.5 Run the end-to-end showcase

```
./mycc example.c -o /tmp/opencode/demo.s
/tmp/opencode/gccwrap -no-pie -o /tmp/opencode/demo /tmp/opencode/demo.s
/tmp/opencode/demo
echo $?
```

The last line prints `197` — same result as Windows.

### 6.6 Try your own program (WSL)

Create a test file by typing the `cat` command, the program, then **Ctrl+D**
to finish:

```
cat > /tmp/opencode/test.c
int triple(int x) {
    return x * 3;
}
int main(void) {
    int n = triple(7);
    return n + 1;
}
```

(press Ctrl+D here)

Then run the full pipeline on it:

```
./mycc /tmp/opencode/test.c -o /tmp/opencode/test.s
/tmp/opencode/gccwrap -no-pie -o /tmp/opencode/test /tmp/opencode/test.s
/tmp/opencode/test
echo $?
```

`7 * 3 + 1 = 22`, so the last line should print:

```
22
```

### 6.7 See the error reporting (WSL, optional)

```
printf 'int main(void){ return missing_var; }\n' > /tmp/opencode/err.c
./mycc /tmp/opencode/err.c
```

Expected:

```
/tmp/opencode/err.c:1:22: error: undeclared variable 'missing_var'
/tmp/opencode/err.c: compilation aborted during semantic analysis
```

`1:22` means line 1, column 22 — exactly where the mistake is.

---

## 7. Supported-language quick reference

```c
/* This is a valid mycc program. */

int g_count;                    /* global (zero-initialized) */

int is_prime(int n) {           /* function, one parameter */
    if (n < 2) return 0;
    for (int d = 2; d <= n / 2; d = d + 1) {   /* for with declaration */
        if (n % d == 0) return 0;              /* % and == */
    }
    return 1;
}

int main(void) {
    int a, b = 10;              /* multiple declarators */
    int s = 0;
    a = 1;
    a += 2;                     /* compound assignment: a = 3 */
    a++;                        /* postfix increment: a = 4 */
    ++a;                        /* prefix increment:  a = 5 */
    b -= a;                     /* b = 5 */
    if (a == 5 && !(b < 0) || b == 5) {        /* &&, ||, ! */
        s = s + 1;
    }
    for (a = 0; a < 5; a++) {   /* for with expression init */
        s += 2;
    }
    g_count++;
    return s;                   /* returns 11 */
}
```

### Not supported (by design)
- No other types: no `char`, `float`, `long`, structs, pointers, arrays.
- No `break` / `continue` inside loops.
- No `do...while`, `switch`, or `else if` keyword (write `else { if ... }`).
- No `#include`, no preprocessor, no string literals, no printing.
  Programs communicate results through their **exit code** (`return`).

---

## 8. Troubleshooting

| Symptom                                          | Fix |
|--------------------------------------------------|-----|
| `gcc is not recognized as the name of a cmdlet...` or `'gcc' is not recognized` | MinGW is not on `PATH` (or not installed). See section 4, Option A. |
| `'.\build.bat' is not recognized`                | Run in PowerShell/CMD from the project folder (section 5). |
| `mycc.exe` not found                             | Run `.\build.bat` first (section 5.1). |
| `export: The term 'export' is not recognized...` | You are in PowerShell but followed the WSL steps. Run `wsl` first (section 6.1), or use the Windows steps in section 5. |
| `make: command not found`                        | Re-run the two `export` lines from section 6.1 in this terminal. |
| `gcc: command not found` (in WSL)                | Same as above — the private toolchain is not on `PATH`. |
| `cannot open source file '...'`                  | The file does not exist, or the path has a typo/spaces issue; quote the path. |
| `error: unexpected character`                    | The program uses a feature the language does not support (see section 7). |
| `FAIL: ... expected exit X, got Y`               | A genuine compiler bug — this is worth reporting/fixing. |
| Linker errors like `cannot find libc_nonshared` | `LD_LIBRARY_PATH` is missing; re-run the exports. |
| `mycc` not found (in WSL)                        | You are not in the project folder, or the build in 6.2 has not been run. |

---

## 9. Quick-start recap

**Windows (PowerShell)** — fastest path:

```
cd "C:\Folder D\C Compiler Test THROUGH OPENCODE\my_c_compiler"
.\build.bat
.\test.bat        # all 5 tests should pass
.\showcase.bat    # should print "The program returned: 197"
```

**Linux (WSL)**:

```
cd "/mnt/c/Folder D/C Compiler Test THROUGH OPENCODE/my_c_compiler"
export PATH=/tmp/opencode/root/usr/bin:$PATH
export LD_LIBRARY_PATH=/tmp/opencode/root/usr/lib/x86_64-linux-gnu
make clean
make CC=/tmp/opencode/gccwrap
make CC=/tmp/opencode/gccwrap test      # all 5 tests should pass
./mycc example.c -o /tmp/opencode/demo.s
/tmp/opencode/gccwrap -no-pie -o /tmp/opencode/demo /tmp/opencode/demo.s
/tmp/opencode/demo
echo $?                                 # should print 197
```

---

## 10. Showing it off

### The 7-step narrated tour (recommended)

On Windows, open PowerShell in the project folder and run:

```
.\tour.bat
```

Inside WSL (`wsl` first, plus the two `export` lines from section 6.1):

```
bash tour.sh
```

The tour walks through 7 steps and pauses for you to talk:

1. A tiny program (`demos/simple.c`) that returns 26.
2. mycc turns it into **assembly** — all 31 lines shown on screen.
3. The assembly is assembled, linked, and run → exit code 26.
4. Real gcc compiles the same program → also 26 (proof it is a real
   compiler).
5. The difference: gcc at `-O2` constant-folds `square(5)+1` into
   `movl $26, %eax` and deletes the function call; mycc emits exactly
   what you wrote, one instruction per operation.
6. `example.c` exercises the whole feature set → exit 197.
7. The full test suite passes, then a look inside the compiler's four
   stages (lexer → parser → semantic → codegen).

### The deep-dive tour (tour2) — recommended for the curious

Same platforms as tour.bat, run `.\tour2.bat` (Windows) or `bash tour2.sh`
(WSL). tour2 opens the hood and traces one program through **all four
stages** using the new dump flags:

1. The program we will trace (`demos/simple.c`) — and why the CPU sees it
   as a meaningless character string.
2. **Stage 1, lexer**: `mycc --dump-tokens demos\simple.c` prints each
   token with its line:column. Point out that a typo like `retrun` would
   be misread here.
3. **Stage 2, parser**: `mycc --dump-ast demos\simple.c` shows the tree;
   note how `*` sits *below* `+` (precedence is baked into the structure).
3b. **Desugaring**: `--dump-ast demos\desugar.c` reveals `a += 2` → `a = a + 2`,
   `a++` → `POSTFIX`, `!a` → `a == 0`, `-b` → `0 - b`.
4. **Stage 3, semantic analysis**: `mycc demos\broken.c` catches an
   undeclared variable and a wrong-argument-count call, each pointing at
   its exact `line:col`.
5. **Stage 4, codegen**: `mycc demos\simple.c` prints the assembly; walk
   the frame setup, the `push/pop` expression engine, and `call`.
6. End to end: `example.c` → 197, plus the full test suite, then a
   by-file line count of the ~2,400-line hand-written compiler.

Great talking point: the same tree you can print with `--dump-ast` is
exactly what the code generator walks to emit assembly.

### The one-line explanations

- "A compiler turns readable code into CPU instructions. gcc does this
  inside a black box; mycc is ~2,300 lines of C that shows you every step."
- "mycc emits **exactly** what you write — one instruction per operation.
  gcc is an optimizer that rewrites your program until it no longer looks
  like your source. For learning, readable wins; for production, gcc wins."
- "Same source, both platforms: the same `.c` file compiles with mycc to
  a native Windows `.exe` *and* a native Linux binary, both returning the
  same result."

### Alternate quick demos

- **The return-code trick**: `.\demos` programs have no `printf` — they
  "print" by their exit code. `mycc.exe demos\loop.c -o build\loop.s`,
  then `gcc -o build\loop.exe build\loop.s` and run it → `%errorlevel%`
  is `55`.
- **Read the whole compiler**: `src/` is ~2,370 lines across 8 files.
  `src/codegen.c` is the shortest path to "aha" — it is just a walk over
  the syntax tree emitting instructions.
- **Catch a mistake live**: feed mycc a broken program and show the
  `file:line:col` error pointing exactly at the problem.
- **Show the parser rewriting your code**: `demos/desugar.c` is full of
  sugar (`+=`, `a++`, `!a`, `-b`); `mycc --dump-ast demos\desugar.c`
  prints the simplified tree the compiler actually uses. `demos/broken.c`
  is pre-filled with two errors to demonstrate semantic analysis.
