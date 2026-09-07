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
  semantic analysis, and code generator (~2,600 lines of C99).
- Supports: functions, recursion, parameters, `if`/`else`, `while`, `for`,
  `&&`/`||`/`!` with short-circuiting, `%`, `++`/`--`, compound assignment,
  globals, block scoping, and multi-declarator declarations.
- `--dump-tokens` / `--dump-ast` reveal the compiler's internals.
- Programs report results through their **exit code** (there is no `printf`).

## Quick start (Windows / PowerShell)

```
cd "C:\Folder D\C Compiler Test THROUGH OPENCODE\my_c_compiler"
.\build.bat        # builds mycc.exe
.\test.bat         # runs the 5-test suite
.\showcase.bat     # end-to-end demo, returns 197
```

## Quick start (Linux / WSL)

```
make clean
make CC=/tmp/opencode/gccwrap
make CC=/tmp/opencode/gccwrap test
./mycc example.c -o /tmp/opencode/demo.s
/tmp/opencode/gccwrap -no-pie -o /tmp/opencode/demo /tmp/opencode/demo.s
/tmp/opencode/demo; echo $?      # prints 197
```

## Document

- [instructions.md](instructions.md) — full manual: language, build/test/showcase, troubleshooting.
- [ROADMAP.md](ROADMAP.md) — planned phases: C-fidelity fixes, more language, I/O,
  optimization, diagnostics, and a true Windows calling convention.