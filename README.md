# mycc — A C-Subset Compiler for x86-64

A small but complete compiler written in **plain C99** (no Lex, Yacc, or LLVM).
`mycc` reads a C-subset source file and produces **x86-64 AT&T assembly**
that assembles and runs on **both Linux** (ELF, System V) and **Windows**
(PE/COFF, MinGW-w64) with ordinary gcc.

```
source.c ──> mycc ──> source.s ──> gcc ──> executable ──> exit code
```

## What's inside

- Hand-written lexer, recursive-descent parser, scoped symbol table,
  semantic analysis, and code generator (~4,700 lines of C99).
- **Types:** `int` (64-bit), `char` (signed, one byte), and `void` functions,
  plus derived **pointers**, one-dimensional **arrays**, and **structs**.
- **Pointers & arrays:** `int *p` / `int **pp`, address-of `&`, dereference `*`
  (read and write), pointer arithmetic scaled by the element size
  (`p + n`, `p - n`, `p - q`, `++p`, `p++`, `p += n`), one-dimensional arrays
  (`int a[10]`) with indexing `a[i]`, and array-to-pointer decay when arrays
  are passed to functions. `0` acts as the null pointer.
- **Structs:** `struct` definitions with `int`/`char`/pointer/struct members,
  member access with `.` and `->`, struct pointers and arrays of structs,
  self-referential types (`struct Node { struct Node *next; }`), and global
  structs/arrays (zero-initialized).
- **Statements:** `if`/`else`, `while`, `do`/`while`, `for` (loop-local
  declarations), `switch`/`case`/`default` with fallthrough, `break`,
  `continue`, `return`, and nested blocks.
- **Expressions:** the full C precedence ladder — arithmetic, `%`, relational
  and equality, `&&`/`||` (short-circuiting) and `!`, bitwise
  `&` `|` `^` `~`, shifts `<<` `>>`, the conditional `?:`, `++`/`--`
  (prefix and postfix), and compound assignment (`+= -= *= /= %= &= |= ^= <<= >>=`).
- **Programs:** multiple functions with recursion and **prototypes / forward
  declarations** (`int f(int);`), up to 6 parameters, `int`/`void` return
  types, globals with constant-folded initializers, multi-declarator
  declarations, and block scoping.
- **Literals:** decimal, hexadecimal (`0x…`), and octal (`0…`) integers with
  optional `u`/`l` suffixes; character constants (`'A'`, `'\n'`, `'\x41'`).
- **Diagnostics:** friendly errors with `file:line:col`, including lvalue
  checks, unknown struct members, scope/`main`/`break`/`continue`/`switch`
  validation, and rejection of non-constant global initializers.
- `--dump-tokens` / `--dump-ast` reveal the compiler's internals.
- Programs report results through their **exit code** (there is no `printf`).
- **Tests:** 14 positive programs (`tests/01…14`) and 18 rejection tests
  (`tests/errors/`), covering pointers (`12`), arrays (`13`), and structs (`14`).

Still out of scope (by design): `float`/`double`, `unsigned`, multi-dimensional
arrays, arrays with initializer lists, `sizeof`, string literals, and the
preprocessor.

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
cd "C:\Folder D\C_Compiler_Test_MiniProject\my_c_compiler"
.\build.bat        # builds mycc.exe
.\test.bat         # runs the positive + error test suites
.\showcase.bat     # end-to-end demo, returns 197
```

## Quick start (Linux / macOS)

```
make               # builds ./mycc with gcc
make test          # runs the positive + error test suites
./mycc example.c -o demo.s
gcc -no-pie -o demo demo.s
./demo; echo $?    # prints 197
```

Set `CC=/path/to/gcc` to build with a specific compiler.

## Continuous integration

Every push and pull request is built and tested on Linux and Windows
(MSYS2/UCRT64) via [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

## Documentation

- [instructions.md](instructions.md) — full manual: language, build/test/showcase, troubleshooting.
- [ROADMAP.md](ROADMAP.md) — planned phases: C-fidelity fixes, more language, I/O,
  optimization, diagnostics, and a true Windows calling convention.
