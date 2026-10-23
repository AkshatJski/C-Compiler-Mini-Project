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
  semantic analysis, and code generator (~7,000 lines of C99 across `src/`
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
- **One optimizer switch:** `--no-opt` disables Phase 5's constant folding and
  constant-condition elimination, emitting exactly what the pre-optimizer code
  generator emitted. It exists so the two back ends can be compared line for
  line (`mycc hello.c -o a.s` vs `mycc --no-opt hello.c -o b.s`) and so that an
  optimizer bug is always bisectable against a known-good path.
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
- **Tests:** 33 positive programs (`tests/01…33`) and 41 rejection tests
  (`tests/errors/`), 76 checks in total — covering pointers (`12`), arrays
  (`13`), structs (`14`), nested `switch` labels (`15`), member `++`/`--`
  (`16`), multi-dimensional arrays (`17`), `unsigned` types (`18`), the comma
  operator (`19`), casts (`20`), global pointers (`21`), struct assignment
  (`22`), single evaluation of lvalues (`23`), the BOM (`24`), the
  fall-off-the-end warning (`25`), hello world (`26`), the print built-ins
  (`27`), `extern printf` (`28`), string escapes (`29`), NUL bytes inside
  a literal (`30`), constant folding (`31`), constant-condition
  elimination (`32`), and constant operands (`33`). Positive tests
  assert the program's real **output** (`// EXPECT-OUT:`), not just its exit
  code. `tests/diff/` additionally holds programs that are compiled by **both**
  mycc and gcc and required to return the same exit code (`difftest.bat` /
  `make difftest`).

Still out of scope (by design): `float`/`double`, `long`/`short`, `enum`/
`typedef`, aggregate initializer lists (`int a[2] = {1,2}`), `sizeof`, and the
preprocessor. Structs are never passed or returned **by value** — take a
pointer (`struct P *p`) instead, and the compiler says so. `int` remains
64-bit (see the open Phase 1 decision in
[ROADMAP.md](ROADMAP.md)), so `unsigned int` is 64-bit too. `main` must still
be `int main(void)`; `argv` is not available to mycc programs yet.
The optimizer is deliberately shallow so far — constant folding and
constant-condition elimination only. There is no register allocator: every
expression still goes through the stack machine, and that is a documented
tradeoff rather than an oversight (see `ROADMAP.md` Phase 5).

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
.\test.bat         # runs the positive + error test suites (76 checks)
.\difftest.bat     # compiles tests\diff\*.c with mycc and with gcc, compares
.\showcase.bat     # end-to-end demo, returns 197
.\demo.bat         # five-minute demo for a non-technical audience
```

## Quick start (Linux / macOS)

```
make               # builds ./mycc with gcc
make test          # runs the positive + error test suites
make difftest      # differential test: mycc vs the host compiler
./mycc example.c -o demo.s
gcc -no-pie -o demo demo.s     # macOS: drop -no-pie
./demo; echo $?    # prints 197
```

Set `CC=/path/to/gcc` to build with a specific compiler. On macOS the Makefile
drops `-no-pie` and asks mycc for `--target=darwin`, so the emitted symbols
carry the leading underscore Mach-O expects.

## Continuous integration

Every push and pull request is built and tested on Linux, Windows
(MSYS2/UCRT64) and macOS via
[`.github/workflows/ci.yml`](.github/workflows/ci.yml). The Linux job also
fails on any compiler warning (`-Werror`), and every job runs both the
exit-code suite and the differential test.

## Documentation

- [instructions.md](instructions.md) — full manual: language, build/test/showcase, troubleshooting.
- [DEMO.md](DEMO.md) — presenter script for a non-technical audience (60s / 3min /
  5min cuts, the questions you will actually be asked, and what to do when
  something breaks live).
- [ROADMAP.md](ROADMAP.md) — planned phases: C-fidelity fixes, more language,
  I/O and the ABI, optimization, diagnostics and tooling. Phases 1–4 are
  complete. Phase 5 (optimization) is in progress — constant folding,
  constant-condition elimination and constant operands have landed; dataflow
  copy propagation and the register allocator are the remainder, and the roadmap
  says why they are staged last. Phase 6 (diagnostics and tooling) is partly
  done: caret snippets, the warning channel, the differential harness and the
  opt/no-opt equivalence check are in; `--interpret` and the random program
  generator are not. Phase 7 has `--version`, `--sign` and three-target output;
  floating point and an SSA IR are still open.
