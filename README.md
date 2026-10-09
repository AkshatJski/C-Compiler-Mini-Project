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
  semantic analysis, and code generator (~3,500 lines of C99).
- **Types:** `int` (64-bit) and `char` (signed, one byte); `void` functions.
- **Statements:** `if`/`else`, `while`, `do`/`while`, `for` (loop-local
  declarations), `switch`/`case`/`default` with fallthrough, `break`,
  `continue`, `return`, and nested blocks.
- **Expressions:** the full C precedence ladder — arithmetic, `%`, relational
  and equality, `&&`/`||` (short-circuiting) and `!`, bitwise
  `&` `|` `^` `~`, shifts `<<` `>>`, the conditional `?:`, `++`/`--`
  (prefix and postfix), and compound assignment (`+= -= *= /= %= &= |= ^= <<= >>=`).
- **Programs:** multiple functions with recursion, globals with
  constant-folded initializers, multi-declarator declarations, and block scoping.
- **Literals:** decimal, hexadecimal (`0x…`), and octal (`0…`) integers with
  optional `u`/`l` suffixes; character constants (`'A'`, `'\n'`, `'\x41'`).
- **Diagnostics:** friendly errors with `file:line:col`, scope/`main`/
  `break`/`continue`/`switch` validation, and rejection of non-constant
  global initializers.
- `--dump-tokens` / `--dump-ast` reveal the compiler's internals.
- Programs report results through their **exit code** (there is no `printf`).

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
