# mycc Roadmap

Planned work, ordered in phases by dependency and payoff. Each phase ends with
the full test suite (`make test` / `test.bat`) passing. Legend:

- **[ ]** not started · **[~]** in progress · **[x]** done

> **Status:** Phases 0, 1 (except multi-error recovery), 2, 3, 4, 5 and 8 are
> complete. The `int`-width question was decided on 2026-10-24 - it stays 64-bit,
> as the compiler's own ABI, with the consequences written down (`decision.md`).
> Phase 5 (optimization) landed in four pieces - constant folding, dead-branch
> elimination, dead local stores and expression temporaries in a register - with
> copy propagation measured and deliberately rejected and the liveness allocator
> staged behind the Phase 7 IR. Phase 6 has caret diagnostics, the warning
> channel, the differential harness, the opt/no-opt equivalence check and, as of
> 2026-10-24, the **random program generator** (`tools/fuzz.c`); `--interpret`
> and therefore the third oracle are still open. Phase 7 has `--version`,
> `--sign`, three-target output and published CI artifacts; floating point, the
> SSA IR and a 32-bit-`int` mode are open. The suite now has 35 positive tests,
> 41 error/diagnostic tests and 5 contract assertions (**81 checks**), and every
> job also runs a differential test that requires mycc and gcc to return the same
> exit code (`make difftest` / `difftest.bat`) plus a fuzz campaign
> (`make fuzz` / `fuzz.bat`). Every positive test is additionally built with
> `--no-opt` and required to exit the same way and print the same bytes, so the
> optimizer is checked against behaviour on every program.
>
> **CI (2026-10-24):** the matrix is green on all three platforms for the first
> time - run `40086581248`, Linux 44s, macOS 1m4s, Windows 2m49s. The claim in
> the README had been false for as long as the job existed; two defects had to
> be found and fixed (BUG-030, missing `cmp`; BUG-031, x86-64 assembly read as
> arm64 on the Apple Silicon runner). See `report.md`.
>
> **Correctness sweep (2026-10-23):** a full source review plus ~30 adversarial
> probes against gcc turned up 21 defects, all of which are now fixed. They are
> logged, with reproductions and fix commits, in the local bug ledger
> `report.md`. The high-severity class — silently wrong code — is
> gone: `a[i++] += 5` and `a[i++]++` evaluate their lvalue once, `b = a` copies
> a whole struct, struct-by-value is rejected instead of miscompiled,
> `a[i] += 1` no longer double-scales the index, and assignments/calls are
> type-checked. Phase 4 added two more (BUG-022, BUG-023), the toy front end and
> the build path added three (BUG-027, BUG-028, BUG-029), and CI truthfulness
> added two more (BUG-030, BUG-031). The ledger's own tally was off by one until
> 2026-10-24 16:20 (the table had 28 rows while the count said 27), so the
> authoritative count is **31 logged, 31 fixed, 0 open** — 11 × P0, 6 × P1,
> 6 × P2, 8 × P3.
>
> **Tooling added:** a macOS CI job, a differential test against the host
> compiler, warnings with a `-w` switch, `--target=darwin`, `--no-opt`,
> crash-proof nesting limits, published build artifacts, and a random-program
> fuzzer that checks mycc against gcc on programs nobody wrote.

---

## Phase 0 — Housekeeping & tooling

- [x] Git repo + `.gitignore` (build artifacts, object files, editor cruft)
- [x] Accurate `README.md` + this `ROADMAP.md`
- [x] CI (GitHub Actions): build + test matrix on Linux (gcc), Windows
      (MinGW-w64 / MSYS2) and macOS (Apple clang), gated on every push; the
      Linux job builds with `-Werror`
- [x] Differential test against the host compiler (`difftest.sh` /
      `difftest.bat`, also wired into CI) — it closes the gap that let four
      silently-wrong-code bugs pass a green 48-test suite
- [x] `make clean` also removes the stale `src/*.o` objects; objects now build
      into a `build/` dir so the source tree stays clean
- [x] Decide and document the official calling-convention posture
      (see Phase 4) — **done**: option B, the real platform ABI on every
      target. The reasoning and the tradeoffs are in `decision.md`
      (2026-10-23 14:39)

**Acceptance:** fresh clone → `build.bat`/`make` → `test.bat`/`make test` green
on all three platforms automatically in CI, with the differential test agreeing.

---

## Phase 1 — C-fidelity & correctness

Make the language match real C semantics more closely without adding syntax.

- [x] Fix `for (int i = ...)`: the loop variable currently leaks into the
      enclosing scope; in C it is loop-local (two consecutive `for (int i...)`
      loops must not conflict)
- [x] Semantic check that a `main` function exists, giving a clean compiler
      diagnostic instead of a linker error
- [x] Abort cleanly on an unterminated `/* comment` instead of cascading bogus
      token errors
- [x] Decide the width of `int`: it stays **64-bit**, recorded as the compiler's
      own ABI rather than as an oversight (`decision.md`, 2026-10-24 16:15).
      Consequences are now committed, not implicit: no 32-bit wrap-around,
      `sizeof(int) == 8`, and the fuzz generator must avoid overflow cases or a
      mismatch would be C undefined behaviour rather than a compiler bug.
      Making `int` 32-bit (`movl`/`imull` + sign extension after every op) is
      recorded in Phase 7 as the alternative that was not taken.
- [x] Remove dead code: `scope_depth()`, `Symbol.is_param`,
      `SYMBOL_TYPE_VOID`, `Scope.child` (`xrealloc` was removed with them and
      later brought back as a genuinely-used allocator in this branch)
- [x] Type-check assignments and calls (`struct → int`, `int → pointer`, and a
      struct passed as `int` are errors, with casts as the explicit opt-in) —
      `report.md` BUG-019
- [x] Evaluate an lvalue with a side effect exactly once (`a[i++] += 5`,
      `*p++ = 1`, `a[i++]++`) — BUG-001, BUG-002, BUG-020
- [x] Copy a whole struct on assignment (`b = a`, nested structs included) —
      BUG-003; and reject struct **by value** parameters/returns instead of
      silently miscompiling them — BUG-004
- [x] Reject malformed literals (`08`, `09`, `0x` with no digits) and skip a
      UTF-8 BOM — BUG-008, BUG-011
- [x] Never crash on hostile input: nesting depth is capped at 256 levels in
      the parser and in codegen's break/continue/switch stacks — BUG-009,
      BUG-010
- [x] Precise single-error diagnostics for preprocessor directives, string
      literals and unterminated literals, instead of a character-level error
      plus a cascade — BUG-012, BUG-013
- [x] Reject `int main(int argc, char **argv)` (the arguments are garbage on
      Windows until the Phase-4 ABI split) — BUG-014
- [x] Report a declared-but-never-defined function at compile time, and a
      non-`void` function that can fall off its end as a **warning** — BUG-015,
      BUG-016
- [x] Leave no output file behind when compilation fails (temp file + rename)
      — BUG-017
- [ ] Parse errors recover so more than one error is reported per run

**Acceptance:** the existing suite passes; new tests cover `for`-scope
shadowing, missing `main`, unterminated comment, assignment compatibility,
single evaluation of lvalues, whole-struct copy, struct-by-value rejection,
malformed literals, the BOM, the nesting limits, the diagnostics above, and
multi-error recovery.

---

## Phase 2 — Language surface (control flow + data)

- [x] `break` and `continue` (with target-label bookkeeping in `codegen.c`)
- [x] `do-while` loops
- [x] `switch` / `case` / `default`
- [x] Integer literal formats: hex `0x...`, octal `0...`, `U`/`L` suffixes
- [x] `char` type (bytes stored/loaded with sign extension), `void` functions
- [x] `else if` spelling naturally (already works via nested `else` + `if`)
- [x] Conditional `?:` operator, bitwise `& | ^ ~`, shifts `<< >>`, and the
      matching compound assignments
- [x] Comma operator (`for (…; …; i++, j--)`, `a = (b, c)`) — precedence level
      added between assignment and the rest; argument lists unaffected
- [x] Cast expressions (`(int)x`, `(char *)p`, `(struct Node *)0`), including
      the truncate/extend codegen for narrowing to `char`/`unsigned char`
- [x] Globals with constant-folded initializers
- [x] Global **pointer** variables and pointer **return** types
      (`int *gp;`, `struct Node *make(void)`) — BUG-005

**Acceptance:** `tests/06_control_flow.c`, `tests/07_bitwise.c`,
`tests/08_literals.c`, `tests/09_types_globals.c`, `tests/10_ternary.c`, and
`tests/11_scope.c` (all exit-code self-checking) plus `tests/errors/*.c`
diagnostics; all green.

---

## Phase 3 — Memory & types (the big teaching milestone)

- [x] Pointers: `&`, `*`, pointer arithmetic, `NULL`, null checks
      (this unlocks everything else and is worth doing thoroughly)
- [x] Arrays: declaration-time size checks (positive, constant bounds),
      address-to-first-element decay in expressions, and multi-dimensional
      (`int m[3][4]`, `int t[2][3][4]`) built by nesting dimensions inside
      out so the row stride is right
- [x] `struct` definitions, member access `.`, plus `->`, nested structs,
      arrays of structs, and self-referential members
- [x] Whole-struct assignment (`b = a`) via `rep movsb`, sized from the type —
      BUG-003. Struct-by-value parameters and returns stay **rejected** (a
      clear message points at the pointer form) rather than half-implemented —
      BUG-004. Revisit only if a demo needs `struct Point add(struct Point a,
      struct Point b)`; the honest cheap version is a hidden pointer to a
      caller-allocated copy, not the SysV classification rules.
- [x] Function prototypes / forward declarations (formal `param decls`)
- [x] `unsigned` variants: `unsigned`, `unsigned int`, `unsigned char` (and
      `signed` as the explicit default) — signedness now selects
      `divq`/`shrq`/unsigned `setcc` and a zero-extending `movzbq` byte load.
      Implemented independently of the `int`-width decision in Phase 1: here
      `unsigned int` is 64-bit, matching the existing 64-bit `int`.

**Acceptance:** `tests/12_pointers.c`, `tests/13_arrays.c`,
`tests/14_structs.c`, `tests/17_multidim_arrays.c`, `tests/18_unsigned.c`,
`tests/21_global_pointers.c`, `tests/22_struct_assign.c`, and
`tests/23_lvalue_single_eval.c`; a real `main` that reads/writes an array and
sums it via pointers. This phase proves the compiler can host real algorithms.

---

## Phase 4 - I/O & runtime

- [x] Built-in `print_int` / `print_char` / `print_str` / `print_endl` wired to
      the CRT: semantic analysis rewrites them into `printf` / `putchar` calls,
      so they need no declaration and no `#include`, and `--dump-ast` shows the
      call that will actually run
- [x] String literals: the lexer decodes them (full C escape set, including
      octal - BUG-022) and codegen emits them into `.rodata` (Mach-O:
      `__TEXT,__cstring`), deduplicated. `"..."` is a real `char *`
- [x] `extern` declarations at file scope for library functions, exempt from
      the declared-but-never-defined check (BUG-015) and checked with variadic
      arity (`argc >= param_count`), because `printf` takes more arguments than
      it declares. `printf`, `puts` and `putchar` are pre-registered, so the
      common cases need no declaration at all
- [x] A separate runtime library was not needed: every program is still a
      single self-contained `.s`, and the four helpers cost four call sites
      rather than a linked object
- [x] Enforce the interim posture: `main` must be declared `int main(void)`.
      `int main(int argc, char **argv)` still gets a clean compile-time
      rejection rather than a garbage `argv` on Windows - BUG-014
- [x] Define the official calling convention now that external calls exist:
      **option B** - the real platform ABI on every target. SysV passes 6
      arguments in `rdi/rsi/rdx/rcx/r8/r9` and needs `%al` = number of vector
      registers for variadic calls; Windows x64 passes 4 in `rcx/rdx/r8/r9`
      plus a 32-byte shadow space. Arguments past the register slots go on the
      stack and are read above the callee's frame, so the old "more than 6
      parameters" limit is gone. External calls go through `@PLT` on ELF, and
      internal calls stay direct

**Acceptance:** met. `tests/26_hello_world.c` prints `Hello, world!` and exits
0; `tests/28_extern_printf.c` calls `printf` through `extern` on Windows and
Linux; `tests/diff/02_stack_args.c` cross-checks stack arguments against gcc.

---

## Phase 5 — Optimization (beyond the "no optimizer" claim)

**Complete.** Four optimizations, one switch (`--no-opt`) that turns all of them
off, and a test suite that builds every program both ways and requires the same
behaviour.

- [x] Constant folding (`2 + 3` -> `5`, comparisons of constants, folding into
      `movq $k`) - **folds in semantic analysis**, after the node's type is
      known, rewriting the node *in place* so parents see the folded form
      without re-parenting. `--dump-ast` shows `14`, not `(2 + 3 * 4)`.
      Conservative by design: only integer results, never unsigned operands,
      never a division by zero, never the comma operator
- [x] Dead code / constant-condition elimination (`if (0) ...`) - **in
      codegen**, not semantic: only the live arm of `if`/`? :` is emitted,
      `while (0)` emits nothing, `for (init; 0; step)` emits only the init,
      and `do { } while (0)` emits the body with its labels but no back-jump.
      The dead arm is still parsed and type-checked, so deleting output never
      deletes a diagnostic
- [x] Dead local stores - **`src/opt.c`**, a new pass between semantic analysis
      and code generation (so `--dump-ast` still shows the code that runs). It
      classifies every use of every local as read / written / address-taken and
      deletes a store when the variable is read nowhere, its address never
      escapes, it is a scalar, and the stored expression is pure. Locals are
      keyed by `stack_offset`, not by name, so shadowing cannot let one
      variable's reads pay for another's stores. The address-taken rule is the
      load-bearing one: `&b` is *not* a write, and treating it as one made
      `int *p = &b; ... *p` read a deleted store (caught by `tests/12`, and
      `tests/35` exists to keep it caught)
- [x] Expression temporaries in a register - **in `codegen.c`**, the "register
      allocator" item. The stack machine used to park the left operand with
      `pushq %rax` and pick it back up with `popq %rcx`; it now parks it in
      `%r10` and combines straight out of it (`addq %r10, %rax`), spilling to
      the machine stack only when an expression needs a second temporary. That
      is one instruction per binary operation, and it lands on the common case
      (`a + b`; `a + 1` already used the immediate path)
- [x] ~~Copy propagation~~ - **measured, then deliberately not implemented.**
      `t = x; ... use t ...` and `... use x ...` compile to the same frame load
      on this back end, so the rewrite buys zero instructions; it pays only when
      a value can stay in a register *across statements*, which is the liveness
      allocator below. Implementing it would have been motion without a
      measurement
- [ ] Liveness-based global allocator (values living in registers across
      statements) - **staged with the Phase 7 IR**, see below

**Acceptance - met.** `--no-opt` produces byte-identical assembly to the
pre-Phase-5 compiler (checked with SHA-256 over all 35 test programs), and
`test.bat` / `make test` now compile *every* test a second time with `--no-opt`
and require the same exit status and the same bytes on stdout, so the optimizer
is verified not to change behaviour on every program, not just on the ones with
a dedicated test. `tests/diff/03_folding.c` still cross-checks the folded
semantics against gcc.

**The measurements that shaped it** (assembly line counts, Windows target):

| build | lines | `pushq`/`popq` |
|---|---|---|
| the 33 programs before this phase, optimized | 6,852 | 793 |
| ... with the dead-store pass only | 6,848 | 793 |
| ... with two *callee-saved* temp registers (rejected) | 7,497 | 236 |
| ... shipped: one *caller-saved* temp register (`%r10`) | 6,734 | 338 |
| whole suite (35 programs), `--no-opt` | 11,055 | 2,378 |
| whole suite (35 programs), optimized | 7,315 | 433 |

The two-register row is the interesting one: it removed 70% of the push/pops and
still *added 9.4% to the output*, because two callee-saved registers cost a
save/restore pair in every function and at every return, and this suite is full
of small functions. One caller-saved register costs nothing to use, so it keeps
the per-operation win and drops the fixed cost. That reversal is why the pool is
one register and not five: the expensive part of a register on this back end is
not using it, it is *owning* it.

**Order matters for what is left.** The remaining item - values living in
registers across statements - is a *liveness* question, and it needs a
representation with basic blocks and explicit definitions and uses, which is the
`SSA-lite` item in Phase 7. Doing it on the AST means re-deriving liveness from
a tree that cannot express "is this variable live across the call?", and an
allocator that is subtly wrong emits *silent wrong code* rather than a bad
diagnostic. So it stays behind the IR, behind `--no-opt` parity, and behind its
own staging - which is the order this phase ended up following for the parts
that could be done safely.

---

## Phase 6 — Diagnostics & developer tooling

- [x] Source-snippet printing in errors (`prog.c:3:9: error: ...` with the
      offending line and a caret) — for errors *and* warnings. The message line
      is unchanged, so every `EXPECT-ERROR:` marker still matches; the snippet
      is additional. Long lines are clipped (a wrapped line makes the caret
      lie), tabs pass through on both rows, and a column past end-of-line puts
      the caret at the end
- [~] Warning flags: the channel exists — `report_warning()` writes
      `<file>:<line>:<col>: warning: <msg>`, never changes the exit status, and
      `-w` silences it. One warning is emitted so far (control reaches the end
      of a non-`void` function, BUG-016). Still open: unused variable, unused
      parameter, constant condition, shadowed declaration. *Deliberately
      deferred:* a constant-condition warning would fire on `tests/31`–`33`,
      which use `if (0)` on purpose, so it needs a suppression marker in the
      test harness first — otherwise the diagnostic punishes the tests that
      prove the optimizer
- [x] Random program generator: **`tools/fuzz.c`** generates programs nobody
      wrote (fixed skeleton - helper, two loops, an if/else - with random
      expressions, literals, comparisons and bounds), and `make fuzz` /
      `fuzz.bat` point the existing differential harness at the corpus:
      `SRCDIR=build/fuzz QUIET=1`. Deterministic in (seed, index), so a failing
      case is reproducible by number. Three invariants keep a mismatch
      meaningful: every program terminates, no undefined behaviour, no
      expression overflows a 32-bit `int` (mycc's is 64-bit).
      **Campaign: 1000/1000 agree with gcc** (`make fuzz FUZZ_N=1000`), plus
      200 on the Windows CI job and 100 on macOS per push.
- [ ] `--interpret`: a tree-walking interpreter that runs the AST
      (mirrors codegen semantics; ideal for teaching and for differential
      fuzzing against the compiled output)
- [~] Third oracle: `difftest` and the fuzzer both compare mycc's *compiled*
      output with gcc. The plan's third opinion - mycc's own interpreter - needs
      `--interpret` first, so the 10,000-program run below is still open.

**Acceptance:** a fuzz run of N=10,000 generated programs finds zero
mismatches. The generator half is done and green at N=1000 per push; the
10,000-program **interpreter-vs-codegen** run needs `--interpret`, so this
acceptance criterion is still open by its own wording, and is not claimed.

---

## Phase 7 — Stretch goals

- [ ] Floating point: `double`, SSE instructions, comparisons, conversions
- ~~Strings as a real type (`char *s = "hi"`, length, escape sequences)~~ —
  **shipped early, in Phase 4**: string literals are decoded bytes, pooled in
  `.rodata` and deduplicated, with the full C escape set (BUG-022) and NUL
  bytes preserved (BUG-023)
- ~~CLang-style `-S`/`-o` flag parity and `--vectorize`?~~ — **closed, not
  planned.** `-o` already exists and mycc only ever emits assembly, which *is*
  `-S`; `--vectorize` is out of scope for a stack machine and always was.
- [ ] Custom IR (SSA-lite) as the middle representation, enabling aggressive
      optimization and a backend split (teaching showcase)
- [x] Release packaging: `w64devkit`/MSYS2 instructions exist in
      `instructions.md`, and `--version` reports what the compiler is and which
      targets it has. `--sign` embeds a provenance string in each program's
      `.rodata`, so a built binary can name its compiler
      (`strings prog.exe | find "generated by mycc"`). CI publishes the three
      binaries as build artifacts (`mycc-linux-x86_64`,
      `mycc-windows-x86_64`, `mycc-macos-x86_64`). Still open: a tagged release
      with notes rather than per-run artifacts
- [ ] A 32-bit-`int` mode, the alternative to the 2026-10-24 width decision:
      `movl`/`imull` throughout, sign extension after every 64-bit operation,
      struct layout and `sizeof` re-derived from it, `printf("%d")` becoming a
      real varargs promotion. This is the change that would make `mycc`'s
      integer semantics match the platform C ABI exactly

---

## Phase 8 — A second language (`--lang=myc`)

- [x] **`myc` front end producing the existing AST.** A small language with its
      own grammar (`fn`/`end`, `let`, `say`, `if`/`else`, `while`, `return`,
      `and`/`or`/`not`, no semicolons, no braces) that reuses the C lexer and
      emits the *same* `ASTNode`s the C parser emits. Nothing is translated to
      C: `src/myc.c` is a parser, and `semantic.c` + `codegen.c` never learn
      which language they are compiling
- [x] **The claim is executed, not asserted.** `demo\4_pair.myc` and
      `demo\4_pair.c` are the same program in the two languages; `test.bat`
      (`:myc_contract`) and `make test` compile both and require
      **byte-identical** assembly with `fc /b` / `cmp -s`, then link and run the
      result to prove the file is not merely equal but correct
- [x] **`if`/`else` is exercised in both directions.** The pair above had no
      `else` in either file, which hid a parser bug: `parse_block_body()` stopped
      a block on `word("else")`, and `word()` only matches identifiers, while
      `else` is a C keyword token - so the block swallowed it and the parser
      reported `expected a statement (found 'else')` (BUG-027). `tests/myc/else.myc`
      and its C twin now take the else branch at runtime, print its text, and are
      compared byte for byte
- [x] Demo scene 5 shows the `FC: no differences encountered` line on screen

**Acceptance:** met — a second front end whose output is provably the same
back end's output, checked on every test run. Deliberately **not** in scope:
arrays, pointers, structs and compound assignment in `myc`. Those belong to the
C front end; duplicating them in a teaching language would create a second place
for each rule to be wrong without teaching anything new.

---

## How work flows

1. Each feature adds a self-checking test first (`tests/NN_feature.c` with an
   `EXPECT:` marker), which fails before the change and passes after. Rejection
   behaviour goes in `tests/errors/CODE.c` with an `EXPECT-ERROR:` marker
   naming the diagnostic.
2. **Bug fixes go through `report.md`** (local-only): the defect is logged with
   its severity, reproduction, root cause and fix plan *before* the fix, and the
   entry is re-stamped with the fix commit hash and the regression test after
   it. Commit messages carry the ID: `fix(BUG-014): require int main(void)`.
   Nothing is marked `FIXED` without a test that fails before and passes after.
3. Cross-platform check: Linux (`make test`), Windows (`test.bat`) and macOS CI
   all green — plus `make difftest`, which is the only check that can tell
   "plausible" from "correct" (it compares mycc's exit codes against gcc's).
4. Tour narration (`tour.bat` / `tour2.bat`) is updated only when it stops
   matching reality.
5. User-facing docs (`README.md`, `instructions.md`, this file) are updated in
   the same change as the behaviour they describe; the local engineering docs
   (`decision.md`, `Flow.md`, `learn.md`, `report.md`) are updated the same day.
