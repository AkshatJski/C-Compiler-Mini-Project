# mycc — A C-Subset Compiler for x86-64

A small but complete compiler written in **plain C99** (no Lex, Yacc, or LLVM).
`mycc` reads a C-subset source file and produces **x86-64 AT&T assembly**
that assembles and runs on **Linux** (ELF, System V) and **Windows**
(PE/COFF, MinGW-w64) with ordinary gcc. A third flavour, `--target=darwin`,
emits Mach-O symbol names for macOS.

```
source.c ──> mycc ──> source.s ──> gcc ──> executable ──> output + exit code
```

## What's inside

- Hand-written lexer, recursive-descent parser, scoped symbol table,
  semantic analysis, an AST optimizer, and a code generator (~8,800 lines of C99 across `src/`
  and `include/`).
- **Types:** `int` (64-bit), `char` (signed, one byte), their `unsigned`
  counterparts (`unsigned int`, `unsigned char`, and bare `unsigned`), plus
  derived **pointers**, **arrays** (any number of dimensions), and **structs**.
- **Pointers & arrays:** `int *p` / `int **pp`, address-of `&`, dereference `*`
  (read and write), pointer arithmetic scaled by the element size
  (`p + n`, `p - n`, `p - q`, `++p`, `p++`, `p += n`), arrays (`int a[10]`,
  `int m[3][4]`) with indexing `a[i]` / `m[i][j]`, and array-to-pointer decay
  when arrays are passed to functions. `0` acts as the null pointer.
  Global pointers (`int *gp;`) and pointer-returning functions
  (`struct Node *make(void)`) work too. Array bounds are checked at
  **declaration** time (non-constant, zero, or over-long dimension lists are
  rejected); indexing is **not** bounds-checked at run time, exactly as in C.
- **Structs:** `struct` definitions with `int`/`char`/pointer/struct members,
  member access with `.` and `->`, struct pointers and arrays of structs,
  self-referential types (`struct Node { struct Node *next; }`), and global
  structs/arrays (zero-initialized).
- **`unsigned`:** `unsigned`, `unsigned int` (64-bit) and `unsigned char`
  (one byte), plus `signed` as the explicit default. `unsigned` values use
  `divq` instead of `idivq`, `shrq` instead of `sarq`, and the unsigned
  `setb`/`setbe`/`seta`/`setae` comparisons; `unsigned char` is loaded with
  `movzbq` (zero extension) rather than `movsbq`. Narrow `unsigned char`
  arithmetic still promotes to a signed 64-bit value, which is exactly C's
  integer-promotion rule, and pointers compare as unsigned addresses.
- **Statements:** `if`/`else`, `while`, `do`/`while`, `for` (loop-local
  declarations), `switch`/`case`/`default` with fallthrough, `break`,
  `continue`, `return`, and nested blocks. Nesting (blocks and expressions) is
  capped at 256 levels, reported as a clean error rather than a crash.
- **Structs and assignment:** structs are copied whole (`b = a;` copies every
  member), but struct *by value* parameters and struct returns are rejected
  with a message pointing at the pointer-based idiom (`struct P *p`).
  Assignments and calls are type-checked: struct→scalar, `int`→pointer, and
  struct→`int`-parameter are errors, and a cast is the explicit opt-in.
- **Expressions:** the full C precedence ladder — arithmetic, `%`, relational
  and equality, `&&`/`||` (short-circuiting) and `!`, bitwise
  `&` `|` `^` `~`, shifts `<<` `>>`, the conditional `?:`, `++`/`--`
  (prefix and postfix), compound assignment
  (`+= -= *= /= %= &= |= ^= <<= >>=`), casts (`(int)x`, `(char *)p`,
  including struct-pointer casts), and the comma operator (`a = (b, c)`,
  `for (…; …; i++, j--)`). An lvalue with a side effect — `a[i++] += 5`,
  `*p++ = 1`, `a[i++]++` — is evaluated **exactly once**.
- **Programs:** multiple functions with recursion and **prototypes / forward
  declarations** (`int f(int);`), any number of parameters (arguments past the
  register slots are passed on the stack), `int`/`void` return
  types (including pointer returns such as `int *`), globals with
  constant-folded initializers, multi-declarator declarations, and block
  scoping. `main` must be declared `int main(void)`; a program without
  `main`, or with an undefined function still being called, is rejected
  during compilation instead of failing at link time.
- **Literals:** decimal, hexadecimal (`0x…`), and octal (`0…`) integers with
  optional `u`/`l` suffixes; character constants (`'A'`, `'\n'`, `'\x41'`);
  and string literals with the full C escape set (`\n \t \r \0 \a \b \f \v \\
  \' \" \xHH \ooo`). A literal string is emitted once into `.rodata` and shared
  by every use. Malformed literals are errors rather than silent zeroes: `08`
  and `09`, `0x` with no digits, and an unterminated character constant or
  string literal are all reported. A UTF-8 byte-order mark (which Windows
  editors add) is skipped.
- **Output and library calls:** programs can print. Four built-ins need no
  declaration and no `#include` — `print_int(x)` (decimal + newline),
  `print_char(c)`, `print_str(s)` and `print_endl()`. String literals
  (`"hello\n"`) are real `char *` values stored in `.rodata`. `extern int
  printf(char *fmt);` declares any C library function, and `printf`, `puts`
  and `putchar` are pre-registered so the common cases work directly.
- **Calling convention:** the real platform ABI, not a private one. System V
  passes 6 integer arguments in `rdi/rsi/rdx/rcx/r8/r9` and sets `%al` for
  variadic calls; Windows x64 passes 4 in `rcx/rdx/r8/r9` plus the mandatory
  32-byte shadow space. Anything beyond that goes on the stack. External calls
  go through `@PLT` on ELF; internal calls stay direct.
- **Diagnostics:** friendly errors with `file:line:col`, including lvalue
  checks, unknown struct members, scope/`main`/`break`/`continue`/`switch`
  validation, rejection of non-constant global initializers, assignment to an
  array, dereferencing a `void *`, incomplete/zero-sized objects, struct
  redefinition, and precise messages (one error, not a cascade) for
  constructs the subset does not have — `#include` and other preprocessor
  directives.
- **Warnings:** advisory diagnostics that never change the exit status
  (`<file>:<line>:<col>: warning: …`), e.g. `control reaches end of non-void
  function 'f'`. `-w` silences the warning channel without affecting errors.
- `--dump-tokens` / `--dump-ast` reveal the compiler's internals;
  `--target=linux|windows|darwin` picks the assembler flavour (defaulting to
  the host mycc was built for).
- **One optimizer switch:** `--no-opt` disables all of Phase 5 - constant
  folding, constant-condition elimination, dead local stores and expression
  temporaries in registers - emitting exactly what the pre-optimizer code
  generator emitted (that claim is checked: every test is built both ways and
  must agree on exit status and complete output). It exists so the two paths can
  be compared line for line (`mycc hello.c -o a.s` vs `mycc --no-opt hello.c -o
  b.s`) and so that an optimizer bug is always bisectable against a known-good
  path.
- **Carets, not line numbers.** Every error and warning quotes the offending
  line and points at the column:

  ```
  prog.c:3:5: error: expected ';' after declaration (found 'int')
       3 |     int y = ;
         |     ^
  ```

- `--version` prints what the compiler is and which targets it has; `--sign`
  embeds a provenance string in the program's data, so `strings prog.exe` shows
  which compiler built it (off by default).
- A failed compilation leaves **no** output file behind (the assembly is
  written to a temporary file and renamed on success).
- **Tests:** 35 positive programs (`tests/01-36`) and 41 rejection tests
  (`tests/errors/`), plus 5 contract assertions — 81 checks in total — covering
  pointers (`12`), arrays
  (`13`), structs (`14`), nested `switch` labels (`15`), member `++`/`--`
  (`16`), multi-dimensional arrays (`17`), `unsigned` types (`18`), the comma
  operator (`19`), casts (`20`), global pointers (`21`), struct assignment
  (`22`), single evaluation of lvalues (`23`), the BOM (`24`), the
  fall-off-the-end warning (`25`), hello world (`26`), the print built-ins
  (`27`), `extern printf` (`28`), string escapes (`29`), NUL bytes inside
  a literal (`30`), constant folding (`31`), constant-condition
  elimination (`32`), constant operands (`33`), dead local stores (`35`) and
  temporaries in registers (`36`). Positive tests assert the program's real
  **output** (`// EXPECT-OUT:`), not just its exit code, and each of them is
  also built with `--no-opt` and required to print the same bytes, so the
  optimizer is checked for changing behaviour on every test.
  `tests/diff/` additionally holds programs that are compiled by **both**
  mycc and gcc and required to return the same exit code (`difftest.bat` /
  `make difftest`), and `tools/fuzz.c` generates programs nobody wrote for the
  same check (`fuzz.bat` / `make fuzz`).

Still out of scope (by design): `float`/`double`, `long`/`short`, `enum`/
`typedef`, aggregate initializer lists (`int a[2] = {1,2}`), `sizeof`, and the
preprocessor. Structs are never passed or returned **by value** — take a
pointer (`struct P *p`) instead, and the compiler says so. `int` is 64-bit
**by decision**, not by oversight (2026-10-24): it is this compiler's own ABI,
so `unsigned int` is 64-bit too, `sizeof(int)` is 8, and an expression that
overflows a 32-bit `int` in C simply does not overflow here. Making `int` 32
bits is a real change to the data model (`movl`/`imull` + sign extension after
every operation) and is listed as future work rather than pretended away. `main`
must still be `int main(void)`; `argv` is not available to mycc programs yet.
The optimizer has four pieces, and they are deliberately the ones that can be
argued for in a paragraph: constant folding, constant-condition elimination,
dead local stores, and expression temporaries kept in a register. There is still
no *global* register allocator - a value only lives in a register inside the
expression that computes it - and that is a documented tradeoff rather than an
oversight: copy propagation was measured as well and rejected (on a stack back
end it saves nothing), and a liveness-based allocator needs an IR that does not
exist yet.

## A second language (`--lang=myc`)

`mycc` has two front ends and one back end. The second reads **myc**, a small
language with its own grammar, and lowers it to the *same* AST the C front end
builds:

```
fn twice(n)
  return n * 2
end

fn main() -> int
  let total = 0
  let i = 1
  let name = "myc"

  while i <= 5
    total = total + twice(i)
    i = i + 1
  end

  say "hello from "
  say name
  say total

  if total == 30 and not 0
    say "correct"
  end

  return total
end
```

```
mycc --lang=myc demo\4_pair.myc -o build\pair_myc.s
mycc            demo\4_pair.c   -o build\pair_c.s
fc /b build\pair_myc.s build\pair_c.s      # FC: no differences encountered
```

That is not a translation step: `demo\4_pair.myc` is never turned into C. Both
front ends produce the same tree (a `while` is an `AST_WHILE` with the same
children, `say x` becomes `print_int(x)`), and the single code generator turns
that tree into assembly. Byte-identical output is the proof, and `test.bat`
checks it on every run rather than trusting this paragraph: `demo\4_pair` for
the shape of the language, and `tests/myc/else.myc` + `tests/myc/literals.myc`
for behaviour that is asserted (the `else` branch is actually taken; `0x10` is
16, not 0 — that was BUG-029) against a hand-written C twin each.

The language: `fn` / `end`, `let`, `say`, `if`/`else`, `while`, `return`, `and`,
`or`, `not`, blocks terminated by `end` instead of `}`, no semicolons. Numbers
and string literals only; a number follows C's own literal rules (decimal,
`0x` hex, leading-`0` octal, optional `u`/`l` suffix), because the lexer is
shared with the C front end — and since 2026-10-24 both front ends also share
the *meaning* (`util.c:parse_integer_literal()`), so `0x10` is 16 in either
language. Every parameter is an `int`; `say` prints a number, or a string
literal (or a `let` name initialised from one). The whole grammar is about 600
lines of `src/myc.c` — the statement forms are `parse_stmt`, and the expression
ladder is `parse_expr` down to `parse_primary`.

## A tiny example

```c
struct Point { int x; int y; };

int sum_array(int *a, int n);   /* prototype: array decays to int* */

int main(void) {
    int a[4];                                  /* one-dimensional array */
    for (int i = 0; i < 4; i++) a[i] = i * i;  /* 0, 1, 4, 9 */
    int total = sum_array(a, 4);               /* 14 */

    struct Point p;
    p.x = total;                               /* 14 */
    p.y = 30;

    struct Point *pp = &p;                     /* pointer to struct */
    pp->y = p.y + 4;                           /* 34 */

    int *q = a;
    q++;                                       /* moves one int (8 bytes) */
    return p.x + p.y + *q;                     /* 14 + 34 + 1 = 49 */
}

int sum_array(int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}
```

Compiling and running this returns **49** as the process exit code.

## Hello, world

```c
int main(void) {
    print_str("Hello, world!\n");
    print_int(6 * 7);
    return 0;
}
```

```
$ mycc hello.c -o hello.s && gcc -o hello hello.s && ./hello
Hello, world!
42
```

`print_str` and `print_int` are compiler built-ins: semantic analysis rewrites
them into `printf("%s", …)` and `printf("%lld\n", …)` calls, which is why no
declaration or `#include` is needed. To call the C library directly, declare
it the way C does:

```c
extern int printf(char *fmt);

int main(void) {
    printf("x = %lld, name = %s\n", 42, "mycc");   /* real variadic call */
    return 0;
}
```

`--dump-ast` shows the rewrite, so there is no hidden magic:

```
CALL printf  [3:5]
  STRING "%lld\n"  [0:0]
  ...
```

## Quick start (Windows / PowerShell)

```
cd "C:\Folder D\Feature_anant\C-Compiler-Mini-Project"
.\build.bat        # builds mycc.exe
.\test.bat         # runs the positive + error test suites (81 checks)
.\difftest.bat     # compiles tests\diff\*.c with mycc and with gcc, compares
.\fuzz.bat         # generates 1000 random programs and compares mycc vs gcc
.\showcase.bat     # end-to-end demo, returns 197
.\demo.bat         # five-minute demo for a non-technical audience
```

## Quick start (Linux / macOS)

```
make               # builds ./mycc with gcc
make test          # runs the positive + error test suites
make difftest      # differential test: mycc vs the host compiler
make fuzz          # 1000 generated programs: mycc vs the host compiler
./mycc example.c -o demo.s
gcc -no-pie -o demo demo.s     # macOS: drop -no-pie
./demo; echo $?    # prints 197
```

Set `CC=/path/to/gcc` to build with a specific compiler. On macOS the Makefile
drops `-no-pie`, asks mycc for `--target=darwin` (so the emitted symbols carry
the leading underscore Mach-O expects) and passes `-arch x86_64`, because the
backend emits x86-64 and current Macs are Apple Silicon — the output runs under
Rosetta 2, which the CI job checks for explicitly.

## Continuous integration

Every push and pull request is built and tested on Linux, Windows
(MSYS2/UCRT64) and macOS via
[`.github/workflows/ci.yml`](.github/workflows/ci.yml). The matrix is green on
all three platforms (first green run: `40086581248`, 2026-10-24). The Linux job
also fails on any compiler warning (`-Werror`); every job runs the exit-code
suite, the differential test and a fuzz campaign (1000 generated programs on
Linux, 200 on Windows, 100 on macOS); and each job publishes its `mycc` binary
as a build artifact.

## Random-program testing

`tests/diff/` proves mycc agrees with gcc on four programs somebody wrote down,
which only covers bugs somebody thought of. `tools/fuzz.c` generates programs
nobody wrote — a fixed skeleton (helper function, two loops, an `if`/`else`)
with every expression, literal, comparison and loop bound chosen at random —
and `make fuzz` / `fuzz.bat` run them through the same check: mycc's output and
gcc's must return the same exit code.

```
make fuzz                 # 1000 programs, seed 1
make fuzz FUZZ_N=5000 FUZZ_SEED=7
```

Generation is deterministic in `(seed, index)`, and case N does not depend on
the count, so a failing case is reproducible by number on any machine. Three
invariants keep a mismatch meaningful: every generated program terminates, it
contains no undefined behaviour (division only by literals, no uninitialised
reads), and no expression overflows a 32-bit `int` — which matters because
`mycc`'s `int` is 64-bit and an overflow would be C undefined behaviour rather
than a compiler bug.

## Repository layout

| Path | What lives there |
|---|---|
| `src/`, `include/` | the compiler: lexer, parser, semantic analysis, optimizer, code generator, diagnostics, and the `myc` front end |
| `tests/` | the positive suite (one program, one labelled exit status) and `tests/errors/` (programs that must be rejected, with the message expected) |
| `tests/diff/` | programs compiled by **both** mycc and gcc and required to agree |
| `tests/myc/` | the toy language's programs and their hand-written C twins |
| `tools/fuzz.c` | the random-program generator behind `make fuzz` / `fuzz.bat` |
| `demo/`, `demos/` | the demo kit and the example programs the walkthroughs compile |
| `test.bat`, `difftest.bat`, `fuzz.bat`, `Makefile` | the two independent ways to run the suite (Windows batch and POSIX make) |
| `demo.bat`, `showcase.bat`, `tour.bat` | scripted walkthroughs of the compiler's output |

## Documentation

`README.md` is the front page: how to build, run and test the compiler. The rest
of the documentation lives in **`docs/`**:

| File | What it is |
|---|---|
| `docs/REPORT-DRAFT.md` | the full project report: objectives, module-by-module implementation, grammar, a worked C → assembly example, the optimization measurements, the testing methodology, the defect analysis and 25 viva questions |
| `docs/report.md` | the bug ledger: 31 defects, each with a reproduction, a root cause, a fix commit and the regression test that fails on the old code |
| `docs/Flow.md` | the execution flow map: which function calls which, from `main()` to the emitted assembly |
| `docs/ROADMAP.md` | the phase plan, what landed in each phase, and the future work |
| `docs/DEMO.md` | the demo playbook: every feature, the command that shows it, and what to say |

Three working notes are deliberately kept out of the repository and maintained
locally: `decision.md` (the decision log), `learn.md` (a learning guide) and
`instructions.md` (a long user manual). They change constantly and read badly
out of context; the report above summarises everything they contain that a
reviewer needs.
