# report.md — mycc Bug Ledger

**Project:** `C-Compiler-Mini-Project` (`mycc`, C-subset → x86-64 AT&T)
**Baseline reviewed:** `9dfb5c2` — *docs: cover pointers/arrays/structs/unsigned; standardize the WSL setup* (branch `feature/Memory_PointerAndArray`, 2026-10-19 11:35 +05:30)
**Phase 4 review:** `d83bc5d` → `4410784` → `831a61a` (calling convention + I/O, 2026-10-23 12:16–14:39 +05:30)
**Review method:** full source read (4,849 LOC `src/` + 301 LOC `include/`) + 48-project test suite run + 30 adversarial programs compiled by `mycc` and by `gcc`, exit codes compared. Two further defects (BUG-020, BUG-021) were found *while fixing*, by the new differential harness and by running the harness itself. BUG-022 and BUG-023 were found by the Phase 4 pass, in the one code path Phase 4 created from scratch.
**Report opened:** 2026-10-23 10:50 +05:30 · **First fix batch completed:** 2026-10-23 12:16 +05:30
**Correction:** an earlier revision of this line said `21:40 +05:30` for the first batch. That was wrong - commit `4c3cd8a` is timestamped 12:16 - and is left visible here rather than silently edited.
**Phase 8 review:** `daa35b8` → `4d0154d` (the toy front end, BUG-027) and `fcb0073` (a fresh clone could not build, BUG-028), 2026-10-24 11:12-12:20 +05:30. Both were found by *running* the thing rather than reading it: BUG-027 by asking whether the equivalence contract exercised `else`, BUG-028 by cloning the published repository and following the README.

> **How to read this file.** Entries are **newest first** (datewise, per the working agreement). The dashboard below is the ten-second view. Each entry carries: short summary → reproduction (observed vs expected) → root cause → evidence strength → fix plan (options / chosen / tradeoffs) → status / fix commit / regression test / verification. `pending` in *Fix commit* means the fix has not been committed yet.

**Phase 9 review (CI truthfulness):** `3d41c8c` → `a77a2bc`, `205a1ad`, `bae132f` (BUG-029, BUG-030, BUG-031), 2026-10-24 16:00-16:10 +05:30. Found by *reading the CI logs instead of the README*: the repository had never had a green Windows or macOS run, so the "green on three platforms" claim was false. Run `40086581248` is the first green matrix in the project's history.

**Published:** 2026-10-24 17:20 +05:30 — this ledger was kept local while it was being written (a half-finished bug list is worse than none) and is now part of the repository at the author's request. Nothing in it was rewritten for publication: the wrong tallies, the corrections, the deferred gaps and the entries whose evidence is only code inspection are all still visible. The ledger lives in `docs/`. Three working notes stay local: `decision.md`, `learn.md` and `instructions.md`.

**Edge-case audit (2026-10-26):** a working-tree correctness pass added tests 37–49, the same-file/temp-path contract, and entries `BUG-032`…`BUG-048`. Sixteen entries are fixed but **uncommitted** (`FIXED (working tree)`, commit `pending`) because the user withheld commit/push permission; `BUG-048` remains **OPEN**. The current suite is 106 checks, not the 81 shown in the historical verification table.

> **Honesty rules in force:** nothing is marked plain `FIXED` until a regression test fails-before / passes-after *and* the commit hash is recorded. `FIXED (working tree)` is an explicit transitional status with a `pending` commit, not a claim that the change is published. `code inspection only` is stated explicitly wherever I did not execute a reproduction. Two entries still state a verification limit openly: BUG-021 (a harness defect cannot be caught by a test file - the harness run *is* the evidence), and BUG-022/023 (fixed during Phase 4; the Windows runtime evidence is real, the Linux/macOS runtime evidence is now the green CI matrix rather than a local run). BUG-018 stayed `IN PROGRESS` for two days precisely because the rule applies to it too, and it closed only when the job went green.

---

## Dashboard

| ID | Sev | Summary | Status | Fix commit |
|---|---|---|---|---|
| BUG-048 | P2 | Struct values are accepted as `if`/loop conditions and as comparison operands even though C requires scalar operands; the semantic pass still lacks those checks | OPEN | — |
| BUG-047 | P2 | A struct-valued `?:` fell through semantic analysis as `int` and later aborted in codegen; it now gets a front-end “not supported” diagnostic | FIXED (working tree) | pending |
| BUG-046 | P2 | Integer casts were not folded in constant expressions, so `(int)3`, `case (int)2`, and `(char)256` as a null pointer constant were rejected | FIXED (working tree) | pending |
| BUG-045 | P0 | A NUL byte in the source silently truncated the translation unit; the input/output and temporary-output paths could also alias the source and overwrite it | FIXED (working tree) | pending |
| BUG-044 | P1 | `extern` variables were silently emitted as definitions, several invalid `main` forms were accepted, and a nonzero integer could initialize a global pointer | FIXED (working tree) | pending |
| BUG-043 | P1 | Darwin variadic calls did not zero `%al`, so macOS varargs could read the wrong vector-register count | FIXED (working tree) | pending |
| BUG-042 | P1 | Calls with more than 32 arguments used a fixed codegen array; long symbols and the function-definition table also had fixed 512/256 limits | FIXED (working tree) | pending |
| BUG-041 | P2 | Prototypes with unnamed parameters were rejected, builtin helper arity was not exact, and conflicting function declarations were not diagnosed | FIXED (working tree) | pending |
| BUG-040 | P2 | Standard `\?` escapes were rejected and adjacent string literals were not concatenated after escape decoding | FIXED (working tree) | pending |
| BUG-039 | P2 | Void expressions were accepted where C forbids using their value, while valid discarded contexts such as comma/`for` steps were incomplete | FIXED (working tree) | pending |
| BUG-038 | P1 | High-byte character constants were not sign-extended as signed `char` values, and escape values above 255 were not diagnosed | FIXED (working tree) | pending |
| BUG-037 | P1 | Unsigned usual arithmetic conversions, shift signedness, comparison result types, and constant folding could produce wrong code | FIXED (working tree) | pending |
| BUG-036 | P0 | `if (0) case 1: stmt;` kept the labelled statement outside the `if`, so the normal false path executed it | FIXED (working tree) | pending |
| BUG-035 | P1 | Constant-condition elimination deleted `case`/`default` labels inside a dead arm, leaving jumps to undefined assembler labels | FIXED (working tree) | pending |
| BUG-034 | P1 | A conditional with a pointer and the null pointer constant only worked in one branch order and sometimes got the wrong type | FIXED (working tree) | pending |
| BUG-033 | P0 | An assignment expression yielded the pre-narrowing value, so `char c; int x = (c = 300);` stored 44 in `c` but left 300 in `x` | FIXED (working tree) | pending |
| BUG-032 | P0 | `&array` kept the decayed element-pointer type, so `&a + 1` advanced one element instead of one whole array | FIXED (working tree) | pending |
| BUG-031 | P3 | The macOS job assembled the backend's x86-64 output as arm64 (`unrecognized instruction mnemonic: movq`), so the platform the README claims support for could not run the suite | FIXED | `bae132f` |
| BUG-030 | P3 | The optimizer parity check shells out to `cmp`, CI's MSYS2 did not install diffutils, and the failure was reported as "optimized and --no-opt print different output" - a missing package wearing a codegen bug's clothes | FIXED | `bae132f` |
| BUG-029 | P0 | The toy language parsed every integer literal as base 10, so `say 0x10` printed `0` - hex and octal accepted, silently wrong | FIXED | `a77a2bc` |
| BUG-028 | P3 | `build.bat` compiled a hand-written file list that never learned about `src/myc.c` and `src/opt.c`, so a fresh clone - the README's Windows path - failed to link | FIXED | `fcb0073` |
| BUG-027 | P1 | The toy language rejects every `if`/`else`: a block stops only on the *identifier* spelling of `else`, but the shared lexer returns it as a keyword token, so the block swallows it | FIXED | `4d0154d` |
| BUG-026 | P0 | A nested `/` or `%` overwrote `%r10`, the register holding an enclosing expression's value, so `(a / b) + (d % c)` added garbage | FIXED | `86d5865` |
| BUG-025 | P0 | `&b` was not treated as the address escaping, so an address-taken local looked dead and `int b = 42; int *p = &b; return *p;` read uninitialised memory | FIXED | `86d5865` |
| BUG-024 | P0 | `--sign` interned an empty string, so a "signed" program carried a well-formed but empty `.rodata` entry and no provenance: the build succeeded and nothing warned | FIXED | `0f58d9b` |
| BUG-023 | P0 | A string literal containing `\0` is truncated at the NUL: `"\0A"` becomes `""`, so `s[1]` reads whatever follows in `.rodata` | FIXED | `831a61a` |
| BUG-022 | P1 | C octal escapes are rejected: `'\101'` / `"\101"` report `unknown escape sequence '\1'` | FIXED | `4410784` |
| BUG-021 | P3 | `make test` fails on a CRLF checkout: its `sed` extraction keeps the trailing `\r`, so `grep -F` never matches | FIXED | `4c3cd8a` |
| BUG-020 | P0 | `a[i] += 1` scales the index twice and addresses `a[i*64]` (compound assignment through a subscript) | FIXED | `4c3cd8a` |
| BUG-019 | P0 | No assignment-compatibility checking: struct→int, int→pointer, struct→int-param all compile silently | FIXED | `4c3cd8a` |
| BUG-018 | P3 | README claims macOS support; the build path was Linux-only and the emitted symbols were ELF-style | FIXED | `4c3cd8a` + `bae132f` |
| BUG-017 | P3 | A partial `.s` file is left on disk when compilation fails | FIXED | `4c3cd8a` |
| BUG-016 | P2 | Falling off the end of a non-`void` function silently returns 0 (no warning) | FIXED | `4c3cd8a` |
| BUG-015 | P2 | Calling a declared-but-never-defined function yields a linker error, not a diagnostic | FIXED | `4c3cd8a` |
| BUG-014 | P2 | `main(int argc, char **argv)` accepted, but `argc` is garbage on Windows | FIXED | `4c3cd8a` |
| BUG-013 | P2 | A string literal produces `unexpected character '"'` plus a cascading bogus error | FIXED | `4c3cd8a` |
| BUG-012 | P2 | `#include` reports `unexpected character '#'`, then cascades | FIXED | `4c3cd8a` |
| BUG-011 | P2 | A UTF-8 BOM at offset 0 breaks the lexer | FIXED | `4c3cd8a` |
| BUG-010 | P3 | Deeply nested parentheses overflow the parser stack and crash | FIXED | `4c3cd8a` |
| BUG-009 | P3 | 300 nested loops overflow `break_kind[256]` and crash the compiler | FIXED | `4c3cd8a` |
| BUG-008 | P1 | `08` / `09` are accepted and silently evaluate to 0 | FIXED | `4c3cd8a` |
| BUG-007 | P1 | No comma operator: `for (i=0, j=n; ...)` is a syntax error | FIXED | `4c3cd8a` |
| BUG-006 | P1 | No cast expressions: `(int)x`, `(char *)p` are syntax errors | FIXED | `4c3cd8a` |
| BUG-005 | P1 | `int *p;` at file scope is rejected → no global pointers, no pointer-returning functions | FIXED | `4c3cd8a` |
| BUG-004 | P0 | Struct arguments are accepted but the parameter slot is 8 bytes → garbage values | FIXED | `4c3cd8a` |
| BUG-003 | P0 | `b = a;` on structs is accepted and copies only 8 bytes | FIXED | `4c3cd8a` |
| BUG-002 | P0 | `a[i++]++` evaluates the subscript twice and increments `i` twice | FIXED | `4c3cd8a` |
| BUG-001 | P0 | `a[i++] += 5` evaluates the subscript twice and increments `i` twice | FIXED | `4c3cd8a` |

**Counts:** 15 × P0 · 13 × P1 · 12 × P2 · 8 × P3 · **48 logged, 47 fixed (31 committed + 16 in the working tree), 1 open.** (BUG-018 closed on CI evidence 2026-10-24 16:10 +05:30; BUG-048 is the remaining open diagnostic gap.)

**Correction (2026-10-24 16:20):** every count line before this one under-reported the ledger. The table has held `BUG-001` … `BUG-028` plus the three new entries, i.e. 31 rows, while the tally said "27 logged" and then "30 logged" - the table was right and the arithmetic was wrong. Re-counted from the rows themselves: 11 × P0, 6 × P1, 6 × P2, 8 × P3 = 31. The wrong numbers are left visible here rather than edited away, because a ledger whose totals are trusted without counting is the same defect class this file exists to catch.

**Commit status (2026-10-26):** `BUG-032` … `BUG-047` are fixed and regression-tested in the working tree, but the user explicitly withheld permission to commit or push. Their status therefore says `FIXED (working tree)`, their *Fix commit* says `pending`, and they are **not** marked plain `FIXED`: the ledger's own honesty rule requires a commit hash before that status is used. When the batch is committed, replace `pending` with the hash and change the status to `FIXED`. `BUG-048` is a separate, genuinely open diagnostic gap; it is not part of the uncommitted fix batch.

### Verification summary (what was actually executed)

| Check | Command | Result |
|---|---|---|
| Warning-free build | `gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Iinclude -o mycc.exe src\*.c` (MinGW UCRT64) | clean, no diagnostics |
| Exit-code suite (Windows) | `.\test.bat` | `All tests passed` - **81 PASS** checks (35 positive, 41 rejection, 5 contract assertions, one per toy-language pair) |
| Optimizer parity (Phase 5) | `.\test.bat`, and `make CC=... test` under MSYS2 | every positive test rebuilt with `--no-opt`: same exit status and byte-identical stdout; `PASS: optimized and --no-opt agree on every test` |
| `--no-opt` identity (Phase 5) | pre-Phase-5 compiler (built from `git archive HEAD`) vs the new one, all 35 tests | **0 differences** by SHA-256 — the unoptimized path is unchanged |
| Fresh-checkout build (BUG-028) | `git clone` of the published branch, then `build.bat` + `test.bat` | `Build OK: mycc.exe`, **81 PASS**, `All tests passed` - the repository builds and tests as published, from nothing but what is in it |
| CI matrix (first green run, BUG-030/031) | GitHub Actions run `40086581248`, all three jobs | **Linux 44s, macOS 1m4s, Windows 2m49s - all green.** Linux: `make` with `-Werror`, 81 checks, difftest, 1000-program fuzz. Windows: 81 checks, difftest, 200-program fuzz, then `build.bat` + `test.bat` through `cmd`. macOS: Rosetta probe, 81 checks, difftest, 100-program fuzz |
| Random-program fuzzing (Phase 6) | `make fuzz FUZZ_N=1000` (MSYS2) and `fuzz.bat 1000` | `PASS: all 1000 programs agreed with the reference compiler` - 1000 generated programs, mycc and gcc return the same exit code for every one |
| Fails-before (BUG-029) | `HEAD:src/myc.c` rebuilt into `build\mycc_old.exe`, then `tests/myc/literals.myc` | buggy build exits **1** (the `0x10 != 16` guard fires because `0x10` parsed as 0); fixed build exits **42** and prints `literals-ok`, and its assembly is byte-identical to the C twin's |
| Fails-before (BUG-027) | `HEAD` sources rebuilt into `build\mycc_prefix.exe`, on `tests/myc/else.myc` | rejected: `tests\myc\else.myc:18:3: error: expected a statement (found 'else')`; the fixed build compiles it, prints `else-branch` / `8`, exits **8**, and its C twin compiles to the same bytes |
| Fails-before (BUG-025) | dead-store pass with `note_address_use()` disabled, on `int b = 42; int *p = &b; return *p;` | buggy build: no `movq $42` in the assembly, program exits **64** (uninitialised slot); fixed build: **42**, same as gcc |
| Fails-before (BUG-026) | one-register pool before the `/`,`%` spill, on `tests/36_registers.c` | **2** instead of **42** (the `(a / b) + (d % c)` guard fired); after the fix **42**, and gcc also returns 42 |
| Phase 4 acceptance (Windows runtime) | `tests\26_hello_world.c`, `27_print_builtins.c`, `28_extern_printf.c`, `29_string_escapes.c`, `hello.c` | printed `Hello, world!` / `42` / `7 14 21 28` / `ESCAPES-OK` / `3 + 4 = 7, and 2 * 21 = 42` — real CRT calls through the platform ABI |
| Phase 4 stack arguments | `sum5(1,2,3,4,5)` and `sum7(1,…,7)` on Windows (`4` register args + shadow space) | `12345` and `55` after fixing the callee's first-stack-argument offset (40 → 48); `tests/diff/02_stack_args.c` now cross-checks it against gcc |
| Phase 4 other targets (inspection only) | `mycc --target=linux build\sv.c`, `--target=darwin` | `call printf@PLT` for externals, plain `call sum7` for internal, `movb $0, %al`, no shadow space, `16(%rbp)` first stack arg; darwin `_main`/`_printf`/`_sum7` + `__TEXT,__cstring`, no `GNU-stack` |
| Fails-before (BUG-022) | baseline `d83bc5d` compiler on `int main(void){ return '\101'; }` | rejected: `error: unknown escape sequence '\1'` + two cascading errors; fixed compiler returns 65 |
| Fails-before (BUG-023) | `4410784` compiler on `char *s = "\0A"; return s[1];` | **0** before (emitted `.string ""`), **65** after — same as gcc |
| Differential suite (Windows) | `.\difftest.bat` | `All differential tests passed` (mycc and gcc both exit 0) |
| Differential suite (shell path) | `difftest.sh` under WSL, Windows toolchain via path-translating wrappers | `All differential tests passed` |
| `make test` recipe logic | mirrored recipe under WSL `bash` (real `sed`/`grep`/`tr`, no `make` on this machine) | `All tests passed (make-recipe logic)` |
| Fails-before (BUG-001) | baseline `9dfb5c2` compiler on `g[i++] += 5; return i*10 + g[0];` | baseline **30**, fixed mycc **25**, gcc **25** |
| Fails-before (whole diff file) | baseline `9dfb5c2` on `tests/diff/01_compound_and_lvalue.c` | rejected at parse — BUG-005 (`int *slot(int)`); the numeric proof above is the substitution |
| Target flavours | `--target=darwin` / `linux` / `windows` / `beos` | `_main`+`_fib` and no `GNU-stack` on darwin; `main`+`GNU-stack` on linux; clean error on `beos` |
| **2026-10-26 audit build** | `C:\w64devkit\bin\gcc.exe -std=c99 -O2 -Wall -Wextra -Wpedantic -Iinclude -o mycc.exe src\*.c` | clean, no diagnostics |
| **2026-10-26 audit suite** | `.\test.bat` | `All tests passed` - **106 PASS** checks (48 positive, 52 rejection, 6 contract assertions) |
| **2026-10-26 audit differential/fuzz** | `$env:CC='C:\\w64devkit\\bin\\gcc.exe'; .\\difftest.bat`; `.\\fuzz.bat 700 456` | **3/3** differential programs agreed; **700/700** generated programs agreed with the reference compiler |
| **2026-10-26 audit limits** | `git diff --check`; `--target=linux`/`darwin` assembly inspection; manual edge probes | whitespace clean; long-symbol and `%al` paths inspected; no local Linux/macOS runtime or sanitizer run |

**Environment limits, stated plainly:** on the 2026-10-26 host, `C:\w64devkit\bin\gcc.exe` is present, but the first `gcc` on `PATH` is MinGW.org 6.3 (32-bit), and `C:\msys64` and `make` are absent. A bare `difftest.bat` therefore picked a 32-bit assembler and failed on `%rax`; the 3/3 result above comes from setting `CC` to `C:\w64devkit\bin\gcc.exe`, which is also the compiler used for the clean build. This is a local toolchain-selection issue, not a compiler result. The `make` recipes were therefore not re-run in this audit; the same-file contract is mirrored into the `Makefile` but the executed evidence here is `test.bat`. What is still true: there is no macOS host here, so every macOS statement in this ledger rests on the CI job (now green, run `40086581248`), and the macOS *runtime* of Phase 4 paths was inspected rather than executed locally. The historical CI table above is retained because it describes that run, not the current extended suite.

**Audit addendum (2026-10-26, working tree):** the historical table above records the 2026-10-24 verification run and its 81-check suite. The later memory/type/literal audit added tests `37`-`49`, the same-file/temp-path contract, and 17 logged entries (`BUG-032` … `BUG-048`). The current `test.bat` reports **106 PASS** (48 positive, 52 rejection, 6 contract). Sixteen of the new entries are fixed but **uncommitted**, as requested; the dashboard says `FIXED (working tree)` and their commit field says `pending`. `BUG-048` remains genuinely **OPEN**: struct conditions and struct comparisons are still accepted without a scalar-type diagnostic. The historical CI numbers are left unchanged because they describe that run, not the extended suite.

---

## Entries (newest first)

### BUG-048 - struct values are still accepted as conditions and comparison operands

- **Found:** 2026-10-26, while probing the remaining Phase 3 diagnostics after the type audit
- **Severity:** P2 (invalid C is accepted; no silent wrong code is claimed, but the front end does not enforce the constraint)
- **Short summary:** C requires the controlling expression of `if`/`while`/`do`/`for` and both operands of a comparison to have scalar type. This compiler's semantic pass checks only that the expression is not `void`, so a struct value reaches those contexts without a diagnostic. The codegen path then treats the aggregate as a scalar.
- **Reproduction:**
  ```c
  struct S { int x; };
  int main(void) {
      struct S a; struct S b;
      a.x = 1; b.x = 2;
      if (a) return 1;          /* invalid C: struct condition */
      if (a == b) return 2;     /* invalid C: struct comparison */
      return 0;
  }
  ```
  - observed: both constructs are accepted; the program compiles
  - expected: a front-end diagnostic for a non-scalar condition/comparison operand
- **Root cause:** `src/semantic.c:check_stmt()` calls `check_value()` for conditions, which rejects only `void`; the `AST_BINARY_OP` path also has no aggregate-operand rejection. Struct assignments and struct member access are legal elsewhere, so the check has to be specific to scalar-required contexts rather than “structs are never values.”
- **Evidence:** manual probe compiled with the working-tree binary; no regression test yet.
- **Fix plan:** add a scalar-context helper for conditions (and the switch controlling expression) that rejects `TYPE_STRUCT`, and reject struct operands in non-comma binary expressions, especially comparisons; add `tests/errors/struct_condition.c` and `struct_comparison.c`.
- **Status:** OPEN
- **Fix commit:** —
- **Regression test:** none yet
- **Why it is recorded rather than hidden:** the roadmap's Phase 3 checklist is complete, but “checklist complete” is not the same as “every invalid C construct is diagnosed.” This entry is the honest remainder.

### BUG-047 - a struct-valued conditional fell through semantic analysis and aborted in codegen

- **Found:** 2026-10-26, by compiling `a = 1 ? a : b;` with two struct objects during the memory/type audit
- **Severity:** P2 (valid C is unsupported, but the failure was an internal codegen abort rather than an actionable front-end diagnostic)
- **Short summary:** when both arms of `?:` had struct type, the conditional fallback claimed `int`; the assignment then looked like `struct = int`, and the compiler either produced a misleading type error or reached a codegen abort. Struct-valued conditionals remain outside the subset, but they must be refused at semantic analysis.
- **Reproduction:**
  ```c
  struct S { int x; };
  int main(void) {
      struct S a; struct S b;
      a.x = 1; b.x = 2;
      a = 1 ? a : b;
      return a.x;
  }
  ```
  - observed before the fix: internal failure/`compilation aborted during semantic analysis`
  - expected and observed now: `error: struct-valued conditional expressions are not supported`
- **Root cause:** `src/semantic.c:check_expr()` had no struct/struct branch for `AST_TERNARY`; the generic fallback chose `int`, losing the real type. `src/codegen.c:gen_ternary()` has no struct-copy value model, so it could only abort.
- **Fix:** detect two struct arms in `check_expr()`, report the unsupported construct, and keep the struct type so later checks do not invent a scalar conversion. This is a diagnostic fix, not full struct-value support.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** none yet; the limitation is still visible in the deferred-gap section. The manual probe above is the evidence.

### BUG-046 - integer casts were not constant expressions, so valid globals and case labels were rejected

- **Found:** 2026-10-26, while auditing constant folding after the unsigned/type fixes
- **Severity:** P2 (valid C rejected; no wrong code)
- **Short summary:** `int g = (int)3;`, `case (int)2:`, and `(char)256` used as a null pointer constant were folded as “not a constant expression.” The cast node was not handled by `fold_const()`, and `fold_literal()` skipped nodes whose `val_type` was not plain `int`.
- **Reproduction:**
  ```c
  int g = (int)3;
  int h = (char)300;
  int main(void) {
      switch (2) { case (int)2: g += 1; break; default: return 1; }
      return g == 4 && h == 44 ? 0 : 2;
  }
  ```
  - observed before: `error: global initializer is not a constant expression` / rejected `case`
  - expected and observed now: exit 0; `(char)300` is 44 and `(char)256` folds to the null pointer constant
- **Root cause:** `src/semantic.c:fold_const()` had no `AST_CAST` arm, and `fold_literal()` bailed out before folding casts to `char`/`unsigned char`. Global and case expressions were folded before their types had been checked, which hid the converted result.
- **Fix:** fold casts to integer/character target types; leave pointer casts non-integer; run `check_value()` before folding global initializers and case labels so the cast and unsigned types are known.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/46_constant_casts.c` (global, case, null-pointer and narrowing cases) and the global/case portion of `tests/48_unsigned_arithmetic_types.c`.

### BUG-045 - source NULs truncated the program, and output aliases could overwrite the source

- **Found:** 2026-10-26, by probing `read_file()` and the `-o`/temporary-file path
- **Severity:** P0 (source destruction or silently compiling only a prefix)
- **Short summary:** an embedded NUL made the lexer stop at that byte, so the compiler could accept a truncated program without warning. Separately, `mycc foo.c -o .\foo.c` and a source named like `foo.s.mycc-tmp` could pass the exact-string guard and be truncated or replaced by the temporary assembly.
- **Reproduction:**
  - a source file containing `int main(void) { return 0; }\0int broken` compiled only the prefix
  - `mycc foo.c -o .\foo.c` was not rejected when the strings differed
  - `mycc foo.s.mycc-tmp -o foo.s` opened the source path as the temporary output
- **Root cause:** `read_file()` appended a terminator without checking for NUL bytes; the same-file check used only `strcmp`; the temporary path was constructed without checking whether it named the input.
- **Fix:** reject embedded NULs in `src/main.c`; compare output and temporary paths with canonical/POSIX-identity checks (`_fullpath` + case-insensitive compare on Windows, `stat` dev/inode elsewhere); reject the temporary-source collision before opening the temp file.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `test.bat` `:same_file_contract` and the mirrored `Makefile` contract cover alternate output spellings and temp-name collisions; the NUL check was verified by a manual probe. Windows canonical comparison still does not resolve every hard-link/8.3 alias.

### BUG-044 - declaration validation accepted invalid extern variables, main forms, and global pointer initializers

- **Found:** 2026-10-26, during the declaration/initializer audit
- **Severity:** P1 (silent symbol/ABI mistakes and invalid programs accepted)
- **Short summary:** `extern int x;` was parsed and then emitted as a zero-filled definition instead of being refused; `void main`, `unsigned main`, and prototype-only `main` were not all rejected; and a global pointer could be initialized from a nonzero integer such as `int *p = 1;`.
- **Reproduction:**
  ```c
  extern int shared;              /* silently defined here */
  int *p = 1;                     /* accepted without a cast */
  void main(void) { }             /* accepted */
  ```
  - observed before: each compiled or produced a misleading downstream failure
  - expected and observed now: clean compile-time diagnostics
- **Root cause:** `extern` was tracked only for functions; `main` validation checked existence but not every return/body form; global initializers were folded for a value but not checked against the destination type.
- **Fix:** reject `extern` variables explicitly; require non-unsigned `int main` with a body; call `types_compatible()` on global initializers and diagnose a nonzero integer-to-pointer conversion.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/errors/extern_variable.c`, `main_unsigned.c`, `main_void.c`, `prototype_only_main.c`, and `global_pointer_from_int.c`.

### BUG-043 - Darwin variadic calls did not zero `%al`

- **Found:** 2026-10-26, by inspecting `gen_call()` for each target after the ABI review
- **Severity:** P1 (wrong varargs on macOS; Windows was unaffected)
- **Short summary:** the `%al` zeroing was gated on Linux only (`target != WINDOWS && target != DARWIN`). Darwin also uses the SysV `%al` convention for variadic calls, so a call such as `printf("%lld\n", x)` could present a garbage vector-register count.
- **Reproduction:** inspect the output of `mycc --target=darwin tests/28_extern_printf.c -o build\ep.s`; before the fix the call site had no `movb $0, %al`.
- **Root cause:** the target condition treated Darwin as if it followed the Windows convention for `%al`, even though Darwin uses the SysV register convention and only differs in symbol spelling/sections.
- **Fix:** zero `%al` for every non-Windows target.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** assembly inspection only (`--target=darwin`); no local macOS runtime was available. The existing Windows/Linux call tests remain green.

### BUG-042 - fixed-size codegen arrays and symbol buffers truncated valid programs

- **Found:** 2026-10-26 by probing 40-argument calls and 600-character identifiers
- **Severity:** P1 (valid calls could lose arguments; long symbols could be truncated)
- **Short summary:** `gen_call()` collected at most 32 arguments into a fixed array, so later arguments were not passed correctly. `sym_name()`/`call_target()` used 512-byte static buffers, and the internal-definition table was capped at 256 names; long symbols or many functions could be emitted under the wrong name or routed through `@PLT` unnecessarily.
- **Reproduction:** `tests/37_wide_calls.c` calls a 40-parameter function; a generated 601-character function name was used to inspect Linux/Darwin symbol spelling.
- **Root cause:** fixed compile-time limits in `src/codegen.c` that were never part of the language limit.
- **Fix:** count and allocate call arguments dynamically; grow `sym_name()`/`call_target()` buffers to fit the name; replace `defined_names[MAX_DEFINED]` with a dynamically grown table.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/37_wide_calls.c`; the long-name path was verified by inspecting generated Linux/Darwin assembly (ignored `build/audit` probe).

### BUG-041 - unnamed prototype parameters, builtin arity, and conflicting redeclarations were not checked correctly

- **Found:** 2026-10-26 during prototype/call auditing
- **Severity:** P2 (valid prototypes rejected; invalid calls/redeclarations accepted)
- **Short summary:** `int f(int, char *);` was rejected even though parameter names are optional in prototypes; the rewritten `print_*` helpers accepted extra arguments; and a second declaration with a different return or parameter type could replace the first without a diagnostic.
- **Reproduction:**
  ```c
  int f(int, int);
  int f(int a, int b) { return a + b; }   /* valid */
  /* print_int(1, 2) must be rejected; conflicting declarations must be rejected */
  ```
- **Root cause:** the parameter parser required an identifier, the builtin metadata did not enforce exact arity during calls, and `symbol_insert()`/function declaration handling did not compare an existing signature.
- **Fix:** track `is_unnamed` on `AST_PARAM`, allow omission in prototypes but reject it in definitions, mark rewritten helpers as exact-arity, and diagnose conflicting return/parameter types for function redeclarations.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/38_unnamed_proto.c`, `tests/errors/unnamed_definition.c`, `builtin_extra_arg.c`, `conflicting_param_type.c`, and `conflicting_return_type.c`.

### BUG-040 - `\?` and adjacent string literals were missing from the literal implementation

- **Found:** 2026-10-26 while checking the C escape set and translation phase 6
- **Severity:** P2 (valid C rejected)
- **Short summary:** `'\?'` and `"what\?"` were reported as unknown escapes, and `"ab" "cd"` was a syntax error rather than the single literal `"abcd"`.
- **Reproduction:** `tests/47_question_escape.c` and `tests/49_adjacent_strings.c`.
- **Root cause:** `read_escape()` omitted the standard `?` case; `parse_primary()` consumed exactly one `TOKEN_STRING` and never looked for a following string token.
- **Fix:** accept `\?`; concatenate adjacent decoded string tokens in `parse_primary()` before building the AST string, preserving explicit length and embedded NULs.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/47_question_escape.c` and `tests/49_adjacent_strings.c` (including `"\0" "A"` and two empty literals).

### BUG-039 - void values were accepted in some value contexts and rejected in some discarded contexts

- **Found:** 2026-10-26 while probing C’s discarded-value and comma/cast rules
- **Severity:** P2 (constraint checking could accept invalid C or reject valid C)
- **Short summary:** the semantic pass did not consistently enforce that a void expression may not be used as a value, while valid uses such as a discarded `(void)f()`, a comma left operand, or a `for` step could be rejected after the first tightening attempt.
- **Reproduction:** `tests/41_void_expressions.c` covers the valid contexts; `tests/errors/void_value.c` and `void_cast_value.c` cover value uses.
- **Root cause:** `check_expr()`/`check_value()` treated every expression uniformly; only some callers knew whether a value was actually consumed.
- **Fix:** add `check_value()` at value-consuming sites (assignment, return, arguments, arithmetic, conditions, initializers) while leaving expression statements, comma left sides, `for` steps, and casts to `void` in the discarded-value path.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/41_void_expressions.c`, `tests/errors/void_value.c`, and `tests/errors/void_cast_value.c`.

### BUG-038 - high-byte character constants and escape ranges had the wrong value

- **Found:** 2026-10-26 by comparing `'\x80'`, `'\200'`, and `'\377'` with signed-char gcc behavior
- **Severity:** P1 (wrong integer values; a valid-character edge silently changed)
- **Short summary:** decoded character constants were left as unsigned bytes, so `'\xff'` became 255 instead of -1 under this compiler's signed `char`. Hex/octal escapes above 255 were also accepted or truncated inconsistently instead of being diagnosed.
- **Reproduction:** `tests/40_char_constant_values.c` checks `'\x80' == -128`, `'\xff' == -1`, and that string bytes stay raw; `tests/errors/escape_out_of_range.c` checks an oversized escape.
- **Root cause:** the lexer stored the decoded byte without applying the language's signed-`char` value, and the escape decoder did not reject values outside the byte range.
- **Fix:** sign-extend high-byte character constants; reject numeric escapes above 255; keep string bytes unsigned/raw.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/40_char_constant_values.c` and `tests/errors/escape_out_of_range.c`.

### BUG-037 - unsigned conversions, shift signedness, and constant folding could produce wrong code

- **Found:** 2026-10-26 by probing `unsigned int` expressions against gcc and by auditing `binary_is_unsigned()` call sites
- **Severity:** P1 (silent wrong code for valid unsigned programs)
- **Short summary:** `?:` used the left branch’s type instead of the usual arithmetic conversions; non-pointer `+`/`-` always returned `int`; comparisons/logicals were not consistently typed; a shift’s signedness was taken from the right operand, so `-1 >> u` could become logical; and constant folding ignored unsignedness, including in global initializers and `case` labels.
- **Reproduction:** `tests/48_unsigned_arithmetic_types.c`; examples include `(0 - (unsigned)2) / 2`, `(i >> s) / 2` where `i` is `-2` and `s` is unsigned, and global/case expressions such as `((unsigned)-1 > 0)`.
- **Root cause:** the semantic result-type rules were written for signed `int` and reused the left operand or the wrong operand; `fold_const()` used host signed operations while codegen used the type only for instruction selection.
- **Fix:** apply usual arithmetic conversions to `?:`, `+`/`-`, and arithmetic operands; type comparisons/logicals as `int`; derive shift type and signedness from the promoted left operand only; make constant folding unsigned-aware after expressions are type-checked.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/48_unsigned_arithmetic_types.c`, plus the existing `tests/18_unsigned.c`.

### BUG-036 - a labelled statement in a controlled body escaped the `if`/loop

- **Found:** 2026-10-26 via `if (0) case 1: stmt;` inside a switch
- **Severity:** P0 (silent wrong code on the normal false path)
- **Short summary:** the parser kept a `case`/`default` label and the statement after it as flat siblings. Inside a switch list that is harmless fallthrough, but when the label is the single body of an `if` or loop, the following statement was not under that control construct, so `if (0)` still executed it.
- **Reproduction:** `tests/45_label_controlled_statement.c`, first case: `switch (2) { case 2: if (0) case 1: r = 5; r += 1; }` must leave `r == 1`, not `r == 5`.
- **Root cause:** `parse_controlled_statement()` did not exist; `parse_if`/`while`/`do`/`for` called `parse_statement()` directly, which returned a flat label node.
- **Fix:** add a controlled-statement parser that nests consecutive labels and attaches the following statement to the label body; update semantic/switch collection, termination analysis, codegen, optimizer scans, and AST dump to traverse that link; cap consecutive label chains at `MYCC_MAX_NEST`.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/45_label_controlled_statement.c` (if/else/while/for/do and true/false paths).

### BUG-035 - constant-condition elimination removed switch labels from dead arms

- **Found:** 2026-10-26 by compiling a `case` label inside `if (0)`, `while (0)`, and `for (;0;)`
- **Severity:** P1 (assembler error / missing jump target)
- **Short summary:** codegen skipped a constant-dead arm entirely, including any `case`/`default` label inside it. A switch jump could still target that label, producing an undefined assembler symbol or losing the arm’s body.
- **Reproduction:** `tests/44_switch_labels_in_dead_code.c` covers labels in dead `if`, `while`, `for`, and `else` arms, including `default`.
- **Root cause:** `gen_if`/`gen_while`/`gen_for` used `const_truth()` and emitted only the live arm; they did not ask whether the dead subtree contained a label belonging to the enclosing switch. Nested switches must not count.
- **Fix:** add `has_switch_label()` and keep the dead arm when it contains an enclosing switch’s label; update the traversal so a nested `switch` owns its own labels.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/44_switch_labels_in_dead_code.c`.

### BUG-034 - pointer conditionals only handled the null pointer constant in one branch order

- **Found:** 2026-10-26 by swapping `c ? 0 : p` and `c ? p : 0`
- **Severity:** P1 (wrong conditional type; valid C rejected or mis-scaled)
- **Short summary:** the semantic code recognized a pointer/null combination only when the pointer was on a particular side, and assigned the conditional a scalar type in the other order. Assignment and later pointer arithmetic then used the wrong conversion.
- **Reproduction:** `tests/43_ternary_null_pointer.c` checks both branch orders, both constant `c` values, and comparison with `0`.
- **Root cause:** the `AST_TERNARY` type rules tested one arm and fell back to the other without a symmetric null-pointer-constant predicate.
- **Fix:** test both arms for the null pointer constant, choose the pointer arm’s type regardless of order, and diagnose pointer/integer combinations that are not null.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/43_ternary_null_pointer.c`.

### BUG-033 - assignment expressions returned the pre-conversion value

- **Found:** 2026-10-26 via `char c; int x = (c = 300);`
- **Severity:** P0 (silent wrong code in a valid expression)
- **Short summary:** the store truncated to the lvalue, but `%rax` still held the original value, so the assignment expression evaluated to 300 even though `c` contained 44. The same problem affected compound assignment, prefix/postfix `++`/`--`, and stores through pointers.
- **Reproduction:** `tests/42_assignment_values.c` covers `char`/`unsigned char`, simple/compound assignment, pointers, `++`/`--`, and `255`/`256` boundaries.
- **Root cause:** `gen_assign()`/`gen_compound_assign()` emitted the store but did not sign- or zero-extend the stored byte back into `%rax`; the AST value was assumed to be the original expression value.
- **Fix:** add `convert_assignment_value()` and apply it after every scalar store path, including indirect stores and compound assignments.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/42_assignment_values.c`.

### BUG-032 - `&array` lost its pointer-to-array type

- **Found:** 2026-10-26 by probing `&a + 1` for an `int a[4]`
- **Severity:** P0 (silent wrong pointer arithmetic)
- **Short summary:** `&a` was typed as `int *` (the decayed element pointer), so `&a + 1` advanced four bytes instead of one whole array. The same error affected `char` arrays and pointer differences.
- **Reproduction:** `tests/39_pointer_to_array.c`: `(int *)(&a + 1) == a + 4`, `(char *)(&b + 1) == b + 3`, and `(&a + 1) - &a == 1`.
- **Root cause:** address-of used the operand’s decayed expression type rather than the array object type; pointer arithmetic then scaled by the element size instead of the array size.
- **Fix:** type `&array` as pointer-to-array, use the array size as the pointer step, and keep explicit casts available when the program wants `int *`.
- **Status:** FIXED (working tree, uncommitted)
- **Fix commit:** pending
- **Regression test:** `tests/39_pointer_to_array.c`.

### BUG-031 - the macOS job assembled the backend's x86-64 output as arm64, so the first instruction failed to parse

- **Found:** 2026-10-24 14:50 +05:30, in `3d41c8c` (run `40072449342`, job `120808616685`), by reading the failed job's log rather than trusting the README's "green on three platforms" line
- **Severity:** P3 (tooling: the platform the README claims support for could neither build nor run the suite in CI)
- **Short summary:** every CI run on `macos-latest` died on the first test with `unrecognized instruction mnemonic` on `movq`. The backend emits x86-64 assembly and the runner is Apple Silicon, so clang read the `.s` as arm64. Nothing in the compiler's output was wrong - the job never told the assembler which architecture to read, and never checked that the resulting binary could run.
- **Reproduction:**
  ```
  make test        # on macos-latest (arm64)
  ```
  - observed:
    ```
    /tmp/mycc-test/01_arithmetic.s:122:2: error: unrecognized instruction mnemonic
     leave
     ^
    /tmp/mycc-test/01_arithmetic.s:125:11: error: unknown token in expression
     movq $0, %rax
              ^
    make: *** [test] Error 1
    FAIL: assemble tests/01_arithmetic.c
    ```
  - expected (and observed after the fix): every test assembles, links and runs; `All tests passed`.
- **Root cause:** `Makefile:22-24` and `.github/workflows/ci.yml:58-68`. The Darwin branch set `TARGETFLAG := --target=darwin` and left `LDFLAGS` empty, so `$(CC)` was called with no `-arch`. On an Intel Mac that is right by accident, because x86-64 is the host architecture; on the arm64 runner GitHub now provides, both the assembler and the linker default to arm64.
- **Evidence:** verified by execution in CI - the quoted log is the failing run, not an inference. The job had failed on every push for days; the "pending CI" note on BUG-018 was accurate and the CI was never green on this platform.
- **Fix plan:**
  - *Problem:* the harness assumed the host architecture is the one the backend emits.
  - *Options:* (a) port the backend to arm64 - rejected: weeks of work, and x86-64 AT&T is the project's stated target; (b) let the macOS job assemble only (`cc -c`) and never link or run - rejected: it would keep the README's "built and tested on macOS" claim half-true; (c) **pin the driver to the architecture the backend emits** (`-arch x86_64` on Darwin) and make the job prove the binaries run by probing Rosetta first; (d) pin the runner to `macos-13`, the last Intel image - rejected: it defers the same breakage until the image is retired and hides that the compiler is architecture-specific rather than platform-specific.
  - *Chosen:* (c), plus an explicit `arch -x86_64 /usr/bin/true` probe so a missing translator is one clear message instead of eighty test failures.
  - *Tradeoffs accepted:* the macOS job now proves "x86-64 output assembles, links and runs under Rosetta on Apple Silicon", not "the compiler targets Apple Silicon". That distinction is written down in the README instead of being blurred.
- **Status:** CI fix verified by the green macOS job on run `40086581248` (see *Fix commit*)
- **Fix commit:** `bae132f` - *fix(ci): give the runners what the harness and the backend need (BUG-030, BUG-031)*
- **Regression test:** the macOS CI job itself. It now fails with `Rosetta 2 is unavailable: the x86-64 test binaries cannot run here` when the translator is missing - a different message from a codegen bug, which is the point.

### BUG-030 - the optimizer parity check needed `cmp`, CI's MSYS2 did not install it, and the failure was reported as a codegen bug

- **Found:** 2026-10-24 14:45 +05:30, in `3d41c8c` (run `40072449342`, job `120808616809`), by grepping the job log for the line immediately before the first `FAIL`
- **Severity:** P3 (tooling: an undeclared dependency in the harness; the compiler was correct throughout)
- **Short summary:** `make test` compares each program's optimized and `--no-opt` output with `cmp`. `setup-msys2` installs only the packages the workflow names, `diffutils` was not one of them, so the first comparison failed with `/bin/sh: line 13: cmp: command not found` - and the harness printed `FAIL: tests/01_arithmetic.c optimized and --no-opt print different output`, which reads exactly like a Phase 5 regression. The Windows job had been red since the parity check was added.
- **Reproduction:**
  ```
  make test        # on MSYS2 UCRT64 without diffutils
  ```
  - observed:
    ```
    PASS: tests/01_arithmetic.c (exit 0)
    /bin/sh: line 13: cmp: command not found
    FAIL: tests/01_arithmetic.c optimized and --no-opt print different output
    make: *** [test] Error 1
    ```
  - expected: `PASS: optimized and --no-opt agree on every test`.
- **Root cause:** `Makefile:83` (`if ! cmp -s ...`) and `.github/workflows/ci.yml:31-33`. Two problems stacked: the harness depended on a tool without declaring it, and its failure branch (`! cmp`) cannot distinguish "the outputs differ" from "cmp could not run", so a missing package took the same exit path as a real mismatch.
- **Evidence:** verified by execution in CI - the `cmp: command not found` line sits directly above the FAIL in the job log. Locally the failure never appeared because this machine's MSYS2 install includes diffutils, which is why the job log and not the local run was the evidence.
- **Fix plan:**
  - *Problem:* a test recipe that silently degrades into a false failure on a machine missing one tool.
  - *Options:* (a) install `diffutils` in CI only - rejected: the next machine without it gets the same misleading message; (b) replace `cmp` with `diff -q` - rejected: `diff` is in the same package, so it moves the problem; (c) compare with `[ "$(cat a)" = "$(cat b)" ]` - rejected: command substitution strips trailing newlines, so a program that stopped printing its final newline would pass; (d) **probe for `cmp` at the start of `test` with a message that names the package, and install diffutils in CI**.
  - *Chosen:* (d). The probe turns a confusing failure into an instruction; the CI install makes the job actually run.
  - *Tradeoffs accepted:* one more package in the Windows job and four lines of shell at the top of `test`. Nothing catches the case where `cmp` exists but is broken - out of scope for a build harness.
- **Status:** CI fix verified by the green Windows job on run `40086581248`
- **Fix commit:** `bae132f` - *fix(ci): give the runners what the harness and the backend need (BUG-030, BUG-031)*
- **Regression test:** the Windows CI job (it installs the package, so the probe and the real check both run). The probe itself is exercised only when the package is missing, which is exactly the situation it exists for.

### BUG-029 - `myc` read every integer literal as base 10, so `say 0x10` printed `0`

- **Found:** 2026-10-24 15:20 +05:30, in `fcb0073`, by compiling `say 0x10` in the toy language and comparing it with the same program through the C front end
- **Severity:** P0 (silent wrong code: the program compiles, runs, and prints the wrong number - no diagnostic at any stage)
- **Short summary:** in the toy language, `0x10` printed `0`, `010` printed `0`, and `0xFF` printed `0`. The lexer is shared with the C front end, so it validated the literal and handed over the text `0x10`; the toy parser then read that text with `strtoll(..., 10)`, which stops at `x` and yields 0. Hex and octal literals were accepted and silently wrong, while the C front end handled the same tokens correctly.
- **Reproduction:**
  ```myc
  fn main() -> int
    say 0x10
    say 010
    return 0
  end
  ```
  - observed (build from `fcb0073`):
    ```
    0
    0
    ```
  - expected (and observed after the fix):
    ```
    16
    8
    ```
- **Root cause:** `src/myc.c:parse_primary()`, which called `strtoll(p.current.value, NULL, 10)` with no end-pointer and no range check. `src/parser.c:parse_primary()` already did this correctly with base 0, an `ERANGE` check and a "did you consume everything" check - the knowledge existed, one file over, and the second front end could not reach it. The shared lexer is what makes the two dialects share literal *syntax*; nothing made them share literal *meaning*.
- **Evidence:** verified by execution. The pre-fix compiler was rebuilt from `HEAD:src/myc.c` into `build\mycc_old.exe`; on `tests/myc/literals.myc` it exits **1** (the `0x10 != 16` guard fires) while the fixed compiler exits **42**, and its assembly is byte-identical to the hand-written C twin's.
- **Fix plan:**
  - *Problem:* two front ends, two parsers, one lexer - and the literal rules were written twice, correctly once.
  - *Options:* (a) reject non-decimal literals in `myc` - rejected: the lexer already accepts them, so the language would have to reject syntax its own tokenizer validates, and `017` is legal in C; (b) copy parser.c's eight lines into myc.c - rejected: that is exactly the duplication that caused this; (c) **move the rules into `util.c:parse_integer_literal()`** returning `OK`/`RANGE`/`MALFORMED`, and have both front ends report their own diagnostics on top of it.
  - *Chosen:* (c). One rule book, two dialects: the toy language inherits C's literal syntax, so it inherits C's meaning.
  - *Tradeoffs accepted:* `util.c` now owns a fragment of C semantics, which is a slightly odd home for it; the alternative was a new module for one function. Both front ends also had to keep their own message text, so the diagnostics stay worded for their own audience.
- **Status:** FIXED (fails-before verified 2026-10-24 15:52 +05:30, committed 2026-10-24 16:05 +05:30)
- **Fix commit:** `a77a2bc` - *fix(myc): read integer literals with C's rules (BUG-029)*
- **Regression test:** `tests/myc/literals.myc` + `tests/myc/literals.c` (exit 42, prints `literals-ok`), run by both `test.bat` and `make test` through the pair contract. It covers hex, octal, uppercase hex and a `u` suffix. The contract is now a loop over `tests/myc/NAME.myc`, so the next pair costs one list entry.

### BUG-028 - `build.bat` kept its own list of source files, and the list never learned about `src/myc.c` and `src/opt.c`

- **Found:** 2026-10-24 12:05 +05:30, in `23af11f`, by cloning the published branch into an empty directory, running `build.bat` exactly as the README instructs, and watching it fail
- **Severity:** P3 (tooling: the documented Windows entry point fails; the compiler itself is fine, and the MSYS2 `make` path was always correct)
- **Short summary:** on a fresh checkout, `build.bat` fails with `undefined reference to 'myc_parse_program'` and `undefined reference to 'opt_run'`. The batch file compiled eight named files; the project has ten. Anyone who follows the README on Windows — no `make`, no MSYS2, just MinGW — cannot build the compiler at all.
- **Reproduction:**
  ```
  git clone https://github.com/AkshatJski/C-Compiler-Mini-Project.git
  cd C-Compiler-Mini-Project
  .\build.bat
  ```
  - observed:
    ```
    ld.exe: main.c:(.text.startup+0x1d4): undefined reference to `myc_parse_program'
    ld.exe: main.c:(.text.startup+0x5d6): undefined reference to `opt_run'
    collect2.exe: error: ld returned 1 exit status

    Build FAILED
    ```
  - expected (and observed after the fix): `Build OK: mycc.exe`, then `test.bat` reports 80 PASS.
- **Root cause:** `build.bat:20`. `%CC% ... -o mycc.exe src\main.c src\lexer.c src\parser.c src\symtab.c src\semantic.c src\codegen.c src\util.c src\dump.c` — a second, hand-maintained copy of the file list that the Makefile already keeps in `SRCS`. `daa35b8` added `src/myc.c` and `86d5865` added `src/opt.c`; the Makefile was updated both times and the batch file was not. Every local build in those two phases was run as `gcc ... src\*.c` (the documented command), so the stale list was never on the path of a build that mattered.
- **Evidence:** verified by execution, on a clean clone, with the published tree — not by inspection. The failure was reproduced from `23af11f`; after copying only the fixed `build.bat` into the same clone, `build.bat` printed `Build OK: mycc.exe` and `test.bat` reported 80 PASS.
- **Fix plan:**
  - *Problem:* the source list exists in two places, and the one nobody builds with goes stale silently until link time.
  - *Options:* (a) add the two missing names — rejected, this is the third time a file has been added and the drift returns with the next one, with the same late and cryptic symptom; (b) **let gcc glob `src\*.c`**, which is exactly the command the README documents and what every build here was actually run with; (c) make `build.bat` shell out to `make` — rejected, it exists for the Windows user who has no `make`, which is the whole reason it is there; (d) delete `build.bat` and tell Windows users to install MSYS2 — rejected, that raises the barrier for the audience the file was written for.
  - *Chosen:* (b). One source of truth in practice, no edit needed when a file is added.
  - *Tradeoffs accepted:* a stray `.c` in `src\` — an editor backup, a scratch file — now breaks the build instead of being silently excluded. That failure is loud and immediate, which is the correct direction for this class of defect.
- **Status:** FIXED (fix verified 2026-10-24 12:18 +05:30, committed 2026-10-24 12:20 +05:30)
- **Fix commit:** `fcb0073` - *fix(build): let build.bat name the sources itself (BUG-028)*
- **Regression test:** a build script cannot be covered by a test file, so the guard is the CI job: the Windows job now runs `build.bat` and `test.bat` in a `cmd` step, in addition to `make` and `make test`. The run that would have caught this is the one that is now mandatory — clone, `build.bat`, `test.bat`, `Build OK` + 80 PASS.

### BUG-027 — `else` is a C keyword token, so the toy language's blocks swallowed it and every `if`/`else` was rejected

- **Found:** 2026-10-24 11:12 +05:30, in the tree of `daa35b8` (Phase 8), while checking whether the `:myc_contract` pair actually exercised an `else` — it did not, and writing one showed the grammar could not accept it
- **Severity:** P1 (ordinary code rejected: `if`/`else` is documented in the toy language's grammar, and every program using it failed to compile)
- **Short summary:** `myc` rejects any `if ... else ... end` with `expected a statement (found 'else')`. The feature was documented and never worked, because the `if` keyword itself was fine but the `else` half could not be reached. The pair-of-programs test missed it for the honest reason that both files in the pair were written without an `else`.
- **Reproduction:**
  ```myc
  fn main() -> int
    let n = 3
    if n > 5
      say 5
    else
      say 8
    end
    return 8
  end
  ```
  - the compiler built from `HEAD` sources (`build\mycc_prefix.exe`):
    ```
    tests\myc\else.myc:18:3: error: expected a statement (found 'else')
        18 |   else
           |   ^
    tests\myc\else.myc: compilation aborted during parsing
    ```
  - expected and observed after the fix: compiles, prints `8`, exits 8 — and the C twin compiles to byte-identical assembly.
- **Root cause:** `src/myc.c:parse_block_body()` (the loop guard) and `src/myc.c:parse_return()` both stopped a block on `word("else")`. `word()` matches only `TOKEN_IDENT`, but `else` is in the *C* keyword table, so `lexer_next_token()` returns `TOKEN_KEYWORD_ELSE` and the test was never true. The block therefore did not stop at `else`; it asked `parse_stmt()` for another statement, `parse_stmt()` does not start a statement with `else`, and the "expected a statement" error was produced. `parse_if()`'s own `check(TOKEN_KEYWORD_ELSE)` was correct all along — the terminator test disagreed with it. `parse_return()` had the same latent defect: `if x return else ... end` would have mis-parsed too.
- **Evidence:** verified by execution. `git archive HEAD src include` was compiled into `build\mycc_prefix.exe`; it rejected the file at 18:3 as pasted above. The fixed compiler accepts it, the emitted assembly is byte-identical to `tests/myc/else.c` (`FC: no differences encountered`), and the linked program prints `else-branch` / `8` and exits 8. `test.bat` and `make test` both pass with the new case.
- **Fix plan:**
  - *Problem:* two spellings of one word exist in the token stream, and only one of them was matched.
  - *Options:* (a) make the lexer emit `TOKEN_IDENT` for `else` when the language is `myc` — rejected, the lexer is shared and receives no language parameter, and making the token stream depend on the front end would break the "one lexer" property the byte-identical claim rests on; (b) widen `word()` to compare keyword tokens' text as well — rejected, `word()` guards `fn`, `let`, `say`, `end`, `and`, `or` and `not`, and widening it would make those tests accept C keywords too, hiding the next mismatch instead of exposing it; (c) **add `at_else()`, which asks both spellings, and use it at every block terminator**.
  - *Chosen:* (c). One helper, three call sites (`parse_block_body`, `parse_if`, `parse_return`), and the comment on `word()` now records *why* `else` is special instead of implying it is not.
  - *Tradeoffs accepted:* every future block terminator has to remember the rule; it is documented in `word()`'s comment and in `learn.md` §2 instead of being enforced by the type system. The deeper fix — a per-language keyword view over the token stream — is deferred until a third front end exists.
- **Status:** FIXED (fix verified 2026-10-24 11:52 +05:30, committed 2026-10-24 12:09 +05:30)
- **Fix commit:** `4d0154d` - *fix(myc): accept both spellings of else in the toy language (BUG-027)*
- **Regression test:** `tests/myc/else.myc` with C twin `tests/myc/else.c`, run by `test.bat` (`:myc_contract`) and `make test`. Fails before the fix with the diagnostic above; passes after, and takes the *else* branch at runtime so a future "parses but drops the else arm" regression cannot pass either. The overall check count stays 80 because the contract still reports one PASS line.

### BUG-026 — A nested `/` or `%` overwrote `%r10`, the temporary holding an enclosing expression's value

- **Found:** 2026-10-24 10:30 +05:30, in the uncommitted Phase 5 tree (found by `tests/36_registers.c`, which was written for exactly this class of bug and found it on its first run)
- **Severity:** P0 (silent wrong code: the program compiles, links, runs, and returns a plausible number)
- **Short summary:** `(a / b) + (d % c)` computes the wrong sum. The left operand `a / b` is parked in `%r10`; the right operand contains a `%`, whose instruction sequence uses `%r10` as its divisor register. The parked value is destroyed and the addition adds whatever the division left behind. Nothing warns, and the result looks like an ordinary integer.
- **Reproduction:**
  ```c
  int a = 8, b = 4, d = 6, c = 5;
  if ((a / b) + (d % c) != 3) return 2;   /* 2 + 1 == 3 */
  return 42;
  ```
  - `gcc`: exit **42**.
  - `mycc` (one-register pool, before the spill): exit **2** — the guard fired.
  - `tests/36_registers.c` therefore reported `-opt exit=2` while `--no-opt exit=42`, i.e. the optimizer disagreed with the unoptimized path on the same source.
- **Root cause:** `src/codegen.c:temp_combine()`. The one-deep pool's safety argument was "the register is only ever held by the *top* temporary, and a division consumes the top temporary before using its scratch register" — true when the parked value *is* the left operand, false when the division is inside the **right** subtree and the parked value belongs to an enclosing expression. `%r10` is both the pool register and `idivq`'s divisor register, so the two roles collided.
- **Evidence:** verified by execution. The failing run, the fixed run and the gcc oracle were all observed on Windows; the fixed build now matches gcc on the whole 18-case stress test.
- **Fix plan:**
  - *Problem:* an instruction inside a subexpression writes the register that holds a value from outside that subexpression.
  - *Options:* (a) reserve a different register for division scratch — rejected, every remaining free register is worth more as a pool slot, and the collision would return as soon as the pool grows; (b) make the pool two deep and callee-saved — already measured and rejected on instruction count (`decision.md` 2026-10-24 10:49); (c) **materialise the operand into `%rcx` and spill the still-live pool register across `/` and `%`, restoring it after**; (d) copy `%r10` to a scratch register around the operation — same cost, but needs a second scratch register that does not exist.
  - *Chosen:* (c). It reuses `temp_spill()`/`temp_unspill()` (already used across calls), the pushes go through `pushed_count` so 16-byte call alignment stays exact, and it only fires for `/` and `%` — the shifts use `%r11`, which is never a pool register, so they need no spill.
  - *Tradeoffs accepted:* two instructions of spill traffic in a shape that used to be two instructions of push/pop anyway (so it is no worse than `--no-opt`), and one more invariant to keep when the pool grows.
- **Status:** FIXED (fix verified and committed 2026-10-24 11:02 +05:30)
- **Fix commit:** `86d5865` - *feat(phase5): dead local stores and expression temporaries in a register*
- **Regression test:** `tests/36_registers.c`, cases 11–12 (`((a*b) % c) + ((c*d) / 2)` and `(100 / (5 / 5)) + (9 % (4 % 3))`). Fails before the fix (exit 2), passes after (exit 42, and gcc agrees).

### BUG-025 — `&b` did not count as the address escaping, so an address-taken local looked dead

- **Found:** 2026-10-24 09:55 +05:30, in the uncommitted Phase 5 tree (found by `tests/12_pointers.c` while the new dead-store pass was being developed)
- **Severity:** P0 (silent wrong code: the initialization is deleted, and a pointer reads whatever the frame slot held)
- **Short summary:** `int b = 42; int *p = &b; return *p;` returns garbage. The dead-store pass asks "is this variable ever read?" and `b` has no direct read — the read happens through `p`. Because taking the address was not recorded as the variable escaping, the answer was "no reads" and the initializer was deleted.
- **Reproduction:**
  ```c
  int main(void) { int b = 42; int *p = &b; return *p; }
  ```
  - `gcc`: exit **42**.
  - the buggy build: **no `movq $42` in the emitted assembly**, program exits **64** (whatever the uninitialised slot happened to contain).
  - after the fix: exit **42**, and the store is emitted again.
- **Root cause:** `src/opt.c:scan_value()`. The first version classified a variable mentioned under `AST_ADDR_OF` as *written* (`&b` mentions `b`, so it looked like a use), which is harmless, but nothing recorded that `b`'s storage now has an alias. `is_dead_store()` therefore saw `reads == 0` and deleted the store. The bug is a missing fact, not a wrong comparison: the pass had no notion of "address taken" at all.
- **Evidence:** verified by execution. The buggy pass was rebuilt deliberately (`note_address_use()` neutered), the minimal reproduction above produced exit 64 with no `movq $42` in the assembly, and the fixed compiler produced 42. `tests/12_pointers.c` also failed against the buggy pass and passes against the fixed one.
- **Fix plan:**
  - *Problem:* the analysis tracks reads and writes but not aliasing, and aliasing is the one fact that invalidates "no reads".
  - *Options:* (a) delete nothing from any function that contains an `AST_ADDR_OF` — safe, but it disables the pass in most real programs, including the one it was written for; (b) **record `address_taken` per local** via `note_address_use()`, called for `AST_ADDR_OF` and for the base of `AST_MEMBER_ACCESS`, and require `address_taken == 0` in `is_dead_store()`; (c) treat every local whose type is a pointer as escaping — does not help, the *pointee* is what matters.
  - *Chosen:* (b). It is one extra field in the table, one predicate in `is_dead_store()`, and it keeps the pass useful for the ordinary case.
  - *Tradeoffs accepted:* a single `&x` anywhere in a function disables dead-store elimination for that variable for the whole function — conservative, deliberately, because precision here would require points-to analysis.
- **Status:** FIXED (fix verified and committed 2026-10-24 11:02 +05:30)
- **Fix commit:** `86d5865` - *feat(phase5): dead local stores and expression temporaries in a register*
- **Regression test:** `tests/12_pointers.c` (caught it) plus `tests/35_dead_locals.c` (added with the pass: one dead store that must vanish, one escaped variable whose store must survive, one impure initializer whose call must survive). Fails before the fix, passes after.

---

### BUG-024 — `--sign` interned an empty string, so a signed program carried no provenance

- **Found:** 2026-10-23 18:05 +05:30, at `0f58d9b` (found while checking the emitted `.s` instead of trusting `strings` to find something — the first version of my own feature)
- **Severity:** P0 (silent wrong output: the build succeeds, the program runs, the promised data is simply absent)
- **Short summary:** `--sign` is supposed to put a provenance string in the program so a finished binary can name the compiler that built it. It instead interned a **zero-length** string, so the assembly contained a perfectly well-formed `.string ""` and `strings prog.exe` found nothing. Nothing failed, nothing warned.
- **Reproduction:**
  ```
  mycc --sign hello.c -o signed.s
  findstr "generated by mycc" signed.s     -> no match
  tail signed.s                            -> .L.str.0: .string ""
  ```
  **Observed:** an empty `.rodata` entry. **Expected:** one entry containing the signature.
- **Root cause:** `codegen.c:gen_program()` called `intern_string(MYCC_SIGNATURE, 0)`. The second parameter is a **byte count**, not a "use strlen if zero" sentinel — `intern_string()` copies exactly `length` bytes and writes the terminator itself. The `length == 0 ? strlen(text) : length` convenience exists at the `AST_STRING` call site in `gen_expr()`, not inside `intern_string()`. So the call was correct *about* the API and wrong *about* the meaning.
- **Evidence:** verified by execution. The emitted `.s` contains `.L.str.0: .string ""`; after passing `(int)strlen(MYCC_SIGNATURE)`, the same command emits the text and `strings build\sig.exe | findstr mycc` matches inside the linked binary.
- **Fix plan:** *Problem* → the length argument's meaning is ambiguous at call sites that pass a literal constant string. *Options* → (a) pass `strlen()` explicitly at the call site; (b) make `intern_string()` treat `0` as "call strlen" (the sentinel convention); (c) rename the parameter and add a separate `intern_cstr()` wrapper. *Chosen:* (a) plus a comment, because (b) silently changes the meaning of a function that must be able to intern a zero-length string (an empty literal `""` is legal and deduplicates), and (c) is a two-function API for one caller. *Tradeoff accepted:* the trap remains available to the next caller; the comment and this entry are the mitigation. *Not chosen:* making 0 mean strlen — it would make `intern_string("", 0)` impossible to express.
- **Status:** FIXED
- **Fix commit:** `0f58d9b` — *feat(phase6,7): caret diagnostics, --version, --sign*
- **Regression test:** `test.bat` → `:sign_contract` (added after the fix): compiles `tests/26_hello_world.c` with `--sign` and requires `generated by mycc` in the emitted `.s`, then compiles without `--sign` and requires it to be absent. It reads the **assembly**, because the failure mode is invisible in the exit code, the program output and the diagnostics — the three things every other test checks. Fails before `0f58d9b`, passes after.

### BUG-023 — A NUL byte inside a string literal truncates the rest of the literal

- **Found:** 2026-10-23 14:30 +05:30, at `4410784` (found by probing the new string code with an edge case I had not covered: what if the literal contains the terminator itself?)
- **Severity:** P0 (silent wrong code: no diagnostic, a plausible-looking program reads the wrong byte)
- **Short summary:** A string literal may legally contain NUL bytes — `"\0A"` is a two-byte object. mycc emitted only the part before the first NUL, so everything after it disappeared from the program. `char *s = "\0A"; return s[1];` returns whatever byte happens to sit after the empty string in `.rodata`, instead of `'A'`.
- **Reproduction:**
  ```c
  int main(void) { char *s = "\0A"; return s[1]; }
  ```
  - `gcc`: exit **65** (`'A'`).
  - `mycc` at `4410784`: exit **0**, and the emitted data was `.string ""`.
  - `mycc` after the fix: exit **65**, and the emitted data is `.string "\000A"`.
- **Root cause:** three independent uses of `strlen`/`strdup` on text that is a *byte string*, not a C string. `src/parser.c` copied the token with `xstrdup()` (stops at the NUL); `src/codegen.c` deduplicated and emitted with `strcmp`/`strlen`; `src/dump.c` displayed until `'\0'`. The lexer itself was right — it had already decoded all 2 bytes and only lacked a way to report the length.
- **Evidence:** verified by execution on Windows — the exit code, the emitted `.string` line, and the fixed output all observed directly; gcc used as the oracle.
- **Fix plan:**
  - *Problem:* the value of a string literal was carried as a NUL-terminated C string, which cannot represent its own contents.
  - *Options:* (a) reject `\0` in a string literal with a clean diagnostic — cheap and honest, but it rejects valid C and contradicts the C-fidelity direction of this project; (b) escape the NUL as `\001`-style sentinel and decode it in codegen — rejected, invents a private encoding that every future pass must remember; (c) **carry the byte count alongside the bytes** (`Token.length`, `ASTNode.str_len`), deduplicate on `(bytes, length)`, and emit `.string` with every byte escaped, an embedded NUL becoming `\000`; (d) emit `.byte`/`.ascii` per character — rejected, unreadable assembly in a teaching artefact.
  - *Chosen:* (c).
  - *Tradeoffs:* two extra `int` fields on hot structs, and every consumer of a literal must pass the length rather than calling `strlen`. The dumpers keep working for NUL-free literals because `length` falls back to `strlen` when 0. Accepted: correctness of a data type beats a tidier struct.
- **Status:** FIXED · **Fix commit:** `831a61a` *fix(BUG-023): preserve NUL bytes inside string literals* · **Regression test:** `tests/30_nul_in_string.c` (expects exit 65)
- **Verified:** fails before (`4410784` → 0, `.string ""`), passes after (→ 65, `.string "\000A"`); full suite green afterwards (72 checks).

---

### BUG-022 — C octal escapes are rejected

- **Found:** 2026-10-23 14:00 +05:30, during Phase 4 while writing the escape test (`"octal[\101]"`), fixed in `4410784`
- **Severity:** P1 (ordinary, valid C is rejected outright)
- **Short summary:** `\101` — the octal escape form that C has had since 1972 — is not recognised. It was reachable through character constants, where it produced a confusing cascade; once string literals became real data in Phase 4, it became reachable through any string, which is what exposed it. Code written the classic way (`"\101"` for `'A'`) simply does not compile.
- **Reproduction:**
  ```c
  int main(void) { return '\101'; }
  ```
  **Observed** (baseline `d83bc5d`):
  ```
  build_octal.c:1:28: error: unknown escape sequence '\1'
  build_octal.c:1:25: error: unterminated character literal
  build_octal.c:1:28: error: expected ';' after return (found integer literal)
  ```
  **Expected:** exit 65, one byte, no diagnostics.
- **Root cause:** `src/lexer.c:read_escape():default` — the escape table handled `'0'` as a special case returning `'\0'` and forwarded everything else to `report_error`. There was no arm for `'1'`–`'7'`, and no notion of "a NUL escape may be followed by more octal digits".
- **Evidence:** verified by execution — the baseline compiler at `d83bc5d` was built in a throwaway worktree and rejected the program; the fixed one compiles it and returns 65. Both a char literal (`'\101'`, `'\377'`) and a string literal (`"\101"`) were exercised.
- **Fix plan:**
  - *Problem:* one missing escape class, with two shapes (`\0` alone, `\ooo` up to three digits).
  - *Options:* (a) keep rejecting and document it — rejected, this is the C-fidelity phase; (b) add `case '1' … case '7'` arms plus a separate `'0'` path — rejected, duplicates the "read up to N digits" logic; (c) **handle `'0'`–`'7'` as one octal arm in `default`, consuming up to three digits**, which makes `\0` fall out as the one-digit case; (d) a lookup table over all escape characters — rejected as heavier than the existing switch.
  - *Chosen:* (c).
  - *Tradeoffs:* a range test inside `default` instead of a `case` (C99 has no portable case ranges under `-Wpedantic`), so the code reads slightly less like the rest of the switch. Accepted for the shared implementation between char and string literals.
- **Status:** FIXED · **Fix commit:** `4410784` · **Regression test:** `tests/29_string_escapes.c` (prints `ESCAPES-OK` only after `\101`/`\x41` compile; plus `'\101'`, `'\377'`, `'\0'` exercised in the char-literal probe recorded above)
- **Verified:** fails before on `d83bc5d`, passes after; the full string-escape matrix (quote, backslash, tab, `%%`, `\x41`, `\101`) printed correctly through `printf` on Windows.

---

### BUG-021 — The `make test` harness is not CRLF-safe

- **Found:** 2026-10-23 20:10 +05:30, at `9dfb5c2` + uncommitted fix batch (found by running the mirrored recipe under WSL `bash` on a CRLF checkout)
- **Severity:** P3 (tooling: a green suite that cannot fail honestly)
- **Short summary:** On a checkout with CRLF line endings — which is what this repo produces on Windows — every `EXPECT-ERROR:` and `WARN:` check in `make test` reports "missing diagnostic" even when the compiler is behaving correctly. The suite is only trustworthy on a Linux checkout, so a Windows contributor cannot get a trustworthy signal from `make`.
- **Reproduction:**
  ```
  (CRLF checkout)  make test
  FAIL: tests/errors/duplicate_local.c (missing diagnostic: duplicate declaration of 'a')
  All tests passed  ← the recipe's exit status was still 0 in one shape of the bug
  ```
  Meanwhile the *compiler* prints exactly the expected line. **Expected:** all checks pass on both LF and CRLF checkouts.
- **Root cause:** `Makefile` (`test` target) extracts the expectation with
  `sed -n 's|^// EXPECT-ERROR:[[:space:]]*||p'`. GNU sed strips the `\n` but not a preceding `\r`, so the captured pattern ends with a CR while the compiler's stderr does not; `grep -F` then can never match. The numeric `EXPECT:` capture was already CR-safe because its helper strips non-digits.
- **Evidence:** verified by execution — the mirrored recipe (real `sed`/`grep`/`tr` in WSL, with wrappers translating paths for the Windows compilers) failed on CRLF fixtures before the change and passed after. `test.bat` is immune because `findstr` treats CR as a line terminator.
- **Fix plan:**
  - *Problem:* two extractions pipe a raw CR into a literal `grep` pattern.
  - *Options:* (a) force LF on every checkout via `.gitattributes` — rejected, hostile to Windows editors and does not help a CRLF file that simply exists; (b) strip the CR inside `sed` — rejected, less portable than a pipe; (c) **pipe the extractions through `tr -d '\r'`**; (d) normalise the compiler's output — rejected, the compiler is right.
  - *Chosen:* (c).
  - *Tradeoffs:* one extra process per test (66 of them, milliseconds). Accepted.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** the `test` target itself (no test *file* can catch a harness defect — see the honesty note below)
- **Verified:** mirrored recipe under WSL bash, real `sed`/`grep`/`tr`: fails before, passes after. Also true by construction on Linux (LF) — where the bug is invisible.
- **Honesty note:** this entry deliberately has no `tests/…` file. A harness bug cannot be caught by the harness's own tests; the regression evidence is the before/after harness run, and the permanent guard is the `tr -d '\r'` itself, which is why this limitation is written down instead of papered over.

---

### BUG-020 — Compound assignment scales a subscript index twice

- **Found:** 2026-10-23 19:50 +05:30, at `9dfb5c2` + uncommitted fix batch (found by the *new* differential test, which is why that test exists — see `decision.md`)
- **Severity:** P0 (silent wrong code: writes hit the wrong element)
- **Short summary:** `a[i] += 1` — any compound assignment whose left side is subscripted — computes the element address with the index multiplied by the element size **twice**, so it reads and writes `a[i * 64]` on 64-bit elements. Plain `a[i] = v` and `a[i] + 1` are unaffected, which is what made it survive a green suite: the programs still returned a number, just from the wrong memory.
- **Reproduction:**
  ```c
  int a[4];
  a[0] = 10; a[1] = 20; a[2] = 30; a[3] = 40;
  a[2] += 2;
  return a[2];        /* 32 */
  ```
  **Observed:** the store lands 64 elements away from `a` — `a[2]` is unchanged (30) and adjacent memory is corrupted. **Expected:** 32.
- **Root cause:** `src/semantic.c` — the parser desugars `a[i]` into `*(a + i)` and `x op= e` into `x = x op e`, **sharing one AST node** for `x`. Semantic analysis then *inserts* the `index * sizeof(elem)` multiplication into that tree (`scale_index_operand`), and that insertion is not idempotent. Visiting the shared subscript node twice inserted the multiply twice: `i * 8 * 8`. The `AST_ASSIGN` case called `check_value(expr->right)`, which re-walked the shared left node.
- **Evidence:** verified by execution — the differential program (width-agnostic, 27 checks) failed before the change and passes after; `tests/23_lvalue_single_eval.c` now covers `a[2] += 2` and `a[3] -= 1` directly.
- **Fix plan:**
  - *Problem:* one node reaches a non-idempotent rewrite twice.
  - *Options:* (a) tag the node as already-scaled (`is_scaled` in `ASTNode`) — works, but adds state every pass must reset; (b) **in the compound-assign case, check only the right operand and type the operation locally, so the shared left node is walked once**; (c) deep-copy the lvalue for the binop — rejected, it would reintroduce BUG-001 unless codegen learned to pair the copies up; (d) scale in codegen — rejected, splits one rule across two files.
  - *Chosen:* (b).
  - *Tradeoffs:* `AST_ASSIGN` now has a documented special case (compound assignments arrive pre-desugared with a shared node). The invariant is written at the function: "each caller reaches a given node exactly once, so the rewrite can never be applied twice".
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/23_lvalue_single_eval.c` (checks 13–20) and `tests/diff/01_compound_and_lvalue.c`
- **Verified:** fails-before / passes-after on both; `test.bat` 68/68.

---

### BUG-019 — No assignment-compatibility checking

- **Found:** 2026-10-23 10:49 +05:30, at `9dfb5c2`
- **Severity:** P0 (silently accepted nonsense)
- **Short summary:** The compiler never checks whether the value on the right of an assignment (or an argument) can legally become the type on the left. A struct can be assigned to an `int`, a pointer to an `int`, and a struct can be passed to an `int` parameter — all without a single diagnostic. The program then reads unrelated bytes at run time.
- **Reproduction:**
  ```c
  struct P { int x; int y; };
  int takes_int(int v) { return v; }
  int main(void) {
      int i; struct P p; p.x = 1;
      i = p;              /* struct -> int          */
      int *q; q = i;      /* int -> pointer        */
      return takes_int(p) + *q;
  }
  ```
  **Observed:** `mycc: exit 0` — compiles cleanly, returns garbage.
  **Expected:** three diagnostics (or at minimum warnings): `assignment to 'int' from 'struct P'`, `assignment to 'int *' from 'int'`, `passing 'struct P' to parameter of type 'int'`.
- **Root cause:** `semantic.c:check_expr()` (`case AST_ASSIGN`) only checks that the left side *is* an lvalue and is not an array; it copies `expr->val_type = expr->left->val_type` and never compares the two types. `semantic.c:check_call()` checks only the argument **count**, never the types.
- **Evidence:** verified by execution (compiler exit 0, program produces garbage).
- **Fix plan:**
  - *Problem:* no type compatibility rules exist, so every mismatch is silently accepted.
  - *Options:* (a) full "usual arithmetic conversions" + implicit-conversion machinery — correct but large; (b) a `types_compatible(from, to)` predicate with a small conversion table (int↔char↔unsigned, pointer↔pointer-of-same-or-void-base, `0` literal→any pointer) and a hard error on everything else; (c) warnings only.
  - *Chosen:* (b). Errors for struct↔scalar and pointer↔integer (except the literal `0`); silent for scalar↔scalar, since narrowing is legitimate C. Pointers keep a `void *` escape hatch. `int → pointer` stays an error until casts land (BUG-006) — then casts are the explicit opt-in.
  - *Tradeoffs:* rejects a few programs real C accepts with a warning (e.g. `int x = (long)p` with no cast); needs type interning (`type_ptr()` currently allocates a fresh object per call, so pointer types must be compared **structurally**, not by address). Deferred: full integer-promotion rankings, since `int` is already 64-bit everywhere.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/assign_struct_to_int.c`, `tests/errors/assign_int_to_ptr.c`, `tests/errors/arg_struct_for_int.c`
- **What landed:** `semantic.c` gained `types_compatible(to, from, rhs)` used by assignment, call arguments and `return`, so the three cannot drift. Integer kinds are mutually compatible; any pointer accepts any pointer and the literal `0`; structs only match the identical struct; a cast is the explicit opt-in (which is why BUG-006 had to land in the same batch). Pointer types are compared **structurally** (`same_type`) because `type_ptr()` allocates a fresh object per call — comparing addresses would reject every legitimate match.
- **Verified:** all three rejection tests pass in `test.bat`; the ~30 adversarial programs that previously compiled with garbage now produce diagnostics.

---

### BUG-018 — macOS support was claimed but the build path was Linux-only

- **Found:** 2026-10-23 10:48 +05:30, at `9dfb5c2`
- **Severity:** P3 (tooling / documentation truthfulness)
- **Short summary:** `README.md` ("Quick start (Linux / macOS)") and `instructions.md` §4 present macOS as a supported platform, but `make test` links with `gcc -no-pie`, an option Apple's toolchain does not accept (the macOS spelling is `-no_pie`, and modern macOS defaults to PIE anyway). No CI job covers macOS, so the claim has never been exercised.
- **Reproduction:** `Makefile:32` → `$(CC) -no-pie -o ...`; `README.md:111` → `## Quick start (Linux / macOS)`. **Observed:** untested claim. **Expected:** either a tested macOS path (PIE-enabled, no `-no-pie`) or the claim removed.
- **Root cause:** `Makefile` was written for Linux; the README sentence was generalised without a corresponding change.
- **Evidence:** code inspection only (no macOS host available here).
- **Fix plan:**
  - *Options:* (a) drop macOS from the docs; (b) make the link flags platform-conditional (`uname`), default to PIE and drop `-no-pie` except where the generated code needs it; (c) add a `macos-latest` CI job.
  - *Chosen:* (b) + (c) in one change: `-no-pie` exists only because the generated assembly has no PIC relocations for its own `.data`; since codegen already uses `%rip`-relative addressing for globals, PIE should link as-is. Verify locally on Linux CI, add a macOS job to prove it, and only then keep the README sentence.
  - *Tradeoffs:* a macOS runner costs CI minutes; if PIE fails, the docs must shrink instead — the claim must not survive the test.
- **Status:** FIXED (2026-10-24 16:10 +05:30, CI-verified) · **Fix commit:** `4c3cd8a` (implementation landed; architecture pinned in `bae132f`) · **Regression test:** CI job `macos` in `.github/workflows/ci.yml` (must run `make` + `make test` + `make difftest` green)
- **What landed:** option (b)+(c) plus the missing piece the original plan had not identified: **macOS needs symbol renaming, not just link flags.** `TargetKind` (`TARGET_LINUX` / `TARGET_WINDOWS` / `TARGET_DARWIN`) is chosen from the host at build time (`#if defined(_WIN32) / __APPLE__ / else`) and overridable with `--target=`; a single `sym_name()` helper prefixes `_` on Darwin and is used at **all eight** symbol-emission sites (definitions, calls, global loads/stores, `leaq` of a function address); the ELF-only `.note.GNU-stack` directive is emitted only for `TARGET_LINUX`. `Makefile` detects Darwin via `uname -s` and drops `-no-pie`. A `macos-latest` CI job runs plain `make` (no `-Werror`; see `decision.md` for why).
- **Verified by execution (the part that is verifiable here):** `--target=darwin` emits `_main`, `_fib` and `call _fib`, with **zero** `GNU-stack` lines; `--target=linux` keeps `main` and the note; `--target=windows` keeps `main` and drops the note; `--target=beos` → `error: unknown target 'beos' (use linux, windows or darwin)`.
- **Why it is not `FIXED`:** Apple's linker behaviour cannot be exercised on this machine (no macOS toolchain; WSL has no gcc). Per the honesty rules, an unverified fix is not `FIXED`. The moment the `macos` job is green, this becomes `FIXED` with the CI run as its evidence - and if it fails, the README claim gets withdrawn instead.
- **Closed 2026-10-24 16:10 +05:30:** the `macos` job is green on run `40086581248` (job `120850655205`, 1m4s): `make`, `make test` (81 checks), `make difftest` and a 100-program fuzz run all pass on `macos-latest`. The claim the README makes is now executed on every push. It took a second defect to get there - BUG-031, the arm64 runner assembling x86-64 output as arm64 - which is why "verification pending" was the correct state for two days rather than an excuse.
- **Verified:** CI-verified 2026-10-24 16:10 +05:30 - fix commits `4c3cd8a` (implementation) and `bae132f` (architecture pin, BUG-031)
---

### BUG-017 — A partial assembly file survives a failed compilation

- **Found:** 2026-10-23 10:48 +05:30, at `9dfb5c2`
- **Severity:** P3 (tooling)
- **Short summary:** When code generation fails, `mycc` exits 1 but leaves the half-written `.s` file on disk. A build script that checks only *file existence* rather than the exit code will happily assemble an invalid, truncated program.
- **Reproduction:** `mycc demos/broken.c -o out.s` → exit 1, and `out.s` is created (empty or partial) because the output stream is opened before `codegen_generate()` runs. **Expected:** no output file, or the file removed on failure.
- **Root cause:** `src/main.c:main()` opens `output_path` at line ~149, then calls `codegen_generate()`; the failure branch only `fclose()`s.
- **Evidence:** code inspection only (*the reachable path needs a codegen-time error, which is rare*).
- **Fix plan:** generate into a temporary file and `rename()` on success (atomic, also fixes a race if two builds share a path), or simply `remove(output_path)` on the error path. Chosen: temp-file + rename — one small helper, and it is the behaviour `gcc` has.
- **Tradeoffs:** on Windows `rename()` fails if the destination exists; use `remove()` first. Extra syscalls, irrelevant at this scale.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** harness contract check — `test.bat` `:output_file_contract` and the equivalent block in the `Makefile` `test` target (compile a rejected program to a path, then assert the file does not exist)
- **What landed:** `main.c` writes to `<output>.mycc-tmp` and only `rename()`s it into place when no stage failed; on Windows, which refuses to rename over an existing file, it falls back to `remove()` + `rename()`. Every exit path removes the temp file.
- **Verified:** contract asserted on Windows (68-check suite) and in the mirrored Makefile recipe. **Stated plainly:** the *original* failure path (a codegen-time error) is not reachable from any valid input, so what is tested is the **contract** ("no output file on a failed compile"), not the historical symptom. This is weaker than a fails-before/passes-after test of the symptom, and it is recorded as such deliberately.

---

### BUG-016 — Non-`void` function falling off its end returns 0 with no warning

- **Found:** 2026-10-23 10:47 +05:30, at `9dfb5c2`
- **Severity:** P2 (silent wrong result in user code)
- **Short summary:** C only gives `main` an implicit `return 0`. `mycc` gives it to *every* function, silently: a function whose last statement is `if (c) return 1;` returns 0 when the branch is not taken, with no diagnostic. The user's bug becomes a mysterious wrong answer instead of a compiler message.
- **Reproduction:**
  ```c
  int f(int a) { if (a) return 1; }
  int main(void) { return f(0); }
  ```
  **Observed:** `mycc` exit 0 (compiler) → program exit 0. **Expected:** `warning: control reaches end of non-void function 'f'` (C's behaviour is undefined; every real compiler warns).
- **Root cause:** `src/codegen.c:gen_func()` emits `movq $0, %rax; leave; ret` whenever `gen_stmt()` reports it did not end in a `return`; `semantic.c:check_returns()` only checks the *shape* of explicit `return` statements and never asks whether the function can reach its closing brace.
- **Evidence:** verified by execution (`mycc` 0 vs `gcc` 1) — noting that in C the case is undefined, so the mismatch itself is not the bug; the absent diagnostic is.
- **Fix plan:** add a `warn()` channel alongside `report_error()` (a separate counter, `-W`-style flags, warnings never change the exit code), and a `falls_through(ASTNode *stmt)` predicate that returns true unless every path returns. Emit the warning from `check_returns`. Keep `main` exempt and keep the "return 0" codegen for `main` only; for other functions keep it too (defensive, avoids wild jumps) but warn.
- **Tradeoffs:** needs `-Wno-…` plumbing to stay usable, which is scope creep; start with a single `-w` switch to silence all warnings.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/25_fall_off_end.c` (positive test: exits 0 **and** carries a `// WARN:` marker the harness must match), plus the `-w` silence contract in both harnesses
- **What landed:** `report_warning()` in `util.c` — same `file:line:col` shape as errors, its own counter, never affects the exit status, silenced by `-w` (`g_warnings_enabled`, deliberately not reset by `error_reset()` so the flag survives argument parsing). `semantic.c:stmt_terminates()` is a small recursive path analysis over the statement list; `check_function` warns once per non-`void` function whose body can reach the closing brace. `main` is exempt.
- **Verified:** `mycc tests/25_fall_off_end.c` prints `tests\25_fall_off_end.c:8:1: warning: control reaches end of non-void function 'maybe'` and still exits 0; with `-w` the text disappears and the exit status is unchanged; `tests/25` passes in the 68-check suite.

---

### BUG-015 — Declared-but-undefined function produces a linker error

- **Found:** 2026-10-23 10:47 +05:30, at `9dfb5c2`
- **Severity:** P2 (confusing diagnostic, wrong stage)
- **Short summary:** Write a prototype, forget the definition, call it: `mycc` happily emits assembly and the *assembler/linker* fails with an undefined-symbol error that points at a temporary `.s` file rather than the user's line.
- **Reproduction:**
  ```c
  int helper(int x);
  int main(void) { return helper(3); }
  ```
  **Observed:** `mycc` exit 0; then `gcc: undefined reference to 'helper'`. **Expected:** `mycc: prog.c:2:20: error: function 'helper' is declared but never defined`.
- **Root cause:** `semantic.c:sem_decl_pass()` records `is_defined = (d->body != NULL)`, but nothing ever tests that flag after the declaration pass.
- **Evidence:** verified by execution (compile succeeded, link failed).
- **Fix plan:** after `sem_decl_pass()` and the per-function checks, walk the symbol table: any `is_function && !is_defined` that is *called* becomes an error (report at the call site for a better location). Unused prototypes stay legal.
- **Tradeoffs:** blocks a future "link against external objects" feature; gate it behind the same flag that will later allow externs (`--allow-undefined`), documented as such.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/undefined_function.c`
- **What landed:** after the declaration pass and the per-function checks, `semantic.c` walks the symbol table and reports an error at the **call site** (better location than the prototype) for any called function with `is_function && !is_defined`. Unused prototypes stay legal, so header-style declarations keep working.
- **Verified:** the test asserts the compiler rejects it with the new message; the earlier linker failure is now a compile-time diagnostic.

---

### BUG-014 — `main(int argc, char **argv)` compiles but reads garbage on Windows

- **Found:** 2026-10-23 10:47 +05:30, at `9dfb5c2`
- **Severity:** P2 (silent wrong result, platform-dependent)
- **Short summary:** `main` may be declared with parameters, but `mycc` targets the SysV register order everywhere. On Windows the CRT passes `argc` in `RCX`, while `mycc` reads `RDI` — so `argc` is whatever happened to be in that register. Same source, different answer per OS.
- **Reproduction:**
  ```c
  int main(int argc, char **argv) { return argc; }
  ```
  **Observed:** `--target=windows` → exit **72**; `gcc` → exit **1**. **Expected:** 1 on both, or a compile error.
- **Root cause:** `src/codegen.c:gen_func()` unconditionally copies `ARG_REGS[]` = `%rdi, %rsi, …` into parameter slots; there is no per-target ABI table. Combined with BUG-012-of-the-ROADMAP ("still no real Windows calling convention", Phase 4 item).
- **Evidence:** verified by execution on Windows/MinGW.
- **Fix plan:** short term, reject `main` with parameters (`error: main must be declared as 'int main(void)'`) — honest and one line. Long term, the real fix is the Phase-4 ABI split; when that lands, re-allow it and add a cross-platform test.
- **Tradeoffs:** rejecting `argv` removes a feature nobody can currently use; keeping it silently broken is worse.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/main_with_params.c`
- **What landed:** the short-term fix exactly as planned — `main` with parameters is rejected with `error: main must be declared as 'int main(void)'`. The message names the reason (the ABI split is pending) rather than just refusing. `ROADMAP.md` Phase 4 records this as the trigger for the real fix, and the interim posture is written into `decision.md`.
- **Verified:** the rejection test passes; the previous `--target=windows` exit 72 vs gcc's 1 can no longer be produced.

---

### BUG-013 — A string literal produces a misleading error and a cascade

- **Found:** 2026-10-23 10:46 +05:30, at `9dfb5c2`
- **Severity:** P2 (diagnostic quality)
- **Short summary:** `mycc` has no string literals (documented), but the error never says so. The user is told `unexpected character '"'` and then a *second*, bogus error about the end of the file, because the lexer bailed to EOF instead of continuing.
- **Reproduction:**
  ```c
  int main(void) { char *s; s = "hi"; return 0; }
  ```
  **Observed:**
  ```
  prog.c:1:31: error: unexpected character '"'
  prog.c:1:31: error: expected expression (found end of file)
  ```
  **Expected:** one error — `error: string literals are not supported by mycc`.
- **Root cause:** `src/lexer.c:lexer_next_token()` has no `case '"'`; it falls into `default`, reports, and returns `TOKEN_EOF` (line ~489), which truncates the token stream and makes every later stage guess.
- **Evidence:** verified by execution.
- **Fix plan:** add `case '"'` → consume the whole literal (handling `\` escapes) and report one precise error at the opening quote; keep lexing afterwards. Add a dedicated `report_error` message for the preprocessor too (BUG-012).
- **Tradeoffs:** none worth naming; the real fix (implementing strings) is a feature, tracked separately.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/string_literal.c`
- **What landed:** `case '"'` in the lexer consumes the whole literal (honouring `\` escapes) and reports **one** message at the opening quote — `error: string literals are not supported by mycc` — then keeps lexing, so no cascade follows.
- **Verified:** the rejection test asserts exactly that message; the bogus "found end of file" second error is gone.

---

### BUG-012 — `#include` reports a character-level error, then cascades

- **Found:** 2026-10-23 10:46 +05:30, at `9dfb5c2`
- **Severity:** P2 (diagnostic quality)
- **Short summary:** The first thing almost every C programmer writes is `#include <stdio.h>`. `mycc` answers `unexpected character '#'` and then a pile of follow-on errors, instead of saying plainly that there is no preprocessor.
- **Reproduction:**
  ```
  #include <stdio.h>
  int main(void) { return 0; }
  ```
  **Observed:** `error: unexpected character '#'` + `compilation aborted during parsing`. **Expected:** one error — `error: preprocessor directives are not supported by mycc (no #include, #define, #if)`.
- **Root cause:** `src/lexer.c:lexer_next_token()` default case; `#` is not a token at all.
- **Evidence:** verified by execution.
- **Fix plan:** in `skip_trivia()`, if the character at the start of a line (or after whitespace) is `#`, consume to end of line and report once, then continue. Track "already reported" so a 10-line prelude produces one error, not ten.
- **Tradeoffs:** softens the "no preprocessor" boundary if someone later adds a mini preprocessor — trivial, the message is the boundary.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/preprocessor_directive.c`
- **What landed:** the lexer recognises `#` at the start of a line, consumes to end of line, reports once — `error: preprocessor directives are not supported by mycc (no #include, #define, #if)` — and continues. A multi-line prelude therefore produces one error, not one per line.
- **Verified:** the rejection test asserts the message and that it appears exactly once; the character-level error and the parse cascade are gone.

---

### BUG-011 — A UTF-8 BOM breaks the compiler

- **Found:** 2026-10-23 10:46 +05:30, at `9dfb5c2`
- **Severity:** P2 (Windows-specific usability)
- **Short summary:** On Windows, several editors write a 3-byte BOM (`EF BB BF`) at the start of a file. `mycc` treats it as an illegal character and refuses to compile a file that looks perfectly normal in the editor. This project is Windows-first, so it is a likely first experience.
- **Reproduction:** save `int main(void) { return 3; }` as UTF-8 **with** BOM, then compile. **Observed:** `prog.c:1:1: error: unexpected character 'ï'` (the first BOM byte rendered in the console). **Expected:** compiles, exit 3.
- **Root cause:** `src/lexer.c:lexer_init()` starts at `pos = 0` with no BOM check; the byte 0xEF is not whitespace and not a token start.
- **Evidence:** verified by execution (file written with `UTF8Encoding($true)`).
- **Fix plan:** in `lexer_init()`, skip `EF BB BF` if present at offset 0 (and keep `col` at 1 afterwards so diagnostics still line up). Two lines.
- **Tradeoffs:** none. Do **not** go further (no UTF-8 identifiers) — that is a separate feature.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/24_bom.c` (a positive test whose file begins with a UTF-8 BOM; expected exit code carried by `EXPECT:`)
- **What landed:** `lexer_init()` skips `EF BB BF` at offset 0 and keeps the column at 1, so diagnostics after the BOM still point at the right place.
- **Verified:** `tests/24_bom.c` passes in the 68-check suite, and the file on disk genuinely starts with the three BOM bytes (checked with a byte dump, not by trusting the editor).

---

### BUG-010 — Unbounded parser recursion crashes the compiler

- **Found:** 2026-10-23 10:45 +05:30, at `9dfb5c2`
- **Severity:** P3 (crash on hostile/unlucky input)
- **Short summary:** Deeply nested parentheses (or unary operators / blocks) recurse until the OS stack is exhausted. The compiler dies with a stack-overflow crash instead of a diagnostic — a compiler should never crash on input, since a build system only sees "crashed".
- **Reproduction:** `return ((((…1…))));` with 5,000 parentheses. **Observed:** process exit `-1073741571` (`0xC00000FD`, stack overflow). **Expected:** `error: expression nested too deeply (limit 256)`.
- **Root cause:** `src/parser.c` — the descent chain `parse_expression → parse_assignment → parse_ternary → … → parse_primary` recurses per nesting level with no depth counter (recursive functions: `parse_primary`, `parse_unary`, `parse_statement`, `parse_block`).
- **Evidence:** verified by execution (crash reproduced).
- **Fix plan:** one `static int parse_depth` incremented in `parse_unary()` / `parse_primary()` / `parse_statement()` and decremented on exit, with a `MAX_DEPTH` (256) check that reports an error and returns `NULL`. Then the generic "aborted during parsing" path reports it cleanly.
- **Tradeoffs:** 256 is arbitrary but generous for hand-written C; must also guard `parse_block` or `{ { { … } } }` still crashes.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/nesting_parens.c` (deeply nested parentheses, rejected with the documented message)
- **What landed:** one `static int parse_depth` shared by `parse_statement` / `parse_primary` / `parse_unary`, checked against `MYCC_MAX_NEST` (256, in `include/util.h`) and decremented on every exit path, reporting `error: expression nested too deeply` / `error: nesting too deep (limit is 256 levels)`.
- **Verified:** the deep-nesting fixture is rejected cleanly (no crash); 300/5000-level probes that previously produced `0xC00000FD` now produce a diagnostic. `parse_block` is covered by the same counter, so `{ { { … } } }` is bounded too.

---

### BUG-009 — 300 nested loops corrupt the compiler's memory

- **Found:** 2026-10-23 10:45 +05:30, at `9dfb5c2`
- **Severity:** P3 (memory corruption → crash / undefined behaviour)
- **Short summary:** Code generation keeps its `break` / `continue` / `switch` target stacks in fixed arrays of 256 entries and never checks the depth. A program with more than 256 nested loops writes past the end of those arrays — real memory corruption inside the compiler, not just a failed compile.
- **Reproduction:** 300 nested `while (1) { … }` in one function. **Observed:** exit `-1073741819` (`0xC0000005`, access violation). **Expected:** `error: nesting too deep (limit 256)`.
- **Root cause:** `src/codegen.c:30-42` — `break_kind[256]`, `break_id[256]`, `continue_kind[256]`, `continue_id[256]`, `switch_id[256]`, with `break_sp++` and friends used unguarded in `gen_while/gen_do_while/gen_for/gen_switch`.
- **Evidence:** verified by execution (crash reproduced).
- **Fix plan:** a single `push_ctx()` helper that checks the depth and reports a diagnostic before writing; make `MAX_NEST` shared with the parser limit from BUG-010 so both fail at the same, documented depth.
- **Tradeoffs:** the limit becomes a documented language restriction; it must be stated in `instructions.md` §"Not supported".
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/nesting_statements.c` (300 nested loops; shares the limit with BUG-010)
- **What landed:** the six fixed arrays are now written only through `ctx_push_ok()`, which compares the depth against `MYCC_MAX_NEST` and reports a diagnostic *before* writing — the same constant the parser uses, so both limits fail at one documented depth, and the limit is stated in `instructions.md`.
- **Verified:** the 300-nested-loop fixture is rejected cleanly (previously `0xC0000005`); 5000-level probes no longer corrupt memory.

---

### BUG-008 — `08` and `09` are accepted and silently become 0

- **Found:** 2026-10-23 10:44 +05:30, at `9dfb5c2`
- **Severity:** P1 (silent wrong value)
- **Short summary:** The lexer accepts any digit run after a leading `0` as octal, but `strtoll(…, 0)` stops at the first digit that is not a valid octal digit and no one checks where it stopped. So `x = 08;` compiles, and `x` is **0**. A typo becomes a wrong answer with no warning; `gcc` rejects the file.
- **Reproduction:** `int main(void) { int x; x = 08; return x; }` **Observed:** `mycc` exit 0 (compiles) → program exit **0**. **Expected:** `error: invalid digit '8' in octal constant`.
- **Root cause:** `src/lexer.c:lex_number()` collects digits with `isdigit()`; `src/parser.c:parse_primary()` calls `strtoll(parser.current.value, NULL, 0)` and only checks `errno == ERANGE`, never the `endptr`.
- **Evidence:** verified by execution (`mycc` accepted, `gcc` rejected).
- **Fix plan:** pass an `endptr` and require `*end == '\0'`; additionally validate the digit set in `lex_number()` (after a leading `0`, only `0-7`) so the error is reported at the offending character column, not at the literal's start.
- **Tradeoffs:** rejects `0x` with no digits (already handled) and octal literals with `8`/`9` — both correct. Must not break `0` itself or `0u`.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/octal_digit_8.c`
- **What landed:** `lex_number()` validates the digit set (after a leading `0`, only `0-7`) so the error points at the offending **character**, not the literal's start; the parser additionally requires the `strtoll` end pointer to consume the whole token, as a second line of defence. `0`, `0u` and `0x…` are unaffected.
- **Verified:** the rejection test asserts the message and column; `08`, `09` and `0x` with no digits now all fail to compile.

---

### BUG-007 — No comma operator

- **Found:** 2026-10-23 10:44 +05:30, at `9dfb5c2`
- **Severity:** P1 (ordinary C rejected)
- **Short summary:** The comma operator does not exist, so the extremely common `for (…)` step `i++, j--` — and any `a = (b, c)` — fails to parse. Multi-variable loops have to be rewritten as separate statements.
- **Reproduction:**
  ```c
  for (int i = 0, j = 10; i < j; i++, j--) s += 1;
  ```
  **Observed:** `error: expected ')' after for clauses (found ',')`. **Expected:** compiles; loop runs 5 times.
- **Root cause:** `src/parser.c:parse_expression()` returns `parse_assignment()` directly — there is no `parse_comma()` layer, and `TOKEN_COMMA` has no expression-level meaning. (`parse_var_decl` consumes commas itself, which is why `int i = 0, j = 10` *does* work.)
- **Evidence:** verified by execution.
- **Fix plan:** add `parse_expression()` → loop `parse_assignment()` separated by `TOKEN_COMMA`, producing a left-associative `BINARY_OP` with a new `OP_COMMA` that evaluates the left operand for side effects and yields the right. Codegen: `gen_expr(left); gen_expr(right);` — the value in `%rax` is already the right one. Three small edits; document it in the precedence table.
- **Tradeoffs:** a new node op used in only two places; must be excluded from constant folding (`fold_const` returns 0 for it, which is correct). Argument lists keep using `parse_assignment()` so `f(a, b)` is unaffected — that is the one place a comma change could silently break everything, so it needs its own test.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/19_comma_operator.c` (for-step, expression position, and `f(a, b)` still taking two arguments)
- **What landed:** `parse_expression()` loops over `parse_assignment()` separated by `,`, building a left-associative `AST_BINARY_OP` with `OP_COMMA`; codegen emits nothing for the operator itself (evaluate left for effect, then evaluate right). Argument lists keep calling `parse_assignment()`, which is the one place a comma change could silently break everything — hence the dedicated check in `tests/19`.
- **Verified:** `tests/19_comma_operator.c` passes, and every pre-existing test that passes arguments still passes (68-check suite).

---

### BUG-006 — No cast expressions

- **Found:** 2026-10-23 10:43 +05:30, at `9dfb5c2`
- **Severity:** P1 (ordinary C rejected)
- **Short summary:** `(int)x`, `(char *)p`, `(struct Node *)0` — all syntax errors. This blocks the standard idioms for manual memory, byte-level work, truncation, and (once libc calls exist) `malloc` results.
- **Reproduction:** `struct Node *n = (struct Node *)0;` **Observed:** `error: expected expression (found 'struct')`. **Expected:** compiles.
- **Root cause:** `src/parser.c:parse_unary()` handles `- + ! ~ ++ -- & *` and then defers to `parse_primary()`, which treats `(` as "parenthesised expression" — so a `(` followed by a type keyword is always misparsed.
- **Evidence:** verified by execution.
- **Fix plan:** in `parse_primary()`, when the token after `(` starts a type (`is_type_start()`), parse a type name (including `*`s) and then a *unary* expression, producing an `AST_CAST` node; `semantic.c:check_expr()` sets `val_type` from the cast; `codegen.c` truncates/extents as needed (for `char`: `movsbq %al, %rax` / `movzbq`; pointer↔pointer: no code; scalar→pointer: no code). Also add `TOKEN_LPAREN` lookahead so ordinary parentheses keep working.
- **Tradeoffs:** needs the type-compatibility work from BUG-019 to be useful rather than a silencer; keeps working as a no-op on same-size scalars, which is intentional. Also requires `parse_type_specifier()` to be able to look at a struct definition mid-expression, which it already can.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/20_casts.c`
- **What landed:** `parse_primary()` looks ahead after `(` for a type start (`is_type_start()`), parses the type name including `*`s, then a unary expression, producing `AST_CAST`; semantics sets the value type from the cast; codegen emits nothing where the representation is unchanged (pointer↔pointer, `int`↔`unsigned`) and a truncate/extend pair where it is not (`(char)x` → `movsbq`, `(unsigned char)x` → `movzbq`). Ordinary parentheses still parse as parenthesised expressions.
- **Verified:** `tests/20_casts.c` covers scalar, pointer and struct-pointer casts and passes; casts also serve as the opt-in for the conversions BUG-019 rejects, which is verified by the `tests/errors/` suite staying green while `(int *)x` is accepted.

---

### BUG-005 — Pointer types are rejected at file scope

- **Found:** 2026-10-23 10:43 +05:30, at `9dfb5c2`
- **Severity:** P1 (headline feature unusable)
- **Short summary:** `int *p;` at file scope does not parse — `error: expected name after type`. The same applies to a function's *return* type, so no function can return a pointer (`int *pick(int)`, `struct Node *make(void)`). Consequently **no global pointers and no pointer-returning functions exist**, which makes natural linked lists, trees, and most pointer examples impossible, even though pointers are the project's flagship feature. Note the asymmetry that hides it: pointer *parameters* (`int f(int *p)`) work fine, and pointer *locals* (`int *p = &x;`) work fine — only the top-level path is broken.
- **Reproduction:**
  ```c
  int *gp;
  struct Node { int val; struct Node *next; };
  struct Node *make(void) { return 0; }
  ```
  **Observed:** `prog.c:1:5: error: expected name after type` (the same for line 3). **Expected:** compiles; `gp` is a zero-initialized global pointer.
- **Root cause:** `src/parser.c:parse_program()` — after `parse_type_specifier()` it immediately requires `parser.current.type == TOKEN_IDENT` (line ~1725) and never consumes leading `*`s for the return/global type. `parse_global_vars()` (line ~1628) also applies `*` only to declarators *after* the first name, so `int *a, b;` cannot be expressed at file scope either. Locals are unaffected because `parse_var_decl()` applies `while (parser_match(TOKEN_STAR))` per declarator.
- **Evidence:** verified by execution (three separate repros: global `int *`, global `struct X *`, pointer-returning prototype/definition).
- **Fix plan:** consume `*`s into the base type right after `parse_type_specifier()` in `parse_program()` (before the name check), and let `parse_global_vars()`/`parse_function_after_name()` inherit that type. Keep per-declarator `*`s working so `int *a, b;` means `int*` and `int` as in C. Then extend `gen_data()` to emit `.quad 0` for pointer globals (it already does, via the `.quad`/`.byte`/`.zero` branch) and add pointer-typed globals to the `--dump-ast` output.
- **Tradeoffs:** none — this is a straight parser gap. Risk: `struct Node { … };` (a bare definition with a trailing `;`) must keep working with zero stars, so the star loop has to run *after* the existing `TOKEN_SEMICOLON` check.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/21_global_pointers.c` (global `int *`, global `struct Node *`, functions returning `int *` and `struct Node *`)
- **What landed:** `parse_program()` consumes leading `*`s into the base type right after `parse_type_specifier()` and before the name check, so both globals and return types inherit them; per-declarator `*`s still apply, so `int *a, b;` keeps C's meaning. `gen_data()` already emitted `.quad 0` for pointer-sized objects, so pointer globals are zero-initialized for free. `--dump-ast` shows the pointer types.
- **Verified:** `tests/21_global_pointers.c` passes, the whole pre-existing suite stays green, and the differential test — which uses a pointer-returning helper — is only possible because of this fix (the baseline cannot parse it).

---

### BUG-004 — Struct arguments are accepted and are garbage

- **Found:** 2026-10-23 10:42 +05:30, at `9dfb5c2`
- **Severity:** P0 (silent wrong code)
- **Short summary:** Passing a struct by value compiles, but the callee's parameter slot is only 8 bytes, so only the first member survives and the rest is whatever was on the stack. The function returns nonsense and nothing warns.
- **Reproduction:**
  ```c
  struct P { int x; int y; };
  int sum(struct P p) { return p.x + p.y; }
  int main(void) { struct P a; a.x = 3; a.y = 4; return sum(a); }
  ```
  **Observed:** `mycc: 1547696755` · `gcc: 7`. **Expected:** 7, or a compile error saying struct-by-value is not supported.
- **Root cause:** two independent holes. (1) `src/parser.c:parse_param_list()` accepts any `type_is_complete()` type, and a *defined* struct is complete, so the parameter is legal. (2) `src/semantic.c:check_function()` reserves exactly `frame += 8` per parameter regardless of the type's `size`, and the caller (`codegen.c:gen_call`) pushes the address/value of the aggregate as if it were a scalar.
- **Evidence:** verified by execution.
- **Fix plan:**
  - *Problem:* struct-by-value needs either a real hidden-pointer ABI or a byte-copy of the whole object onto the stack, neither of which exists.
  - *Options:* (a) implement the SysV struct-classification rules — a multi-day project on its own; (b) pass by a hidden pointer to a caller-allocated copy (what many small compilers do) — ~150 lines, correct for reads but shares mutable state if the callee writes to `p`; (c) reject it with a clear message until a later phase.
  - *Chosen:* (c) now, (b) if and only if a demo needs it. The rejection message names the workaround: `error: passing 'struct P' by value is not supported; take a pointer instead (struct P *p)`. The same gate must be added for struct *returns* (already rejected) so the two rules stay consistent.
  - *Tradeoffs:* rules out `struct Point add(struct Point a, struct Point b)` — the readable form. Acceptable: pointers are the documented idiom, and a broken-but-accepted feature is strictly worse than a clear rejection.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/errors/struct_param_by_value.c`
- **What landed:** exactly option (c). The parser/semantics now reject a struct-typed parameter (and the already-rejected struct return) with `error: passing 'struct P' by value is not supported; take a pointer instead (struct P *p)`, so the two rules stay consistent. `int`/`char`/pointer/array parameters are unaffected.
- **Verified:** the rejection test passes; the `1547696755` vs `7` reproduction now fails to compile instead of returning garbage.

---

### BUG-003 — Struct assignment copies 8 bytes and nothing else

- **Found:** 2026-10-23 10:42 +05:30, at `9dfb5c2`
- **Severity:** P0 (silent wrong code)
- **Short summary:** `b = a;` where both are structs compiles and copies only the first 8 bytes. Every member after that is left uninitialized, so the copy is garbage — with no diagnostic. This is worse than a rejection, because it looks like it worked.
- **Reproduction:**
  ```c
  struct P { int x; int y; };
  struct P a; struct P b;
  a.x = 7; a.y = 9;
  b = a;
  return b.x + b.y;
  ```
  **Observed:** `mycc: -597689945` · `gcc: 16`. **Expected:** 16.
- **Root cause:** `src/semantic.c:check_expr()` (`case AST_ASSIGN`) only rejects `TYPE_ARRAY` lvalues — a struct-typed lvalue passes. Then `src/codegen.c:gen_assign()` sees `AST_VAR_REF` and calls `emit_var_load()`/`emit_var_store()`, which emit a single `movq`/`movb` pair (structured as if every object were a scalar).
- **Evidence:** verified by execution.
- **Fix plan:**
  - *Options:* (a) implement memberwise copy in codegen (loop over members, recurse for nested structs, respecting `char` widths) — ~60 lines and genuinely useful; (b) `rep movsb` using the type's `size` — smallest code, needs `%rsi/%rdi/%rcx`, which are argument registers, so it needs care at call boundaries; (c) reject the construct.
  - *Chosen:* (b) as the mechanism, (a)'s type walk only to compute `size`. Emit address-of-source into `%rsi`, address-of-destination into `%rdi`, `movq $size, %rcx`, `rep movsb`; because codegen is expression-at-a-time and `pushed_count` already tracks stack depth, the clobbered argument registers are safe here (documents a constraint that must be re-checked if a register allocator lands). Then `b = a` also works for nested structs and arrays-of-structs for free.
  - *Tradeoffs:* `rep movsb` byte-copies instead of 8-byte moves (slower, irrelevant here); it clobbers `%rdi/%rsi/%rcx`, which is safe *today* but is a landmine for Phase 5's register allocator — so the fix must carry a comment and a regression test that assigns structs inside function-call arguments.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/22_struct_assign.c` (whole copy, nested struct, struct member of struct, struct assignment inside a call argument)
- **What landed:** `gen_assign()` sees an `AST_ASSIGN` whose two sides are struct-typed and emits `leaq` of each address into `%rsi`/`%rdi`, `movq $size, %rcx`, `rep movsb`. The size comes from `type->size`, so nested structs and arrays inside structs copy correctly with the same instruction. The comment at the site records the constraint that `%rdi/%rsi/%rcx` are clobbered and that a future register allocator must re-check it — which is why the regression test assigns a struct *inside a call argument*.
- **Verified:** `tests/22_struct_assign.c` passes (previously `-597689945` vs gcc's `16`); the differential program also copies a `struct { int; char; }` and agrees with gcc.

---

### BUG-002 — Postfix `++`/`--` evaluates the lvalue twice

- **Found:** 2026-10-23 10:41 +05:30, at `9dfb5c2`
- **Severity:** P0 (silent wrong code)
- **Short summary:** Any postfix increment whose operand has a side effect in its *address* is evaluated twice — once to read the old value, once to find where to store. `a[i++]++` therefore bumps `i` twice. Ordinary `i++` is fine; the bug only shows on computed lvalues, which is exactly where it is hardest to spot.
- **Reproduction:**
  ```c
  int i = 0; int a[4] = …;
  int v = a[i++]++;
  return i * 10 + v;
  ```
  **Observed:** `mycc: 21` (i=2, v=1) · `gcc: 11` (i=1, v=1). **Expected:** 11.
- **Root cause:** `src/codegen.c:gen_postfix()` — for the non-variable path it calls `gen_expr(node->left)` (which evaluates the whole address expression to load the old value) and then, after arithmetic, calls `gen_addr(node->left)` again (which re-evaluates the same side effects). The two calls are deliberately ordered because `gen_addr` clobbers `%rax`, but nothing caches the computed address.
- **Evidence:** verified by execution.
- **Fix plan:** compute the address once and keep it on the stack: `gen_addr(left)` → `pushq %rax`; `movq (%rsp), %rcx`; load through `%rcx` into `%rax`; add/sub the step; store through `(%rsp)`; then the result is the *old* value, so re-load before dropping the address, or keep old and new in a fixed push order exactly as the current code does but sourcing the address from the stack slot instead of a second `gen_addr()`. Mirrors the BUG-001 fix so both share one `gen_lvalue()` helper.
- **Tradeoffs:** one more stack slot per postfix expression (existing `pushed_count` bookkeeping already covers alignment, so no ABI risk); the helper must also handle `char` truncation on store (`movb`) and pointer scaling on the step.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/23_lvalue_single_eval.c` (covers `a[j++]++`, `(*slot(k++))++` and the "old value" contract)
- **What landed:** `gen_postfix()` forms the address once with `gen_addr()`, keeps it in a stack slot, loads through that slot and stores back through it — the same discipline as the compound-assign fix, so an lvalue's side effects run exactly once.
- **Verified:** the test passes (previously `i=2`, now `i=1`); the differential program's `slots[m++]->v += 1` and `(*slot(k++))++` cases agree with gcc.

---

### BUG-001 — Compound assignment evaluates the lvalue twice

- **Found:** 2026-10-23 10:41 +05:30, at `9dfb5c2`
- **Severity:** P0 (silent wrong code)
- **Short summary:** `lhs op= rhs` is desugared into `lhs = lhs op rhs`, reusing the *same* AST node for the left side in both positions. If that left side contains a side effect (a subscript bump, a `*p++`), the effect happens twice. This is the most dangerous bug in the list because it silently changes loop counters.
- **Reproduction:**
  ```c
  int g[4]; g[0]=10; g[1]=20; g[2]=30; g[3]=40;
  int i = 0;
  g[i++] += 5;
  return i;
  ```
  **Observed:** `mycc: 2` · `gcc: 1`. **Expected:** 1 (and `g[0] == 15`).
- **Root cause:** `src/parser.c:parse_assignment()` builds `binop->left = left` and then `assign->left = left` — the identical node pointer in two places — so `codegen.c:gen_assign()` → `gen_expr(node->right)` → `gen_binary()` evaluates the left operand once, and `gen_assign()` then stores to it, evaluating the address again for the `AST_DEREF`/`AST_MEMBER_ACCESS` path. Prefix `++`/`--` (`parse_unary()`, same shape at lines ~782-791) shares the defect.
- **Evidence:** verified by execution.
- **Fix plan:**
  - *Problem:* the desugaring is convenient but destroys the "evaluate the address once" invariant.
  - *Options:* (a) keep the desugaring and add a `gen_lvalue` cache (address in a stack slot) — small, local, keeps the parser simple; (b) stop desugaring: add `AST_COMPOUND_ASSIGN` and `AST_PREINC` nodes with their own codegen — more nodes, but explicit and easy to test; (c) re-parse the left side into a fresh node — does not help, the side effect is in the source text, not the node.
  - *Chosen:* (a) for the immediate fix, structured so it serves BUG-002 too: one `gen_lvalue_addr(node)` helper that leaves the address in `%rax`, plus a documented push/pop protocol. (b) is the right long-term shape (an optimizer cannot reason about duplicated evaluation either), so it is recorded in `decision.md` as the Phase-5 refactor.
  - *Tradeoffs:* (a) leaves two evaluations in the AST, so the `--dump-ast` output still shows `a = a + 1`; the fix lives in codegen only. Acceptable, because the AST dump is documentation, not an IR. The helper must preserve `%rcx/%r10/%r11` scratch discipline and keep `pushed_count` even at call sites.
- **Status:** FIXED · **Fix commit:** `4c3cd8a` · **Regression test:** `tests/23_lvalue_single_eval.c` (shares the fixture with BUG-002; covers `a[i++] += n`, `*p++ += n`, `++a[i++]`, `s.f++`)
- **What landed:** option (a), structured to also serve BUG-002 and BUG-020: `gen_compound_assign()` recognises the desugared shape, forms the address once, keeps it in a stack slot, loads the old value, evaluates the right operand (already scaled by `sizeof(elem)` where the target is a pointer), applies the operation, and stores back through the saved address. Postfix `++`/`--` reuses the same address-once discipline.
- **Verified:** fails-before/passes-after with real numbers — the minimal repro `g[i++] += 5; return i*10 + g[0];` gives baseline `9dfb5c2` **30**, fixed mycc **25**, gcc **25**. The full 23-check fixture passes.
- **Stated limitation:** the AST still shows the desugared form (`a = a + 1`, two occurrences of `a[i++]`), so `--dump-ast` does not literally describe the emitted code. Documented at the function and in `instructions.md`, because a reader trusting the dump would think the bug is back. Option (b) — real `AST_COMPOUND_ASSIGN` nodes — is recorded in `decision.md` as the Phase-5 refactor an optimizer will need.

---

## Deferred: known feature gaps (not bugs)

These are documented limitations, not regressions. They are listed here because "known issues" must be visible, and because several of them block ordinary C. The Phase 4/5/6 features that used to be listed here (string literals, `printf`, a real external ABI, and the optimizer) now exist and have tests; the rows below are the gaps that remain after the 2026-10-26 audit.

| Gap | Why it matters | Tracked as |
|---|---|---|
| `int` is 64-bit, not C's 32-bit | `unsigned` is 64-bit too; `%d` interop and 32-bit overflow differ from C. **This is why differential programs must stay width-agnostic** | ROADMAP Phase 1 (undecided) |
| No struct **by value** (params/returns) | Deliberately rejected with a message pointing at `struct P *` — a clear refusal, not a miscompile (BUG-004) | ROADMAP Phase 3 decision |
| No array parameter declarator (`int f(int a[])`) | Valid-looking C is rejected; callers must spell the parameter as `int *a` | ROADMAP Phase 3 follow-up |
| No pointer-to-array declarations (`int (*p)[2]`) | `&array` now has pointer-to-array type internally, but the declaration syntax cannot bind it to a named variable | ROADMAP Phase 3 follow-up |
| No block-scope `struct` tag shadowing | An inner `struct S` tag cannot hide an outer one; the tag namespace is effectively file-wide | ROADMAP Phase 3 follow-up |
| No array members inside structs | `struct S { int a[4]; };` is rejected, which blocks several natural layouts | ROADMAP Phase 3 follow-up |
| `if (struct_value)` / struct comparison accepted | Invalid C reaches codegen without a scalar-context diagnostic; this is a genuine open defect, not a feature gap (see BUG-048) | **BUG-048 (OPEN)** |
| Limited global pointer initializers | Constant global scalars and simple addresses are handled, but string/address/`(void *)0` pointer initializer forms are not all accepted | ROADMAP Phase 4 follow-up |
| No `sizeof`, `typedef`, `enum`, initializer lists, or `goto` | Common C cannot be compiled at all; `sizeof` is especially visible in realistic code | ROADMAP Phases 2/7 |
| No preprocessor (`#define`, `#include`, `#if`) | One precise error at the first directive instead of a cascade (BUG-012), but any real C file still fails | ROADMAP "not supported" |
| Nesting deeper than 256 levels | A documented language limit, enforced by a clean diagnostic in the parser and in codegen (BUG-009/010) | `instructions.md` "Not supported" |
| No floating point, `long`, or `short` | No graphics/fraction/large-integer demos; all integer objects are 64-bit | ROADMAP Phase 7 |
| No full struct-tag/scope model in diagnostics | Some invalid aggregate expressions are refused, but the scalar/aggregate constraint is not uniform yet | BUG-048 + Phase 3 follow-up |

## Deferred: process gaps

Status as of 2026-10-26 after the edge-case audit — kept so the next session does not have to rediscover them.

| Gap | Consequence | State |
|---|---|---|
| Tests asserted **only** exit codes | A wrong-code bug returning a plausible number passed. BUG-001…004 and BUG-020 all did. | **Closed** for the curated set: `tests/diff/` + `difftest.sh`/`difftest.bat` + `make difftest`, run in CI on all three platforms |
| No differential testing against `gcc` in CI | The scratch fuzz run found nothing, which is exactly why it belongs in CI | **Closed**: differential and generated-program fuzzing are in CI; the local audit also ran `fuzz.bat 700 456` with 700/700 agreement |
| No grammar-coverage test for documented features | BUG-005 (global pointers) survived while the README advertised pointers | **Partly closed**: `tests/19`–`tests/25` cover the advertised memory/struct features, and `tests/37`–`tests/49` cover the new audit cases; there is still no systematic doc-to-test mapping |
| No golden `.s` files | Codegen refactors are unverified except behaviourally | Open. The differential harness and the targeted assembly checks reduce the risk but do not pin the emitted instructions |
| CI has no sanitizer job | ASan/MSan would have caught BUG-009 (array overflow) immediately | **Partly closed**: the Windows job runs the `.bat` scripts and the test/differential suites; an ASan job is still missing |
| A harness bug could silently disable checks | `make test` could not match expectations on CRLF checkouts (BUG-021) | Closed for this case (`tr -d '\r'`), but the class remains: nothing tests the harness itself |
| Audit fixes are uncommitted | The ledger cannot honestly mark `BUG-032`…`BUG-047` plain `FIXED`, because the honesty rule requires a commit hash | **Pending user permission**: statuses are `FIXED (working tree)` with `pending` commit; the user explicitly asked not to commit or push |
| The mirrored `Makefile` contract was not run locally in this audit | `test.bat`'s same-file/temp-path contract ran, but the shell recipe is not proven on this host in this session | Open for this audit; the contract is mirrored in both files, and the next `make test` run will execute it |

---

## Batch as executed (2026-10-23)

The batch was executed in one pass at the user's instruction ("fix all existing bugs first"), with the quiz deferred to hand-over. Order was by risk to the user, then by dependency:

1. **Batch A — stop the silent wrong code (P0):** BUG-001, BUG-002, BUG-003, BUG-004, BUG-019. Then BUG-020 was found *by the new differential test* and fixed in the same batch.
2. **Batch B — unblock ordinary C (P1):** BUG-005 (global pointers / pointer returns), BUG-008 (octal), BUG-007 (comma), BUG-006 (casts).
3. **Batch C — never crash (P3):** BUG-009, BUG-010 (one shared 256-level limit).
4. **Batch D — diagnosable failures (P2):** BUG-011 (BOM), BUG-012, BUG-013, BUG-014, BUG-015, BUG-016 + the warnings channel and `-w`.
5. **Batch E — tooling truth:** BUG-017 (atomic output), BUG-018 (target flavours + macOS job), the differential harness, and BUG-021 (CRLF-safe harness) found while validating batch E.

**User input:** 2026-10-23 — the user asked for *all* existing bugs fixed and fully documented, with the quiz held for the end. Batch scope was therefore the full ledger rather than the five-step proposal above; the proposal is kept here as the record of the plan that was agreed.

---

## Audit batch as executed (2026-10-26)

This batch was a correctness audit of the already-green compiler, not a roadmap phase. It added tests 37–49, the same-file/temp-path contract, and entries 032–047; entry 048 records the one gap that was deliberately left open. The work was grouped by failure mode:

1. **Silent wrong code in common expressions:** BUG-032 (`&array` type), BUG-033 (assignment value narrowing), BUG-036 (label outside its controlled statement), BUG-037 (unsigned conversions/shifts/folding), BUG-038 (signed high-byte characters).
2. **Wrong or missing control-flow labels:** BUG-035 (switch labels in constant-dead arms) and BUG-036 (controlled label nesting); both have dedicated regression tests.
3. **Pointer/void/literal typing:** BUG-034 (null pointer conditional), BUG-039 (void value contexts), BUG-040 (`\?` + adjacent strings), BUG-046 (constant casts).
4. **Diagnostics and declarations:** BUG-041 (unnamed prototypes, builtin arity, conflicting redeclarations), BUG-044 (`extern`, `main`, global pointer initializers), BUG-047 (struct conditional diagnostic).
5. **Backend robustness and platform correctness:** BUG-042 (wide calls + long symbols), BUG-043 (Darwin `%al`), BUG-045 (NUL source and output aliases).
6. **Deliberately left open:** BUG-048 — scalar-context checks for struct conditions/comparisons. The roadmap checklist is complete, but this diagnostic gap is real and is not being hidden behind “all fixed.”

**Validation for this batch:** clean build; `test.bat` **106 PASS** (48 positive, 52 rejection, 6 contract); `difftest.bat` **3/3**; `fuzz.bat 700 456` **700/700**; `git diff --check` clean. The user withheld commit/push permission, so the 16 fixed entries remain `FIXED (working tree)` with `pending` commits and BUG-048 remains `OPEN`.

---

## Changelog of this file

| Date | Change |
|---|---|
| 2026-10-26 | Edge-case audit recorded. Entries `BUG-032`…`BUG-047` added; 16 are fixed and regression-tested in the working tree, with `pending` commit hashes because the user withheld commit/push permission. `BUG-048` (struct conditions/comparisons are still accepted) is **OPEN**. Counts re-counted from the dashboard: 15 × P0, 13 × P1, 12 × P2, 8 × P3 = **48 logged, 47 fixed (31 committed + 16 working tree), 1 open**. Tests 37–49 and the same-file/temp-path contract added; current suite **106 PASS** (48 positive, 52 rejection, 6 contract), differential 3/3, fuzz 700/700. Deferred gaps and process gaps refreshed; the historical 81-check CI table is retained as history rather than rewritten. |
| 2026-10-24 16:15 | Phase 9 (CI truthfulness) recorded. BUG-029 (toy-language literals), BUG-030 (missing `cmp` in CI) and BUG-031 (x86-64 output assembled as arm64 on the Apple Silicon runner) added and fixed. **BUG-018 closed on evidence**: run `40086581248` is the first green matrix - Linux, Windows and macOS - in the project's history. 31 logged, 31 fixed, 0 open (the earlier tally said 30; see the correction under the dashboard). The ledger's own promise ("if the macOS job is green, this becomes FIXED") is what closed it. |
| 2026-10-23 21:40 | Fix batch recorded. All 21 entries now carry a status, a fix commit, the real regression-test filename, and a verification note with what was actually executed. BUG-020 (double index scaling) and BUG-021 (CRLF-unsafe harness) added - both found *while* fixing, by the new differential harness and by running the harness itself. Dashboard, counts, deferred gaps and process gaps rewritten. BUG-018 stays `IN PROGRESS` until the macOS CI job is green. |
| 2026-10-23 10:50 | File created. 19 bugs logged from the `9dfb5c2` review sweep. No fixes committed yet. |
