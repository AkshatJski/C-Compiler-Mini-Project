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
C-Compiler-Mini-Project/
├── Makefile            Build + test automation (Linux / macOS / WSL)
├── build.bat           Build script (Windows: compiles mycc.exe)
├── test.bat            Test script (Windows: positive + error suites)
├── difftest.sh         Differential test vs the host compiler (Linux / WSL)
├── difftest.bat        Differential test vs gcc (Windows)
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
│   ├── 05_advanced.c     && || !, ++/--, compound assignment, scoping
│   ├── 06_control_flow.c do-while, switch/case/default, break, continue
│   ├── 07_bitwise.c      & | ^ ~ << >>, precedence, compound forms
│   ├── 08_literals.c     hex/octal literals, suffixes, char constants
│   ├── 09_types_globals.c char type, global initializers, void, implicit 0
│   ├── 10_ternary.c      the conditional ?: operator
│   ├── 11_scope.c        for-init scoping, shadowing, nested blocks
│   ├── 12_pointers.c     & and *, pointer arithmetic, null checks
│   ├── 13_arrays.c       arrays, indexing, array-to-pointer decay
│   ├── 14_structs.c      structs, member access, ->, prototypes
│   ├── 15_nested_switch.c case labels nested in blocks/if/nested switch
│   ├── 16_member_incdec.c ++/-- on struct members (. and ->)
│   ├── 17_multidim_arrays.c  int m[3][4] / int t[2][3][4], row stride
│   ├── 18_unsigned.c     unsigned int / unsigned char semantics
│   ├── 19_comma_operator.c   the comma operator in for-steps and expressions
│   ├── 20_casts.c        (int) / (char *) / struct-pointer casts
│   ├── 21_global_pointers.c  global int*, global struct*, pointer returns
│   ├── 22_struct_assign.c    whole-struct copy (b = a), nested structs
│   ├── 23_lvalue_single_eval.c  a[i++] += n and a[i++]++ evaluate once
│   ├── 24_bom.c          a UTF-8 BOM at the start of the file
│   ├── 25_fall_off_end.c warns about falling off a non-void function
│   ├── diff/             programs compiled by mycc *and* gcc; the exit
│   │                     codes must match (see difftest.sh / difftest.bat)
│   └── errors/           Programs that must be rejected, with EXPECT-ERROR
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
- `int` — all values are 64-bit signed.
- `char` — one byte, signed: stored with `movb` and loaded with sign
  extension, so `char c = 200;` then reads back as `-56`.
- `unsigned`, `unsigned int` — same 64-bit storage as `int`, but the value is
  read as unsigned: `/` and `%` emit `divq` (not `idivq`), `>>` emits `shrq`
  (not `sarq`), and `< <= > >=` use `setb`/`setbe`/`seta`/`setae`.
  Bare `unsigned` means `unsigned int`, as in C. `signed` is accepted as the
  explicit spelling of the default.
- `unsigned char` — one byte, loaded with **zero** extension (`movzbq`), so
  `unsigned char c = 200;` reads back as `200`. Arithmetic on it still
  operates on a signed 64-bit value — with `c = 200`, `c - 300` is `-100`,
  not a wrapped-around huge number — which is exactly what C's
  integer-promotion rule does.
- `void` — only as a function return type (functions with side effects).
- **Pointers:** `int *p`, `int **pp`, `struct Node *n`; `&x` takes an
  address, `*p` reads/writes through one, and `p + n`, `p - n`, `p - q`,
  `++p`, `p++`, `p += n` scale by the element size. `0` is the null pointer.
  Pointer differences come back in *elements*, not bytes, and pointers
  compare as unsigned addresses. Pointers work as locals, as parameters, as
  **globals** (`int *gp;` — zero-initialized) and as **return types**
  (`struct Node *make(void)`).
- **Arrays:** `int a[10]`, `char buf[64]`, and any number of dimensions —
  `int m[3][4]`, `int t[2][3][4]` — indexed with `a[i]` / `m[i][j]`.
  An array used in an expression decays to a pointer to its first element,
  including when passed to a function. The **declaration** is checked
  (bounds must be positive integer constants; lists longer than 8
  dimensions are rejected); indexing is *not* bounds-checked at run time,
  as in C. Locals are **not** zero-filled, so initialize before reading.
- **Structs:** `struct Point { int x; int y; };` with `int`/`char`/pointer/
  struct members (including self-referential `struct Node *next`), access
  via `.` and `->`, nested structs (`o.in.x`), arrays of structs, struct
  pointers, and global structs (zero-initialized). Assignment copies the
  **whole** object (`b = a;`), including nested structs and arrays inside
  structs. Passing or returning a struct **by value** is rejected — take a
  pointer (`struct P *p`) instead.
- Integer literals in decimal (`42`), hexadecimal (`0x2A`), and octal
  (`052`), with optional `u`/`l` suffixes (e.g. `42u`, `0xFFL`). The
  suffixes are accepted but do **not** change the type — declare the
  variable `unsigned` to get unsigned semantics. A malformed literal is an
  error, never a silent zero: `08` and `09` (invalid octal digits), `0x`
  without digits, and an unterminated character constant are all reported
  at the offending column.
- Character constants with escapes: `'A'`, `'\n'`, `'\t'`, `'\0'`,
  `'\\'`, `'\''`, and hex escapes like `'\x41'`.
- A UTF-8 **byte-order mark** at the very start of the file is skipped, so a
  file saved by a Windows editor (`EF BB BF int main…`) compiles normally.

### Operators (with correct precedence)
| Group                | Operators                          |
|----------------------|------------------------------------|
| Comma                | `,`                                |
| Assignment           | `=  +=  -=  *=  /=  %=`            |
| Compound bitwise     | `&=  \|=  ^=  <<=  >>=`            |
| Conditional          | `?:`                               |
| Logical OR           | `\|\|`                             |
| Logical AND          | `&&`                               |
| Bitwise OR           | `\|`                               |
| Bitwise XOR          | `^`                                |
| Bitwise AND          | `&`                                |
| Equality             | `==  !=`                           |
| Relational           | `<  <=  >  >=`                     |
| Shift                | `<<  >>`                           |
| Additive             | `+  -`                             |
| Multiplicative       | `*  /  %`                          |
| Cast                 | `(type)expr`                       |
| Unary                | `-  +  !  ~`                       |
| Prefix               | `++x  --x`                         |
| Postfix              | `x++  x--`                         |

- The **comma operator** evaluates the left operand for its side effects and
  yields the right one: `a = (b = 1, b + 1)` sets `b` and then `a` to 2. Its
  main use is a `for` step — `for (int i = 0, j = n; i < j; i++, j--)`. It is
  *not* the comma that separates arguments: `f(a, b)` still takes two
  arguments, because an argument list is parsed at a higher precedence than
  the comma operator.
- **Casts** convert between the supported types: `(int)x`, `(char)x`,
  `(unsigned char)x`, `(unsigned int)x`, `(int *)p`, `(struct Node *)0`. A
  cast is also the explicit opt-in for the conversions that are otherwise
  errors — `(int *)some_int` is accepted where `p = some_int` is not.
- An lvalue with a **side effect** is evaluated exactly once, including on
  the left of `op=` and under `++`/`--`: `a[i++] += 5` increments `i` once,
  and `*p++ = 1` stores through the old `p`. (Both used to be wrong; see the
  bug ledger, `report.md`.)
- `&&` and `||` use **short-circuit evaluation**: the right operand is only
  evaluated if the left operand does not already decide the result. This
  means `0 && (1 / 0)` is safe (no division by zero).
- `?:` is right-associative: `a ? b : c ? d : e` means `a ? b : (c ? d : e)`.
- `%` follows C semantics: the result has the sign of the dividend
  (truncated toward zero) for signed operands; `>>` is arithmetic
  (sign-filling) on signed values and logical (zero-filling) on `unsigned`
  and on pointers. Both are chosen by `binary_is_unsigned()`, so the same
  source picks `idivq`/`sarq` or `divq`/`shrq` from the operand types.
- `!x`, `-x`, `~x`, `x += y`, and `++x` are implemented by *desugaring*: the
  parser rewrites them into simpler forms the code generator already knows
  (see section 3).

### Statements
- Blocks `{ ... }`
- Local variable declarations with or without initializers, e.g.
  `int a;`, `int a = 5;`, and `char c = 'x';`
- Multiple declarators: `int a, b = 3, c;`
- `if (cond) stmt` and `if (cond) stmt else stmt`
- `while (cond) stmt`
- `do stmt while (cond);` — runs the body at least once
- `for (init; cond; step) stmt` — `init` may be a declaration
  (`for (int i = 0; ...)`), an expression, or empty
- `switch (expr) { case K: ... default: ... }` with C fallthrough
- `break;` (leaves the innermost loop or switch) and `continue;` (next
  iteration of the innermost loop)
- `return expr;` and `return;`
- Nesting is **bounded**: blocks and expressions deeper than 256 levels are
  rejected with `error: nesting too deep (limit is 256 levels)` (or
  `expression nested too deeply`) instead of crashing the compiler.

### Functions
- Global functions returning `int`, `unsigned int`, or `void`, with `void` or
  empty parameter lists. A pointer return type is allowed
  (`int *pick(void)`, `struct Node *make(void)`). (`char` and
  `unsigned char` return types are not supported — use `int` or
  `unsigned int`.)
- Up to **6 parameters** (the x86-64 register-argument limit we support).
  Parameters may be scalars, pointers, or arrays (which decay to pointers);
  a **struct by value** is rejected with a message suggesting the pointer
  form.
- `main` must be declared `int main(void)`. Programs without `main`, or that
  *call* a function which is declared but never defined, are rejected during
  compilation rather than failing later at link time.
- **Recursion** (functions call themselves safely).
- **Forward references**: a function may call another function that is
  defined later in the file.
- Nested function calls, and calls inside expressions:
  `add(mul(a, b), divq(a, b))`.
- A non-`void` function that can reach its closing brace returns `0`
  (C only guarantees that for `main`) **and** produces a warning:
  `warning: control reaches end of non-void function 'f'`. The exit status is
  unaffected — warnings are advisory (see "Warnings" below).

### Variables and scope
- Global variables, including ones with **constant-folded initializers**
  (`int g = 2 * 3 + 4;`); uninitialized globals are zero-filled in `.data`.
- Local variables (stack-allocated in the function's own frame).
- Proper **block scoping**: an inner block may declare a variable with the
  same name as one outside it (shadowing), and the outer variable is
  untouched after the block ends. A `for (int i = ...)` declaration is
  loop-local and does not leak into the enclosing scope.

### Comments
- `// line comment` and `/* block comment */`

### Error reporting
The compiler reports problems with file, line, and column, for example:

```
prog.c:3:9: error: undeclared variable 'y'
prog.c: compilation aborted during semantic analysis
```

Unsupported constructs get one precise message instead of a cascade — a
string literal says `error: string literals are not supported by mycc`, and a
preprocessor line says `error: preprocessor directives are not supported by
mycc (no #include, #define, #if)`. Neither is followed by bogus follow-on
errors: the lexer keeps reading after reporting.

A failed compilation also leaves **no output file** behind: the assembly is
written to a temporary file next to the target and only renamed into place
when generation succeeds. A stale `.s` from an earlier successful build
therefore survives a failed rebuild untouched.

### Warnings (advisory diagnostics)
Some problems do not stop the compile, so they are reported as warnings:

```
prog.c:2:1: warning: control reaches end of non-void function 'maybe'
```

- The format is `<file>:<line>:<col>: warning: <message>`.
- A warning **never** changes the exit status: `mycc` still returns 0 and
  writes the assembly.
- `-w` silences the whole warning channel. Errors are never silenced.

```c
int maybe(int a) { if (a) return 1; }   /* warning: control reaches end … */
```

### Where warnings sit
There is exactly one warning today (falling off the end of a non-`void`
function). The mechanism — a separate counter from `report_error()`, plus the
`-w` switch — exists so that later passes (unused variables, sign
comparisons, …) can be added without touching the error path.

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
  produces a normal `0`/`1` value. Signed operands use
  `setl`/`setle`/`setg`/`setge`; `unsigned` operands and pointers use the
  unsigned `setb`/`setbe`/`seta`/`setae`, so pointer ordering and large
  `unsigned` values compare correctly.

- **Division and modulo** use the signed divide instruction: `cqto` sign
  extends the dividend into `%rdx:%rax`, `idivq %r10` computes quotient in
  `%rax` and remainder in `%rdx`. `%` simply moves `%rdx` into `%rax`.
  Unsigned operands instead **clear** `%rdx` (`xorq %rdx, %rdx`) and use
  `divq` — `cqto` would be wrong there, because `divq` treats `%rdx:%rax`
  as an *unsigned* 128-bit dividend. Both shapes are emitted from one case
  in `gen_binary`, keyed off `binary_is_unsigned(left, right)`.
  (`%r10` is a caller-saved scratch register, so this does not clobber a
  callee-saved register without preserving it.)

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
  addressing (`movq g_count(%rip), %rax`). Their initializers are evaluated
  at compile time by a small constant folder, which emits `.quad` for `int`
  and `.byte` for `char`; anything non-constant is reported as an error.

- **`char`** is one byte. Stores emit `movb` (truncating to the low byte).
  Loads emit `movsbq` (sign-extending) for `char` and `movzbq`
  (zero-extending) for `unsigned char`, so a stored `200` reads back as
  `-56` or `200` respectively — matching signed/unsigned one-byte types on
  both Linux and Windows. Struct members, `*p`, `a[i]`, and global loads all
  route through one `load_op_for()` helper so the choice is made in exactly
  one place.

- **Pointers, arrays, and structs share one address model.** `a[i]` is
  desugared by the *parser* into `*(a + i)` (before any type information
  exists), then semantic analysis turns `array + int` into `base + i *
  sizeof(elem)` by inserting the scale factor into the tree, so the code
  generator only ever adds and dereferences. `&x` is `gen_addr` on the
  operand, `*p`'s address *is* its value, and `p.x` is `base + offset`
  where the offset comes from the struct member list (members are aligned
  and padded as they are added, so `char` then `int` get a hole between
  them). An array-typed *dereference* is the one exception to the "load"
  rule: on `int m[3][4]`, `m[i]` must yield the row's **address**, not the
  value stored in its first slot, so `gen_expr(AST_DEREF)` skips the load
  whenever the result type is an array.

- **Multi-dimensional arrays** fall out of that same model once the parser
  applies dimensions *inside out*. `int m[3][4]` is read left to right but
  built as `array[4] of int` first, then `array[3] of (array[4] of int)` —
  giving a 32-byte row stride. `m` decays to a pointer whose element type
  is `int[4]`, so `m + i` scales by 32, the inner dereference yields
  `int[4]`, and `+ j` then scales by 8. Applying dimensions in source order
  instead would silently produce a 16-byte stride and corrupt every
  `m[i][j]`.

- **`unsigned` is a flag on the type, not a new kind.** `Type.is_unsigned`
  lets `unsigned int` keep `TYPE_INT` and `unsigned char` keep `TYPE_CHAR`,
  so every existing check keeps working and only two things change: loads
  pick `movsbq` vs `movzbq`, and `gen_binary` picks the signed/unsigned
  instruction for `/`, `%`, `>>`, and the ordered comparisons. The decision
  is deliberately *not* "any unsigned operand": a narrow `unsigned char`
  does not make an operation unsigned (it is zero-extended into the 64-bit
  domain, which is exactly C's integer promotion), while `unsigned int`,
  which can hold values above 2^63, does — as do pointers.

- **`switch` / `break` / `continue`.** Codegen keeps two stacks of
  target labels: one for `break` (pushed by loops *and* `switch`) and one for
  `continue` (pushed only by loops). `break` jumps to the top of the break
  stack and `continue` to the top of the continue stack, so a `break` inside
  a `switch` nested in a loop leaves only the switch, while `continue` inside
  a switch continues the enclosing loop. A `switch` compiles to a chain of
  `cmpq`/`je` against each `case` value plus a jump to `default`. A `case` or
  `default` label may sit anywhere inside the switch body — nested in a block,
  an `if`, or a loop — so the labels are collected recursively (a third stack
  tracks the innermost enclosing switch, and each label is emitted at its
  source position, including inside nested compound statements). A nested
  `switch` owns its own labels and numbering.

- **Lvalues are evaluated once.** `x op= e` is still desugared into
  `x = x op e`, but the *code generator* now recognises the compound-assign
  shape, computes the address of `x` **once**, keeps it in a stack slot, and
  loads/stores through it (`gen_compound_assign`). The same trick serves
  postfix `++`/`--` on computed lvalues: the address is formed once, the old
  value read through it, and the new value stored back. Before this, `a[i++] += 5`
  incremented `i` twice — the AST still shows two occurrences of `a[i++]`,
  which is why the fix had to live in codegen (and why `--dump-ast` shows the
  desugared form, not the fixed one).

- **…and once only.** The parser rewrites `a[i]` into `*(a + i)`, and semantic
  analysis *inserts* the `index * sizeof(elem)` multiplication into the tree
  so codegen only ever adds. That insertion is not idempotent: walking the
  same node twice scaled the index twice, so `a[i] += 1` addressed
  `a[i * 64]` on 64-bit elements. The compound-assignment case now checks only
  the right-hand operand and types the operation itself, so each node reaches
  the scaling code exactly once (`scale_index_operand`).

- **Casts.** `(type)expr` becomes an `AST_CAST` node. Semantics simply
  replaces the value's type; codegen emits nothing where the representation is
  unchanged (pointer ↔ pointer, `int` ↔ `unsigned`), and a truncate/extend
  pair where it is not (`(char)x` → `movsbq`, `(unsigned char)x` → `movzbq`).
  Because the result type is known, a cast also legalises the conversions that
  assignment otherwise rejects.

- **The comma operator** is one extra precedence level: `parse_expression()`
  loops over `parse_assignment()` separated by `,`, building a left-associative
  node whose codegen is "evaluate left for effect, then evaluate right and keep
  its value" — no instruction is emitted for the operator itself. Argument
  lists keep calling `parse_assignment()`, so `f(a, b)` is unaffected.

- **Whole-struct copy** uses `rep movsb` with `%rsi` = address of the source,
  `%rdi` = address of the destination and `%rcx` = `type->size`. All three are
  caller-saved argument registers, and codegen holds nothing live in them at
  that point (the `pushed_count` bookkeeping keeps the stack aligned across
  calls), so one instruction copies a struct of any size, nested structs
  included. This is the one place codegen deliberately clobbers argument
  registers, and it is commented as such.

- **Warnings are a separate channel.** `report_warning()` mirrors
  `report_error()` but increments its own counter and never affects the exit
  status; `-w` flips a file-static flag that `error_reset()` deliberately does
  not touch, so the switch survives argument parsing. Semantic analysis asks
  "can this function reach its closing brace?" (`stmt_terminates`, a small
  recursive path analysis) and warns once per function when the answer is yes.

- **Never crash on input.** Two explicit depth guards keep hostile input from
  killing the process: the parser counts recursion depth in
  `parse_statement`/`parse_primary`/`parse_unary`, and codegen's
  `break`/`continue`/`switch` target stacks check their depth before writing.
  Both share one limit (`MYCC_MAX_NEST`, 256) and report a normal
  `file:line:col` error. The same value is documented in "Not supported", so
  the limit is part of the language's contract rather than an accident.

- **One code generator, three object formats.** Instruction selection is
  identical everywhere; only symbol naming and one directive differ. A
  `TargetKind` (`TARGET_LINUX` / `TARGET_WINDOWS` / `TARGET_DARWIN`) is picked
  from the host at build time and overridable with `--target=`; a single
  `sym_name()` helper is the only place a program symbol is spelled, so
  Mach-O's leading underscore is applied to definitions and references alike,
  and the ELF-only `.note.GNU-stack` directive is skipped on the other two.

---

## 4. Which platform? (choose one)

The compiler itself is portable, and its assembly output works with gcc on
**both** Windows and Linux. Pick whichever you have:

| Option | Where you run it | What you need |
|--------|------------------|---------------|
| **A. Windows (native)** | PowerShell / CMD on Windows | MinGW-w64 gcc (free, see below) |
| **B. Linux via WSL** | A `wsl` shell inside Windows | gcc + make: `sudo apt install build-essential` |

If your main device is Windows, **use Option A** — the compiler builds to a
real `mycc.exe` and the whole workflow runs in PowerShell with no WSL
involved. Option B suits you if you are more at home in a Linux shell; it
needs a one-time package install (below).

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

### Option B requirement: gcc and make

WSL runs Ubuntu, which does **not** ship with a compiler. Install the
standard toolchain once, from any WSL shell:

```
sudo apt update
sudo apt install build-essential
```

`build-essential` pulls in gcc, make and the libc development headers —
everything this project needs. Check it worked:

```
gcc --version
make --version
```

Both must print a version line. No `export` lines, helper wrappers, or
`CC=` overrides are required: plain `gcc` and plain `make` do the job
(section 6).

---

## 5. Option A — Build, test, and showcase on Windows (recommended)

Open **PowerShell** in the project folder:
```
cd "C:\Folder D\Feature_anant\C-Compiler-Mini-Project"
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

This compiles every `tests\*.c` (positive tests), assembles each into an
`.exe`, runs it, and compares the exit code with the `EXPECT:` marker in the
file. A `WARN:` marker additionally requires the compile to succeed *and*
print that warning text. It then runs every `tests\errors\*.c` and checks that
`mycc` rejects it with the diagnostic named by `EXPECT-ERROR:`, and finally
checks two contracts: a failed compile leaves no output file, and `-w`
silences the warning channel. **Expected output (abridged):**

```
PASS: tests\01_arithmetic.c (exit 0)
PASS: tests\02_conditionals.c (exit 0)
...
PASS: tests\25_fall_off_end.c (exit 0)
PASS: tests\errors\break_outside.c (rejected)
...
PASS: tests\errors\void_variable.c (rejected)
PASS: failed compile leaves no output file
PASS: -w silences warnings without failing the build

All tests passed
```

`difftest.bat` then goes one step further: it compiles each
`tests\diff\*.c` **twice** — once with `mycc.exe`, once with gcc — runs both
and requires the same exit code. The exit-code suite cannot tell "correct"
from "wrong but plausible"; this can.

```
PASS: tests\diff\01_compound_and_lvalue.c (both returned 0)

All differential tests passed
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
  pass it explicitly: `mycc.exe --target=windows example.c -o out.s`. The
  other flavours are `--target=linux` (ELF, the default on Linux and WSL) and
  `--target=darwin` (Mach-O symbols for macOS). An unknown name is a clean
  error: `error: unknown target 'beos' (use linux, windows or darwin)`.
- `-w` silences warnings (errors are unaffected), e.g.
  `mycc.exe -w src.c -o out.s`.
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

### 6.1 Enter WSL and verify the toolchain

Run these in **PowerShell** first, then in the Linux shell:

```
wsl
```

Your prompt should now end in `$` (not `PS`). The Windows folder
`C:\Folder D\...` is visible inside WSL at `/mnt/c/Folder D/...`. Now run:

```
cd "/mnt/c/Folder D/Feature_anant/C-Compiler-Mini-Project"
```

Confirm gcc and make are available (install them first if not — see
"Option B requirement" in section 4):

```
gcc --version
make --version
```

Sanity check:
```
ls
```
Expected: `Makefile  build.bat  build  demos  difftest.bat  difftest.sh  example.c  include  instructions.md  mycc  README.md  ROADMAP.md  src  test.bat  tests`

### 6.2 Build the compiler

```
make clean
make
```

- `make clean` removes old build artifacts.
- `make` compiles `src/*.c` into the `mycc` binary.

Expected: compile lines and no error messages.

### 6.3 Run the automated test suite

```
make test
```

Expected output (abridged):

```
PASS: tests/01_arithmetic.c (exit 0)
PASS: tests/02_conditionals.c (exit 0)
...
PASS: tests/25_fall_off_end.c (exit 0)
PASS: tests/errors/break_outside.c (rejected)
...
PASS: tests/errors/void_variable.c (rejected)
PASS: failed compile leaves no output file
PASS: -w silences warnings without failing the build
All tests passed
```

Then the differential test, which compiles `tests/diff/*.c` with both mycc and
the host compiler and requires identical exit codes:

```
make difftest
```

```
PASS: tests/diff/01_compound_and_lvalue.c (both returned 0)
All differential tests passed
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
./mycc example.c -o /tmp/demo.s
gcc -no-pie -o /tmp/demo /tmp/demo.s
/tmp/demo
echo $?
```

The last line prints `197` — same result as Windows.

### 6.6 Try your own program (WSL)

Create a test file by typing the `cat` command, the program, then **Ctrl+D**
to finish:

```
cat > /tmp/test.c
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
./mycc /tmp/test.c -o /tmp/test.s
gcc -no-pie -o /tmp/test /tmp/test.s
/tmp/test
echo $?
```

`7 * 3 + 1 = 22`, so the last line should print:

```
22
```

### 6.7 See the error reporting (WSL, optional)

```
printf 'int main(void){ return missing_var; }\n' > /tmp/err.c
./mycc /tmp/err.c
```

Expected:

```
/tmp/err.c:1:22: error: undeclared variable 'missing_var'
/tmp/err.c: compilation aborted during semantic analysis
```

`1:22` means line 1, column 22 — exactly where the mistake is.

---

## 7. Supported-language quick reference

```c
/* This is a valid mycc program. */

int g_count;                    /* global (zero-initialized) */
char g_last = 'A';              /* global with a folded initializer */

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
    char c = 0x41;              /* char holds one byte: 'A' */
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
    switch (s & 1) {            /* switch + bitwise + fallthrough */
        case 0: s += 1;         /* (not reached here) */
        default: s += 0;
    }
    do { s += (c == 'A') ? 0 : 100; } while (0);   /* do-while + ?: */
    g_count++;
    return s;                   /* returns 11 */
}
```

The memory and `unsigned` side of the language:

```c
struct Point { int x; int y; };

int sum_row(int *row, int n);            /* prototype */

int matrix[2][3];                        /* global 2-D array, zero-filled */

int main(void) {
    int m[3][4];                         /* local 2-D array */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            m[i][j] = i * 10 + j;

    int *row = m[1];                     /* a row decays to int* */
    if (row[2] != 12) return 1;
    if (m[2] - m[1] != 4) return 2;      /* row stride = 4 ints */

    struct Point p;                      /* struct, dot access */
    p.x = 5;
    struct Point *pp = &p;               /* pointer to struct */
    pp->y = 7;                           /* arrow access */
    if (p.x + p.y != 12) return 3;

    int *q = &m[0][0];                   /* address-of a nested element */
    *q = 100;                            /* write through the pointer */
    if (m[0][0] != 100) return 4;

    unsigned big = 1;
    big = big << 63;                     /* 0x8000000000000000 */
    if (big / 2 != 4611686018427387904) return 5;   /* unsigned, not negative */
    if ((big >> 63) != 1) return 6;      /* logical shift, not arithmetic */

    unsigned char c = 200;               /* zero-extended byte */
    if (c != 200) return 7;
    char s = 200;                        /* sign-extended byte */
    if (s != -56) return 8;

    if (sum_row(m[2], 4) != 86) return 9;/* 20+21+22+23 */
    return 0;
}

int sum_row(int *row, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += row[i];
    return s;
}
```

### The newest additions: commas, casts, global pointers, struct copy

This program returns 0 (verified against gcc; also in `tests/19`–`tests/23`):

```c
struct Node { int val; struct Node *next; };

int *g_head;                     /* global pointer, zero-initialized */
int g_ints[4];                   /* global array */
struct Node g_nodes[4];          /* global array of structs */

int *pick(int *base, int i) {    /* pointer return type */
    return base + i;             /* scaled by sizeof(int) */
}

int main(void) {
    /* comma operator: two counters in one for step */
    int pairs = 0;
    for (int i = 0, j = 6; i < j; i++, j--) pairs++;
    if (pairs != 3) return 1;

    /* comma as an expression: evaluates left for effect, yields right */
    int b = 0;
    int a = (b = 5, b + 1);
    if (a != 6 || b != 5) return 2;

    /* casts: explicit conversions, including legalising int -> pointer */
    char c = (char)300;              /* truncates to 44 */
    if (c != 44) return 3;
    int *p = (int *)0;               /* null, written explicitly */
    if (p != 0) return 4;

    /* struct assignment copies every member */
    struct Node n1;
    n1.val = 7;
    n1.next = (struct Node *)0;
    struct Node n2 = n1;             /* or: struct Node n2; n2 = n1; */
    if (n2.val != 7) return 5;

    /* a global pointer filled by a pointer-returning function */
    g_ints[2] = 9;
    g_head = pick(g_ints, 2);
    if (g_head != &g_ints[2] || *g_head != 9) return 6;

    /* pointer arithmetic over a global array of structs scales by the
     * struct size, not by sizeof(int) */
    g_nodes[1].val = 4;
    struct Node *np = &g_nodes[0];
    np = np + 1;
    if (np->val != 4) return 7;

    return 0;
}
```

### Not supported (by design)
- No `float`/`double`, `long`/`short`, `enum`/`typedef`, or bitfields.
  (`unsigned` *is* supported — see §2 Data. `int` stays 64-bit, so
  `unsigned int` is 64-bit too.)
- No aggregate initializers: `int a[2] = {1,2}` and
  `struct Point p = {1,2}` are rejected — assign element by element.
  Uninitialized **local** objects are therefore indeterminate, not zero.
- No **struct by value**: passing or returning a struct is an error; take a
  pointer. Everything else about structs (members, `->`, nested, arrays,
  whole-struct copy) works.
- No run-time bounds checking on `a[i]` (indexing trusts the programmer, as
  in C), and no `sizeof`.
- Nesting deeper than 256 levels (blocks or expressions) is rejected; there
  is no way to raise that limit except rebuilding.
- Function return type must be `int`, `unsigned int`, `void`, or a pointer
  type; `char` returns are rejected. Function *parameters* are declared as
  pointers — `int f(int a[10])` is a syntax error; write `int f(int *a)`
  instead.
- No `#include`, no preprocessor, no string literals, no printing.
  Programs communicate results through their **exit code** (`return`).
- `switch` supports integer/enum-style cases only — no `range` cases, and
  `case` labels must be compile-time constants.

---

## 8. Troubleshooting

| Symptom                                          | Fix |
|--------------------------------------------------|-----|
| `gcc is not recognized as the name of a cmdlet...` or `'gcc' is not recognized` | MinGW is not on `PATH` (or not installed). See section 4, Option A. |
| `'.\build.bat' is not recognized`                | Run in PowerShell/CMD from the project folder (section 5). |
| `mycc.exe` not found                             | Run `.\build.bat` first (section 5.1). |
| `export: The term 'export' is not recognized...` | You are in PowerShell but followed the WSL steps. Run `wsl` first (section 6.1), or use the Windows steps in section 5. |
| `make: command not found`                        | make is not installed in WSL — `sudo apt install build-essential` (section 4, Option B). |
| `gcc: command not found` (in WSL)                | gcc is not installed in WSL — `sudo apt install build-essential` (section 4, Option B). |
| `cannot open source file '...'`                  | The file does not exist, or the path has a typo/spaces issue; quote the path. |
| `error: unexpected character`                    | The program uses a feature the language does not support (see section 7). |
| `error: preprocessor directives are not supported` / `string literals are not supported` | `#include`, `#define`, `#if` and `"..."` do not exist in the subset (section 7). |
| `error: nesting too deep (limit is 256 levels)`  | The program nests blocks/expressions more than 256 deep — restructure it. |
| `warning: control reaches end of non-void function` | A non-`void` function can reach its closing brace. Add a `return`, or silence it with `-w`. |
| `make difftest` prints a mismatch                   | mycc and gcc disagree: a real miscompilation. Worth reporting/fixing. |
| `FAIL: ... expected exit X, got Y`               | A genuine compiler bug — this is worth reporting/fixing. |
| Linker errors like `undefined reference to 'helper'` | Older builds only: mycc now rejects a declared-but-undefined function during compilation (section 2). |
| `mycc` not found (in WSL)                        | You are not in the project folder, or the build in 6.2 has not been run. |
| `-no-pie: unknown argument` (macOS)              | You are linking by hand on macOS — drop `-no-pie` (the Makefile does this for you). |

---

## 9. Quick-start recap

**Windows (PowerShell)** — fastest path:

```
cd "C:\Folder D\Feature_anant\C-Compiler-Mini-Project"
.\build.bat
.\test.bat        # all tests should pass
.\difftest.bat    # mycc and gcc must agree on tests\diff\*.c
.\showcase.bat    # should print "The program returned: 197"
```

**Linux (WSL)**:

```
cd "/mnt/c/Folder D/Feature_anant/C-Compiler-Mini-Project"
make clean
make
make test                              # all tests should pass
make difftest                          # mycc and gcc must agree
./mycc example.c -o /tmp/demo.s
gcc -no-pie -o /tmp/demo /tmp/demo.s
/tmp/demo
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
   by-file line count of the ~5,800-line hand-written compiler.

Great talking point: the same tree you can print with `--dump-ast` is
exactly what the code generator walks to emit assembly.

### The one-line explanations

- "A compiler turns readable code into CPU instructions. gcc does this
  inside a black box; mycc is ~6,200 lines of C that shows you every step."
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
- **Read the whole compiler**: `src/` is ~5,800 lines across 8 files.
  `src/codegen.c` is the shortest path to "aha" — it is just a walk over
  the syntax tree emitting instructions.
- **Catch a mistake live**: feed mycc a broken program and show the
  `file:line:col` error pointing exactly at the problem.
- **Show the parser rewriting your code**: `demos/desugar.c` is full of
  sugar (`+=`, `a++`, `!a`, `-b`); `mycc --dump-ast demos\desugar.c`
  prints the simplified tree the compiler actually uses. `demos/broken.c`
  is pre-filled with two errors to demonstrate semantic analysis.
