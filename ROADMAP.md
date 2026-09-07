# mycc Roadmap

Planned work, ordered in phases by dependency and payoff. Each phase ends with
the full test suite (`make test` / `test.bat`) passing. Legend:

- **[ ]** not started · **[~]** in progress · **[x]** done

---

## Phase 0 — Housekeeping & tooling

- [x] Git repo + `.gitignore` (build artifacts, object files, editor cruft)
- [x] Accurate `README.md` + this `ROADMAP.md`
- [ ] CI (GitHub Actions): build + test matrix on Linux (gcc) and Windows
      (MinGW-w64 / MSYS2), gated on every push
- [ ] `make clean` also removes the stale `src/*.o` objects; move objects into
      a `build/` dir so the source tree stays clean
- [ ] Decide and document the official calling-convention posture
      (see Phase 4)

**Acceptance:** fresh clone → `build.bat`/`make` → `test.bat`/`make test` green
on both platforms automatically in CI.

---

## Phase 1 — C-fidelity & correctness

Make the language match real C semantics more closely without adding syntax.

- [ ] Fix `for (int i = ...)`: the loop variable currently leaks into the
      enclosing scope; in C it is loop-local (two consecutive `for (int i...)`
      loops must not conflict)
- [ ] Semantic check that a `main` function exists, giving a clean compiler
      diagnostic instead of a linker error
- [ ] Abort cleanly on an unterminated `/* comment` instead of cascading bogus
      token errors
- [ ] Decide the width of `int`: today it is 64-bit (`movq`/`long long`),
      unlike C's 32-bit `int`. Either shrink `int` to 32-bit (`movl`/`imull`,
      sign extension after every 64-bit op) or add a distinct `long` type and
      document the choice
- [ ] Remove dead code: `scope_depth()`, `Symbol.is_param`,
      `SYMBOL_TYPE_VOID`, `Scope.child`, `xrealloc`
- [ ] Parse errors recover so more than one error is reported per run

**Acceptance:** existing 5 tests pass; new tests cover `for`-scope shadowing,
missing `main`, unterminated comment, and multi-error recovery.

---

## Phase 2 — Language surface (control flow + data)

- [ ] `break` and `continue` (with target-label bookkeeping in `codegen.c`)
- [ ] `do-while` loops
- [ ] `switch` / `case` / `default`
- [ ] Integer literal formats: hex `0x...`, octal `0...`, `U`/`L` suffixes
- [ ] `char` type (bytes stored/loaded with sign extension), `void` functions
- [ ] `else if` spelling naturally (already works via nested `else` + `if`)

**Acceptance:** new self-checking test file `tests/06_control.c` and
`tests/07_types.c`; all exit-code tests green.

---

## Phase 3 — Memory & types (the big teaching milestone)

- [ ] Pointers: `&`, `*`, pointer arithmetic, `NULL`, null checks
      (this unlocks everything else and is worth doing thoroughly)
- [ ] One-dimensional arrays (size-checked? address-to-first-element decay
      in expressions), then multi-dimensional
- [ ] `struct` definitions, member access `.`
- [ ] Function prototypes / forward declarations (formal `param decls`)
- [ ] `unsigned` variants if `int` becomes 32-bit (Phase 1)

**Acceptance:** `tests/08_pointers.c`, `tests/09_arrays.c`,
`tests/10_structs.c`; a real `main` that reads/writes an array and sums it
via pointers. This phase proves the compiler can host real algorithms.

---

## Phase 4 — I/O & runtime

- [ ] Built-in `print_int` / `print_char` / `print_str` wired to the CRT
      (`printf`-subset or raw `write`) so programs stop living only in exit
      codes
- [ ] `--run-io` / runtime library compiled into every program (still a single
      self-contained `.s`)
- [ ] Define the official calling convention **now** that external calls exist:
      option A keep the internal 6-register scheme but only call mycc code;
      option B emit the real Microsoft x64 ABI (`rcx/rdx/r8/r9` + 32-byte
      shadow space) under `--target=windows` — enabling calls into the OS/CRT

**Acceptance:** a hello-world test prints text and returns 0; a program calls
`printf` on Windows via option B (or is clearly documented as internal-only
under option A).

---

## Phase 5 — Optimization (beyond the "no optimizer" claim)

- [ ] Constant folding (`2 + 3` → `5`, comparisons of constants, folding into
      `movq $k`)
- [ ] Dead code / constant-condition elimination (`if (0) ...`)
- [ ] Copy propagation + dead variable elimination for the current
      push/pop stack machine
- [ ] Replace the expression stack machine with a simple register allocator
      (a few `%r` temps instead of `push`/`pop` per operation)

**Acceptance:** identical exit codes across all tests (differential check), and
the tour's step-5 contrast can now show *some* of mycc's own folding as a
learning example.

---

## Phase 6 — Diagnostics & developer tooling

- [ ] Source-snippet printing in errors (`prog.c:3:9: error: ...` with the
      offending line and a caret)
- [ ] Warning flags (unused variable, missing return, constant condition)
- [ ] `--interpret`: a tree-walking interpreter that runs the AST
      (mirrors codegen semantics; ideal for teaching and for differential
      fuzzing against the compiled output)
- [ ] Differential test harness: random program generator feeds the same exit
      codes through `--interpret`, `--emit-asm`, and real gcc

**Acceptance:** a fuzz run of N=10,000 generated programs finds zero
interpreter-vs-codegen mismatches (a strong correctness proof).

---

## Phase 7 — Stretch goals

- [ ] Floating point: `double`, SSE instructions, comparisons, conversions
- [ ] Strings as a real type (`char *s = "hi"`, length, escape sequences)
- [ ] CLang-style `-S`/`-o` flag parity and `--vectorize`? (no — keep scope)
- [ ] Custom IR (SSA-lite) as the middle representation, enabling aggressive
      optimization and a backend split (teaching showcase)
- [ ] Release packaging: CI artifacts, `w64devkit`/MSYS2 instructions for
      end users

---

## How work flows

1. Each feature adds a self-checking test first (`tests/NN_feature.c` with an
   `EXPECT:` marker), which fails before the change and passes after.
2. Cross-platform check: Linux (`make test`) and Windows (`test.bat`) both
   green before the phase is marked done.
3. Tour narration (`tour.bat` / `tour2.bat`) is updated only when it stops
   matching reality.