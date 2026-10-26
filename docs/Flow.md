# Flow.md — Execution Flow Map (mycc)

How execution actually travels between files, functions and modules. This is
the *mechanical* companion to `learn.md` (why it works) and `decision.md`
(why it is shaped that way). Local-only, like the rest of the engineering docs.

**Legend**

| Marker | Meaning |
|---|---|
| ▲ NEW | added in the 2026-10-23 bug-fix batch (`4c3cd8a`) |
| 🔷 unchanged | existed before that batch and was not touched |
| 🁢 | loop / repeated pass |
| ⤴ | error path (leaves early) |
| ▣ | buffer / data structure being walked |

---

## 1. Startup: one process, one job

`src/main.c:main()` is the only entry point. It does argument parsing, then
dispatches to exactly one of three outputs: tokens, AST, or assembly.

```
main()                                              [main.c]
├─ parse argv, in order
│  ├─ "-o" <path>             → output_path (default: none, stdout is not used)
│  ├─ "--dump-tokens"         → mode = DUMP_TOKENS
│  ├─ "--dump-ast"            → mode = DUMP_AST
│  ├─ "-w"                    → set_warnings_enabled(0)            ▲ NEW
│  ├─ "--no-opt"              → g_optimize = 0                     ▲ NEW
│  ├─ "--sign"                → g_sign = 1                          ▲ NEW
│  ├─ "--version"             → version_flag = 1                    ▲ NEW
│  ├─ "--target=linux|windows|darwin" → target_kind                ▲ NEW
│  │     ⤴ unknown → report_error "unknown target '…'" + usage + exit 1
│  ├─ "-h" / "--help"         → usage() + exit 0
│  └─ anything else           → source_path (only one allowed)     🔷
├─ if version_flag → printf version + targets, exit 0              ▲ NEW
├─ source = read_file(source_path)                     ⤴ cannot open → exit 1
├─ set_source_path(source_path); set_source_text(source)           ▲ NEW
│     ⤴ the text is borrowed, not copied: every diagnostic below can
│       quote the offending line and place a caret
├─ lexer_init(&lexer, source)
├─ if DUMP_TOKENS → dump_tokens(&lexer, stdout); return 0
├─ root = parse_program(&lexer)                        [parser.c]
│     ⤴ error_count() > 0 → "compilation aborted during parsing"; exit 1
├─ if DUMP_AST → dump_ast(root, stdout); return 0
├─ semantic_analyze(root)                              [semantic.c]
│     ⤴ error_count() > 0 → "compilation aborted during semantic analysis"; exit 1
└─ codegen_generate(root, out, target_kind)            [codegen.c]  ▲ target arg
      ▲ output goes to "<output>.mycc-tmp", renamed on success
        ⤴ failure → remove(temp), no output file left behind
      ▲ if g_sign → intern_string(MYCC_SIGNATURE, strlen) before gen_data,
        so gen_strings() emits one unreferenced .rodata entry (the stamp)
```

**Where the target default comes from** ▲ NEW: `#if defined(_WIN32)` →
`TARGET_WINDOWS`, `#elif defined(__APPLE__)` → `TARGET_DARWIN`, else
`TARGET_LINUX`. `--target=` overrides it.

**Warnings never change the exit status** ▲ NEW: `report_warning()` touches
`warning_count()`, never `error_count()`, so the pipeline above is unaffected.

**Diagnostic path** ▲ NEW (Phase 6) — identical shape for errors and warnings:

```
report_error(line, col, fmt, …) / report_warning(…)          [util.c]
├─ "file:line:col: error: " + formatted message        (unchanged contract)
├─ print_snippet(line, col)                            ▲ NEW
│  ├─ g_source_text == NULL or line <= 0 → return (nothing to point at)
│  ├─ walk g_source_text counting '\n' until the requested line
│  ├─ clip the line at 78 columns (a wrapped line would lie)
│  ├─ "  %4d | <line>"
│  └─ "       | " + (col-1) spaces/tabs + "^"
└─ counter++ (error or warning)
```

The caret row repeats tabs instead of expanding them: the compiler cannot know
the terminal's tab width, so both rows advance identically. `main()` calls
`set_source_text(source)` right after `read_file()`; the buffer lives until exit,
so no diagnostic ever reads freed memory.

---

## 2. Mode A — lexing (`--dump-tokens`)

```
dump_tokens(&lexer, out)                            [dump.c]
└─ 🁢 loop: lexer_next_token(&lexer)                [lexer.c]
   └─ lexer_next_token()
      ├─ skip_trivia()          — whitespace, // comments, /* */, BOM ▲ NEW,
      │                            and '#' → one "preprocessor not supported" ▲ NEW
      ├─ identifier/keyword → lex_ident_or_keyword()
      ├─ digit             → lex_number()           ▲ validates octal digits
      ├─ '\''              → lex_char_literal() → read_escape()
      ├─ '"'               → skip_string_literal()  ▲ reports once, keeps lexing
      └─ operator/punct    → make_token()
         ⤴ unknown character → report_error(), return TOKEN_EOF
   └─ print "TYPE line:col value"
```

`error_reset()` clears the error counter but **not** `g_warnings_enabled` ▲ NEW,
which is why `-w` survives argument parsing.

---

## 3. Mode B — parsing (`parse_program`)

The parser is a hand-written recursive-descent parser whose function nesting
*is* the precedence ladder (lowest first).

```
parse_program(lexer)                                [parser.c]
└─ 🁢 while not EOF
   ├─ parse_type_specifier()                        — int/char/unsigned/void/struct
   ├─ consume leading '*'s into the base type       ▲ NEW (BUG-005)
   ├─ if ';'  → bare struct definition, continue     🔷
   ├─ expect identifier
   ├─ if '('  → parse_function_after_name(name, ret) 🔷
   │            ├─ parse_param_list(fn)  — rejects struct-by-value ▲ NEW
   │            └─ ';' → prototype, else parse_block() for the body
   └─ else    → parse_global_vars(first_name, base_type)
                 └─ 🁢 parse_array_dimensions() + optional '=' const initializer
```

Expression ladder (each level calls the next one down):

```
parse_expression()        ← ▲ NEW: loops over parse_assignment() with ','
└─ parse_assignment()     — handles '=', '+=', … , and the desugaring
   └─ parse_ternary()  → parse_or() → parse_and() → parse_bit_or()
      → parse_bit_xor() → parse_bit_and() → parse_equality()
      → parse_relational() → parse_shift() → parse_additive()
      → parse_multiplicative()
         └─ parse_unary()          — - + ! ~ ++ -- & *
            └─ parse_unary_inner() — also 'sizeof'-less casts      ▲ NEW
               └─ parse_primary()  — literals, names, calls, '(' …
                  ├─ '(' + is_type_start() → cast: type then unary  ▲ NEW
                  └─ '[' → builds *(base + index)
```

Statements:

```
parse_statement()  → parse_statement_inner()
├─ parse_depth_enter() checks MYCC_MAX_NEST (256)     ▲ NEW (BUG-010)
├─ '{'  → parse_block()        🔷
├─ if / while / do / for / switch → parse_if() … parse_switch()
├─ break / continue → parse_jump()      🔷
├─ return → parse_return()              🔷
└─ expression → parse_expr_stmt()
```

**Key desugarings** (the AST the next stage sees):

| Source | AST |
|---|---|
| `a[i]` | `AST_DEREF(AST_BINARY_OP(+, a, i))` |
| `a[i] = v` | `AST_ASSIGN` with that deref on the left |
| `a[i] += v` | `AST_ASSIGN(left, AST_BINARY_OP(+, left, v))` — **one shared `left` node** |
| `p->m` | `AST_MEMBER_ACCESS(AST_DEREF(p), m)` |
| `(int)x` ▲ NEW | `AST_CAST` with `vtype = TYPE_INT` |
| `a, b` ▲ NEW | `AST_BINARY_OP(OP_COMMA, a, b)` |

The shared node in the compound-assign row is the whole reason BUG-001/002/020
existed; see §5.

---

## 3b. Mode B′ — the toy grammar (`myc_parse_program`)

`--lang=myc` swaps the front end out at one ternary in `main.c` and nothing else:

```
main.c:  ASTNode *ast = lang_myc ? myc_parse_program(&lexer) : parse_program(&lexer);
```

Everything downstream — §4 semantic analysis, §5 code generation — is unaware
that a language other than C was read. That is the architectural claim the demo
makes, and it is why `demo\4_pair.myc` and `demo\4_pair.c` emit byte-identical
assembly.

```
myc_parse_program(lexer)                              [myc.c]
├─ 🁢 while current is the word "fn"        (▲ NEW: this grammar)
│  ├─ parse_fn()
│  │  ├─ name, '(' , parse_param_list → all int (no type syntax in myc)
│  │  ├─ '->' int|void  (optional; default int)
│  │  ├─ vars_reset()            — `say`'s name table is per function
│  │  ├─ parse_block_body()      → AST_BLOCK, stops at end/else/EOF
│  │  └─ expect word "end"       — the only block terminator there is
│  └─ append_node(&head, fn)
└─ vars_reset(); free current/previous token values
```

```
parse_block_body()   walks parse_stmt() until "end"/"else"/EOF, does NOT consume it
parse_stmt()         let | say | if | while | return | NAME ('=' | '(')
parse_if()           'if' expr block ['else' block] 'end' → AST_IF(cond, body, right)
parse_while()        'while' expr block 'end'             → AST_WHILE(cond, body)
parse_return()       'return' [expr]                      → AST_RETURN(left)
parse_let()          'let' NAME '=' expr                  → AST_VAR_DECL + AST_ASSIGN chain
say                   expr is STRING or a `let` string name
                      → AST_CALL print_str + AST_CALL print_endl
                      otherwise → AST_CALL print_int  (already ends its line)
expression ladder    parse_expr → parse_and → parse_eq → parse_rel → parse_add
                     → parse_mul → parse_unary_or_primary → parse_primary
```

**Where the two grammars meet — the same nodes, not similar ones:**

| myc source | AST produced | identical to C's |
|---|---|---|
| `let x = 5` | `AST_VAR_DECL` → `AST_ASSIGN(AST_VAR_REF, AST_INT_LIT)` | `int x = 5;` |
| `let s = "hi"` | same, `val_type = char *` | `char *s = "hi";` |
| `while c ... end` | `AST_WHILE(cond, AST_BLOCK)` | `while (c) { ... }` |
| `if c ... else ... end` | `AST_IF(cond, block, block)` | `if (c) { ... } else { ... }` |
| `say 3 + 4` | `AST_CALL(print_int, AST_BINARY_OP)` | `print_int(3 + 4);` |
| `say "hi"` | `print_str("hi")` → `print_endl()` | `print_str("hi"); print_endl();` |
| `-x` | `AST_BINARY_OP(OP_SUB, INT 0, x)` | `-x` (C desugars the same way) |
| `not x` / `!x` | `AST_BINARY_OP(OP_EQ, x, INT 0)` | `x == 0` |
| `f(a, b)` | `AST_CALL(name, args)` | `f(a, b)` |

Token ownership mirrors `parser.c`: `advance()` frees `previous.value`, names and
string bytes are `xstrdup`/`xmalloc`'d into the node, and a string's byte count
comes from `Token.length` (never `strlen`), so a literal containing NUL survives.

Two conveniences specific to this grammar: the C lexer already skips newlines,
so `end` — not `;` — is what terminates a block, and an expression simply stops
when the next token cannot continue it. `if`/`else`/`while`/`return` arrive as
their C keyword tokens; `fn`/`let`/`say`/`end`/`and`/`or`/`not` arrive as
`TOKEN_IDENT` and are matched by text via `word()`/`match_word()`.

---

## 4. Mode C — semantic analysis

Two passes over the AST: declare everything, then check bodies. Nothing is
emitted here.

```
semantic_analyze(root)                              [semantic.c]
├─ symtab_init()
├─ sem_decl_pass(root)                 🁢 — structs first, then globals, then functions
│  ├─ struct_register() / struct_add_member()        [symtab.c]
│  ├─ symbol_insert(name, type, is_global)
│  └─ d->is_extern → sym->is_extern                  ▲ NEW (Phase 4)
├─ register_builtins()                 ▲ NEW (Phase 4) print_int/print_char/
│  └─ register_builtin()               print_str/print_endl + printf/puts/putchar,
│                                      skipping any name the program already declared
├─ 🁢 for each function → check_function(fn)
│  ├─ parameter count unchecked (no cap)            ▲ NEW (Phase 4: stack args)
│  ├─ check_stmt(body, &ctx)           🁢 every statement
│  │  ├─ declarations → check_object_complete()   (no incomplete/zero-sized objects)
│  │  ├─ switch       → check_switch()            (duplicate case, one default)
│  │  ├─ break/continue → depth checks            (reported in the parser/ctx)
│  │  └─ expressions   → check_expr() → check_value() → check_expr()
│  │     ├─ assignment → types_compatible(lhs, rhs)          ▲ NEW (BUG-019)
│  │     ├─ pointer arithmetic → scale_index_operand()       ▲ NEW (BUG-020)
│  │     ├─ call → check_call() → types_compatible per arg   ▲ NEW (BUG-019)
│  │     │       arity: == param_count, or >= when extern   ▲ NEW (Phase 4)
│  │     │       sym->is_builtin → rewrite_builtin_call()   ▲ NEW (Phase 4)
│  │     ├─ string literal → char *                  ▲ NEW (Phase 4)
│  │     ├─ cast → result type = the cast's type             ▲ NEW
│  │     └─ fold_literal()   ▲ NEW (Phase 5) last step of typing a node:
│  │            └─ fold_const()   rewrites it in place into AST_INT_LIT,
│  │               gated on g_optimize (--no-opt), skipped for unsigned
│  │               operands / div-by-zero / comma / pointer results
│  ├─ check_returns(fn, body)          — 'return expr' shape vs ret_type ▲ NEW
│  └─ stmt_terminates(body)            ▲ NEW (BUG-016)
│     ⤴ false and ret_type != void → report_warning("control reaches end …")
├─ 🁢 for each called-but-not-defined function → report_error at the call site ▲ NEW (BUG-015)
│     (skipped when the symbol is extern or built-in)        ▲ NEW (Phase 4)
└─ no `main` → report_error                              🔷
```

`register_builtin()` only inserts a symbol when `symbol_lookup()` finds nothing,
so a program that defines its own `print_int` keeps it — and `rewrite_builtin_call()`
therefore fires only for the compiler's own helpers ▲ NEW.


`scale_index_operand(binop, ptr_type)` inserts `index * sizeof(*ptr)` into the
tree. It runs **once per node by construction**: the ordinary binary case calls
it after typing both operands, and the compound-assign case checks only the
right operand and types the operation itself, so the shared left node is not
walked twice ▲ NEW (BUG-020).

`type_ptr()` allocates a fresh `Type` per call, so **all** pointer comparisons go
through `same_type()`, which compares kind and recurses through `base` — never
`a == b` on pointers ▲ NEW.

---

## 4b. Mode C' - the AST optimizer (`opt_run`)   NEW (Phase 5)

`src/opt.c`, called from `main()` **after** `semantic_analyze()` and **before**
`dump_ast()` and `codegen_generate()`, gated on `g_optimize`. Sitting there is
the whole design: after semantic analysis every `AST_VAR_REF` has a
`stack_offset` and every expression has a type, so the pass never has to guess
what a name means; before code generation, so `--dump-ast` keeps its promise of
showing the code that actually runs.

```
opt_run(program)                                    [opt.c]
+- for each AST_FUNC_DECL with a body, not extern:
|  +- scan_stmt(fn->body)        build the local table
|  |  +- scan_value(node)        read position: a var ref is a READ
|  |  |  +- AST_ADDR_OF     -> note_address_use()   ADDRESS TAKEN, not a read
|  |  |  +- AST_MEMBER_ACCESS -> note_address_use()  base's address escapes
|  |  |  +- AST_DEREF/POSTFIX -> recurse as a value  (a read of the pointer)
|  |  |  +- AST_CALL        -> every argument is a read
|  |  +- scan_lvalue(node)       write position: a plain var ref is a WRITE
|  |  +- scan_stmt(node)         if/while/do/for/switch/block/return branches
|  +- if the table overflowed (OPT_MAX_LOCALS), skip the function
|  +- prune(fn->body)            rebuild each statement chain
|     +- recurse into the blocks it contains
|     +- is_dead_store(node)?  -> unlink the node
+- done. No node is ever added, no type changed, no frame size touched.
```

`is_dead_store()` is the entire safety argument, and it is four conditions:

```
is_dead_store(n)                                    [opt.c]
+- n is AST_ASSIGN with a plain AST_VAR_REF on the left (not a member/element)
+- that variable is a local (not global: another function may read it)
+- the type is scalar (not an array: it decays; not a struct: it is copied whole)
+- the variable has reads == 0 and address_taken == 0 in this function
   (keyed by stack_offset, so shadowed locals cannot be confused)
+- expr_is_pure(n->right): literal, variable, cast, or arithmetic on those
+- otherwise: leave it alone (a call on the right keeps its statement)
```

Note the asymmetry that makes it correct: `&b` is **not** a write. Treating it
as one made `int *p = &b; ... *p` look like a dead store - the pointer then read
whatever the slot contained (BUG-025, caught by `tests/12_pointers.c`).

---

## 5. Mode D - code generation

A stack machine: every binary operator pushes its left operand, evaluates its
right, pops into a register, computes, leaves the result in `%rax`. `%rcx`,
`%r10`, `%r11` are scratch; `pushed_count` tracks stack depth so calls stay
16-byte aligned.

```
codegen_generate(root, out, target)                 [codegen.c]
├─ target = target                      ▲ NEW (TARGET_LINUX/WINDOWS/DARWIN)
├─ defined_names[] = every function with a body   ▲ NEW (Phase 4: @PLT only for externals)
├─ gen_program(root)
│  ├─ gen_data(prog)                    🁢 globals: .data/.bss, .quad/.byte/.zero
│  │                                     (zero-initialized → .zero)
│  ├─ 🁢 gen_func(fn)
│  │  ├─ ".globl" + label via sym_name(name)         ▲ NEW (Mach-O '_')
│  │  ├─ prologue: pushq %rbp / movq %rsp,%rbp / subq $aligned_frame
│  │  ├─ 🁢 parameters: i < arg_reg_count() → arg_reg(i) into its frame slot
│  │  │                 i >= that        → stack_arg_base()+8*(i-n)  ▲ NEW (Phase 4)
│  │  ├─ gen_stmt(body)                 🁢 statements
│  │  │  ⤴ returns 1 if the last statement cannot fall through
│  │  ├─ if not terminated: movq $0,%rax   (defensive; warned about ▲ NEW)
│  │  └─ epilogue: leave / ret
│  └─ gen_strings()                     ▲ NEW (Phase 4) .rodata at the end
│                                        (Mach-O: __TEXT,__cstring)
└─ ".note.GNU-stack" only when target == TARGET_LINUX  ▲ NEW
```

`gen_stmt()` → `gen_stmt_one()` per statement; `gen_stmt_one()` dispatches to
`gen_if/gen_while/gen_do_while/gen_for/gen_switch/gen_return` or
`gen_expr/gen_assign`.

**Constant conditions** ▲ NEW (Phase 5). `const_truth(cond, &truth)` returns 1
only when the condition is an `AST_INT_LIT` and `g_optimize` is on. Each
control-flow generator checks it first and emits only what can run:

| Generator | Constant condition | What is emitted |
|---|---|---|
| `gen_if` | `if (1)` / `if (0)` | only the live arm — no labels, no compare |
| `gen_while` | `while (0)` | nothing at all (body unreachable) |
| `gen_do_while` | `while (0)` | the body once, with its labels (a `break`/`continue` needs a target) but no back-jump |
| `gen_for` | `for (init; 0; step)` | the init only — it may have side effects |
| `gen_ternary` | `c ? a : b` | only the chosen operand |

Because this happens in codegen, semantic analysis has already type-checked the
dead arm and already emitted its warnings — deleting output can never delete a
diagnostic.

Expression dispatch (`gen_expr`):

| Node | Function | Notes |
|---|---|---|
| `AST_BINARY_OP` | `gen_binary()` | `gen_binary_imm()` first; otherwise `temp_push` left, eval right, `temp_combine()` |
| `AST_BINARY_OP` + literal right | `gen_binary_imm()` ▲ NEW | one instruction, no push/pop: `addq $1, %rax` |
| `AST_BINARY_OP (OP_COMMA)` | `gen_expr` twice ▲ NEW | left for effect, right is the value |
| `AST_ASSIGN` (compound shape) | `gen_compound_assign()` ▲ NEW | address → stack slot, load, op, store through slot |
| `AST_ASSIGN` (plain) | `gen_assign()` | struct-to-struct → `rep movsb` ▲ NEW |
| postfix `++`/`--` | `gen_postfix()` | address once, old value, store through slot ▲ NEW |
| prefix `++`/`--` | `gen_expr` on `AST_BINARY_OP` | 🔷 |
| `AST_CAST` | `gen_expr(left)` + truncate/extend ▲ NEW | nothing when the width is unchanged |
| `AST_STRING` | `intern_string()` then `leaq .L.str.N(%rip)` ▲ NEW | the pool is emitted by `gen_strings()` at EOF |
| `AST_CALL` | `gen_call()` | per-target arg regs, stack args, shadow space, `%al`, `@PLT` ▲ NEW |
| `AST_DEREF` / `AST_MEMBER_ACCESS` / `AST_ARRAY_INDEX` | `gen_addr()` then load | scaling already applied in semantic |
| lvalue targets | `gen_addr()` | pushes computed address |

**Constant operands** ▲ NEW (Phase 5) — `gen_binary()` asks
`gen_binary_imm(node)` before doing anything else. It returns 1 only when the
right child is an `AST_INT_LIT` **and** the value encodes exactly:

```
gen_binary_imm(node)                                [codegen.c]
├─ g_optimize == 0            → 0  (the --no-opt path is byte-identical)
├─ right is not AST_INT_LIT   → 0
├─ OP_DIV / OP_MOD            → 0  (`/0` must still trap; `/2` is not `>>1` for
│                                   negatives)
├─ OP_AND / OP_OR / OP_COMMA  → 0  (short-circuit / sequencing, handled earlier)
├─ OP_SHL / OP_SHR, count ∉ 0..63 → 0
├─ other ops, |v| > 2^31-1    → 0  (x86 immediates are sign-extended 32-bit)
└─ otherwise
   ├─ gen_expr(left)                       → %rax holds the left operand
   └─ one instruction: addq/subq/andq/orq/xorq/imulq $v, %rax
      or shlq/sarq/shrq $v, %rax
      or cmpq $v, %rax + setcc %al + movzbq %al, %rax
```

Skipping `gen_expr(right)` is safe **only** because a literal has no side
effects; the left is still evaluated first, so order is unchanged. Nothing is
pushed or popped, so `pushed_count` (and therefore 16-byte alignment at calls)
is untouched.

**Expression temporaries**   NEW (Phase 5) - the `pushq`/`popq` pair that used to
park the left operand is replaced by a one-register pool, `%r10`:

```
temp_push()                     park %rax as a temporary
+- g_optimize off, or %r10 taken -> pushq %rax         (the old path, exactly)
+- otherwise                     -> movq %rax, %r10     (one instruction, no memory)

temp_combine(op, uns)           consume the top temporary as the LEFT operand
+- in %r10 and op is +,-,*,and/or/xor/compare -> emit_binop_reg(op, uns, "%r10")
|                                                NO move at all: addq %r10, %rax
+- op is / or % -> temp_pop_into("%rcx") first: those use %r10 as scratch, so the
|                  operand has to leave the register before the instruction runs
+- otherwise    -> temp_pop_into("%rcx") then emit_binop_reg(op, uns, "%rcx")
```

The rest of the bookkeeping, because three other places need it:

```
temp_pop_into(reg)      value -> reg, entry gone    (movq from %r10, or popq)
temp_peek_into(reg, d)  value d levels down -> reg, entry kept
temp_drop()             entry gone, nothing emitted
temp_spill()/temp_unspill(n)  park/unpark %r10 around a call, counted in
                              pushed_count so 16-byte alignment stays exact
```

Two rules keep it honest. First, the register is *caller-saved*, so a call may
destroy it: `gen_call()` spills it before the argument layout is computed and
reloads it after the cleanup. Second, `%r10` is division's scratch register, so
a value parked by an **enclosing** expression has to survive a nested `/` or `%`:
`temp_combine()` spills the pool across those two operators (BUG-026, caught by
`tests/36_registers.c`). Entries above the register are ordinary stack slots,
which is why the structure is still a stack - one that starts in a register.

**`gen_call()` in full** ▲ NEW (Phase 4) — every step exists because two ABIs
disagree:

```
gen_call(node)
  nreg = arg_reg_count()          4 (Windows) / 6 (SysV)
  stack_args = max(0, n - nreg)   these stay on the stack, in push order
  if (pushed_count + stack_args + shadow_units) & 1: subq $8      (pad FIRST)
  for i = n-1 .. 0:  gen_expr(args[i]); pushq %rax     (right-to-left: nested calls safe)
  for i = 0 .. min(n,nreg)-1:  popq arg_reg(i)
  if Windows: subq $32            (home space BELOW the stack args)
  if SysV:    movb $0, %al        (the variadic marker printf reads)
  call call_target(name)          name@PLT when external on ELF, else plain
  addq $(stack_args + shadow + pad)*8, %rsp
```

The callee side of the same contract, in the prologue: register parameters are
copied into their frame slots, and the ones passed on the stack are read from
`16(%rbp)` (SysV) or `48(%rbp)` (Windows — past the saved `%rbp`, the return
address, and 32 bytes of home area).


**The two "evaluate once" fixes** ▲ NEW — same shape, two places:

```
gen_compound_assign(node)                gen_postfix(node)
  gen_addr(target)      → %rax             gen_addr(left)       → %rax
  pushq %rax            (address slot)     pushq %rax           (address slot)
  movq (%rsp),%rcx                         movq (%rsp),%rcx
  load through %rcx     (old value)        load through %rcx    (old value)
  pushq %rax            (left operand)     … arithmetic …
  gen_expr(binop->right)                   store new value through (%rsp)
  popq %rcx; <op> %rcx,%rax                popq %rcx            (drop address)
  store %rax through (%rsp)                (result = old value)
  popq %rcx            (drop address)
```

`--dump-ast` still shows the desugared `a = a + 1` for a compound assignment, so
the dump does **not** literally describe this code. That is deliberate and
documented (see `decision.md`, 2026-10-23 14:20).

---

## 6. One-liner data flow per feature

| Feature | Flow |
|---|---|
| Arithmetic | `parse_additive` → `AST_BINARY_OP` → `gen_binary` → `temp_push`/`temp_combine`/`%r10` (`--no-opt`: `pushq`/`emit_binop`) |
| Dead local store | `opt_run` → `scan_stmt` classifies reads/writes/address-taken → `is_dead_store` → `prune` unlinks the assignment |
| Expression temporary | `temp_push` → `movq %rax, %r10` (or `pushq` when taken) → `temp_combine` reads that register → `gen_call`/`/`%` spill it |
| Local variable | `symbol_insert(offset)` → `resolve_var` → `emit_var_load/store` via `-offset(%rbp)` |
| Global variable | `gen_data` allocates `.comm`/`.data` → `sym_name()` at each reference |
| Array index | parser → `*(a + i)` → semantic inserts `i*sizeof(elem)` → `gen_addr` adds |
| Pointer arithmetic | `check_expr` recognises pointer±int → `scale_index_operand` → plain `addq` |
| Struct member | `gen_addr` adds the member offset → load/store with the member type |
| Struct copy | `gen_assign` → `leaq` src/dst into `%rsi`/`%rdi`, `movq $size,%rcx`, `rep movsb` |
| Function call | `gen_call` → per-target `arg_reg(i)` / stack args / shadow space → `call call_target(f)` → result in `%rax` |
| String literal | `lex_string_literal` (decoded bytes + `length`) → `AST_STRING` (`name`, `str_len`) → `intern_string(bytes,len)` → `gen_strings` emits `.string` |
| `print_int(x)` | `check_call` (type-checks) → `rewrite_builtin_call` → `printf("%lld\n", x)` → ordinary `gen_call` |
| `printf` with no declaration | `register_builtins` inserts `printf`/`puts`/`putchar` as externs → `check_call` accepts `argc >= param_count` |
| `extern int f(...)` declaration | `parse_program` sees `TOKEN_KEYWORD_EXTERN` → `fn->is_extern` → `Symbol.is_extern` → exempt from BUG-015 |
| Constant expression | `check_expr` types the node → `fold_literal` → `fold_const` → node becomes `AST_INT_LIT` (visible in `--dump-ast`) |
| `if (0)` / `while (0)` / `?:` with a constant | `const_truth` in each control-flow generator → only the live arm reaches `emit_line` |
| `x + 1` / `i < 10` / `a * 2` | `gen_binary` → `gen_binary_imm` → one instruction carrying the literal; no push, no register for it |
| Literal too large / `/` / `%` / shift ≥ 64 | `gen_binary_imm` returns 0 → the general two-register path |
| `--no-opt` | `g_optimize = 0` in `main` → `fold_literal`, `const_truth` and `gen_binary_imm` all return immediately (pre-Phase-5 output) |
| Any error or warning | `report_error`/`report_warning` → message line (unchanged) → `print_snippet` → `file:line:col`, the quoted line, a caret |
| `--sign` | `main` sets `g_sign` → `gen_program` interns `MYCC_SIGNATURE` with its **length** → `gen_strings` emits it as an unreferenced `.rodata` entry |
| `--version` | parsed before any file is read → prints version, targets, optimizer state → exits 0 |
| `switch` | `check_switch` validates labels → `gen_switch` emits a compare chain into `break_id`/`continue_id` stacks |
| Cast | `AST_CAST` → semantic sets the type → codegen truncates/extends only if the width changes |
| Warning | `stmt_terminates` → `report_warning` → stderr, exit status untouched |
| Target flavour | `main` picks `TargetKind` → `sym_name()` for `_`; `.note.GNU-stack` gated |

---

## 7. Recently modified / in-flight

| Date | Area | What changed | State |
|---|---|---|---|
| 2026-10-26 | `semantic.c` / `codegen.c` type rules | `&array` keeps pointer-to-array type; assignment results narrow to the stored value; `?:`, `+`/`-`, comparisons and shifts follow C's usual arithmetic conversions; shift signedness uses the promoted left operand; typed constant folding and global/case type-checking agree with codegen; integer casts fold in constant expressions (BUG-032/033/034/037/038/046) | working tree (tests 39, 42-43, 46, 48) |
| 2026-10-26 | `parser.c` label bodies, `util.c`/`opt.c`/`codegen.c` traversal | `case`/`default` in an unbraced controlled body (`if (0) case 1: ...`) is nested under the label, so the false path cannot fall into it; dead constant arms still keep labels reachable by the switch (BUG-035/036) | working tree (tests 44-45) |
| 2026-10-26 | `lexer.c`, `parser.c`, `semantic.c` | high-byte character constants sign-extend, oversized escapes are diagnosed, `\?` is accepted, unnamed prototype parameters are allowed/rejected in definitions, adjacent strings concatenate after escape decoding, and `void`/`main`/`extern`/global-initializer declarations are type-checked (BUG-038/039/040/041/044) | working tree (tests 38, 40-41, 47, 49; error fixtures) |
| 2026-10-26 | `main.c`, `codegen.c` | source NULs are rejected, the same-file/temp-path guard follows alternate spellings, Darwin variadic calls zero `%al`, and function/symbol tables plus call argument storage are dynamically sized (BUG-042/043/045) | working tree (same-file contract; tests 37; Darwin assembly inspected) |
| 2026-10-26 | `semantic.c` scalar contexts | **not fixed:** struct values are still accepted as `if`/loop conditions and comparison operands; the fix is a targeted scalar-context check, not a broad “structs are never values” rule (BUG-048) | **OPEN** (manual probe; no regression test yet) |
| 2026-10-24 | `util.c` `parse_integer_literal`, `parser.c`/`myc.c` `parse_primary` | one literal hand-off (base 0 + range + consumed-everything) for both front ends, so `0x10` is 16 in the toy language too (BUG-029) | landed `a77a2bc` |
| 2026-10-24 | `test.bat` `:myc_pair`, `Makefile` `test` pair loop | the pair contract became a loop over `tests/myc/NAME.myc`; a third pair costs one list entry | landed `a77a2bc` |
| 2026-10-24 | `tools/fuzz.c` `emit_program`, `difftest.sh`/`.bat` `SRCDIR`+`QUIET`, `Makefile`/`fuzz.bat` | random programs checked by the existing differential harness; deterministic per (seed, index) | landed `205a1ad` (Phase 6) |
| 2026-10-24 | `Makefile` Darwin `LDFLAGS := -arch x86_64`; `ci.yml` `diffutils`, Rosetta probe, artifacts, fuzz steps | the two real CI failures: missing `cmp` (BUG-030) and x86-64 assembly read as arm64 (BUG-031) | landed `bae132f`; **CI green** run `40086581248` |
| 2026-10-24 | `myc.c` `at_else`, `parse_block_body`, `parse_if`, `parse_return` | a block stopped only on the *identifier* spelling of `else`, but the C lexer returns it as a keyword token, so `if`/`else` never parsed (BUG-027); `tests/myc/else.myc` + C twin now take the else branch and compile to the same bytes | landed `4d0154d` (BUG-027) |
| 2026-10-24 | `codegen.c` `temp_push`/`temp_combine`/`temp_spill`, `emit_binop_reg` | left operand parks in caller-saved `%r10` instead of `pushq`/`popq`; the two-callee-saved-register design was measured and rejected | landed `86d5865` (Phase 5) |
| 2026-10-24 | `opt.c` `opt_run`, `main.c` pipeline | dead local stores as an AST pass between semantic analysis and codegen (BUG-025) | landed `86d5865` (Phase 5) |
| 2026-10-24 | `tests/35`, `tests/36`, `test.bat`/`Makefile` parity loop | dead-store test, temporary-register stress test, and opt/no-opt parity on every test | landed `86d5865` (Phase 5) |
| 2026-10-24 | `myc.c` `myc_parse_program` + `main.c` `--lang=myc` | a second front end building the same AST; `fc /b` proves identical assembly | landed `daa35b8` (Phase 8) |
| 2026-10-24 | `test.bat` `:myc_contract`, `Makefile` `test` | the toy language's equivalence is executed on every run, not asserted | landed `daa35b8` (Phase 8) |
| 2026-10-24 | `demo.bat` scene 5, `DEMO.md`, `demo/expected.txt` | six-scene demo; the `FC: no differences encountered` moment | landed `05de3df` |
| 2026-10-24 | `codegen.c` `gen_binary_imm` | literal right operand encoded in the instruction; 33.4% smaller output (re-measured) | landed `016ff82` (Phase 5) |
| 2026-10-24 | `util.c` `print_snippet`, `set_source_text` | errors **and** warnings quote the line and point with a caret | landed `0f58d9b` (Phase 6) |
| 2026-10-24 | `main.c` `--version`, `--sign`; `codegen.c` `MYCC_SIGNATURE` | version banner; provenance string in `.rodata` | landed `0f58d9b` (Phase 7) |
| 2026-10-24 | `demo.bat`, `demo/`, `DEMO.md` | five-scene non-technical demo + presenter script + real transcript | landed `05de3df` |
| 2026-10-23 | `semantic.c` `fold_literal`/`fold_const` | constant folding, in place, after typing; gated on `g_optimize` | landed (Phase 5) |
| 2026-10-23 | `codegen.c` `const_truth` + 5 generators | constant conditions: `if`/`while`/`do`/`for`/`?:` emit only the live arm | landed (Phase 5) |
| 2026-10-23 | `util.c` `g_optimize`, `main.c` `--no-opt` | one optimizer switch; unoptimized path = pre-Phase-5 compiler | landed (Phase 5) |
| 2026-10-23 | `tests/31`, `tests/32`, `tests/diff/03` | folding + dead-branch tests, gcc cross-check | landed (Phase 5) |
| 2026-10-23 | `lexer.c` `read_escape` | octal escapes `\ooo`, `\0` as the 1-digit case (BUG-022) | landed `4410784` |
| 2026-10-23 | `lexer.c` `lex_string_literal` | real string literals: decoded bytes + `Token.length` | landed `4410784`/`831a61a` |
| 2026-10-23 | `parser.c` `parse_program` | `extern` at file scope; `is_extern` on `AST_FUNC_DECL` | landed `4410784` |
| 2026-10-23 | `semantic.c` `register_builtins`/`rewrite_builtin_call` | `print_*` lowered to `printf`/`putchar`; `printf`/`puts`/`putchar` pre-declared | landed `4410784` |
| 2026-10-23 | `semantic.c` param cap | "more than 6 parameters" rejection deleted | landed `4410784` |
| 2026-10-23 | `codegen.c` `gen_call`/`arg_reg`/`stack_arg_base`/`call_target` | real SysV + Win64 ABI, stack args, shadow space, `%al`, `@PLT` | landed `4410784` |
| 2026-10-23 | `codegen.c` `str_pool`/`gen_strings` | `.rodata` / `__TEXT,__cstring`, deduplicated by (bytes,len) (BUG-023) | landed `831a61a` |
| 2026-10-23 | `codegen.c` `gen_assign` | Windows: save/restore `%rsi`/`%rdi` around `rep movsb` | landed `4410784` |
| 2026-10-23 | `util.c` `ast_node` | promoted from `parser.c` so semantic can synthesise nodes | landed `4410784` |
| 2026-10-23 | `test.bat`/`Makefile` | `// EXPECT-OUT:` marker (assert real stdout) | landed `4410784` |
| 2026-10-23 | `semantic.c` `AST_ASSIGN` | compound-assign case no longer re-walks the shared lvalue (BUG-020) | landed `4c3cd8a` |
| 2026-10-23 | `codegen.c` `gen_compound_assign` / `gen_postfix` | address computed once (BUG-001/002) | landed `4c3cd8a` |
| 2026-10-23 | `codegen.c` `gen_assign` | `rep movsb` struct copy (BUG-003) | landed `4c3cd8a` |
| 2026-10-23 | `codegen.c` `sym_name`, `gen_program` | `TargetKind` + Mach-O symbols (BUG-018) | landed; CI-verified `bae132f` |
| 2026-10-23 | `main.c` output path | temp file + `rename()` (BUG-017) | landed `4c3cd8a` |
| 2026-10-23 | `util.c` `report_warning` | warning channel + `-w` (BUG-016) | landed `4c3cd8a` |
| 2026-10-23 | `parser.c` depth guard | `MYCC_MAX_NEST` 256 (BUG-010) | landed `4c3cd8a` |
| 2026-10-23 | `Makefile` `test` | `tr -d '\r'` on extractions (BUG-021) | landed `4c3cd8a` |
| 2026-10-23 | `tests/diff/` + `difftest.*` | differential harness vs gcc | landed `4c3cd8a` |

Not started: `--interpret` and therefore the third oracle (ROADMAP Phase 6
generator half is now landed), the liveness-based global allocator (it needs
the Phase 7 IR; copy propagation was measured and rejected - see `decision.md`
2026-10-24), `double`, the SSA IR and a 32-bit-`int` mode (Phase 7).
Phase 4 (external ABI + I/O), Phase 8 (the `myc` front end) and Phase 9 (CI
truthfulness) are landed. Phase 4's SysV/Darwin *runtime* verification is now
the green CI matrix rather than a local run. See `ROADMAP.md`.

---

## 7b. The fuzz run's call chain (Phase 6)

`fuzz.bat N SEED` (or `make fuzz FUZZ_N=N FUZZ_SEED=SEED`) generates a corpus
and hands it to the *existing* differential harness - the fuzzer adds no
checking code of its own:

```
fuzz.bat 1000 1
  └─ gcc tools/fuzz.c            -> build/fuzzgen.exe
  └─ build/fuzzgen.exe 1000 build/fuzz 1
       └─ main()                 -> emit_program() per case
            ├─ g_state           LCG seeded as seed*2654435761 + i*7919 + 1
            ├─ emit_expr()       recursive over + - * / % < > <= >= == != && ||
            │    ├─ emit_binary()   /,% take a literal RHS (never divide by 0)
            │    │                  * takes a literal LHS (bounds every value)
            │    └─ emit_leaf()      a variable in scope, or a literal 0..20
            ├─ emit_if_else()    both branches assign acc
            └─ fclose()          build/fuzz/case_NNNN.c
  └─ SRCDIR=build/fuzz QUIET=1 call difftest.bat
       └─ :compare              mycc -> .s -> gcc -> run, gcc -> run, compare $?
```

`make fuzz` runs the same chain through `difftest.sh` (`MYCC`, `CC`,
`TARGETFLAG`, `LDFLAGS`, `SRCDIR`, `QUIET`, `OUT` are all environment
variables, which is why the corpus needed no new script on POSIX).

---

## 8. Call map

`Function → Called by → Calls`. Only the load-bearing edges are listed; the
lexer/`util` helpers are omitted when they are leaves.

| Function | Called by | Calls |
|---|---|---|
| `main` | process entry | `read_file`, `lexer_init`, `parse_program` **or** `myc_parse_program`, `semantic_analyze`, `codegen_generate`, `dump_tokens`, `dump_ast`, `usage` |
| `myc_parse_program` | `main` (when `--lang=myc`) | `lexer_next_token`, `parse_fn`, `vars_reset`, `append_node`, `report_error` |
| `parse_fn` (myc) | `myc_parse_program` | `ast_node`, `parse_block_body`, `expect_word("end")`, `vars_reset`, `type_int`/`type_void` |
| `parse_block_body` (myc) | `parse_fn`, `parse_if`, `parse_while` | `parse_stmt`, `append_node`, `at_else`, `word("end")` |
| `at_else` (myc) | `parse_block_body`, `parse_if`, `parse_return` | `check(TOKEN_KEYWORD_ELSE)`, `word("else")` — the two spellings of `else` (BUG-027) |
| `parse_integer_literal` (util) | `parse_primary` (C, `parser.c`) and `parse_primary` (myc, `myc.c`) | `strtoll(..., 0)` — base 0 so `0x10`/`010` mean what C means; returns OK/RANGE/MALFORMED and lets each front end word its own diagnostic (BUG-029) |
| `emit_program` (tools/fuzz.c) | `main` of `fuzzgen` | `emit_assign`, `emit_if_else`, `emit_expr` → `emit_binary` → `emit_leaf`; writes `build/fuzz/case_NNNN.c` |
| `:compare` (difftest.bat) / `difftest.sh` loop | `fuzz.bat` or `make fuzz` via `SRCDIR` | `mycc --target=…` → `$(CC)` link → run, `$(CC)` direct → run, compare exit codes |
| `parse_stmt` (myc) | `parse_block_body` | `parse_let`, `parse_say`, `parse_if`, `parse_while`, `parse_return`, `parse_ident_statement` |
| `parse_let` (myc) | `parse_stmt` | `parse_expr`, `ast_node`, `vars_declare`, `type_ptr`/`type_char`/`type_int` |
| `parse_say` (myc) | `parse_stmt` | `parse_expr`, `vars_is_string` → builds `print_str`+`print_endl` or `print_int` |
| `parse_expr` (myc) | `parse_let`, `parse_say`, `parse_return`, call args, conditions | `parse_and` → `parse_eq` → `parse_rel` → `parse_add` → `parse_mul` → `parse_unary_or_primary` → `parse_primary` |
| `read_file` | `main` | `fopen`/`fread`, `xmalloc` |
| `lexer_next_token` | `parse_program` (via parser), `dump_tokens` | `skip_trivia`, `lex_ident_or_keyword`, `lex_number`, `lex_char_literal`, `lex_string_literal`, `read_escape`, `make_token`, `report_error` |
| `read_escape` | `lex_char_literal`, `lex_string_literal` | `report_error` (unknown escape, `\x` with no digits); octal `\ooo` |
| `skip_trivia` | `lexer_next_token` | `report_error` (BOM check, `#` directive) |
| `parse_program` | `main` | `parse_type_specifier`, `parse_function_after_name`, `parse_global_vars`, `parse_block` (consumes `extern` first) |
| `parse_function_after_name` | `parse_program` | `parse_param_list`, `parse_block`; records `is_extern` |
| `parse_expression` | `parse_statement`, `parse_for`, `parse_expr_stmt`, `parse_return`, call args | `parse_assignment` (looped on `,`) |
| `parse_assignment` | `parse_expression` | `parse_ternary`, `ast_node` |
| `parse_unary` | `parse_multiplicative` | `parse_unary_inner`, `parse_depth_enter` |
| `parse_primary` | `parse_unary_inner` | `parse_expression`, `parse_call`-shape, `parse_depth_enter`, `is_type_start` |
| `parse_statement` | `parse_block`, `parse_if`, loops | `parse_statement_inner`, `parse_depth_enter` |
| `semantic_analyze` | `main` | `sem_decl_pass`, `register_builtins`, `check_function`, `report_error` |
| `register_builtins` | `semantic_analyze` | `register_builtin` ×7 (`print_*`, `printf`, `puts`, `putchar`) |
| `register_builtin` | `register_builtins` | `symbol_lookup` (user definition wins), `symbol_insert`, `ast_node` |
| `sem_decl_pass` | `semantic_analyze` | `symbol_insert`, `struct_register`, `struct_add_member`, `type_describe` |
| `check_function` | `semantic_analyze` | `count_params`, `check_stmt`, `check_returns`, `stmt_terminates`, `report_warning` |
| `check_stmt` | `check_function`, itself | `check_switch`, `check_expr`, `check_object_complete` |
| `check_expr` | `check_stmt`, `check_value`, itself | `check_value`, `check_call`, `types_compatible`, `scale_index_operand`, `resolve_var`, `fold_literal` |
| `fold_literal` | `check_expr` (last step) | `fold_const`, `binary_is_unsigned`, `type_int` |
| `const_truth` | `gen_if`, `gen_while`, `gen_do_while`, `gen_for`, `gen_ternary` | reads `g_optimize` |
| `check_call` | `check_expr` | `count_args`, `types_compatible`, `report_error`, `rewrite_builtin_call` |
| `rewrite_builtin_call` | `check_call` (when `sym->is_builtin`) | `make_string_node`, `make_int_node` — swaps `print_*` for `printf`/`putchar` |
| `types_compatible` | `check_expr`, `check_call`, `check_returns` | `same_type`, `type_is_integer`, `type_is_pointer`, `is_null_constant` |
| `scale_index_operand` | `check_expr` (binary + compound-assign) | `xcalloc`, `type_int` |
| `stmt_terminates` | `check_function` | `list_terminates`, itself |
| `opt_run` | `main` (after `semantic_analyze`, before `dump_ast`/`codegen_generate`) | `scan_stmt`, `scan_value`, `scan_lvalue`, `note_address_use`, `is_dead_store`, `expr_is_pure`, `prune` |
| `temp_push` | `gen_binary`, `gen_assign`, `gen_compound_assign`, `gen_postfix` | `emit_line`; takes `%r10` when `g_optimize` and free, else `pushq %rax` |
| `temp_combine` | `gen_binary`, `gen_compound_assign` | `emit_binop_reg`, `temp_pop_into`, and `temp_spill`/`temp_unspill` for `/` and `%` |
| `temp_pop_into` / `temp_peek_into` / `temp_drop` | the five park sites | `emit_line`, `pushed_count` |
| `temp_spill` / `temp_unspill` | `gen_call`, `temp_combine` | `emit_line`, `pushed_count` (so alignment stays exact) |
| `emit_binop_reg` | `temp_combine`, `gen_compound_assign` | `emit_line` only; takes the left-operand register as a parameter |
| `emit_epilogue` | `gen_return`, `gen_func` | `emit_line` (`leave`/`ret`); the one place an epilogue change lives |
| `codegen_generate` | `main` | `gen_program`; fills `defined_names[]` for `@PLT` decisions |
| `gen_program` | `codegen_generate` | `intern_string(MYCC_SIGNATURE, len)` when `g_sign`, `gen_data`, `gen_func`, `gen_strings` |
| `gen_func` | `gen_program` | `sym_name`, `arg_reg`, `stack_arg_base`, `gen_stmt`, `emit_line` |
| `gen_stmt` | `gen_func`, itself | `gen_stmt_one` |
| `gen_stmt_one` | `gen_stmt` | `gen_if`, `gen_while`, `gen_do_while`, `gen_for`, `gen_switch`, `gen_return`, `gen_expr`, `gen_assign` |
| `gen_expr` | everywhere in codegen | `gen_binary`, `gen_assign`, `gen_compound_assign`, `gen_postfix`, `gen_call`, `gen_ternary`, `gen_addr` |
| `gen_binary` | `gen_expr` | `gen_binary_imm` (first), `gen_expr`, `temp_push`, `temp_combine` |
| `gen_binary_imm` | `gen_binary` | `gen_expr(left)` only, then one `emit_line` - returns 0 to fall back |
| `intern_string` | `gen_expr` (`AST_STRING`), `gen_program` (`--sign`) | dedupe on (bytes, len); the length is a **byte count**, 0 means zero bytes |
| `gen_addr` | `gen_expr`, `gen_assign`, `gen_compound_assign`, `gen_postfix` | `emit_line`, `sym_name` |
| `gen_assign` | `gen_expr`, `gen_stmt_one` | `gen_addr`, `emit_var_store`, `rep movsb` path, `gen_compound_assign` |
| `gen_call` | `gen_expr` | `gen_expr` (args), `arg_reg`, `arg_reg_count`, `call_target`, `emit_line` |
| `call_target` | `gen_call` | `sym_name`, `is_defined_here` → `name@PLT` or plain |
| `sym_name` | every symbol emission | - (returns `name` or `_name`) |
| `report_error` / `report_warning` | every stage | `print_snippet` (quotes the line + caret), counter++ |
| `print_snippet` | `report_error`, `report_warning` | - (walks `g_source_text`; needs `set_source_text` to have been called) |
| `dump_tokens` | `main` | `lexer_next_token` |
| `dump_ast` | `main` | `dump_node`, `dump_chain`, `dump_label`, `op_name`, `type_name` |
