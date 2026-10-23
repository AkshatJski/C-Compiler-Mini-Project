# mycc — A C-Subset Compiler for x86-64

A small but complete compiler written in **plain C99** (no Lex, Yacc, or LLVM).
`mycc` reads a C-subset source file and produces **x86-64 AT&T assembly**
that assembles and runs on **Linux** (ELF, System V) and **Windows**
(PE/COFF, MinGW-w64) with ordinary gcc. A third flavour, `--target=darwin`,
emits Mach-O symbol names for macOS.

```
source.c ──> mycc ──> source.s ──> gcc ──> executable ──> exit code
```

## What's inside

- Hand-written lexer, recursive-descent parser, scoped symbol table,
  semantic analysis, and code generator (~6,200 lines of C99 across `src/`
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
  declarations** (`int f(int);`), up to 6 parameters, `int`/`void` return
  types (including pointer returns such as `int *`), globals with
  constant-folded initializers, multi-declarator declarations, and block
  scoping. `main` must be declared `int main(void)`; a program without
  `main`, or with an undefined function still being called, is rejected
  during compilation instead of failing at link time.
- **Literals:** decimal, hexadecimal (`0x…`), and octal (`0…`) integers with
  optional `u`/`l` suffixes; character constants (`'A'`, `'\n'`, `'\x41'`).
  Malformed literals are errors rather than silent zeroes: `08` and `09`,
  `0x` with no digits, and an unterminated character constant are all
  reported. A UTF-8 byte-order mark (which Windows editors add) is skipped.
- **Diagnostics:** friendly errors with `file:line:col`, including lvalue
  checks, unknown struct members, scope/`main`/`break`/`continue`/`switch`
  validation, rejection of non-constant global initializers, assignment to an
  array, dereferencing a `void *`, incomplete/zero-sized objects, struct
  redefinition, and precise messages (one error, not a cascade) for
  constructs the subset does not have — `#include`, other preprocessor
  directives, and string literals.
- **Warnings:** advisory diagnostics that never change the exit status
  (`<file>:<line>:<col>: warning: …`), e.g. `control reaches end of non-void
  function 'f'`. `-w` silences the warning channel without affecting errors.
- `--dump-tokens` / `--dump-ast` reveal the compiler's internals;
  `--target=linux|windows|darwin` picks the assembler flavour (defaulting to
  the host mycc was built for).
- A failed compilation leaves **no** output file behind (the assembly is
  written to a temporary file and renamed on success).
- Programs report results through their **exit code** (there is no `printf`).
- **Tests:** 25 positive programs (`tests/01…25`) and 41 rejection tests
  (`tests/errors/`), 68 checks in total — covering pointers (`12`), arrays
  (`13`), structs (`14`), nested `switch` labels (`15`), member `++`/`--`
  (`16`), multi-dimensional arrays (`17`), `unsigned` types (`18`), the comma
  operator (`19`), casts (`20`), global pointers (`21`), struct assignment
  (`22`), single evaluation of lvalues (`23`), the BOM (`24`), and the
  fall-off-the-end warning (`25`). `tests/diff/` additionally holds programs
  that are compiled by **both** mycc and gcc and required to return the same
  exit code (`difftest.bat` / `make difftest`).

Still out of scope (by design): `float`/`double`, `long`/`short`, `enum`/
`typedef`, aggregate initializer lists (`int a[2] = {1,2}`), `sizeof`, string
literals, and the preprocessor. Structs are never passed or returned **by
value** — take a pointer (`struct P *p`) instead, and the compiler says so.
`int` remains 64-bit (see the open Phase 1 decision in
[ROADMAP.md](ROADMAP.md)), so `unsigned int` is 64-bit too.

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

## Quick start (Windows / PowerShell)

```
cd "C:\Folder D\Feature_anant\C-Compiler-Mini-Project"
.\build.bat        # builds mycc.exe
.\test.bat         # runs the positive + error test suites (68 checks)
.\difftest.bat     # compiles tests\diff\*.c with mycc and with gcc, compares
.\showcase.bat     # end-to-end demo, returns 197
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
- [ROADMAP.md](ROADMAP.md) — planned phases: C-fidelity fixes, more language, I/O,
  optimization, diagnostics, and a true Windows calling convention.
