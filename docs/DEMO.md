# DEMO.md — the live-demo playbook

**Published in `docs/`. This is the script, not the marketing** — it says where
the claims are weak, which is why it is worth reading before the viva.

Everything in this file was executed on this checkout on 2026-10-24 before being
written down. If an output block appears below, it is copied from a real run —
not from memory. Where a number can drift (line counts), the command to
re-measure it is given next to it.

Shell assumption: **PowerShell** (that is what you present from). Where CMD
differs, it is called out explicitly. Paths assume:

```
C:\Folder D\Feature_anant\C-Compiler-Mini-Project
```

---

## 0. Read this first: the rules of engagement

### 0.1 The five numbers you must be able to say without looking

| Number | What it is |
|---|---|
| **9,502** | lines of compiler (`src/` 9,055 + `include/` 447) |
| **106** | automated checks (48 programs + 52 rejections + 6 contracts) |
| **1,000** | random programs compared against gcc per CI run (700 in the local audit) |
| **48** | defects logged: 47 fixed (16 in the working tree, pending commit), 1 open (`BUG-048`) |
| **30.9%** | smaller assembly on the current 48-test suite (14,290 → 9,872 lines); historical 35-test snapshot was 33.8% |

Plus one identifier you can be asked for and must have ready:
`40086581248` — the first green CI run on all three platforms (2026-10-24).
The latest green run on `main` is on the Actions page; the *first* one is the one
that matters, because before it the project had never been green on Windows or
macOS.

### 0.2 What you are actually selling

Not "a compiler". Every CS department has one of those.

You are selling: **claims that are executable**. Anybody can say "it works".
You can say "here are 106 checks, 1,000 random programs, three green CI jobs, and
a public ledger where 48 bugs have a reproduction, 47 have fixes and one is
listed as still open". That is the differentiator, and it is the line that makes
an examiner look up.

### 0.3 The three sentences you never say

| Never say | Say instead |
|---|---|
| "It compiles C." | "It compiles a C **subset** — here is exactly what it refuses, with a message." |
| "It's as good as gcc." | "It is 9,502 lines you can read; gcc is millions you cannot. Different goals." |
| "It's finished." | "It is complete for its stated scope, and here is the future-work list." |

Honesty is not a hedge here — it is the demo's whole aesthetic. A candidate who
says "this is where my compiler ends, and here is why" is more convincing than
one who claims completeness and then gets caught.

---

## 1. Pre-flight — 60 seconds before you start

### 1.1 Put gcc on PATH and build

```powershell
cd "C:\Folder D\Feature_anant\C-Compiler-Mini-Project"
$env:PATH = "C:\msys64\usr\bin;C:\msys64\ucrt64\bin;" + $env:PATH
.\build.bat
```

**What this does.** `build.bat` compiles all ten `src\*.c` files with
`-std=c99 -O2 -Wall -Wextra -Wpedantic -Iinclude` and links `mycc.exe`. It
globs `src\*.c` rather than carrying a file list, which is why adding a source
file never broke it (a stale list is BUG-028 in the ledger — that is the whole
reason it globs now).

**Suffix trick:** the third line puts MSYS2's `gcc` and `diffutils` on PATH for
*this PowerShell window only*. Without it, `gcc -o x.exe x.s` fails and every
scene dies. If `gcc` is already on PATH, skip it.

**What you should see:**

```
Build OK: mycc.exe
```

### 1.2 Prove the toolchain before an audience

```powershell
.\mycc.exe --version
.\test.bat | Select-Object -Last 3
```

**What you should see:**

```
mycc 0.7 - C-subset compiler for AMD64 AT&T assembly
targets: linux, windows, darwin (default: windows)
optimizer: on: folding, dead stores, registers (--no-opt to disable); --sign stamps programs
...
All tests passed
```

If that says `All tests passed`, the machine is ready and nothing else can
surprise you. **Run it in front of the audience if you have 60 spare seconds** —
"the suite passes, now watch me break it on purpose" is a much stronger opening
than "trust me".

### 1.3 Clean slate (optional)

```powershell
Remove-Item build\*.s, build\*.exe -ErrorAction SilentlyContinue
```

**Warning:** never run `git clean -fdx` in this repository. Three working notes
(`docs\decision.md`, `docs\learn.md`, `docs\instructions.md`) and the demo
transcript (`demo\expected.txt`) are gitignored, so `git clean` deletes them
permanently — they exist in no commit anywhere.

### 1.4 The one shell trap that will bite you live

In **PowerShell**, `fc` is an alias for `Format-Custom`, not the file-compare
tool. This:

```powershell
fc /b build\a.s build\b.s        # WRONG in PowerShell
```

gives `Format-Custom: A positional parameter cannot be found...`. Use the
`.exe` extension:

```powershell
fc.exe /b build\a.s build\b.s    # correct in both shells
```

In CMD, plain `fc /b` is fine. This is the single most likely thing to make you
look flustered on stage; it cost a minute of my own run.

---

## 2. Which demo for which room

| Time you get | Run | Script |
|---|---|---|
| 30 seconds | `.\demo.bat` scene 1–2 only | §3.1 |
| 90 seconds | the six-command chain in §3 (no `.bat`) | §3.2 |
| 5 minutes | `.\demo.bat` (all six scenes) | §3.3 |
| 12 minutes | the feature walkthrough, §4 | §4 |
| examiner-led ("do it again, differently") | §4 + §5 command reference | §4, §5 |

There are five scripted batch files in the repo. **Read §6 before running any of
them** — `tour.bat`/`tour2.bat`/`tour.sh`/`tour2.sh` contain stale numbers and
one claim that is now false.

---

## 3. The scripted demo (`demo.bat`) — every scene explained

### 3.1 What `demo.bat` is

```powershell
.\demo.bat
```

It is the **non-technical** demo: six scenes, each answering one question an
outsider actually asks. It compiles real programs with `mycc.exe`, links them
with gcc, runs them, and prints the result. Nothing is faked and nothing is
pre-recorded.

Its six scenes:

| Scene | The outsider's question | What the script does |
|---|---|---|
| 1 | "what does it do?" | compiles `hello.c`, runs it, prints a sentence |
| 2 | "can I see it work?" | compiles `demo\2_name.c`, which **asks for your name** and greets you |
| 3 | "is it actually doing anything?" | same program with `--no-opt`, half the assembly |
| 4 | "did *you* write it?" | `--sign`, then `strings` shows the provenance string inside the binary |
| 5 | "is it only C?" | compiles `demo\4_pair.myc` (different language) and `demo\4_pair.c`, `fc.exe` says identical |
| 6 | "can I trust it?" | runs the test suite + differential test |

**Why scene 2 is the one that wins the room.** `demo\2_name.c` is the only
program in the demo that reads input. It declares `extern int scanf(char *fmt);`
and calls `scanf("%63s", name)` — a variadic call into the platform C library,
through the real ABI, compiled by *your* compiler. When it prints the name the
audience just typed, the compiler stops being an abstraction.

### 3.2 The 90-second version (what I would actually present)

Six commands, in this order. Do not improvise on the day; rehearse these six.

```powershell
.\mycc.exe example.c -o build\example.s
gcc -o build\example.exe build\example.s
build\example.exe
$LASTEXITCODE
```

**What each part does:**

| Command | What it is doing |
|---|---|
| `mycc.exe example.c -o build\example.s` | lexes, parses, type-checks, optimizes and emits x86-64 AT&T assembly. **No gcc involved yet.** |
| `gcc -o build\example.exe build\example.s` | gcc here is an **assembler and linker only** — it is handed assembly, not C. It never sees `example.c`. |
| `build\example.exe` | runs the machine code your compiler produced |
| `$LASTEXITCODE` | the process exit code, i.e. the number `main` returned |

**What you should see:** no output from the program (it returns its answer
instead of printing), then `197`.

**What to say:** "197 is `fact(5)` = 120 plus the sum of the primes below 20 =
77. The number travelled: C source → my assembly → `.exe` → exit code. gcc
assembled it; it did not compile it."

Then the three visual moments:

```powershell
Select-String -Path build\example.s -Pattern 'call fact|call is_prime|idivq|imulq' | Select-Object -First 6
.\mycc.exe demos\broken.c
.\mycc.exe --dump-ast demos\desugar.c | Select-Object -First 25
```

**Why these three:** the first shows recursion compiled into `call fact`; the
second shows the compiler **refusing** a program with a caret pointing at the
exact column; the third shows the tree, i.e. the thing the compiler actually
thinks in, including the desugarings (`a += 2` becomes `a = a + 2`, `if (!a)`
becomes `if (a == 0)`).

### 3.3 The 5-minute version

```powershell
.\demo.bat
```

Say nothing during scene 1; narrate scene 2; for scene 3 say "same program,
half the instructions"; for scene 4 say "the binary can prove which compiler
built it"; for scene 5 say "that is a different language and the assembly is
byte-identical — `fc` just told you so"; for scene 6 say "and this runs on every
push, on three operating systems".

`demo\expected.txt` holds the captured transcript — if the projector dies, read
from it. **Do not read from it if the demo works**: a live run is the point.

---

## 4. The full technical walkthrough — every feature, one section each

This is the long form: for a lab viva, a project review, or an examiner who says
"show me how pointers work". Every section is **run this → see this → say this**.

Rule for the whole of §4: the **exit code is the answer**. `main` returning `n`
means the process exits with `n`, which the shell shows. Exit codes are 0–255,
so `return 197;` is visible but `return 300;` would show 44.

### 4.0 The pipeline — make the invisible visible

```powershell
.\mycc.exe demos\loop.c | Select-Object -First 20
```

**What it does:** with no `-o`, the assembly is printed **to stdout** instead of
being written to a file. Nothing is created on disk, so you can read the compiler's
output without leaving the terminal — useful when you want to scroll rather than
hunt for a file.

**What you should see:**

```
# Generated by mycc: C-subset compiler for AMD64 AT&T syntax
.text
.globl main
main:
	pushq %rbp
	movq %rsp, %rbp
	subq $16, %rsp
	movq $0, %rax
	movq %rax, -8(%rbp)
	movq $1, %rax
	movq %rax, -16(%rbp)
.L_for_start_0:
```

**What to say:** "that is the whole `main`: `subq $16` is the two locals, `-8` is
`s` and `-16` is `i`, and the labels are the shape of the `for` loop. There is no
C after this point — this *is* the program."

```powershell
.\mycc.exe --dump-tokens demos\simple.c
```

**What it does:** runs only the **lexer** and prints the token stream, then
stops. Output columns are: token index, `line:column`, token kind, token text.

**What you should see (first lines):**

```
  0    7:1    'int'                   'int'
  1    7:5    identifier              'square'
  2    7:11   '('
  3    7:12   'int'                   'int'
  4    7:16   identifier              'x'
```

**What to say:** "the lexer reports position for every token, which is why every
later error can point at a column instead of a line."

```powershell
.\mycc.exe --dump-ast demos\desugar.c
```

**What it does:** runs lexer → parser → semantic analysis (so types and stack
slots exist), prints the tree, then stops before code generation.

**What you should see:**

```
PROGRAM  [1:1]
  FUNCTION main : int  (frame 16 bytes)  [14:1]
    [body]
      BLOCK  [14:16]
        LOCAL a : int  @ -8(%rbp)  [15:9]
        LOCAL b : int  @ -16(%rbp)  [16:9]
        ASSIGN  [17:5]
          [lhs]
            VAR a : int  [17:5]
          [rhs]
            INT 1  [17:9]
```

and further down, the addresses and the desugarings:

```
        ASSIGN  [18:5]
          [rhs]
            BINOP +            <-  a += 2  became  a + 2
        ASSIGN  [19:5]
          [rhs]
            POSTFIX ++         <-  b = a++  keeps the old value first
        IF  [20:5]
          [cond]
            BINOP ==           <-  if (!a)  became  if (a == 0)
              INT 0
              RETURN  [20:13]
                BINOP -        <-  return -b  became  0 - b
                  INT 0
```

**What to say:** "three things are already true at this point and you can see
all three: `a` lives at `-8(%rbp)`, the frame is 16 bytes, and `a += 2` no longer
exists — the parser rewrote it. Nothing in the back end has to know about `+=`."

**Why this matters in a viva:** the usual question is "where does the compiler
do X?". `--dump-ast` turns that into a demonstration instead of a description.

### 4.1 `int` — and the width decision (the question you *will* be asked)

```powershell
.\mycc.exe demos\simple.c -o build\simple.s
Select-String -Path build\simple.s -Pattern 'imulq|call|addq' 
```

**What you should see** (essence):

```
movq -8(%rbp), %rax        <- load x
movq %rax, %r10            <- park it
movq -8(%rbp), %rax        <- load x again
imulq %r10, %rax           <- x * x
call square                <- the call
addq $1, %rax              <- + 1
```

Note `movq` and `imulq` — **64-bit** instructions, not `movl`/`imull`.

**What to say:** "`int` is 64-bit in mycc. That is a written decision, not an
oversight: it is this compiler's own ABI. The consequences are documented — no
32-bit wrap-around, `sizeof` is not supported so nothing can observe the width,
and making it 32-bit would mean `movl`/`imull` plus sign-extension after every
operation. It is the first item in future work, not a hidden defect."

**If they push:** "gcc would use 32-bit `int`; my differential tests are
therefore width-agnostic by construction (no `sizeof`, no byte offsets, no
overflow), and the fuzzer enforces that mechanically."

### 4.2 `char` and the `unsigned` family

```powershell
.\mycc.exe tests\18_unsigned.c -o build\u.s
Select-String -Path build\u.s -Pattern 'divq|shrq|movzbq' | Select-Object -First 6
```

**What you should see:**

```
  24: divq %r10
  45: divq %r10
  50: movzbq %al, %rax
  64: divq %r10
```

**What it shows:** `unsigned` division uses **`divq`** (unsigned) where a signed
value would use `idivq`; `unsigned char` is loaded with **`movzbq`** (zero
extension) where `char` uses `movsbq` (sign extension); unsigned shifts use
`shrq` where signed use `sarq`; unsigned comparisons use
`setb/setbe/seta/setae`.

**What to say:** "the `unsigned` keyword is not cosmetic — it changes which
instruction the back end emits. Signed and unsigned division are different
opcodes on x86-64."

**Demo the difference visibly:**

```powershell
.\mycc.exe tests\18_unsigned.c -o build\u.s
gcc -o build\u.exe build\u.s
build\u.exe; $LASTEXITCODE
```

**Expected:** `0`. (Tests 01–25 are *self-checking*: they return 0 only if every
internal assertion passed, and return a **distinct non-zero code at the first
failure** — e.g. `tests\01` returns 8 for the literal-arithmetic check and 9 for
`a * (b - 1)`. So a failure tells you which assertion broke without a debugger.
That is why the harness only has to look at the exit code.)

### 4.3 Pointers

```powershell
.\mycc.exe tests\12_pointers.c -o build\p.s
gcc -o build\p.exe build\p.s
build\p.exe; $LASTEXITCODE
```

**Expected:** `0` (self-checking).

**Then show the arithmetic is *scaled*:**

```powershell
.\mycc.exe tests\13_arrays.c -o build\a.s
Select-String -Path build\a.s -Pattern 'imulq' | Select-Object -First 4
```

**What you should see:**

```
  28: imulq $8, %rax, %rax
  56: imulq $8, %rax, %rax
  67: imulq $8, %rax, %rax
```

**What to say:** "`p + 1` on an `int *` moves **8 bytes**, not 1, because `int`
is 8 wide here. That multiply is not in my source — the compiler inserted it when
it scaled the index. This is the single most common thing beginners get wrong
about pointers, and it is one instruction away from being visible."

**What the test actually checks** (`tests/12`, `13`): declaration, `&`, `*` read
and write, mutation through a pointer, `int **`, pointer arithmetic `p + n`,
`p - n`, `p - q`, `++p`, `p++`, `p += n`, array decay when passing to a function,
null pointer as `0`, and globals of pointer type.

### 4.4 Arrays, multi-dimensional arrays, and decay

```powershell
.\mycc.exe tests\17_multidim_arrays.c -o build\m2.s
gcc -o build\m2.exe build\m2.s
build\m2.exe; $LASTEXITCODE
```

**Expected:** `0`.

**What it covers:** `int m[3][4]` declared as a contiguous block, `m[i][j]`
computed as `i * 4 * 8 + j * 8`, a **row** `m[i]` decaying to `int *`, and the
row stride being visible in the assembly as a multiply.

**What to say:** "a 2-D array is one flat block; `m[i][j]` is
`base + (i * row_length + j) * element_size`. There is no array object at
run time, exactly like C."

**Known limit, say it before they find it:** array **bounds are checked at
declaration** (zero, negative, non-constant sizes are rejected) but **indexing
is not bounds-checked at run time** — that is C's own behaviour.

### 4.5 Structs — member access, `->`, whole-struct copy

```powershell
.\mycc.exe tests\22_struct_assign.c -o build\st.s
Select-String -Path build\st.s -Pattern 'rep movsb' 
```

**What you should see:**

```
  29: rep movsb
  85: rep movsb
```

**What to say:** "`b = a;` on structs is not a field-by-field copy in the
generated code — it is one `rep movsb` over the object's size. And **that line
is a bug fix**: my first version copied 8 bytes of a 16-byte struct, silently.
It is BUG-003 in the ledger and there is a regression test that fails on the old
code."

```powershell
.\mycc.exe tests\14_structs.c -o build\s14.s
gcc -o build\s14.exe build\s14.s
build\s14.exe; $LASTEXITCODE
.\mycc.exe tests\16_member_incdec.c -o build\s16.s
```

**What these cover:** `struct` definition, `.` and `->`, struct pointers, arrays
of structs, self-referential types (`struct Node { struct Node *next; }`),
globals of struct type, and `p.x++` / `++p->y` (member `++`/`--` through both
access forms — test 16 exists because that path was broken).

**Refusal to demonstrate (deliberately):**

```powershell
.\mycc.exe tests\errors\struct_param_by_value.c
```

**What you should see:** an error saying struct-by-value parameters are not
supported and pointing at the pointer idiom. **What to say:** "I chose to refuse
this rather than miscompile it. Refusing is a feature: the alternative is a
program that compiles and prints garbage."

### 4.6 Functions, prototypes, recursion, globals

```powershell
.\mycc.exe example.c -o build\example.s
gcc -o build\example.exe build\example.s
build\example.exe; $LASTEXITCODE
```

**Expected:** `197`.

**What `example.c` exercises in one file** (this is your "everything at once"
program): two-level recursion (`fact`), a function calling another function
(`sum_primes_up_to` → `is_prime`), `for` loops, `if`/`else`, `%`, `&&`, `||`,
`!`, `++`, `+=`, a **global** (`g_calls`), local variables and block scoping.

**Show the call:**

```powershell
Select-String -Path build\example.s -Pattern 'call fact|call is_prime' | Select-Object -First 4
```

**Say this:** "that `call fact` is the recursion. Every argument goes through the
platform ABI — not a private convention — which is why the same assembly links
against the real C library."

**Prototypes / forward declarations:**

```powershell
.\mycc.exe tests\04_functions.c -o build\f4.s
gcc -o build\f4.exe build\f4.s; build\f4.exe; $LASTEXITCODE
```

**What it covers:** declaration before use, prototypes (`int f(int);`),
parameters, recursion, globals.

### 4.7 Statements — control flow, all of it

```powershell
.\mycc.exe tests\06_control_flow.c -o build\c6.s
gcc -o build\c6.exe build\c6.s; build\c6.exe; $LASTEXITCODE
```

**Covers:** `do`/`while`, `switch`/`case`/`default` **with fallthrough**,
`break`, `continue`, and `while` + `for`.

**Show the switch lowering:**

```powershell
Select-String -Path build\c6.s -Pattern 'cmpq' | Select-Object -First 6
```

**What to say:** "there is no jump table: `switch` becomes a compare-and-branch
chain, `case` values are compile-time constants, and fallthrough is simply the
absence of a jump. That is why the fallthrough test exists — it is easy to emit
an unconditional jump by accident and break legal C."

Then the rest of the statement set:

```powershell
.\mycc.exe tests\15_nested_switch.c -o build\c15.s    # case labels inside blocks/if-bodies
.\mycc.exe tests\03_loops.c -o build\c3.s             # while, accumulators, early return
.\mycc.exe tests\11_scope.c -o build\c11.s            # loop-local declarations, shadowing
.\mycc.exe demos\loop.c -o build\loop.s
gcc -o build\loop.exe build\loop.s; build\loop.exe; $LASTEXITCODE
```

**Expected for `demos\loop.c`:** `55` (1+2+…+10). A countable answer beats a
`0` on stage — use `demos\loop.c`, not `tests\03`, when you want a number.

**What to say about scoping:** "`for (int i = 0; …)` declares `i` in the loop's
own scope — two consecutive loops can each have an `i`, and the second one does
not see the first."

### 4.8 Expressions — the full precedence ladder

Run these three; they are self-checking and each covers a different cluster:

```powershell
.\mycc.exe tests\01_arithmetic.c -o build\e1.s   # + - * / % precedence, unary minus
.\mycc.exe tests\05_advanced.c   -o build\e5.s   # % && || ! for, compound assignment
.\mycc.exe tests\07_bitwise.c    -o build\e7.s   # & | ^ ~ << >> and their precedence
.\mycc.exe tests\10_ternary.c    -o build\e10.s  # ?: nesting, right-associativity
.\mycc.exe tests\19_comma_operator.c -o build\e19.s  # a = (b, c), for (;;i++, j--)
.\mycc.exe tests\20_casts.c      -o build\e20.s  # (int)x, (char*)p, struct-pointer casts
```

For each: `gcc -o build\X.exe build\X.s; build\X.exe; $LASTEXITCODE` → `0`.

**Short-circuiting — the one worth demonstrating live**, because it is
observable:

```powershell
.\mycc.exe tests\05_advanced.c -o build\e5.s
Select-String -Path build\e5.s -Pattern 'jne|je ' | Select-Object -First 6
```

**What to say:** "`&&` and `||` are compiled as **branches**, not as arithmetic.
`0 && f()` never calls `f` — the jump skips the call entirely. That is C's
guarantee, not an optimization."

**The lvalue-once guarantee (a killer demo, because it is a bug story):**

```powershell
.\mycc.exe tests\23_lvalue_single_eval.c -o build\e23.s
gcc -o build\e23.exe build\e23.s; build\e23.exe; $LASTEXITCODE
```

**Expected:** `0`. **What to say:** "`a[i++] += 5` must compute the address
**once** and increment `i` **once**. My first compiler evaluated the lvalue
twice: `i` went up by two and the wrong slot was written. It returned a number,
so the suite was green. This test is the shape of the bug, and `tests\diff\01`
catches the same class against gcc."

### 4.9 Literals and strings

```powershell
.\mycc.exe tests\08_literals.c -o build\lit.s
gcc -o build\lit.exe build\lit.s; build\lit.exe; $LASTEXITCODE
```

**Covers:** decimal, **hex** (`0x`), **octal** (leading `0`), `u`/`l` suffixes,
and character constants (`'A'`, `'\n'`, `'\x41'`, `'\101'`).

**Escapes and real output:**

```powershell
.\mycc.exe tests\29_string_escapes.c -o build\esc.s
gcc -o build\esc.exe build\esc.s; build\esc.exe
```

**Expected output:** `ESCAPES-OK` (then exit code `0`).

**Embedded NUL — the subtle one:**

```powershell
.\mycc.exe tests\30_nul_in_string.c -o build\nul.s
Select-String -Path build\nul.s -Pattern '\.string' | Select-Object -First 3
gcc -o build\nul.exe build\nul.s; build\nul.exe; $LASTEXITCODE
```

**What you should see:** `.string "\000A"` and exit code `65`.

**What to say:** "a C string literal can contain `\0` in the middle. My first
version stored literals with `strlen`, so `"\0A"` became `""` and the byte after
the NUL vanished — BUG-023. The lexer now keeps an explicit length, so the
emitter never has to guess."

**Malformed literals are refused (not silently zeroed):**

```powershell
.\mycc.exe tests\errors\octal_digit_8.c
.\mycc.exe tests\errors\malformed_hex.c
```

**What to say:** "`08` is not a number in C — it is an invalid octal literal.
Saying so is cheaper than printing a wrong answer."

**UTF-8 BOM:**

```powershell
.\mycc.exe tests\24_bom.c -o build\bom.s
gcc -o build\bom.exe build\bom.s; build\bom.exe; $LASTEXITCODE
```

**Expected:** `42`. **What to say:** "Windows editors put an invisible byte at the
top of the file. Before I fixed it (BUG-011), the very first error a new user
saw was about a character they could not see."

### 4.10 Output, input, and the real ABI

**Built-ins, no declarations, no `#include`:**

```powershell
.\mycc.exe hello.c -o build\hello.s
gcc -o build\hello.exe build\hello.s
build\hello.exe
```

**What you should see:**

```
Hello, world!
120
3 + 4 = 7, and 2 * 21 = 42
```

**What to say:** "`print_str` and `print_int` are **compiler built-ins**.
Semantic analysis rewrites them into `printf("%s", …)` and `printf("%lld\n", …)`
— you can see the rewrite in `--dump-ast`, so there is no magic hidden in the
back end."

```powershell
.\mycc.exe --dump-ast hello.c | Select-String -Pattern 'CALL'
```

**What you should see:**

```
CALL fact  [18:16]
CALL printf  [22:5]
CALL printf  [24:5]
CALL fact  [24:15]
CALL printf  [28:5]
```

`hello.c` never mentions `printf` once — those `CALL printf` nodes were created
by semantic analysis. Say exactly that; it is the clearest single proof that
"built-in" is not a different code path, it is a rewrite into ordinary calls.

```powershell
.\mycc.exe --dump-ast hello.c | Select-Object -First 20
```

**Real `printf` and real `scanf`:**

```powershell
.\mycc.exe tests\28_extern_printf.c -o build\ep.s
gcc -o build\ep.exe build\ep.s; build\ep.exe
```

**Expected:** `7 14 21 28`.

```powershell
.\mycc.exe demo\2_name.c -o build\name.s
gcc -o build\name.exe build\name.s
build\name.exe
```

Type a name when prompted. **This is the interactive moment** — a `char name[64]`
local, array decay to `char *`, `extern scanf`, and three string literals in
`.rodata`, all compiled by you.

**Stack arguments (7 parameters) — the ABI proof:**

```powershell
.\mycc.exe tests\diff\02_stack_args.c -o build\sa.s
Select-String -Path build\sa.s -Pattern '48\(%rbp\)|16\(%rbp\)'
```

**What you should see:** `movq 48(%rbp), %rax`.

**What to say:** "Windows x64 passes the first four arguments in
`rcx/rdx/r8/r9`, the fifth and later on the stack — after the mandatory 32-byte
shadow space, which is why the fifth argument is at `48(%rbp)` and not `16`.
Linux passes six in registers, so the same source gets a different offset. Both
are tested: `tests\diff\02` is compiled by mycc **and** by gcc and both must
return 0."

**Shadow space, seen in the emitted code:**

```powershell
.\mycc.exe --target=windows hello.c -o build\win.s
(Select-String -Path build\win.s -Pattern 'subq \$32').Count
```

**Expected:** `5` — one `subq $32` per external call in `hello.c`. **Say:**
"Windows requires the caller to reserve 32 bytes the callee may spill into.
Linux does not — that is why the same source produces different assembly for two
targets that run on the same CPU."

```powershell
.\mycc.exe --target=linux hello.c -o build\lin.s
Select-String -Path build\lin.s -Pattern 'PLT|note.GNU'
```

**What you should see:**

```
call printf@PLT
.section .note.GNU-stack,"",@progbits
```

**Say:** "`@PLT` is the ELF indirection for an external call; macOS and Windows
do not have it. `.note.GNU-stack` marks the stack non-executable — a Linux
linker warns without it. Both are target details, not language details, which is
exactly why the target is a switch and not a fork in the code."

```powershell
.\mycc.exe --target=darwin hello.c -o build\mac.s
Select-String -Path build\mac.s -Pattern '^_' | Select-Object -First 3
```

**What you should see:** `_fact:`, `_main:`, `call _fact` — every symbol gets a
leading underscore, which is what Mach-O expects.

**A wrong target is refused, not guessed:**

```powershell
.\mycc.exe --target=beos hello.c -o build\x.s
```

→ `error: unknown target 'beos' (use linux, windows or darwin)` plus the usage
block. **Say:** "it tells you the valid set instead of emitting something that
almost links."

### 4.11 The optimizer — four pieces, each with a before/after

First, the headline measurement (re-run it, do not recite it):

```powershell
$opt = 0; $plain = 0
foreach ($t in Get-ChildItem tests\*.c) {
  .\mycc.exe            $t.FullName -o build\o.s 2>$null; $opt   += (Get-Content build\o.s | Measure-Object -Line).Lines
  .\mycc.exe --no-opt   $t.FullName -o build\n.s 2>$null; $plain += (Get-Content build\n.s | Measure-Object -Line).Lines
}
"optimized: $opt   no-opt: $plain   reduction: " + [math]::Round(100.0*($plain-$opt)/$plain,1) + "%"
```

**What you should see:** `optimized: 9872   no-opt: 14290   reduction: 30.9%`
(within a few lines of that; re-run the loop rather than reciting it).

**What to say:** "same 48 programs, same behaviour, roughly one third fewer
assembly lines. And 'same behaviour' is not a claim — every one of those programs
is run both ways and must exit identically with byte-identical stdout."

**Piece 1 — constant folding:**

```powershell
.\mycc.exe tests\31_constant_folding.c -o build\cf_o.s
.\mycc.exe --no-opt tests\31_constant_folding.c -o build\cf_n.s
"opt: " + (Get-Content build\cf_o.s | Measure-Object -Line).Lines + "  no-opt: " + (Get-Content build\cf_n.s | Measure-Object -Line).Lines
```

**Measured:** `opt: 198  no-opt: 712` — a 3.6× difference, the biggest single win.

```powershell
.\mycc.exe hello.c -o build\h.s
Select-String -Path build\h.s -Pattern 'movq \$42'
```

**Say:** "`6 * 7` in the source became `movq $42, %rax`. The multiplication does
not exist at run time because it did not survive compile time."

**Piece 2 — constant-condition elimination:**

```powershell
.\mycc.exe tests\32_dead_branches.c -o build\db_o.s
.\mycc.exe --no-opt tests\32_dead_branches.c -o build\db_n.s
"opt: " + (Get-Content build\db_o.s | Measure-Object -Line).Lines + "  no-opt: " + (Get-Content build\db_n.s | Measure-Object -Line).Lines
```

**Measured:** `opt: 227  no-opt: 383`.

**Say:** "`if (0)` and `while (0)` bodies are type-checked — an error inside them
is still an error — but never emitted. The dead branch is not optimized away at
run time; it never reaches the assembler."

**Piece 3 — immediate operands (the visible one):**

```powershell
.\mycc.exe tests\33_constant_operands.c -o build\ci.s
Select-String -Path build\ci.s -Pattern 'addq \$|cmpq \$' | Select-Object -First 4
```

**What you should see:** `cmpq $6, %rax`, `addq $1, %rax`.

**Say:** "`i + 1` does not need a register for the 1: the constant is encoded
inside the instruction. That is three instructions saved per loop iteration —
which is why the loop-heavy tests shrink the most."

**Piece 4 — dead local stores:**

```powershell
.\mycc.exe tests\35_dead_locals.c -o build\dl.s
Select-String -Path build\dl.s -Pattern 'movq \$1000'
```

**What you should see:** nothing (no match). Then run `--no-opt` on the same file
and the store appears. **Say:** "a local that is written and never read, and
whose address was never taken, does not get a store. **That 'address never
taken' clause is a bug fix (BUG-025)** — the first version deleted the store for
a variable read through `&b`, and the program returned garbage."

**Bonus piece — expression temporaries in a register:**

```powershell
.\mycc.exe tests\36_registers.c -o build\r.s
Select-String -Path build\r.s -Pattern '%r10' | Select-Object -First 4
gcc -o build\r.exe build\r.s; build\r.exe; $LASTEXITCODE
```

**Expected exit:** `42`. **Say:** "the left operand of a binary expression parks
in `%r10` instead of being pushed and popped. Nested `/` and `%` spill it,
because those instructions clobber registers — which is exactly the bug BUG-026
was."

**What was NOT done, and say it plainly:** "there is no global register
allocator. Copy propagation was implemented, measured and removed because on a
stack back end it saves nothing. A liveness-based allocator needs an IR that does
not exist yet, so it is future work rather than a half-finished attempt."

### 4.12 Diagnostics — the 30 seconds that impress non-technical people

**Syntax error with a caret:**

```powershell
Set-Content -Path build\typo.c -Value "int main(void) {`n    int y = ;`n    return 0;`n}"
.\mycc.exe build\typo.c -o build\typo.s
```

**What you should see (real output):**

```
build\typo.c:2:13: error: expected expression (found ';')
     2 |     int y = ;
       |             ^
build\typo.c: compilation aborted during parsing
```

**Then prove the second half of the promise:**

```powershell
Test-Path build\typo.s
```

→ `False`. **Say:** "a failed compile leaves **no** output file. The assembly
goes to a temporary file and is renamed only on success — otherwise the next
build silently links the half-written `.s` from the last mistake (BUG-017)."

**Semantic errors — several, each pointing at its own column:**

```powershell
.\mycc.exe demos\broken.c
```

**What you should see:**

```
demos\broken.c:15:9: error: undeclared variable 'nope'
    15 |     x = nope;
       |         ^
demos\broken.c:16:12: error: function 'add' expects 2 argument(s), got 3
    16 |     return add(1, 2, 3);
       |            ^
demos\broken.c: compilation aborted during semantic analysis
```

**Say:** "two independent mistakes, two carets, and **no cascade**. The parser
stops at the first syntax error because everything after it would be invented;
the semantic pass can keep going because its checks are independent."

**Warnings never break the build:**

```powershell
.\mycc.exe tests\25_fall_off_end.c -o build\w.s
$LASTEXITCODE
.\mycc.exe -w tests\25_fall_off_end.c -o build\w.s
$LASTEXITCODE
```

**What you should see:**

```
tests\25_fall_off_end.c:8:1: warning: control reaches end of non-void function 'maybe'
     8 | int maybe(int a) {
       | ^
```

then exit code `0` both times, and the second run prints nothing at all.

**Say:** "falling off the end of a non-void function is undefined behaviour in C,
so I warn — but a warning that fails a build gets disabled, so it never changes
the exit status, and `-w` silences the channel without touching errors."

**The refusals — pick two, never all:**

```powershell
.\mycc.exe tests\errors\preprocessor_directive.c
.\mycc.exe tests\errors\struct_param_by_value.c
.\mycc.exe tests\errors\nesting_statements.c
.\mycc.exe tests\errors\array_assign.c
```

Each prints one precise sentence (e.g. *"preprocessor directives are not
supported"*, *"nesting too deep"*, *"cannot assign to an array"*).

**Say:** "these are not crashes and they are not silence. The compiler's entire
design principle is: **refuse loudly rather than emit best-effort code**. Fifteen
of my 48 logged defects are P0 — the class this prevents: programs that compiled
and printed the wrong number, or destroyed their own source."

**Nesting depth (the anti-crash demo):**

```powershell
.\mycc.exe tests\errors\nesting_parens.c
```

**Say:** "256 levels of nesting, then a clean error. A compiler that segfaults on
input is a compiler nobody can put in front of a user."

### 4.13 The second front end (`--lang=myc`) — the 10-second proof of architecture

```powershell
.\mycc.exe --lang=myc demo\4_pair.myc -o build\pair_myc.s
.\mycc.exe            demo\4_pair.c   -o build\pair_c.s
fc.exe /b build\pair_myc.s build\pair_c.s
```

**What you should see:**

```
Comparing files BUILD\pair_myc.s and BUILD\PAIR_C.S
FC: no differences encountered
```

**Then run it (either file — they are the same bytes):**

```powershell
gcc -o build\pair.exe build\pair_c.s
build\pair.exe
$LASTEXITCODE
```

**What you should see:**

```
hello from
myc
30
correct
```

exit code `30`.

**What to say — this is the single best 20 seconds in the whole demo:**
"These are two different languages. `4_pair.myc` is never translated into C.
Both front ends build the *same* AST, and one code generator turns it into
assembly. The proof is that the output is byte-identical — `fc` just compared
them. If any front-end knowledge had leaked into the back end, that check would
fail."

**And the bug story that proves the check earns its keep:**

```powershell
.\mycc.exe --lang=myc tests\myc\else.myc -o build\e1.s
.\mycc.exe tests\myc\else.c -o build\e2.s
fc.exe /b build\e1.s build\e2.s
gcc -o build\e.exe build\e2.s; build\e.exe; $LASTEXITCODE
```

**Expected:** identical assembly; the program prints `else-branch` and `8`, and
exits `8`. **Say:** "the toy language's `else` never parsed — the parser only
accepted the identifier spelling, the lexer produced the keyword token
(BUG-027). The equivalence check is what caught it, because the C twin took the
branch and the `.myc` file did not."

```powershell
.\mycc.exe --lang=myc tests\myc\literals.myc -o build\l1.s
.\mycc.exe tests\myc\literals.c -o build\l2.s
fc.exe /b build\l1.s build\l2.s
```

**Expected:** identical assembly; the program prints `literals-ok` and exits
`42`. **Say:** "`0x10` is 16, `010` is 8, `0xFF` is 255, `0777` is 511 — in a
language that is not C, because the lexer is shared and BUG-029 was that the toy
parser read the literal *text* as base 10, so `say 0x10` printed 0. One shared
hand-off, `util.c:parse_integer_literal()`, is now the only place literals are
interpreted."

**The language in one screen:**

```powershell
Get-Content demo\4_pair.myc
```

`fn` / `end`, `let`, `say`, `if`/`else`, `while`, `return`, `and`, `or`, `not`,
no semicolons, blocks closed by `end`. Numbers and strings only; every parameter
is an `int`.

### 4.14 The proof machinery — what makes all of the above believable

**The suite (106 checks):**

```powershell
.\test.bat
```

**What you should see at the end (real output):**

```
PASS: tests\errors\zero_length_array.c (rejected)
PASS: failed compile leaves no output file
PASS: -w silences warnings without failing the build
PASS: --sign embeds the provenance string, and only on request
PASS: --lang=myc matches its C twin: tests\myc\else
PASS: --lang=myc matches its C twin: tests\myc\literals

All tests passed
```

**What it is doing:** 48 positive programs (exit code **and** stdout asserted),
52 rejection programs (compile must fail with the expected message), and 6
contract assertions. It also compiles **every** positive test twice — optimized
and `--no-opt` — and requires identical behaviour. That parity loop is the reason
"the optimizer is safe" is a check and not a belief.

**Differential test against gcc:**

If the first `gcc` on `PATH` is not the 64-bit w64devkit build (MinGW.org 6.3
is 32-bit and cannot assemble `%rax`), set the compiler first:

```powershell
$env:CC='C:\w64devkit\bin\gcc.exe'
.\difftest.bat
```

**What you should see:**

```
PASS: tests\diff\01_compound_and_lvalue.c (both returned 0)
PASS: tests\diff\02_stack_args.c (both returned 0)
PASS: tests\diff\03_folding.c (both returned 0)

All differential tests passed
```

**Say:** "my suite can only catch bugs I predicted. This compile-the-same-file-
with-gcc-and-compare check is different in kind — it is what found BUG-020,
`a[i] += 1` scaling the index twice, because gcc returned a different answer."

**Fuzzer — programs nobody wrote:**

Use the same `CC` setting if your default `gcc` is 32-bit:

```powershell
$env:CC='C:\w64devkit\bin\gcc.exe'
.\fuzz.bat 300 1
```

**What you should see:**

```
fuzzgen: wrote 300 program(s) to build\fuzz (seed 1)

PASS: all 300 programs agreed with the reference compiler
```

**Say:** "300 programs today, 1,000 per CI run. Each one is generated from a
skeleton with random expressions, literals and loop bounds, then compiled by mycc
and by gcc and required to return the same exit code. Deterministic in
`(seed, index)`, so a failure is a number you can reproduce."

**If asked why it does not find more:** "it explores expression shapes, not
program structures — no pointers, arrays, structs, strings or `switch`. It cannot
find a bug in code it never emits. That is why it complements the suite instead
of replacing it, and why the 10,000-program three-way criterion on my roadmap is
explicitly *not claimed*."

**The ledger:**

```powershell
Get-Content docs\report.md -TotalCount 20
```

**Say:** "48 bugs, every one with a reproduction and an observed-vs-expected
pair; 47 are fixed and one is openly listed as open (`BUG-048`). The original 31
entries have a fix commit and a regression test; 16 of the new fixes are in the
working tree and marked `pending` because I was asked not to commit. The evidence
strength is stated per entry — the original ledger had 29 'verified by
execution' and 2 'code inspection only'; the newer batch includes an
assembly-only check (`BUG-043`) and a manual probe for the open entry. Nothing
was deleted, including my own wrong tallies."

**The single best story in the ledger:**

> "My README claimed CI was green on three platforms. It had never been green on
> Windows or macOS. Windows failed because `cmp` was missing — and the failure
> *looked* like a codegen bug, so it sat there. macOS failed because the runner
> is Apple Silicon and my backend emits x86-64, so clang read my assembly as
> arm64. Two lines of fix, two days of not knowing. The first green matrix in the
> project's history is run `40086581248`."

That story does more for your credibility than any feature.

---

## 5. Command reference — every flag, what it does, what you see

| Command | What it does | What you see |
|---|---|---|
| `mycc.exe prog.c` | **prints the assembly to stdout** (nothing written; good for `\| more`) | ~80 lines of `.text` |
| `mycc.exe prog.c -o out.s` | writes the assembly to a file — this is the form every other command in this file uses | file appears, terminal silent |
| `mycc.exe --dump-tokens prog.c` | lex only, print tokens, **stop** | index, `line:col`, kind, text |
| `mycc.exe --dump-ast prog.c` | parse + check, print tree, **stop** | `PROGRAM` / `FUNCTION` / `LOCAL … @ -8(%rbp)` |
| `mycc.exe --no-opt prog.c` | disable all four optimizations | larger `.s`, identical behaviour |
| `mycc.exe --target=linux prog.c` | ELF flavour: `@PLT`, `.note.GNU-stack` | different call spelling |
| `mycc.exe --target=windows prog.c` | PE flavour: shadow space, no `@PLT` | `subq $32` before calls |
| `mycc.exe --target=darwin prog.c` | Mach-O flavour: leading `_` on symbols | `_main:`, `call _fact` |
| `mycc.exe --lang=myc prog.myc` | use the toy front end | same AST, same back end |
| `mycc.exe --sign prog.c` | embed a provenance string in `.rodata` | `.string "generated by mycc (C-subset compiler) + github.com/…"` |
| `mycc.exe --version` | print version, targets, optimizer state | three lines |
| `mycc.exe -w prog.c` | suppress warnings (errors unaffected) | silence; exit code unchanged |
| `mycc.exe` (no args) | usage text | flags + one line each, exit 1 |
| `mycc.exe --target=beos prog.c` | invalid target | `error: unknown target 'beos' …` + usage |
| `gcc -o prog.exe prog.s` | **assemble + link**, never compiles C | an executable |
| `fc.exe /b a.s b.s` | byte-compare two assembly files | `FC: no differences encountered` |
| `Select-String -Path x.s -Pattern 'imulq'` | grep the assembly | matching lines with numbers |
| `$LASTEXITCODE` (PowerShell) | the program's exit code | `197`, `42`, `0`… |

**Exit-code reality check:** `main` returning 197 → process exit 197 → visible.
255 is the maximum; values wrap. Do not build a demo on `return 1000;`.

**Reading the assembly you generate:** `movq` = 64-bit move, `%rax` = return
value / accumulator, `%rbp` = frame base, `-8(%rbp)` = a local, `%r10` = scratch
for the left operand of a binary expression, `leaq` = load address (used for
`&x`, arrays, and `%rip`-relative string addresses), `sete`/`setne`/`setl` =
materialize a comparison as 0/1, `idivq`/`divq` = signed/unsigned division,
`rep movsb` = struct copy, `call`/`ret`/`leave` = the call and the frame
teardown.

---

## 6. The batch files, one by one

| File | Audience | What it does | Use it? |
|---|---|---|---|
| `build.bat` | you | compiles `src\*.c` → `mycc.exe`, globs the file list | **yes, always first** |
| `test.bat` | technical | 106 checks; also the opt/no-opt parity loop | yes, as the credibility close |
| `difftest.bat` | technical | mycc vs gcc on `tests\diff\*` | yes |
| `fuzz.bat N SEED` | technical | generates N programs, compares vs gcc | yes, `fuzz.bat 300 1` |
| `demo.bat` | non-technical | six narrated scenes, interactive name prompt | yes, for the 5-minute slot |
| `showcase.bat` | either | compiles `example.c` → runs → 197, then `hello.c` → real output | yes, it is short and safe |
| `tour.bat` | non-technical | 7 paused steps, source → assembly → gcc → run → suite | ⚠ **stale, see below** |
| `tour2.bat` | technical | 6 paused steps through lexer/parser/semantics/codegen | ⚠ **stale, see below** |
| `tour.sh`, `tour2.sh` | Linux/WSL | same as above | ⚠ **stale** |
| `demo\expected.txt` | fallback | captured transcript | only if the projector dies |

### 6.1 `showcase.bat` — the safe end-to-end

```powershell
.\showcase.bat
```

Five steps: compile `example.c` → assemble+link → run (expects **197**) →
compile `hello.c` → run (real text output). It auto-detects MSYS2/w64devkit if
`gcc` is not on PATH, and it exits non-zero on the first failure, so it is a
one-command "does this machine work" check as well as a demo.

### 6.2 ⚠ `tour.bat` / `tour2.bat` / `tour.sh` / `tour2.sh` — do not present these as-is

They are good *scripts* (paused, narrated, well paced) but their facts predate
Phases 4–9, and one claim is now the opposite of the truth:

| Line in the script | Says | Reality |
|---|---|---|
| `tour.bat:33,101` | "about 3,500 lines" | 9,502 lines (`src/` + `include/`) |
| `tour.bat:168-169` | "src: 2,130 lines, include: 225" | 9,055 / 447 |
| `tour.bat:123` | "mycc — **never optimizes**" | it has four optimizations; this is the project's headline result |
| `tour.bat:158` | "five independent programs" | 48 programs + 52 rejections = 106 checks |
| `tour2.bat:191` | "about 3,500 lines of C99" | 9,502 |

Reading "never optimizes" out loud while your slides say "30.9% smaller" (the
current 48-test measurement) is the
one way this project can embarrass itself. Either (a) do not run them, or
(b) read §4 instead, or (c) fix the numbers — they are five edits. Ask and I will
patch all four files.

---

## 7. The eight moments that land (rehearse these, in this order)

| # | Moment | Command | The line to say |
|---|---|---|---|
| 1 | a number comes out of nowhere | `demos\loop.c` → `55` | "the loop I wrote became machine code; the 55 is the exit code" |
| 2 | the caret | `build\typo.c` | "it points at the column, not the line" |
| 3 | folding | `Select-String build\h.s -Pattern 'movq \$42'` | "6 × 7 never happened at run time" |
| 4 | pointer scaling | `imulq $8` | "`+1` on an `int *` is 8 bytes, and the multiply is the compiler's" |
| 5 | struct copy | `rep movsb` | "one instruction, and it is a bug fix" |
| 6 | two languages, one back end | `fc.exe /b` | "byte-identical, and here is the check that caught a bug in it" |
| 7 | the ABI | `movq 48(%rbp)` | "5th argument on Windows lives past the shadow space" |
| 8 | the ledger | `report.md` | "48 bugs: 47 fixed and one openly listed open — including my own wrong README; 16 fixes are marked pending commit" |

If you only get three: **2, 6, 8**. They need no compiler knowledge at all.

---

## 8. Failure drill — if something breaks mid-demo

| Symptom | Cause | Fix in 5 seconds |
|---|---|---|
| `mycc.exe not found` | you are in the wrong directory | `cd "C:\Folder D\Feature_anant\C-Compiler-Mini-Project"` |
| `gcc: command not found` | PATH not set in this window | `$env:PATH="C:\msys64\usr\bin;C:\msys64\ucrt64\bin;"+$env:PATH` |
| `Format-Custom: A positional parameter…` | `fc` is a PowerShell alias | use `fc.exe /b` |
| build errors on `src\*.c` | stale `mycc.exe`/build dir | `.\build.bat` again; it rebuilds from scratch |
| a test suddenly fails | you edited a `tests\*.c` file | `git status`; `git checkout -- tests` |
| .exe returns 0 but should return N | you ran the `.s` with `gcc -S` instead of assembly, or ran the wrong `.exe` | re-check the two-step: `mycc` → `.s`, then `gcc` → `.exe` |
| everything looks fine but the exit code is 255 | a `return` with a negative value, or a crash | read the program, not the compiler |

**The recovery sentence, if you need one:** "That is a demo bug, not a compiler
bug — let me show you the same thing from the recorded run" and open
`demo\expected.txt`. Saying that once costs nothing; bluffing costs everything.

---

## 9. The questions you will actually get

**"Is this really a compiler or does gcc do the work?"**
The compiler emits assembly. gcc is handed that assembly and asked to assemble
and link it — it never sees C. Every stage before that (lex, parse, check,
optimize, emit) is in `src/`.

**"Why 64-bit int?"** Decision, documented, consequences listed, first item in
future work. (§4.1)

**"What about `double`, `sizeof`, `#include`?"** Refused with a message, on
purpose, because miscompiling is worse than refusing. (§4.12)

**"How do you know it's correct?"** Four independent layers: 106 assertions,
differential against gcc, 1,000 generated programs, and optimizer parity on
every test. (§4.14)

**"What is the hardest part?"** Calling conventions across three targets:
register/stack split, shadow space, `%al` for variadics, `rep movsb` clobbering
`%rsi`/`%rdi` on Windows, Mach-O symbol names. (§4.10)

**"What would you do differently?"** Two things: build the differential harness
on day one (it would have caught four of the ten silent-wrong-code bugs
immediately), and read the CI logs on day one instead of trusting the badge.
(§4.14)

**"How long did it take?"** Answer in phases, not hours: the pipeline first, then
the language surface, then the ABI, then the optimizer, then the proof
machinery. The ledger has the dates.

**"Show me a bug."** Open `report.md`, pick BUG-020 (`a[i] += 1` scaling the
index twice) — it has a one-line reproduction you can paste and run.

---

## 10. Say / never say

**Always say:** subset · decision · refused · checked · documented · fails-before.

**Never say:** "complete C" · "as good as gcc" · "bug-free" · "it just works" ·
"trust me".

**The sentence that frames the whole project:**

> "I built a compiler, and then I built the machinery that proves it is correct —
> and then I found out my own README was lying about CI, and fixed that too."

---

## 11. One-page cheat sheet (print this)

```
BUILD      .\build.bat
PATH       $env:PATH="C:\msys64\usr\bin;C:\msys64\ucrt64\bin;"+$env:PATH
COMPILE    .\mycc.exe prog.c -o build\prog.s
LINK       gcc -o build\prog.exe build\prog.s
RUN        build\prog.exe ; $LASTEXITCODE

NUMBERS    9,502 lines · 106 checks · 1,000 fuzz (700 local) · 48 bugs (47 fixed, 1 open) · 30.9% smaller (48-test measurement)
CI         first green 40086581248 (2026-10-24) · Linux 44s · macOS 1m4s · Win 2m49s

3 MOMENTS  caret error · fc.exe identical .s · report.md
3 COMMANDS .\demo.bat · .\test.bat · .\fuzz.bat 300 1
3 WORDS    subset · refused · checked
```
