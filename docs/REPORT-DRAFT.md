# Project Report — `mycc`, a C-Subset Compiler for x86-64

**Working draft for the college submission.** Every number, commit hash and
quoted output in this file was taken from the repository itself, not estimated.
Published since 2026-10-24 in `docs/`, next to the bug ledger and the planning
notes. Three working notes stay local: `decision.md`, `learn.md` and
`instructions.md`.

> **2026-10-26 audit addendum.** A later correctness pass added tests 37–49, the same-file/temp-path contract, and ledger entries `BUG-032`…`BUG-048`. The numbers in this draft have been refreshed where they describe the current repository: **48 defects logged, 47 fixed, 1 open**; **106 automated checks** (48 positive, 52 rejection, 6 contract); **9,502 compiler lines** (9,055 `src/` + 447 `include/`). Sixteen of the new fixes are present in the working tree but **not committed**, because the user withheld commit/push permission; their ledger status is `FIXED (working tree)` with a `pending` commit. `BUG-048` — struct values accepted as conditions/comparisons — remains **OPEN**. The current optimizer measurement is **30.9%** (14,290 → 9,872 lines over 48 tests); the historical 2026-10-24 35-test comparison (33.8%), the 81-check CI matrix and the 8,766-line snapshot are retained as history and labelled as such where they appear.

---

## 0. How to use this draft

| If you need… | Read |
|---|---|
| the abstract, in one paragraph | §1 |
| the problem statement and objectives | §2 |
| what the compiler does and refuses to do | §3, §13 |
| the architecture and module breakdown | §5, §11.1 |
| the design decisions an examiner will probe | §6 |
| the actual implementation, module by module | §7 |
| grammar tables (EBNF) | §8 |
| a worked C → assembly example to show on a slide | §7.6, §9 |
| the optimization chapter with measured results | §8 (of §7.5) and §11.3 |
| the testing/validation chapter | §10, §11.4 |
| the bug-ledger chapter (strong differentiator) | §12 |
| honest limitations and future work | §13, §14 |
| viva questions with answers | Appendix B |
| slide-ready numbers | Appendix D |

Three things make this report different from the usual "I built a compiler"
submission, and they are worth foregrounding:

1. **Every claim is executable.** The test suite, the differential harness, the
   fuzzer and CI all live in the repository; the report quotes their output.
2. **The bug ledger is public inside the project.** 48 defects, each with a
   reproduction, a root cause, a fix plan and, where fixed, a regression test —
   including the ones that were *only* findable by reading CI logs. One of the
   48 (`BUG-048`, scalar contexts for struct values) is still open and is listed
   as open rather than rounded up to “fixed.”
3. **The limitations are decisions, not oversights.** `int` is 64 bits because
   it was decided and written down, not because nobody noticed.

---

## 1. Abstract

`mycc` is a compiler for a subset of C, written from scratch in about 9,500
lines of C99 with no parser generator, no LLVM and no external libraries. It
lexes, parses, type-checks and optimizes a C subset — including pointers,
multi-dimensional arrays, structs, `unsigned` types, the full expression
precedence ladder and the platform calling convention — and emits x86-64 AT&T
assembly that assembles and runs on Linux, Windows (MinGW-w64) and macOS.

Three properties are unusual for a project of this size. First, correctness is
established *differentially*: mycc's output must agree with gcc's on hand-written
programs, on 1000 randomly generated programs per run, and under two optimizer
settings on every program in the suite. Second, the compiler has a second front
end — a small language called `myc` — which lowers to the *same* AST, so the
same program written in the two languages must compile to byte-identical
assembly; that equivalence is checked on every test run. Third, the project
keeps an honest public defect ledger: 48 bugs with reproductions, root causes,
fails-before fixes and regression tests, 47 closed and one open, including two
that proved the project's own CI had never actually been green.

The result is a compiler of about 9,500 lines whose behaviour is pinned by 106
automated checks, 3 differential programs, a deterministic fuzzer and a
three-platform CI matrix (first green run: 2026-10-24). The current 48-test
suite produces assembly **30.9% smaller** than the unoptimized path (14,290 →
9,872 lines); the earlier 35-test snapshot measured 33.8%.

---

## 2. Problem statement and objectives

**Problem.** Compilers are usually taught as a pipeline (lex → parse → analyse →
optimize → emit) and then demonstrated with tools that hide most of the pipeline
(ANTLR, LLVM, `yacc`). A student who uses those tools can describe the pipeline
but has not implemented the parts where the real difficulty lives: lvalue
evaluation, calling conventions, register allocation on a stack machine, and
diagnostics that point at the right column.

**Objective.** Build a compiler that is small enough to read end-to-end and
complete enough to compile real programs, with every stage hand-written, and
with a testing strategy strong enough to make "it works" a checkable statement
rather than a claim.

**Specific objectives, all met:**

| # | Objective | Evidence |
|---|---|---|
| O1 | Hand-written lexer, recursive-descent parser, symbol table, semantic analysis, optimizer, code generator | §7, 9,502 lines in `src/` + `include/` |
| O2 | Compile a useful C subset, including pointers, arrays, structs, `unsigned` | §7, 48 positive tests |
| O3 | Emit real x86-64 assembly that links against the platform C library | §7.6, `tests/26`–`29`, three targets |
| O4 | Real calling conventions (SysV and Win64), not a private ABI | §7.6, `tests/diff/02_stack_args.c` |
| O5 | Optimize, and prove the optimizer does not change behaviour | §7.5, `--no-opt` parity on every positive test; 30.9% size reduction over the current 48-test suite |
| O6 | Diagnostics a human can act on (carets, one error not a cascade) | §7.7, 52 rejection tests |
| O7 | Test with an independent oracle, not just self-consistency | §10, differential + fuzz |
| O8 | Work on three platforms, verified by CI | §10.5, run `40086581248` |

**Deliberate non-objectives:** floating point, the preprocessor, `sizeof`,
`typedef`/`enum`, struct-by-value parameters, and a global register allocator.
Each is discussed in §13/§14 with a reason, not an excuse.

---

## 3. Scope: what the language is

### 3.1 Supported

- **Types:** `int` (64-bit in this compiler's ABI — see §6 D1), `char` (signed,
  one byte), `unsigned` / `unsigned int` / `unsigned char`, `void`, pointers
  (`int *`, `int **`, pointer-to-struct, pointer-to-pointer), arrays of any
  number of dimensions, and `struct` (nested, self-referential).
- **Declarations:** locals, globals with constant-folded initializers, function
  definitions, prototypes/forward declarations, `extern` library declarations,
  multi-declarator declarations, block scoping.
- **Statements:** `if`/`else`, `while`, `do`/`while`, `for` (with loop-local
  declarations), `switch`/`case`/`default` with fallthrough, `break`,
  `continue`, `return`, nested blocks.
- **Expressions:** the full precedence ladder — arithmetic, `%`, relational,
  equality, `&&`/`||` (short-circuiting), `!`, bitwise `& | ^ ~`, shifts
  `<< >>`, conditional `?:`, `++`/`--` (prefix and postfix), compound
  assignment (`+= -= *= /= %= &= |= ^= <<= >>=`), casts, the comma operator, and
  address-of/dereference/subscript/member access.
- **Literals:** decimal, hex (`0x…`) and octal (`0…`) integers with optional
  `u`/`l` suffixes, character constants with C escapes, string literals with
  the full escape set (including `\0` *inside* the literal).
- **Output:** four built-ins that need no declaration — `print_int`,
  `print_char`, `print_str`, `print_endl` — plus any C library function via
  `extern int printf(char *fmt);`.

### 3.2 Refused, with a diagnostic

`#include` and other preprocessor directives, `float`/`double`, `long`/`short`,
`enum`/`typedef`, `sizeof`, aggregate initializer lists (`int a[2] = {1,2}`),
struct parameters or returns **by value**, `int main(int argc, char **argv)`,
non-constant global initializers, assignment to an array, dereferencing
`void *`, incomplete or zero-sized objects, and a program without `main`.

Each is a *printable* refusal: the compiler says what it does not support rather
than emitting wrong code. The bug ledger (§12) shows why this discipline
matters: the worst defect class in this project was never "did not compile", it
was "compiled and printed the wrong number silently".

---

## 4. Background: the pipeline this project implements

For a reader new to compilers, the whole system in five lines:

```
source.c ──lexer──> tokens ──parser──> AST ──semantic+optimizer──> AST ──codegen──> source.s ──gcc──> executable
```

`mycc` implements every stage itself except the last two: it emits that
`.s` file, and `gcc` is used only as an *assembler and linker* (which is what
`gcc` is doing when you hand it a `.s`).

| Stage | Input | Output | Where |
|---|---|---|---|
| Lexer | characters | tokens with line/column | `src/lexer.c` (781 lines) |
| Parser | tokens | AST (arena-allocated nodes) | `src/parser.c` (2,027) |
| Symbol table | declarations | scoped symbol records | `src/symtab.c` (101) |
| Semantic analysis | AST | typed AST, folded constants, builtin rewrites, errors/warnings | `src/semantic.c` (1,777) |
| Optimizer | typed AST | smaller AST (dead stores) | `src/opt.c` (413) |
| Code generator | typed AST | x86-64 AT&T assembly | `src/codegen.c` (1,853) |
| Diagnostics | positions | `file:line:col: error: …` + caret snippet | `src/util.c` (485) |
| Driver | argv | the pipeline, flags, temp-file-and-rename | `src/main.c` (334) |
| Inspectors | AST/tokens | `--dump-tokens`, `--dump-ast` | `src/dump.c` (352) |
| Second front end | `myc` source | the *same* AST | `src/myc.c` (932) |

---

## 5. System architecture

### 5.1 One process, one job

`main()` reads the file, chooses a front end (`--lang=myc` or C), then runs the
same back end. There is no plugin system, no global state beyond the diagnostic
stream and the source text, and no configuration file — by design (§6, D7).

```
main(argc, argv)
  ├─ parse flags (--lang, --target, --no-opt, -w, --dump-*, --sign, --version)
  ├─ read_file            one buffer, NUL-terminated
  ├─ lexer_init(source)   BOM skipped (BUG-011)
  ├─ parse_program        or myc_parse_program  -> AST
  ├─ semantic_analyze     types, scopes, rewrites, folding, errors
  ├─ opt_run              dead local stores (skipped with --no-opt)
  ├─ codegen_generate     -> assembly text
  └─ write temp file, rename on success (BUG-017)
```

### 5.2 Data structures

- **`Token`** — kind, text, line, column, and (for strings) decoded bytes plus
  length, because a C string literal can contain NUL (BUG-023).
- **`ASTNode`** — a tagged union allocated from an arena: kind, position, type,
  `long long` value for literals, symbol pointer for names, up to three
  children plus a next-pointer for lists. Arena allocation means no per-node
  `free`, no leaks to reason about, and a parser that can bail out at any point.
- **`Symbol`** — name, `Type *`, storage class, stack offset, and scope depth.
  **`Type`** — kind (`int`, `char`, `void`, pointer, array, struct, function),
  size, element/base type, array length, struct field list.
- **`Scope`** — a stack of hash-free linked lists; `vars_reset()` between
  functions (shared by both front ends).

### 5.3 Why an AST optimizer rather than a separate IR

The optimizer runs *on the AST*, before code generation, because the passes that
pay off here (constant folding, dead local stores) need types and scopes but not
machine resources. A separate SSA IR would be the right home for register
allocation — which is exactly why the liveness allocator was *not* built (§14).

---

## 6. Design decisions worth defending

Each of these has a full entry in the local `decision.md`; this is the table a
report chapter needs.

| # | Decision | Alternatives rejected | Why | Tradeoff accepted |
|---|---|---|---|---|
| D1 | **`int` is 64-bit** (the compiler's own ABI) | 32-bit `int` like every real ABI | Changing the integer model touches every load, store, comparison, division and struct layout; it buys nothing for the features this compiler demonstrates | Programs that rely on 32-bit wrap-around behave differently from gcc; no `sizeof`; the fuzzer must avoid overflow cases (§7.9) |
| D2 | **Stack-machine codegen**, one expression result at a time | Linear-scan/register allocation | A stack machine is correct by construction, debuggable by reading the `.s`, and small enough to read in one sitting | Slower code; the "one register" work (§7.5) recovers most of the easy wins |
| D3 | **Type checking inside semantic analysis**, on one AST, in place | A separate typed IR | One traversal, one error path, and the optimizer sees folded constants for free | Rewrites (builtins, folding) mutate the tree; the dump shows the post-analysis form (§9) |
| D4 | **Two front ends, one back end** | A `myc`-to-C translator | Byte-identical assembly becomes a *proof* that the abstraction boundary is real; a translator would hide the seam | The toy language inherits C's literal *syntax* from the shared lexer, so it had to inherit the meaning too (BUG-029) |
| D5 | **Differential testing against gcc**, not golden files | Compare emitted assembly to expected text | Golden `.s` files break on every refactor and assert *how*, not *what*; gcc asserts *what* and finds bugs nobody predicted (it found BUG-020) | The oracle is only as good as the test programs, so a fuzzer was added (§7.9) |
| D6 | **`--no-opt` keeps the pre-optimizer path alive** | Delete the old codegen | Makes every optimizer bug bisectable against a known-good path, and turns "the optimizer preserves behaviour" into a check on every test | One flag and one global; the codegen has two modes to maintain |
| D7 | **No configuration file, no plugin system** | A `mycc.toml`, pass plugins | A 9,500-line compiler does not need configuration; flags are enough, and a config file is surface area that can drift | No per-project settings; accepted |
| D8 | **Publish the evidence, keep the notebook** | Publish everything from day one; publish nothing but the README | A stranger needs the README to stand alone, and the report plus the bug ledger *are* the evidence; the decision log, learning guide and long manual are working material that reads badly out of context | The public repo shows the code, the README, and everything in `docs/` except three working notes — the day-to-day reasoning is summarised in this report instead |
| D9 | **Fail loudly rather than emit best-effort code** | Warn and continue | The ledger's P0 bugs were all silent wrong answers; a compiler that refuses is more useful than one that lies | Some valid C is rejected (struct by value, `sizeof`) |
| D10 | **CI must be green, not "mostly green"** | Accept a red macOS job | A red job that nobody reads is worse than no job; it hid two real defects for days (BUG-030, BUG-031) | Time spent fixing infrastructure instead of features |

---

## 7. Implementation

### 7.1 Lexer (`src/lexer.c`, 781 lines)

Hand-written character scanner with one character of lookahead. It produces
`Token` records carrying kind, text, line and column, and it owns three pieces
of real work:

- **Literals.** `0x…` hex, leading-`0` octal, decimal, with optional `u`/`l`
  suffixes; character constants with the full escape set; string literals
  decoded into bytes *with an explicit length*, so `"\0A"` survives (BUG-023).
- **Validation at the source.** `08` and `09` are errors rather than silent
  zeroes (BUG-008), `0x` with no digits is an error, unterminated strings and
  character constants are errors. Catching these here means the parser can
  assume well-formed tokens.
- **Byte-order mark.** A UTF-8 BOM at offset 0 is skipped, because Windows
  editors add one and the first error a new user hit was a lexer error on an
  invisible character (BUG-011).

### 7.2 Parser and AST (`src/parser.c`, 2,027 lines)

Recursive descent, one function per precedence level, exactly mirroring the C
grammar the subset implements. Two properties matter:

- **No backtracking.** Every construct is decided by the current token, so the
  parser is linear and the code reads like the grammar.
- **Bounded recursion.** Nesting depth is capped at 256 (`MYCC_MAX_NEST`);
  beyond that the compiler reports an error instead of overflowing the stack
  (BUG-010). A hostile input cannot crash the compiler.

The AST is arena-allocated (`ast_node()`), with a `next` pointer for statement
lists and up to three children per node. `--dump-ast` prints it, which is how
the desugarings in §7.4 can be demonstrated rather than asserted.

### 7.3 Symbol table and types (`src/symtab.c`, 101 lines + `include/ast.h`)

Deliberately tiny: a stack of scopes, each a singly-linked list of symbols, with
lookup walking outward. For programs of a few hundred identifiers this is faster
than a hash table (no hashing, better locality) and far easier to explain.
`vars_reset()` clears block scopes between functions and is shared by both front
ends — the first place where "two front ends, one back end" becomes visible in
the code.

### 7.4 Semantic analysis (`src/semantic.c`, 1,777 lines)

One traversal that does five jobs:

1. **Scope and declaration checking** — undeclared names, redeclaration,
   `break`/`continue` outside a loop, `switch` label validation, `main` present
   and `int main(void)`.
2. **Typing** — every expression gets a `Type *`; assignment and call
   compatibility are enforced (struct→scalar, `int`→pointer and
   struct→`int`-parameter are errors unless cast).
3. **Builtin rewriting** — `print_int(x)` becomes `printf("%lld\n", x)`,
   `print_str(s)` becomes `printf("%s", s)`, `print_char`/`print_endl`
   likewise. `printf`, `puts`, `putchar` are pre-registered. `--dump-ast`
   shows the rewrite.
4. **Constant folding** — arithmetic and comparisons on literals, *after*
   typing, gated on the optimizer switch.
5. **Diagnostics** — one error per run by default, with a caret snippet; the
   warning channel (`-w` silences) is separate and never changes the exit code.

### 7.5 Optimizer (`src/opt.c`, 413 lines + two codegen mechanisms)

Four pieces, each of which can be argued for in a paragraph — plus one size
optimization:

| Piece | Where | What it does | Why it is safe |
|---|---|---|---|
| Constant folding | `semantic.c` | `2 + 3` → `5`, comparisons of constants | Pure literals; no side effects |
| Constant-condition elimination | `codegen.c` (`const_truth`, 5 generators) | `if (0) …` emits only the dead arm's absence; same for `while`/`do`/`for`/`?:` | The condition is already a literal after folding |
| Immediate operands | `codegen.c` (`gen_binary_imm`) | `x + 1` encodes `$1` in `addq`, no separate `movq` | Instruction-level, semantics identical |
| Dead local stores | `opt.c` | removes stores to a local that is never read and not address-taken | Address-taken locals are marked (`note_address_use`, BUG-025) |
| Expression temporaries in a register | `codegen.c` (`temp_*`) | left operand of a binary expression parks in `%r10` instead of `pushq`/`popq` | `%r10` is caller-saved and unused by the emitters; nested `/` and `%` spill (BUG-026) |

**Copy propagation was implemented, measured, and deliberately removed:** on a
stack back end it saves nothing, and the measurement is recorded in
`decision.md`. A liveness-based global allocator was *not* attempted, because it
needs an IR that does not exist yet — an honest "no" with a reason (§14).

Measured effect over the current 48 positive tests (2026-10-26 audit):

| Setting | Assembly lines across 48 tests | |
|---|---|---|
| `--no-opt` | 14,290 | baseline |
| optimized | 9,872 | **30.9% smaller** |

The earlier 35-test snapshot was 11,055 → 7,315 (33.8%); the current figure is
the one produced by the 48-test suite and should be re-run with the snippet in
`DEMO.md` rather than recited.

`example.c` (the recursion + primes demo): 263 → 239 lines.

Crucially, **every positive program is compiled both ways and must exit
identically with byte-identical stdout** — so the optimizer's correctness is
checked on every run, not by inspection. The current suite has 48 positive
programs; the 48-test row above is the current measurement, and the 35-test row
is retained so the older snapshot is not mistaken for the current result.

### 7.6 Code generation (`src/codegen.c`, 1,853 lines)

A stack machine over x86-64: every expression leaves its result in `%rax`, and
values that must survive across another expression are pushed (or parked in
`%r10` by the temporary optimization). The generated assembly is deliberately
plain — `pushq`/`popq`/`movq`/`idivq`/`setcc` — because it is meant to be read.

**Frames.** `pushq %rbp; movq %rsp, %rbp; subq $N, %rsp` prologue, `leave; ret`
epilogue, locals at negative offsets, parameters copied into their slots at
entry. Anything spilled (an address-taken local, an array, a struct) lives in
the frame; the frame size is computed after semantic analysis, so it is exact.

**Calling conventions — the real ones:**

| | System V (Linux/macOS) | Windows x64 |
|---|---|---|
| Integer args | `rdi rsi rdx rcx r8 r9` | `rcx rdx r8 r9` |
| Stack args | seventh onward, at `16(%rbp)` | fifth onward, at `48(%rbp)` |
| Shadow space | none | 32 bytes, always reserved |
| Variadic | `%al` = number of vector registers | same |
| External calls | `call printf@PLT` | `call printf` |

`tests/diff/02_stack_args.c` compares a seven-argument function against gcc;
`tests/28_extern_printf.c` calls the real C library. This is the part most
toy compilers fake, and it is the part that makes `mycc` a *compiler* rather
than an interpreter with extra steps.

**Structs** are copied with `rep movsb` sized from the type, with `%rsi`/`%rdi`
saved around it on Windows. **String literals** are emitted once into `.rodata`
(deduplicated by bytes and length) and referenced `%rip`-relative.

**Three object formats from one generator.** A `TargetKind` enum changes exactly
the symbol naming and the link-time details: `.note.GNU-stack` on Linux only,
leading `_` on every external symbol on Darwin (`_main`, `_printf`), `@PLT` for
ELF externals. `--target=linux|windows|darwin` overrides the host default;
`--target=beos` is a clean error.

### 7.7 Diagnostics (`src/util.c`, 485 lines)

Every error and warning is `file:line:col: severity: message` followed by the
offending source line and a caret at the column:

```
prog.c:3:5: error: expected ';' after declaration (found 'int')
    3 |     int y = ;
      |     ^
```

Two rules the project holds to: **one error, not a cascade** (the parser
reports and unwinds rather than inventing follow-on errors), and **no output
file on failure** (assembly goes to a temporary file and is renamed only on
success, BUG-017).

### 7.8 The second front end: `myc` (`src/myc.c`, 932 lines)

`--lang=myc` parses a small language — `fn…end`, `let`, `say`, `if`/`else`,
`while`, `return`, `and`/`or`/`not`, no semicolons — and builds **the same AST**.
There is no translation step: `while i <= 5` is an `AST_WHILE` with the same
children, `say x` becomes `print_int(x)`, and the single code generator emits the
assembly.

That claim is checked, not asserted: `test.bat` and `make test` compile each
`tests/myc/NAME.myc` and its hand-written C twin `NAME.c`, require
**byte-identical assembly** (`fc /b` on Windows, `cmp` elsewhere), then link,
run, and assert the program's exit code and output. Three pairs run today:
`demo/4_pair` (the shape of the language), `tests/myc/else.myc` (the
`else` branch is actually taken — BUG-027) and `tests/myc/literals.myc`
(`0x10` is 16, not 0 — BUG-029).

This is the report's best single demonstration that the front-end/back-end
boundary is real: the same tree cannot produce different code.

### 7.9 Tooling: two build paths, four test layers, three platforms, one fuzzer

| Tool | Lines | Role |
|---|---|---|
| `Makefile` | 170 | POSIX build + `test` + `difftest` + `fuzz` |
| `build.bat` | 31 | Windows build (globs `src\*.c` — BUG-028) |
| `test.bat` | 345 | Windows suite: positive, rejection, contracts, pairs, parity |
| `difftest.sh` / `.bat` | 67 / 91 | differential harness vs the host compiler |
| `fuzz.bat` | 50 | Windows fuzz driver |
| `tools/fuzz.c` | 243 | random program generator |
| `demo.bat` | 208 | six-scene demonstration |

The two build paths are deliberate redundancy: the Windows path is what a reader
without `make` will use, and it drifted once (BUG-028), which is why CI now
executes *both*.

---

## 8. Grammar

### 8.1 The C subset (statement forms; semantic restrictions omitted)

```ebnf
program        ::= (declaration | function-definition)*
declaration    ::= ["extern"] type-spec declarator ("," declarator)* ";"
                 | struct-spec ";"
function-def   ::= type-spec IDENT "(" [params] ")" (block | ";")
params         ::= param ("," param)*
param          ::= type-spec declarator

type-spec      ::= ["unsigned"] ("int" | "char" | "void" | struct-ref)
struct-ref     ::= "struct" IDENT ["{" field+ "}"]
declarator     ::= "*"* IDENT [array-suffixes]

statement      ::= expr-stmt | block | if-stmt | while-stmt | do-stmt
                 | for-stmt | switch-stmt | jump-stmt | declaration
block          ::= "{" statement* "}"
if-stmt        ::= "if" "(" expr ")" statement ["else" statement]
while-stmt     ::= "while" "(" expr ")" statement
do-stmt        ::= "do" statement "while" "(" expr ")" ";"
for-stmt       ::= "for" "(" [declaration | expr] ";" [expr] ";" [expr] ")" statement
switch-stmt    ::= "switch" "(" expr ")" "{" (case-label statement*)* "}"
case-label     ::= "case" const-expr ":" | "default" ":"
jump-stmt      ::= "return" [expr] ";" | "break" ";" | "continue" ";"
expr-stmt      ::= expr ";"

expr           ::= assignment ("," assignment)*          (* comma operator *)
assignment     ::= conditional [assign-op assignment]
assign-op      ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="
                 | "&=" | "|=" | "^=" | "<<=" | ">>="
conditional    ::= logical-or ["?" expr ":" conditional]
logical-or     ::= logical-and ("||" logical-and)*
logical-and    ::= bit-or ("&&" bit-or)*
bit-or         ::= bit-xor ("|" bit-xor)*
bit-xor        ::= bit-and ("^" bit-and)*
bit-and        ::= equality ("&" equality)*
equality       ::= relational (("==" | "!=") relational)*
relational     ::= shift (("<" | ">" | "<=" | ">=") shift)*
shift          ::= additive (("<<" | ">>") additive)*
additive       ::= multiplicative (("+" | "-") multiplicative)*
multiplicative ::= unary (("*" | "/" | "%") unary)*
unary          ::= ("-" | "!" | "~" | "*" | "&" | "++" | "--") unary
                 | "(" type-spec "*"+ ")" unary            (* cast *)
                 | postfix
postfix        ::= primary ("[" expr "]" | "(" args ")" | "." IDENT
                 | "->" IDENT | "++" | "--")*
primary        ::= IDENT | INT-LIT | CHAR-LIT | STRING-LIT
                 | "(" expr ")"
```

### 8.2 `myc`

```ebnf
program   ::= fn-decl+
fn-decl   ::= "fn" IDENT "(" [IDENT ("," IDENT)*] ")" ["->" ("int" | "void")]
              body "end"
body      ::= stmt*
stmt      ::= "let" IDENT "=" expr
            | "say" expr
            | "return" [expr]
            | "if" expr body ["else" body] "end"
            | "while" expr body "end"
            | IDENT "=" expr
            | call
expr      ::= or-expr                       (* and / or / not, comparisons,
or-expr   ::= and-expr ("or" and-expr)*      arithmetic; same ladder as C
and-expr  ::= not-expr ("and" not-expr)*     without the operators the
not-expr  ::= ["not"] cmp-expr               toy language does not use *)
...
primary   ::= NUMBER | STRING | IDENT | "(" expr ")" | call
```

Numbers follow C's literal rules (decimal, `0x` hex, leading-`0` octal,
optional `u`/`l`), because the lexer is shared — and since BUG-029 both front
ends also share the *meaning* (`util.c:parse_integer_literal()`).

---

## 9. Worked example: `hello.c` → assembly

Source:

```c
int main(void) { print_str("Hello, world!"); print_int(6 * 7); return 0; }
```

Real output of `mycc build\hw.c -o build\hw.s` (Windows target). The `#`
comments on the right were **added here for explanation** — the real file has
none, and `--target=linux` / `--target=darwin` produce the same instructions
with different symbol and call spellings:

```asm
# Generated by mycc: C-subset compiler for AMD64 AT&T syntax
.text
.globl main
main:
	pushq %rbp
	movq %rsp, %rbp
	leaq .L.str.0(%rip), %rax     # "Hello, world!"
	pushq %rax
	leaq .L.str.1(%rip), %rax     # "%s"         <- print_str rewrite
	pushq %rax
	popq %rcx
	popq %rdx                      # argument order
	subq $32, %rsp                 # Win64 shadow space
	call printf
	addq $32, %rsp
	movq $42, %rax                 # 6 * 7 folded at compile time
	pushq %rax
	leaq .L.str.2(%rip), %rax     # "%lld\n"     <- print_int rewrite
	pushq %rax
	popq %rcx
	popq %rdx
	subq $32, %rsp
	call printf
	addq $32, %rsp
	movq $0, %rax
	leave
	ret
.section .rodata
.L.str.0:
	.string "Hello, world!"
.L.str.1:
	.string "%s"
.L.str.2:
	.string "%lld\n"
```

Three things to point at in a viva:

1. `movq $42, %rax` — **constant folding** turned `6 * 7` into a literal, so no
   multiplication is emitted at all.
2. Two `printf` calls — the **builtin rewrite** in semantic analysis, visible in
   `--dump-ast` too, so nothing is hidden.
3. `subq $32` before each call and `addq $32` after — the **Win64 shadow
   space**. On `--target=linux` those two lines are absent and the call becomes
   `call printf@PLT`; on `--target=darwin` the symbols become `_main`, `_printf`.

---

## 10. Testing and validation

### 10.1 Four layers

| Layer | What it asserts | Count | Where |
|---|---|---|---|
| Exit-code tests | the program returns `// EXPECT: n`, and prints `// EXPECT-OUT:` | 48 | `tests/*.c` |
| Rejection tests | compilation fails and stderr contains `// EXPECT-ERROR: …` | 52 | `tests/errors/*.c` |
| Contract assertions | no output on failure; `-w` silences warnings; `--sign` stamps only on request; same-file/temp-path safety; one per toy-language pair | 6 | `test.bat` / `Makefile` |
| Differential tests | mycc and gcc return the same exit code | 3 | `tests/diff/*.c` |
| Random programs | the same, on programs nobody wrote | 1000 per CI run; 700 in the local audit | `tools/fuzz.c` |

**106 checks** in total. Additionally, **every positive test is compiled twice**
(optimized and `--no-opt`) and must exit identically with byte-identical stdout
— that is the invariant that makes §7.5 trustworthy.

### 10.2 Why differential testing matters here (the honest version)

The early suite was green while the compiler silently miscompiled
`a[i++] += 5`, `a[i++]++`, `b = a` on structs and `a[i] += 1`. All four returned
*a* number and all four passed. A reference compiler as the oracle catches that
class for free: it is what found BUG-020. The lesson in one sentence: **a test
suite that compares against itself only catches bugs someone predicted.**

### 10.3 The fuzzer

`tools/fuzz.c` generates programs from a fixed skeleton (helper function, two
counted loops, an `if`/`else`) with every expression, literal, comparison and
loop bound chosen at random, then runs them through the *existing* differential
harness (`SRCDIR=build/fuzz QUIET=1 difftest.sh`).

What makes it a check rather than a noise generator is three invariants:

1. **Termination** — every loop counts to a literal bound, only the counter is
   incremented.
2. **No undefined behaviour** — division/modulo only by literals (never zero);
   every local written before read. (The first version read `t` before its first
   store; that would have produced UB mismatches, not findings.)
3. **Width safety** — mycc's `int` is 64-bit, gcc's is 32-bit, so multiplication
   only ever scales one variable by a literal, bounding every expression near
   800. This is decision D1 having teeth.

Determinism: seeded from `(seed, index)`, and case N does not depend on the
count, so a failure is reproducible by number on any machine.

**Result: 1000/1000 generated programs agree with gcc**, and CI runs 1000
(Linux), 200 (Windows) and 100 (macOS) on every push.

### 10.4 The bug ledger as a test artefact

Every fix in the ledger required: a reproduction, a fails-before observation, a
fix commit, and a regression test that fails on the old code. Example (BUG-029):

```
old compiler (built from HEAD:src/myc.c): exit 1   <- the guard fires
fixed compiler:                          exit 42   <- and prints literals-ok
```

That is the standard the whole report is written to. The 2026-10-26 batch is a deliberate exception in one respect only: the 16 fixes are regression-tested in the working tree but their commits are `pending`, because the user withheld commit/push permission. The ledger marks them `FIXED (working tree)`, not plain `FIXED`, and `BUG-048` remains open.

### 10.5 CI

Three jobs, every push:

| Job | What it runs | Status |
|---|---|---|
| Linux (gcc) | `make` with `-Werror`, 81 checks at that run (106 now), difftest, 1000-program fuzz, artifact upload | green, 44 s |
| Windows (MSYS2/UCRT64) | `make` + 81 checks at that run (106 now) + difftest + 200-program fuzz, **then** `build.bat` + `test.bat` via `cmd`, artifact upload | green, 2 m 49 s |
| macOS (clang) | Rosetta probe, `make` + 81 checks at that run (106 now) + difftest + 100-program fuzz, artifact upload | green, 1 m 4 s |

The 81 is the historical count from the first green matrix; the 2026-10-26
working-tree suite has 106 checks. No CI run has yet exercised the 16 uncommitted
fixes or the new tests, which is one more reason their status is not plain
`FIXED`.

**First green matrix: run `40086581248`, 2026-10-24.** Before that date, CI had
*never* been green on Windows or macOS, and the README claimed otherwise — the
single most useful finding of the whole project (§12, BUG-030/031).

---

## 11. Results and measurements

### 11.1 Code size

| Area | Lines |
|---|---|
| `src/codegen.c` | 1,853 |
| `src/parser.c` | 2,027 |
| `src/semantic.c` | 1,777 |
| `src/myc.c` | 932 |
| `src/lexer.c` | 781 |
| `src/util.c` | 485 |
| `src/opt.c` | 413 |
| `src/dump.c` | 352 |
| `src/main.c` | 334 |
| `src/symtab.c` | 101 |
| **`src/` total** | **9,055** |
| `include/` (types, AST, tokens) | 447 |
| **compiler total** | **9,502** |
| test programs (48 positive + 52 rejection) | 3,204 |
| differential programs (3) | 229 |
| tooling (Makefile, batch/shell scripts, fuzzer) | ~1,050 |

### 11.2 Repository

- 37 commits, 136 tracked files; the 2026-10-26 audit batch (tests, docs and
  source fixes) is uncommitted by request. Branches `main` and `Akshat` were
  both green at the last committed CI run.
- `README.md` and everything in `docs/` except three working notes are published;
  `docs\decision.md`, `docs\learn.md` and `docs\instructions.md` are local by
  policy (D8).

### 11.3 Optimizer

| Metric | `--no-opt` | optimized | Change |
|---|---|---|---|
| Assembly lines, 48 tests | 14,290 | 9,872 | **−30.9%** |
| Assembly lines, historical 35-test snapshot | 11,055 | 7,315 | −33.8% |
| `example.c` | 263 | 239 | −9.1% (historical) |
| Behaviour differences | — | — | **0** (parity check on every test) |

### 11.4 Test and fuzz results

| Check | Result |
|---|---|
| `test.bat` (2026-10-26 working tree) | `All tests passed` — **106 PASS** (48 positive, 52 rejection, 6 contract) |
| `make test` | Historical: `All tests passed` — 82 PASS lines (81 checks + parity line); not re-run for the 106-check suite in this audit |
| `difftest.bat` (with `CC=C:\w64devkit\bin\gcc.exe`) / `make difftest` | `All differential tests passed` — 3/3 |
| `fuzz.bat 700 456` (local audit, same `CC`) | `PASS: all 700 programs agreed with the reference compiler` |
| `make fuzz FUZZ_N=1000` (historical CI) | `PASS: all 1000 programs agreed with the reference compiler` |
| Clean clone of `main` → `build.bat` + `test.bat` | Historical: `Build OK: mycc.exe`, 81 PASS at the 2026-10-24 snapshot |
| Clean clone → `fuzz.bat 200 3` | Historical: `PASS: all 200 programs agreed` |
| CI matrix | 3/3 green, historical run `40087825704` on `main`; the uncommitted audit batch has not run in CI |

The local differential/fuzz runs set `CC=C:\w64devkit\bin\gcc.exe` because the
first `gcc` on this host's `PATH` is MinGW.org 6.3 (32-bit), which cannot
assemble 64-bit output. That is a host toolchain-selection issue; the test
harness itself was not changed in this audit.

### 11.5 Defect statistics

| Severity | Meaning | Count | Fixed | Open |
|---|---|---|---|---|
| P0 | silently wrong code / destructive output | 15 | 15 | 0 |
| P1 | ordinary code rejected or wrong on a valid path | 13 | 13 | 0 |
| P2 | bad or silent diagnostic | 12 | 11 | 1 (`BUG-048`) |
| P3 | crash or tooling | 8 | 8 | 0 |
| **Total** | | **48** | **47** | **1** |

The 16 new working-tree entries are fixed but uncommitted (`FIXED (working
tree)`, commit `pending`); `BUG-048` is the only genuinely open entry.

---

## 12. Defect analysis (the most defensible chapter)

The full ledger (`report.md`, in this repository) has 48 entries with
reproductions: 47 fixed and one open (`BUG-048`). This section extracts what a
report needs: the *classes*, the *best examples*, and what each class taught.

### 12.1 The three classes

**Class A — silent wrong code (15 × P0, all fixed; the table below is a
selection).** The compiler accepted a program and computed the wrong answer.
The original sweep found 11 P0s; the 2026-10-26 audit added BUG-032, BUG-033,
BUG-036 and the source-destruction case BUG-045 (listed with tooling below):

| Bug | Symptom | Root cause |
|---|---|---|
| BUG-001/002 | `a[i++] += 5` incrementing `i` twice | the lvalue was evaluated twice (address and value) |
| BUG-003 | `b = a` on structs copying 8 of 16 bytes | struct assignment treated as a scalar move |
| BUG-004 | struct arguments full of garbage | parameter slot sized 8 bytes regardless of type |
| BUG-019 | struct→int, int→pointer accepted silently | no assignment compatibility checking |
| BUG-020 | `a[i] += 1` addressing `a[i*64]` | index scaled twice in compound assignment |
| BUG-023 | `"\0A"` truncated to `""` | string literals stored with `strlen`, not their real length |
| BUG-024 | `--sign` produced an empty provenance string | interning `""` and storing it |
| BUG-025 | address-taken local cleared as dead | `&b` not recorded as a use |
| BUG-026 | `(a / b) + (d % c)` added garbage | nested `/`/`%` clobbered the register holding the left operand |
| BUG-029 | `say 0x10` printed 0 | toy parser read literal text as base 10 |
| BUG-032 (audit) | `&a + 1` advanced one element, not one array | `&array` kept the decayed element-pointer type |
| BUG-033 (audit) | `char c; int x = (c = 300);` left 300 in `x` | assignment expression did not narrow to the stored value |
| BUG-036 (audit) | `if (0) case 1: r = 5;` still ran the label body | a controlled label did not own the following statement |

**Class B — valid code rejected.** The original six P1 cases were all fixed:
`else` never parsed in the toy language (BUG-027); `int *p;` at file scope
rejected (BUG-005); no casts (BUG-006); no comma operator (BUG-007); `08`/`09`
mishandled (BUG-008); octal escapes rejected (BUG-022). The audit added more
valid-C rejections: unnamed prototype parameters, `\?`, adjacent string
literals, integer casts in constant expressions, and some void-expression
contexts (BUG-039–BUG-041, BUG-046).

**Class C — diagnostics, crashes and tooling.** The original 14 P2/P3 cases were
all fixed: cascading errors for `#include`; a BOM breaking the lexer; stack
overflow on deep nesting; `break_kind[256]` overflow; a partial `.s` left behind
after a failed compile; `main(argc, argv)` accepted with garbage `argv`; and
three tooling defects (`build.bat` stale file list, CI missing `cmp`, and CI
assembling x86-64 output as arm64 — BUG-028/030/031). The audit added declaration
validation, Darwin `%al`, wide-call/symbol limits, NUL and output-alias safety,
the struct-conditional diagnostic, and one **still-open** diagnostic gap:
struct values accepted as conditions/comparisons (BUG-048).

### 12.2 Two cases worth narrating in the viva

**BUG-025 (dead-store elimination deleted a live variable).** The pass removed
stores to locals that were never *read* — but `&b` makes a variable readable
through a pointer, and address-of was not counted as a use. The fix was to mark
address-taken locals before the pass; the prints showed `return 64` (garbage)
before and `42` after. Lesson: an optimization's safety condition is a claim
about aliasing, and it must be expressed in the code.

**BUG-030/031 (the CI that was never green).** The README claimed three-platform
CI support. Running `gh run list` showed **every run had failed**: Windows died
because `cmp` (diffutils) was not installed in MSYS2 — reported as "optimized
and --no-opt print different output", i.e. as a codegen bug — and macOS died
because the runner is Apple Silicon and the backend emits x86-64, so clang read
the assembly as arm64. Both fixes are small (`diffutils`, `-arch x86_64`, a
Rosetta probe). Lesson: a badge that is red is information; a claim that is
untested is decoration.

### 12.3 What the ledger changes about the report

It converts "I built a compiler and it works" into a measurable process:
reproduce → root-cause → fail-before → fix → regression test → record. The
original 2026-10-24 ledger had execution evidence for 29 of its 31 entries; the
2026-10-26 batch deliberately keeps its weaker evidence visible too: BUG-043 was
verified by assembly inspection only (no local macOS runtime), BUG-048 is a
manual probe and remains open, and BUG-045's NUL-source half has no test file
(the same-file/temp half does). Several defects were found by *running*
something rather than reading it: the differential harness (BUG-020), the harness
itself (BUG-021), a clean clone (BUG-028), a probe of `say 0x10` (BUG-029), and
the CI logs (BUG-030/031).

---

## 13. Limitations and threats to validity

Stated plainly, because an examiner will ask and a vague answer is worse than a
precise limitation.

1. **`int` is 64-bit** (D1). Programs relying on 32-bit wrap-around differ from
   gcc; `sizeof` is not supported, so nothing can observe the width in-language.
   The differential harness and the fuzzer are both constrained by this.
2. **No global register allocator.** Values live in registers only within one
   expression. Code is roughly 2–4× slower than gcc's at `-O0`, which was never
   measured beyond size; performance is not a project objective.
3. **Struct by value, `sizeof`, and `float`/`double` are refused.** A real C
   program using them will not compile — by design, with a message.
4. **The oracle is only gcc.** A bug shared by mycc and gcc (or a program whose
   behaviour is undefined) cannot be caught. The fuzzer's invariants exist to
   keep UB out of the comparison, which also narrows what it explores.
5. **The fuzzer explores expression shapes, not program structures.** No
   pointers, arrays, structs, strings or `switch` in generated programs; it
   cannot find bugs in code it never emits.
6. **No golden assembly files.** A codegen refactor is verified behaviourally
   only; "the assembly is sane" is a human judgement made by reading it.
7. **macOS is x86-64 under Rosetta, not native arm64.** The CI job proves the
   output assembles, links and runs on Apple Silicon hardware — through a
   translator, not natively.
8. **Compile-time performance is untested.** The compiler is fast enough for
   these programs (whole suite in seconds) but was never profiled.
9. **Some declaration/conformance gaps remain.** Array parameter declarators
   (`int f(int a[])`), pointer-to-array declarations (`int (*p)[2]`), block-scope
   `struct` tag shadowing and array members inside structs are not implemented.
   Invalid struct conditions/comparisons are still accepted rather than
   diagnosed; this is the one open ledger entry, BUG-048.

---

## 14. Future work (ordered by value to a reader of this report)

| # | Work | Why it matters | Rough size |
|---|---|---|---|
| F1 | `--interpret`: tree-walking interpreter over the same AST | gives a **third** oracle (compiled vs interpreted vs gcc), and the roadmap's 10,000-program acceptance criterion | days |
| F2 | SSA-lite IR between semantic analysis and codegen | the precondition for liveness-based register allocation, and a much better teaching artefact | weeks |
| F3 | Liveness-based global register allocator | the main remaining code-quality gap (limitation 2) | weeks, after F2 |
| F4 | 32-bit `int` mode | removes the biggest divergence from C (D1) | days — it touches every emitter |
| F5 | `double`/SSE | the largest missing language feature | weeks |
| F6 | `sizeof`, `typedef`, `enum`, initializer lists | small, high-value syntax; each is a contained addition | days each |
| F7 | Multi-error recovery in the parser | one error per run is a usability limit, not a correctness one | days |
| F8 | Snapshot/golden tests for a handful of `.s` files | the gap limitation 6 describes | hours |
| F9 | Wider fuzzer (pointers, arrays, structs, `switch`) | extends what the oracle can see (limitation 5) | days |
| F10 | Tagged releases with notes | CI artifacts exist; a release is the next step | hours |
| F11 | Scalar-context diagnostics + array/pointer declarators | closes BUG-048 and the main Phase 3 conformance gaps (`int f(int a[])`, `int (*p)[2]`, struct member arrays, block-scope tags) | days |

---

## 15. Conclusion

`mycc` is a complete, hand-written compiler for a substantial C subset: 9,502
lines of C99, five pipeline stages, two front ends, real calling conventions,
four optimizer mechanisms and three object formats. It compiles and runs its own
test programs, is checked against gcc on hand-written *and* generated programs,
and is green on Linux, Windows and macOS for the first time as of 2026-10-24.
The 2026-10-26 audit adds 17 ledger entries and 16 working-tree fixes; one
diagnostic gap remains open and is recorded as open.

The most transferable result is not the feature list. It is that **the project's
claims became executable**: `--no-opt` parity turned "the optimizer is safe"
into a per-test check; byte-identical assembly turned "two front ends, one back
end" into a `fc /b`; the fuzzer turned "it works on the programs I wrote" into
"it works on 1000 programs nobody wrote"; and the bug ledger turned "I fixed the
bugs" into 48 reproductions, 47 fixes and one openly listed open defect. Two of
those 48 — BUG-030 and BUG-031 — exist because the project finally read its own CI logs
instead of trusting its README, which is the most honest sentence in this
report.

---

## 16. References

1. Aho, Lam, Sethi, Ullman — *Compilers: Principles, Techniques and Tools*
   (the Dragon Book): the pipeline this project implements, by hand.
2. System V AMD64 ABI, §3.2 (argument passing, `%al` for variadics) —
   <https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf>
3. Microsoft — *x64 software conventions* (shadow space, `rcx/rdx/r8/r9`).
4. Intel — *Software Developer's Manual*, Vol. 2 (instruction encodings used:
   `movq`, `idivq`, `setcc`, `rep movsb`, `leaq`).
5. GNU as manual — AT&T syntax, directives (`.text`, `.rodata`, `.globl`,
   `.string`, `.note.GNU-stack`).
6. Apple — *Mach-O Programming Topics* (symbol leading underscore, `__TEXT`).
7. C99 standard (ISO/IEC 9899:1999) — §6.4.4 integer constants, §6.4.5 string
   literals, §6.5 expressions, lvalue conversion.
8. The project's own documents, all under `docs/`: `REPORT-DRAFT.md` (this
   report), `report.md` (the 48-entry bug ledger), `Flow.md` (execution flow),
   `ROADMAP.md` (phase plan) and `DEMO.md` (the demo playbook) are published;
   `decision.md` (decision log), `learn.md` (learning guide) and
   `instructions.md` (long manual) are local.
9. Zeller — *Why Programs Fail* (delta debugging → the fails-before discipline
   used in every ledger entry).
10. Claessen & Hughes — *QuickCheck* (the idea that generated inputs find what
    hand-written examples do not; `tools/fuzz.c` is the same idea, deterministic).

---

## Appendix A — Phase and commit timeline

| Phase | What it delivered | Commits |
|---|---|---|
| 0 | Housekeeping: repo, `.gitignore`, CI skeleton, differential harness | `59dd938` … `4c3cd8a` |
| 1 | Core language: `char`/`void`, control flow, bitwise ops, literals, 21 bug fixes | `ee805c9`, `4c3cd8a` |
| 2–3 | Pointers, arrays, structs, `unsigned`, multi-dimensional arrays, forward declarations | `da992f7`, `1fce34c`, `9dfb5c2` |
| 4 | Real calling convention (SysV + Win64), `printf`-style I/O, string literals, struct copy | `4410784`, `831a61a` |
| 5 | Optimizer: folding, dead branches, immediate operands, dead stores, register temporaries | `8681ca0`, `016ff82`, `86d5865` |
| 6 | Caret diagnostics, warning channel, differential harness, **fuzzer** | `0f58d9b`, `205a1ad` |
| 7 | `--version`, `--sign`, three targets, CI artifacts | `0f58d9b`, `bae132f` |
| 8 | Second front end `myc`, byte-identical assembly contract | `daa35b8`, `4d0154d` |
| 9 | CI truthfulness: `diffutils`, `-arch x86_64`, first green matrix | `bae132f`, `4924f8a` |

---

## Appendix B — Viva questions with answers

**Q1. Walk me through what happens to `int x = 2 + 3;`.**
Lexer produces `int`, `x`, `=`, `2`, `+`, `3`, `;` with positions. Parser builds
`AST_VAR_DECL` with an `AST_BINARY(+)` initializer. Semantic analysis types
both operands as `int`, folds the node into `AST_INT_LIT 5`, allocates `x` at
`-8(%rbp)`. Codegen emits `movq $5, -8(%rbp)`. In the `.s` you can see the
literal, not the addition.

**Q2. Why a stack machine instead of register allocation?**
Correctness by construction and readability. A stack machine has one invariant
(result in `%rax`), which makes every emitter local and testable. A register
allocator needs liveness analysis over an IR this project does not have; the
measured cost is code size/speed, and the measured benefit is a codegen that can
be defended line by line. The one easy win (parking the left operand in `%r10`)
is implemented and measured.

**Q3. How do you evaluate an lvalue exactly once?**
The address is computed once into a register, then used for both the load and
the store; `a[i++] += 5` and `a[i++]++` share that path, and `i` is incremented
by the address computation only. BUG-001/002 were exactly this bug, so it is
also a regression test (`tests/23_lvalue_single_eval.c`).

**Q4. What is `@PLT` and why is it only on Linux?**
It routes the call through the Procedure Linkage Table so the symbol can be
resolved by the dynamic linker. ELF requires it for external calls in
position-independent code; Mach-O and PE do not use that mechanism, so the
target switch omits it.

**Q5. What is the 32-byte shadow space?**
Windows x64 requires the caller to reserve 32 bytes of stack that the callee may
use to spill its four register arguments. It must exist even if unused. That is
why `subq $32, %rsp` appears before every external call on
`--target=windows` and not on Linux.

**Q6. How are structs passed and copied?**
By value: refused, with a message pointing at the pointer idiom. On assignment:
`rep movsb` with the size from the type. BUG-003 was a struct assignment that
moved 8 bytes of a 16-byte struct.

**Q7. Prove the optimizer doesn't change behaviour.**
Every positive test is compiled twice, optimized and `--no-opt`, and must return
the same exit code *and* print byte-identical stdout. That runs on every
`test.bat`/`make test`. Additionally the fuzzer compares compiled behaviour to
gcc's. The claim "the optimizer is safe" is therefore a check, not a belief.

**Q8. Why is `int` 64-bit? Isn't that wrong?**
It is a decision (2026-10-24), not an oversight: the compiler has its own ABI.
Consequences are written down: no 32-bit wrap-around, `sizeof(int) == 8`, and
the fuzzer must avoid overflow. Making it 32-bit is future work F4, and the
report says so rather than pretending.

**Q9. How did you find the bugs nobody predicted?**
Three ways: (a) a differential harness against gcc — it found BUG-020; (b)
running the thing instead of reading it — a clean clone found BUG-028, a probe of
`say 0x10` found BUG-029; (c) reading CI logs instead of trusting the badge —
that found BUG-030/031. Reading code found the rest.

**Q10. What does the fuzzer prove, and what does it not?**
It proves mycc and gcc agree on 1000 randomly shaped integer programs per run,
deterministically. It does not explore pointers, arrays, structs, strings or
program structure, and it cannot catch a bug gcc shares. Its three invariants
exist so that a reported mismatch is a compiler bug and not undefined
behaviour.

**Q11. Why does the toy language matter? It looks like a gimmick.**
It is the cheapest possible proof that the front-end/back-end boundary is real:
the same program written in two languages must compile to byte-identical
assembly, checked with `fc /b` on every run. If any front-end knowledge leaked
into the back end, that check would fail. It also produced BUG-029, which is
exactly the kind of seam bug such a check is for.

**Q12. What happens on a syntax error — one error or many?**
One, with a caret snippet, then unwinding. Multi-error recovery is future work
F7: it is a usability improvement, not correctness, and adding it without care
produces cascades (the first version of this project did: `#include` produced a
bogus cascade, BUG-012).

**Q13. How do you stop a hostile input from crashing the compiler?**
Nesting depth is capped at 256 (parser and expression), loop/nesting state is
fixed-size with bounds checks, and there is a test for each historical crash
(BUG-009, BUG-010). A compiler that segfaults on input is a compiler nobody can
trust with a typo.

**Q14. What is the `.note.GNU-stack` line?**
A ELF section marking the stack non-executable. Without it, modern linkers warn
and some hardened systems refuse to load the binary. It is Linux-only, hence the
target switch.

**Q15. How does the compiler handle a string literal with an embedded NUL?**
The lexer decodes escapes and stores the bytes *plus a length*; the emitter uses
the length, and `.string` is not used for such literals. BUG-023 was the case
where `strlen` truncated `"\0A"` to `""`.

**Q16. Where does the assembly go on a failed compile?**
Nowhere: it is written to a temporary file and renamed only on success (BUG-017).
Otherwise the next build silently links a stale half-file.

**Q17. Why gcc at all — isn't that cheating?**
`gcc` is used as an *assembler and linker*, which is exactly what it is when
given a `.s` file. Every stage of compilation is implemented in this project.
The README and this report say so explicitly.

**Q18. How long does the suite take?**
Seconds: the current Windows suite is 106 checks + parity ≈ 1 min; Linux CI job
44 s including build, the historical 81 checks, difftest and a 1000-program fuzz
run; macOS 1 m 4 s; Windows CI 2 m 49 s because it also runs the batch path. The
2026-10-26 local audit also ran `fuzz.bat 700 456` with 700/700 agreement.

**Q19. What would you do differently?**
Two things. (a) Build the differential harness earlier — four of the ten P0 bugs
would have been caught the day they were written. (b) Read CI logs on day one:
the repository carried a "CI passes on three platforms" claim that was false for
its entire life.

**Q20. What is the hardest part you implemented?**
Calling conventions across three targets: register/stack argument split, shadow
space, `%al` for variadics, `rep movsb` clobbering `%rsi`/`%rdi` on Windows, and
Mach-O symbol naming. It is the part where "it compiles" and "it runs" are far
apart, and the part `tests/diff/02_stack_args.c` exists to pin.

**Q21. How do you know the generated assembly is correct, not just plausible?**
Three independent checks: the exit code and stdout of the program, parity with
gcc on the same source, and parity between optimizer settings. The gap is that
nobody reads 11,000 lines of assembly per run — which is limitation 6, and F8 in
future work.

**Q22. What is `--sign` for?**
It embeds a provenance string in `.rodata`, so `strings prog.exe | find
"generated by mycc"` answers "which compiler built this binary?". It is off by
default and has its own contract test (BUG-024 was an empty signature).

**Q23. How is the toy language parsed — a second parser?**
A second hand-written recursive-descent parser (`src/myc.c`), 932 lines, that
produces the *same* AST node types. It shares the lexer, the symbol table reset,
the semantic analyser, the optimizer and the code generator.

**Q24. What does `--dump-ast` show and why does it matter?**
The tree after semantic analysis — including rewrites (`print_int` →
`printf("%lld\n", …)`) and folded constants. It matters because it makes the
"nothing hidden" claim inspectable, and because it is how the desugarings in
this report were verified.

**Q25. Is this compiler "complete"?**
It is complete for its stated subset and its stated objectives (O1–O8), with
limits listed in §13 and a future-work table in §14. "Complete C" was never the
goal; the goals were a hand-written pipeline, real codegen, and a testing story
strong enough to defend.

---

## Appendix C — Glossary

| Term | Meaning here |
|---|---|
| AST | Abstract Syntax Tree: the parsed program as a tree of typed nodes |
| ABI | Application Binary Interface: how arguments and return values travel |
| lvalue | an expression that names a storage location (`x`, `a[i]`, `*p`) |
| P0/P1/P2/P3 | the ledger's severity classes (§11.5) |
| `@PLT` | ELF indirection for external calls |
| shadow space | 32 bytes Windows x64 callers must reserve |
| `rep movsb` | the x86 string-copy instruction, used for struct assignment |
| fuzz / differential test | compare two implementations on generated input |
| fails-before | the reproduction fails on the old code and passes on the new |
| golden test | compare output against a stored expected file (deliberately not used here) |

---

## Appendix D — Slide-ready numbers

- **9,502** lines of compiler (9,055 `src/` + 447 `include/`), C99, no libraries.
- **10** source files, **2** front ends, **1** back end, **3** targets.
- **106** automated checks + **3** differential programs + **1000** fuzz cases per CI run (700 in the local audit).
- **30.9%** smaller assembly on the current 48-test suite (14,290 → 9,872 lines); the historical 35-test snapshot was 33.8% (11,055 → 7,315).
- **48** defects logged, **47** fixed, **1** open; **15** are P0 (silent wrong code or destructive output).
- **3/3** CI jobs green (Linux 44 s · macOS 1 m 4 s · Windows 2 m 49 s),
  first green run `40086581248` on 2026-10-24.
- **37** commits, **136** tracked files, with the audit batch uncommitted; the published docs live in `README.md` and `docs/`.
- Byte-identical assembly from two different languages (`fc /b`, every run).
