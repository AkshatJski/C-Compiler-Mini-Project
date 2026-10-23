#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "util.h"

#define MAX_NEST MYCC_MAX_NEST

static FILE *out;
static int label_counter;
static int pushed_count;   /* number of 8-byte temporaries currently on %rsp */
static TargetKind target;  /* selects symbol naming + ELF-only directives */

/* Returns the spelling of an external symbol for the current object format.
 *
 * Mach-O (macOS) prefixes every external name with an underscore, so the
 * symbol the C runtime looks for is `_main`; ELF and COFF use `main`. Every
 * definition and every reference goes through this one function so that a
 * single flag covers the whole file. The buffer is file-static and is only
 * ever consumed immediately by emit_line(), which formats and writes in one
 * step, and no emitted line names two program symbols. */
static const char *sym_name(const char *name)
{
    static char buf[512];
    if (target != TARGET_DARWIN)
    {
        return name;
    }
    snprintf(buf, sizeof(buf), "_%s", name);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Calling convention (ROADMAP Phase 4)                                */
/* ------------------------------------------------------------------ */

/* The two 64-bit ABIs this compiler targets disagree about where the first
 * arguments live, how many of them there are, and whether the caller must
 * reserve scratch space:
 *
 *           1st  2nd  3rd  4th  5th  6th   register args  shadow space
 *   SysV    rdi  rsi  rdx  rcx  r8   r9    6              none
 *   Win64   rcx  rdx  r8   r9   stack stack 4             32 bytes
 *
 * Everything past the register slots is passed on the stack, in order, which
 * is what lets mycc call printf and real C libraries at all. On Windows
 * %rsi/%rdi are callee-saved, so nothing here may clobber them -- and nothing
 * does, except the struct copy below, which saves and restores them. */
static int arg_reg_count(void)
{
    return target == TARGET_WINDOWS ? 4 : 6;
}

static const char *arg_reg(int i)
{
    static const char *const sysv[] = {"%rdi", "%rsi", "%rdx",
                                       "%rcx", "%r8",  "%r9"};
    static const char *const win64[] = {"%rcx", "%rdx", "%r8", "%r9"};
    return target == TARGET_WINDOWS ? win64[i] : sysv[i];
}

/* First stack argument's distance from %rbp inside the callee. Layout at the
 * callee's entry, with R = %rsp before `pushq %rbp`:
 *
 *   [R]        return address
 *   [R+8..40)  Windows: 32-byte home area      (SysV: first stack argument)
 *   [R+40]     Windows: first stack argument
 *
 * After `pushq %rbp; movq %rsp, %rbp` every address above is +8, so the first
 * stack argument sits at 16(%rbp) on SysV and 48(%rbp) on Windows. */
static int stack_arg_base(void)
{
    return target == TARGET_WINDOWS ? 48 : 16;
}

/* Names of functions this file defines. A call to one of these is a plain
 * direct call; anything else is an external library symbol and, on ELF, has
 * to go through the PLT so it still links when the output is position
 * independent. */
#define MAX_DEFINED 256
static const char *defined_names[MAX_DEFINED];
static int defined_count;

static int is_defined_here(const char *name)
{
    for (int i = 0; i < defined_count; i++)
    {
        if (strcmp(defined_names[i], name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static const char *call_target(const char *name)
{
    static char buf[512];
    if (target == TARGET_LINUX && !is_defined_here(name))
    {
        snprintf(buf, sizeof(buf), "%s@PLT", sym_name(name));
        return buf;
    }
    return sym_name(name);
}

/* Breakdown used by gen_call: `%rsp` must be 16-byte aligned at the call. */
typedef struct
{
    int stack_args; /* arguments left on the stack, in 8-byte units */
    int padded;     /* an 8-byte alignment pad was inserted */
    int shadow;     /* Windows home area units (0 or 4) */
} CallLayout;

/* ------------------------------------------------------------------ */
/* Break/continue target stacks                                       */
/* ------------------------------------------------------------------ */

typedef enum
{
    CTX_WHILE,
    CTX_DO,
    CTX_FOR,
    CTX_SWITCH
} CtxKind;

/* Break/continue target stacks. Loops push both; a switch pushes only a
 * break target, so 'continue' inside a switch still finds the loop. Each
 * frame remembers the kind of construct so the right label name is used. */

static CtxKind break_kind[MAX_NEST];
static int break_id[MAX_NEST];
static CtxKind continue_kind[MAX_NEST];
static int continue_id[MAX_NEST];
static int break_sp;
static int continue_sp;

/* Ids of the switch statements currently being generated, innermost last.
 * Kept separate from the break stack because the innermost *breakable*
 * context may be a loop nested inside the switch, while a case label always
 * belongs to the innermost enclosing switch. */
static int switch_id[MAX_NEST];
static int switch_sp;

/* The break/continue/switch stacks are fixed-size arrays. A program nested
 * deeper than MYCC_MAX_NEST used to write past their end and take the process
 * down with an access violation (BUG-009). The parser enforces the same limit,
 * so this is defence in depth: should it ever fire, the diagnostic is clean
 * and no memory is corrupted. */
static int ctx_push_ok(void)
{
    if (break_sp >= MAX_NEST || continue_sp >= MAX_NEST ||
        switch_sp >= MAX_NEST)
    {
        report_error(0, 0,
                     "internal error: nesting limit of %d exceeded during "
                     "code generation (the parser should have caught this)",
                     MAX_NEST);
        return 0;
    }
    return 1;
}

static const char *break_label_fmt(CtxKind kind)
{
    switch (kind)
    {
    case CTX_WHILE:
        return "\tjmp .L_loop_end_%d";
    case CTX_DO:
        return "\tjmp .L_do_end_%d";
    case CTX_FOR:
        return "\tjmp .L_for_end_%d";
    case CTX_SWITCH:
        return "\tjmp .L_switch_end_%d";
    }
    return "\tjmp .L_loop_end_%d";
}

static const char *continue_label_fmt(CtxKind kind)
{
    switch (kind)
    {
    case CTX_WHILE:
        return "\tjmp .L_loop_start_%d";
    case CTX_DO:
        return "\tjmp .L_do_cont_%d";
    case CTX_FOR:
        return "\tjmp .L_for_cont_%d";
    case CTX_SWITCH:
        break; /* switches never carry a continue target */
    }
    return "\tjmp .L_loop_start_%d";
}

static void gen_expr(ASTNode *node);
static void gen_addr(ASTNode *node);
static int gen_stmt(ASTNode *node);
static int gen_stmt_one(ASTNode *node);
static void emit_binop(BinaryOp op, int uns);

static void emit_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

/* ------------------------------------------------------------------ */
/* String pool (ROADMAP Phase 4)                                       */
/* ------------------------------------------------------------------ */

/* String literals become .rodata entries. Codegen discovers them while walking
 * expressions, so their contents are collected here and the whole section is
 * emitted once at the end of the file. */
static char **str_pool;
static int str_count;
static int str_cap;

static int intern_string(const char *text)
{
    /* Identical literals share one entry: repeated format strings then cost
     * one rodata line, not one per use. */
    for (int i = 0; i < str_count; i++)
    {
        if (strcmp(str_pool[i], text) == 0)
        {
            return i;
        }
    }
    if (str_count == str_cap)
    {
        str_cap = str_cap ? str_cap * 2 : 16;
        str_pool = xrealloc(str_pool, (size_t)str_cap * sizeof(*str_pool));
    }
    str_pool[str_count] = xstrdup(text);
    return str_count++;
}

/* Escape a byte string for the assembler's .string pseudo-op. Non-printable
 * bytes become three-digit octal, so no byte can terminate the literal or
 * confuse the assembler. */
static void emit_string_data(const char *s)
{
    size_t n = strlen(s);
    char *buf = xmalloc(n * 4 + 1);
    size_t w = 0;
    for (size_t i = 0; i < n; i++)
    {
        unsigned char c = (unsigned char)s[i];
        switch (c)
        {
        case '\\':
            buf[w++] = '\\';
            buf[w++] = '\\';
            break;
        case '"':
            buf[w++] = '\\';
            buf[w++] = '"';
            break;
        case '\n':
            buf[w++] = '\\';
            buf[w++] = 'n';
            break;
        case '\t':
            buf[w++] = '\\';
            buf[w++] = 't';
            break;
        case '\r':
            buf[w++] = '\\';
            buf[w++] = 'r';
            break;
        default:
            if (c < 0x20 || c >= 0x7f)
            {
                w += (size_t)snprintf(buf + w, 5, "\\%03o", c);
            }
            else
            {
                buf[w++] = (char)c;
            }
            break;
        }
    }
    buf[w] = '\0';
    emit_line("\t.string \"%s\"", buf);
    free(buf);
}

static void gen_strings(void)
{
    if (str_count == 0)
    {
        return;
    }
    /* COFF and ELF both understand `.rodata`. Mach-O does not: it wants an
     * explicit segment,section pair, and string literals belong in the
     * cstring section, which the linker coalesces across object files. */
    if (target == TARGET_DARWIN)
    {
        emit_line(".section __TEXT,__cstring,cstring_literals");
    }
    else
    {
        emit_line(".section .rodata");
    }
    for (int i = 0; i < str_count; i++)
    {
        emit_line(".L.str.%d:", i);
        emit_string_data(str_pool[i]);
    }
    emit_line(".text");
}

/* ------------------------------------------------------------------ */
/* Expressions                                                        */
/* ------------------------------------------------------------------ */

/* Mnemonic that loads a value of `t` into %rax: 1-byte objects are loaded
 * into the full 64-bit register, sign-extended for `char` and zero-extended
 * for `unsigned char` (which is what makes `unsigned char` arithmetic match
 * C's integer promotion). Everything else is a plain 64-bit load. */
static const char *load_op_for(const Type *t, int is_char)
{
    if (!is_char)
    {
        return "movq";
    }
    /* t is NULL when only the coarse vtype is known; fall back to the
     * historical sign-extending load in that case. */
    return (t != NULL && t->is_unsigned) ? "movzbq" : "movsbq";
}

static void emit_var_load(ASTNode *node)
{
    int is_char = (node->val_type && node->val_type->kind == TYPE_CHAR) ||
                  (node->vtype == TYPE_CHAR);
    const char *op = load_op_for(node->val_type, is_char);
    if (node->is_global)
    {
        emit_line("\t%s %s(%%rip), %%rax", op, sym_name(node->name));
    }
    else
    {
        emit_line("\t%s %d(%%rbp), %%rax", op, node->stack_offset);
    }
}

static void emit_var_store(ASTNode *target)
{
    int is_char = (target->val_type && target->val_type->kind == TYPE_CHAR) ||
                  (target->vtype == TYPE_CHAR);
    const char *reg = is_char ? "%al" : "%rax";
    const char *mnemonic = is_char ? "movb" : "movq";
    if (target->is_global)
    {
        emit_line("\t%s %s, %s(%%rip)", mnemonic, reg, sym_name(target->name));
    }
    else
    {
        emit_line("\t%s %s, %d(%%rbp)", mnemonic, reg, target->stack_offset);
    }
}

static void gen_binary(ASTNode *node)
{
    /* Short-circuit logical operators never evaluate the right operand
     * when the left operand decides the result. */
    if (node->op == OP_AND || node->op == OP_OR)
    {
        int id = label_counter++;
        if (node->op == OP_AND)
        {
            gen_expr(node->left);
            emit_line("\tcmpq $0, %%rax");
            emit_line("\tje .L_and_false_%d", id);
            gen_expr(node->right);
            emit_line("\tcmpq $0, %%rax");
            emit_line("\tjne .L_and_true_%d", id);
            emit_line(".L_and_false_%d:", id);
            emit_line("\tmovq $0, %%rax");
            emit_line("\tjmp .L_and_end_%d", id);
            emit_line(".L_and_true_%d:", id);
            emit_line("\tmovq $1, %%rax");
            emit_line(".L_and_end_%d:", id);
        }
        else
        {
            gen_expr(node->left);
            emit_line("\tcmpq $0, %%rax");
            emit_line("\tjne .L_or_true_%d", id);
            gen_expr(node->right);
            emit_line("\tcmpq $0, %%rax");
            emit_line("\tjne .L_or_true_%d", id);
            emit_line("\tmovq $0, %%rax");
            emit_line("\tjmp .L_or_end_%d", id);
            emit_line(".L_or_true_%d:", id);
            emit_line("\tmovq $1, %%rax");
            emit_line(".L_or_end_%d:", id);
        }
        return;
    }

    /* The comma operator evaluates its left operand only for its side effects
     * and yields the right operand, so it cannot go through the
     * push/pop-and-combine path below (BUG-007). */
    if (node->op == OP_COMMA)
    {
        gen_expr(node->left);
        gen_expr(node->right);
        return;
    }

    gen_expr(node->left);
    emit_line("\tpushq %%rax");
    pushed_count++;
    gen_expr(node->right);
    emit_line("\tpopq %%rcx"); /* %rcx = left operand, %rax = right */
    pushed_count--;

    /* Decided from the *operand* types (not the result type) so that div,
     * mod, >> and the ordered comparisons pick the right flavour of x86
     * instruction. Must agree exactly with the folding done in semantic.c. */
    emit_binop(node->op,
               binary_is_unsigned(node->left->val_type, node->right->val_type));
}

/* Applies one binary operator to %rcx (left) and %rax (right) and leaves the
 * result in %rax. Split out of gen_binary() so that compound assignment can
 * reuse it after evaluating its lvalue exactly once (BUG-001). */
static void emit_binop(BinaryOp op, int uns)
{
    switch (op)
    {
    case OP_ADD:
        emit_line("\taddq %%rcx, %%rax");
        break;
    case OP_SUB:
        emit_line("\tsubq %%rax, %%rcx");
        emit_line("\tmovq %%rcx, %%rax");
        break;
    case OP_MUL:
        emit_line("\timulq %%rcx, %%rax");
        break;
    case OP_DIV:
    case OP_MOD:
        /* %r10 is caller-saved scratch, so no callee-saved register is
         * clobbered (the old code used %rbx, which the ABI requires a
         * function to preserve). Signed division sign-extends the dividend
         * into %rdx:%rax with cqto; unsigned division requires %rdx to be
         * *cleared* instead, and uses divq. */
        emit_line("\tmovq %%rax, %%r10"); /* %r10 = divisor */
        emit_line("\tmovq %%rcx, %%rax"); /* %rax = dividend */
        if (uns)
        {
            emit_line("\txorq %%rdx, %%rdx");
            emit_line("\tdivq %%r10");
        }
        else
        {
            emit_line("\tcqto"); /* sign-extend %rax into %rdx:%rax */
            emit_line("\tidivq %%r10");
        }
        if (op == OP_MOD)
        {
            emit_line("\tmovq %%rdx, %%rax"); /* %rdx = remainder */
        }
        break;
    case OP_BIT_AND:
        emit_line("\tandq %%rcx, %%rax");
        break;
    case OP_BIT_OR:
        emit_line("\torq %%rcx, %%rax");
        break;
    case OP_BIT_XOR:
        emit_line("\txorq %%rcx, %%rax");
        break;
    case OP_SHL:
        /* %rcx = value (left), %rax = shift count (right). */
        emit_line("\tmovq %%rcx, %%r11");
        emit_line("\tmovq %%rax, %%rcx");
        emit_line("\tmovq %%r11, %%rax");
        emit_line("\tshlq %%cl, %%rax");
        break;
    case OP_SHR:
        emit_line("\tmovq %%rcx, %%r11");
        emit_line("\tmovq %%rax, %%rcx");
        emit_line("\tmovq %%r11, %%rax");
        /* sarq fills with the sign bit, shrq with zeros: `>>` is an
         * arithmetic shift on signed values and a logical one on unsigned. */
        emit_line(uns ? "\tshrq %%cl, %%rax" : "\tsarq %%cl, %%rax");
        break;
    case OP_AND:
    case OP_OR:
        break; /* handled above */
    case OP_COMMA:
        break; /* handled by the callers before operands are pushed */
    case OP_EQ:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line("\tsete %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_NE:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line("\tsetne %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_LT:
        emit_line("\tcmpq %%rax, %%rcx");
        /* setb/setbe/seta/setae are the unsigned counterparts of
         * setl/setle/setg/setge; pointers and `unsigned int` need them. */
        emit_line(uns ? "\tsetb %%al" : "\tsetl %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_LE:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line(uns ? "\tsetbe %%al" : "\tsetle %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_GT:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line(uns ? "\tseta %%al" : "\tsetg %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_GE:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line(uns ? "\tsetae %%al" : "\tsetge %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    }
}

/* Emits a store of %rax through `addr_reg`, using the width of `t`. */
static void emit_store_indirect(const char *addr_reg, const Type *t, int vtype)
{
    int is_char = (t != NULL && t->kind == TYPE_CHAR) || vtype == TYPE_CHAR;
    if (is_char)
    {
        emit_line("\tmovb %%al, (%s)", addr_reg);
    }
    else
    {
        emit_line("\tmovq %%rax, (%s)", addr_reg);
    }
}

/* Emits `lvalue op= value` for an lvalue whose address is worth computing only
 * once. The address is formed, the old value loaded through it, the right
 * operand evaluated, and the result stored back. Without this,
 * `a[i++] += 5` evaluated `a[i++]` twice and advanced `i` twice (BUG-001). */
static void gen_compound_assign(ASTNode *node)
{
    ASTNode *target = node->left;
    ASTNode *binop = node->right;
    int is_char = (target->val_type != NULL &&
                   target->val_type->kind == TYPE_CHAR) ||
                  target->vtype == TYPE_CHAR;

    gen_addr(target); /* %rax = address of the lvalue */
    emit_line("\tpushq %%rax");
    pushed_count++;

    emit_line("\tmovq (%%rsp), %%rcx");
    emit_line("\t%s (%%rcx), %%rax", load_op_for(target->val_type, is_char));
    emit_line("\tpushq %%rax"); /* keep the old value as the left operand */
    pushed_count++;

    gen_expr(binop->right); /* %rax = right operand (already scaled if needed) */

    emit_line("\tpopq %%rcx"); /* %rcx = old value */
    pushed_count--;
    emit_binop(binop->op, binary_is_unsigned(target->val_type,
                                             binop->right->val_type));

    emit_line("\tpopq %%rcx"); /* %rcx = address */
    pushed_count--;
    emit_store_indirect("%rcx", target->val_type, target->vtype);
    /* %rax already holds the new value, which is the value of the assignment. */
}

static void gen_assign(ASTNode *node)
{
    /* Struct assignment copies the whole object, not one 8-byte slot.
     * `rep movsb` needs %rdi (destination), %rsi (source) and %rcx (count);
     * all three are caller-saved and none holds a live value here. The string
     * direction flag is clear per the ABI, and nothing in this compiler sets
     * it (BUG-003). */
    if (node->left->val_type != NULL &&
        node->left->val_type->kind == TYPE_STRUCT)
    {
        int size = node->left->val_type->size;

        /* Windows keeps %rsi/%rdi across calls, so a struct copy must preserve
         * them for the function's caller. Save them *first*, before they are
         * loaded with the copy addresses; two 8-byte pushes keep the stack
         * alignment unchanged. */
        int saved = 0;
        if (target == TARGET_WINDOWS)
        {
            emit_line("\tpushq %%rdi");
            emit_line("\tpushq %%rsi");
            pushed_count += 2;
            saved = 1;
        }

        gen_addr(node->left);
        emit_line("\tpushq %%rax"); /* destination; also the result value */
        pushed_count++;

        if (node->right->type == AST_ASSIGN)
        {
            /* `c = (b = a)`: an inner struct assignment leaves the address of
             * its destination in %rax, which is this copy's source. */
            gen_assign(node->right);
            emit_line("\tmovq %%rax, %%rsi");
        }
        else
        {
            gen_addr(node->right);
            emit_line("\tmovq %%rax, %%rsi");
        }

        emit_line("\tmovq (%%rsp), %%rdi");
        emit_line("\tmovq $%d, %%rcx", size);
        emit_line("\trep movsb");
        emit_line("\tpopq %%rax");
        pushed_count--;
        if (saved)
        {
            emit_line("\tpopq %%rsi");
            emit_line("\tpopq %%rdi");
            pushed_count -= 2;
        }
        return;
    }

    /* `x += e` was desugared by the parser into `x = x + e`, sharing one node
     * for the lvalue. Generating that expression would evaluate the lvalue
     * twice; for `a[i++] += 5` that increments `i` twice. Detecting the shape
     * here keeps the parser and the AST free of a compound-assignment node,
     * while a hand-written `x = x + 1` has two distinct nodes and is
     * unaffected. A plain variable is exempt because reading it twice cannot
     * have a side effect (BUG-001). */
    if (node->left->type != AST_VAR_REF && node->right->type == AST_BINARY_OP &&
        node->right->left == node->left && node->right->op != OP_COMMA &&
        node->right->op != OP_AND && node->right->op != OP_OR)
    {
        gen_compound_assign(node);
        return;
    }

    if (node->left->type == AST_VAR_REF)
    {
        gen_expr(node->right);
        emit_var_store(node->left);
    }
    else if (node->left->type == AST_DEREF ||
             node->left->type == AST_MEMBER_ACCESS)
    {
        /* Evaluate value and save it */
        gen_expr(node->right);
        emit_line("\tpushq %%rax");
        pushed_count++;

        /* Compute destination pointer address into %rax */
        gen_addr(node->left);
        emit_line("\tmovq %%rax, %%rcx"); /* %rcx = destination address */

        /* Restore value to store */
        emit_line("\tpopq %%rax");
        pushed_count--;

        emit_store_indirect("%rcx", node->left->val_type, node->left->vtype);
    }
}

static void gen_postfix(ASTNode *node)
{
    /* `p++` on a pointer advances by the size of the pointed-to object, not
     * by one byte. `step` is that element size (1 for non-pointers). */
    int step = 1;
    if (type_is_pointer(node->left->val_type) && node->left->val_type->base != NULL &&
        node->left->val_type->base->size > 0)
    {
        step = node->left->val_type->base->size;
    }

    if (node->left->type == AST_VAR_REF)
    {
        gen_expr(node->left);       /* %rax = old value */
        emit_line("\tpushq %%rax"); /* keep old value */
        pushed_count++;
        if (node->op == OP_ADD)
        {
            emit_line("\taddq $%d, %%rax", step);
        }
        else
        {
            emit_line("\tsubq $%d, %%rax", step);
        }
        emit_var_store(node->left); /* variable gets new value */
        emit_line("\tpopq %%rax");  /* result is the old value */
        pushed_count--;
        return;
    }

    /* Dereference / member lvalue (e.g. (*p)++ or a[i]--): compute the address
     * once and read-modify-write through it. The old code evaluated the lvalue
     * first and recomputed its address afterwards, which ran the index
     * expression twice: `a[i++]--` bumped `i` twice and stored through the
     * wrong address (BUG-002). */
    int is_char =
        (node->left->val_type != NULL &&
         node->left->val_type->kind == TYPE_CHAR) ||
        node->left->vtype == TYPE_CHAR;

    gen_addr(node->left);       /* %rax = address of the lvalue */
    emit_line("\tpushq %%rax"); /* [address] */
    pushed_count++;
    emit_line("\tmovq (%%rsp), %%rcx");
    emit_line("\t%s (%%rcx), %%rax", load_op_for(node->left->val_type, is_char));
    emit_line("\tpushq %%rax"); /* [old value, address] */
    pushed_count++;

    if (node->op == OP_ADD)
    {
        emit_line("\taddq $%d, %%rax", step);
    }
    else
    {
        emit_line("\tsubq $%d, %%rax", step);
    }

    emit_line("\tmovq 8(%%rsp), %%rcx"); /* %rcx = address; old value stays on top */
    emit_store_indirect("%rcx", node->left->val_type, node->left->vtype);

    emit_line("\tpopq %%rax"); /* result is the old value */
    pushed_count--;
    emit_line("\taddq $8, %%rsp"); /* drop the address slot */
    pushed_count--;
}

static void gen_ternary(ASTNode *node)
{
    int id = label_counter++;
    gen_expr(node->cond);
    emit_line("\tcmpq $0, %%rax");
    emit_line("\tje .L_tern_else_%d", id);
    gen_expr(node->left); /* then-branch */
    emit_line("\tjmp .L_tern_end_%d", id);
    emit_line(".L_tern_else_%d:", id);
    gen_expr(node->right); /* else-branch */
    emit_line(".L_tern_end_%d:", id);
}

static void gen_call(ASTNode *node)
{
    ASTNode *args[32];
    int n = 0;
    for (ASTNode *a = node->args; a != NULL && n < 32; a = a->next)
    {
        args[n++] = a;
    }

    int nreg = arg_reg_count();
    CallLayout lay;
    lay.stack_args = n > nreg ? n - nreg : 0;
    lay.shadow = (target == TARGET_WINDOWS) ? 4 : 0; /* 32 bytes, in units */
    lay.padded = 0;

    /* %rsp must be 16-byte aligned at the call. Everything that will still
     * be on the stack at that moment counts: the arguments that do not fit in
     * registers plus the Windows home area. The pad goes in first so it ends
     * up below the stack arguments and is never mistaken for one. */
    if ((pushed_count + lay.stack_args + lay.shadow) & 1)
    {
        emit_line("\tsubq $8, %%rsp");
        pushed_count++;
        lay.padded = 1;
    }

    /* Evaluate arguments right-to-left so that nested calls do not clobber
     * argument registers already filled, pushing each result on the stack. */
    for (int i = n - 1; i >= 0; i--)
    {
        gen_expr(args[i]);
        emit_line("\tpushq %%rax");
        pushed_count++;
    }

    /* Pop the first nreg values into their registers. What remains on the
     * stack is already in the order the callee expects: args[nreg] nearest
     * to %rsp, then args[nreg+1], and so on. */
    int moved = n < nreg ? n : nreg;
    for (int i = 0; i < moved; i++)
    {
        emit_line("\tpopq %s", arg_reg(i));
        pushed_count--;
    }

    /* Windows: 32 bytes of home space, immediately below the stack arguments
     * and therefore just above the return address at the callee. */
    if (lay.shadow)
    {
        emit_line("\tsubq $32, %%rsp");
        pushed_count += lay.shadow;
    }

    /* System V uses %al to carry the number of vector registers used by a
     * variadic call; printf reads it. Zero is correct for every call mycc
     * makes, and non-variadic callees ignore it. */
    if (target != TARGET_WINDOWS && target != TARGET_DARWIN)
    {
        emit_line("\tmovb $0, %%al");
    }

    emit_line("\tcall %s", call_target(node->name));

    int cleanup = lay.stack_args + lay.shadow + (lay.padded ? 1 : 0);
    if (cleanup > 0)
    {
        emit_line("\taddq $%d, %%rsp", cleanup * 8);
        pushed_count -= cleanup;
    }
}

static void gen_addr(ASTNode *node)
{
    if (node->type == AST_VAR_REF)
    {
        if (node->is_global)
        {
            emit_line("\tleaq %s(%%rip), %%rax", sym_name(node->name));
        }
        else
        {
            emit_line("\tleaq %d(%%rbp), %%rax", node->stack_offset);
        }
    }
    else if (node->type == AST_DEREF)
    {
        /* The value of *p's child expression is already the address */
        gen_expr(node->left);
    }
    else if (node->type == AST_MEMBER_ACCESS)
    {
        /* Compute address of struct object into %rax */
        gen_addr(node->left);
        /* Add member offset */
        if (node->stack_offset > 0)
        {
            emit_line("\taddq $%d, %%rax", node->stack_offset);
        }
    }
    else
    {
        report_error(node->line, node->col, "cannot compute address of non-lvalue");
    }
}

static void gen_expr(ASTNode *node)
{
    switch (node->type)
    {
    case AST_INT_LIT:
        emit_line("\tmovq $%lld, %%rax", node->value);
        break;
    case AST_STRING:
    {
        /* The value of a string literal is the address of its first byte. */
        int idx = intern_string(node->name != NULL ? node->name : "");
        node->value = idx;
        emit_line("\tleaq .L.str.%d(%%rip), %%rax", idx);
        break;
    }
    case AST_VAR_REF:
        if (node->vtype == TYPE_ARRAY)
        {
            gen_addr(node);
        }
        else
        {
            emit_var_load(node);
        }
        break;
    case AST_ADDR_OF:
        gen_addr(node->left);
        break;
    case AST_DEREF:
    {
        gen_expr(node->left); /* %rax = memory address */
        /* An array-typed dereference -- `a[i]` of a 2-D array, which
         * desugars to `*(a + i)` -- denotes that row's *address*, and
         * gen_expr(node->left) has just produced it. Loading would instead
         * read the row's first element. */
        if (node->val_type && node->val_type->kind == TYPE_ARRAY)
        {
            break;
        }
        int is_char = (node->val_type && node->val_type->kind == TYPE_CHAR) ||
                      (node->vtype == TYPE_CHAR);
        emit_line("\t%s (%%rax), %%rax", load_op_for(node->val_type, is_char));
        break;
    }
    case AST_BINARY_OP:
        gen_binary(node);
        break;
    case AST_CAST:
        /* Values are 64-bit in registers, so only a cast *to* a 1-byte type
         * changes anything: it must narrow the value the way a store would
         * (sign- or zero-extend the low byte back into %rax). Casts between
         * ints, unsigned ints and pointers are no-ops here (BUG-006). */
        gen_expr(node->left);
        if (node->val_type != NULL && node->val_type->kind == TYPE_CHAR)
        {
            emit_line(node->val_type->is_unsigned ? "\tmovzbq %%al, %%rax"
                                                  : "\tmovsbq %%al, %%rax");
        }
        break;
    case AST_TERNARY:
        gen_ternary(node);
        break;
    case AST_ASSIGN:
        gen_assign(node);
        break;
    case AST_POSTFIX:
        gen_postfix(node);
        break;
    case AST_CALL:
        gen_call(node);
        break;
    case AST_MEMBER_ACCESS:
    {
        gen_addr(node);
        int is_char = (node->val_type && node->val_type->kind == TYPE_CHAR) ||
                      (node->vtype == TYPE_CHAR);
        emit_line("\t%s (%%rax), %%rax", load_op_for(node->val_type, is_char));
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Statements                                                         */
/* ------------------------------------------------------------------ */

static void gen_if(ASTNode *node)
{
    int id = label_counter++;
    gen_expr(node->cond);
    emit_line("\tcmpq $0, %%rax");
    if (node->right != NULL)
    {
        emit_line("\tje .L_else_%d", id);
        gen_stmt(node->body);
        emit_line("\tjmp .L_end_%d", id);
        emit_line(".L_else_%d:", id);
        gen_stmt(node->right);
        emit_line(".L_end_%d:", id);
    }
    else
    {
        emit_line("\tje .L_end_%d", id);
        gen_stmt(node->body);
        emit_line(".L_end_%d:", id);
    }
}

static void gen_while(ASTNode *node)
{
    if (!ctx_push_ok())
    {
        return;
    }
    int id = label_counter++;
    break_kind[break_sp] = CTX_WHILE;
    break_id[break_sp++] = id; /* break -> .L_loop_end */
    continue_kind[continue_sp] = CTX_WHILE;
    continue_id[continue_sp++] = id; /* continue -> .L_loop_start */
    emit_line(".L_loop_start_%d:", id);
    gen_expr(node->cond);
    emit_line("\tcmpq $0, %%rax");
    emit_line("\tje .L_loop_end_%d", id);
    gen_stmt(node->body);
    emit_line("\tjmp .L_loop_start_%d", id);
    emit_line(".L_loop_end_%d:", id);
    break_sp--;
    continue_sp--;
}

static void gen_do_while(ASTNode *node)
{
    if (!ctx_push_ok())
    {
        return;
    }
    int id = label_counter++;
    break_kind[break_sp] = CTX_DO;
    break_id[break_sp++] = id; /* break -> .L_do_end */
    continue_kind[continue_sp] = CTX_DO;
    continue_id[continue_sp++] = id; /* continue -> .L_do_cont */
    emit_line(".L_do_start_%d:", id);
    gen_stmt(node->body);
    emit_line(".L_do_cont_%d:", id);
    gen_expr(node->cond);
    emit_line("\tcmpq $0, %%rax");
    emit_line("\tjne .L_do_start_%d", id);
    emit_line(".L_do_end_%d:", id);
    break_sp--;
    continue_sp--;
}

static void gen_for(ASTNode *node)
{
    if (!ctx_push_ok())
    {
        return;
    }
    int id = label_counter++;
    if (node->left != NULL)
    { /* init */
        gen_stmt(node->left);
    }
    break_kind[break_sp] = CTX_FOR;
    break_id[break_sp++] = id; /* break -> .L_for_end */
    continue_kind[continue_sp] = CTX_FOR;
    continue_id[continue_sp++] = id; /* continue -> .L_for_cont (step) */
    emit_line(".L_for_start_%d:", id);
    if (node->cond != NULL)
    {
        gen_expr(node->cond);
        emit_line("\tcmpq $0, %%rax");
        emit_line("\tje .L_for_end_%d", id);
    }
    gen_stmt(node->body);
    emit_line(".L_for_cont_%d:", id);
    if (node->right != NULL)
    { /* step */
        gen_stmt(node->right);
    }
    emit_line("\tjmp .L_for_start_%d", id);
    emit_line(".L_for_end_%d:", id);
    break_sp--;
    continue_sp--;
}

static void gen_switch(ASTNode *node)
{
    if (!ctx_push_ok())
    {
        return;
    }
    int id = label_counter++;

    /* Case / default labels may be nested inside blocks and nested control
     * flow, so collect them recursively. A nested switch owns its own labels
     * and pushes its own id, so it is not collected here. */
    ASTNode **cases = NULL;
    int ncases = 0;
    int cap = 0;
    collect_switch_cases(node->body, &cases, &ncases, &cap);

    int index = 0;
    int has_default = 0;
    for (int i = 0; i < ncases; i++)
    {
        if (cases[i]->type == AST_CASE)
        {
            cases[i]->case_index = index++;
        }
        else if (cases[i]->type == AST_DEFAULT)
        {
            has_default = 1;
        }
    }

    gen_expr(node->cond);

    break_kind[break_sp] = CTX_SWITCH;
    break_id[break_sp++] = id; /* break -> .L_switch_end */
    switch_id[switch_sp++] = id;

    for (int i = 0; i < ncases; i++)
    {
        if (cases[i]->type == AST_CASE)
        {
            emit_line("\tmovq $%lld, %%r11", cases[i]->value);
            emit_line("\tcmpq %%r11, %%rax");
            emit_line("\tje .L_case_%d_%d", id, cases[i]->case_index);
        }
    }
    if (has_default)
    {
        emit_line("\tjmp .L_default_%d", id);
    }
    else
    {
        emit_line("\tjmp .L_switch_end_%d", id);
    }

    /* Labels are emitted at their source position by gen_stmt_one (which
     * handles nested cases too); here we only walk the body in order. */
    for (ASTNode *item = node->body; item != NULL; item = item->next)
    {
        gen_stmt_one(item);
    }
    emit_line(".L_switch_end_%d:", id);

    switch_sp--;
    break_sp--;
    free(cases);
}

static void gen_return(ASTNode *node)
{
    if (node->left != NULL)
    {
        gen_expr(node->left);
    }
    emit_line("\tleave");
    emit_line("\tret");
}

static int gen_stmt_one(ASTNode *n)
{
    switch (n->type)
    {
    case AST_BLOCK:
        return gen_stmt(n->body);
    case AST_VAR_DECL:
        /* space reserved by the function prologue */
        return 0;
    case AST_IF:
        gen_if(n);
        return 0;
    case AST_WHILE:
        gen_while(n);
        return 0;
    case AST_DO_WHILE:
        gen_do_while(n);
        return 0;
    case AST_FOR:
        gen_for(n);
        return 0;
    case AST_SWITCH:
        gen_switch(n);
        return 0;
    case AST_BREAK:
        emit_line(break_label_fmt(break_kind[break_sp - 1]),
                  break_id[break_sp - 1]);
        return 0;
    case AST_CONTINUE:
        emit_line(continue_label_fmt(continue_kind[continue_sp - 1]),
                  continue_id[continue_sp - 1]);
        return 0;
    case AST_RETURN:
        gen_return(n);
        return 1;
    case AST_CASE:
        if (switch_sp > 0)
        {
            emit_line(".L_case_%d_%d:", switch_id[switch_sp - 1], n->case_index);
        }
        return 0;
    case AST_DEFAULT:
        if (switch_sp > 0)
        {
            emit_line(".L_default_%d:", switch_id[switch_sp - 1]);
        }
        return 0;
    default:
        gen_expr(n); /* expression statement */
        return 0;
    }
}

static int gen_stmt(ASTNode *node)
{
    int ends_in_return = 0;
    for (ASTNode *n = node; n != NULL; n = n->next)
    {
        ends_in_return = gen_stmt_one(n);
    }
    return ends_in_return;
}

/* ------------------------------------------------------------------ */
/* Functions and program                                              */
/* ------------------------------------------------------------------ */

static void gen_func(ASTNode *fn)
{
    int frame = fn->frame_size;
    int aligned_frame = (frame + 15) / 16 * 16;

    emit_line(".text");
    emit_line(".globl %s", sym_name(fn->name));
    emit_line("%s:", sym_name(fn->name));
    emit_line("\tpushq %%rbp");
    emit_line("\tmovq %%rsp, %%rbp");
    if (aligned_frame > 0)
    {
        emit_line("\tsubq $%d, %%rsp", aligned_frame);
    }

    /* Copy incoming arguments into their stack slots. The first
     * arg_reg_count() arrive in registers; anything beyond that was passed on
     * the stack by the caller and sits above the return address (and, on
     * Windows, above the 32-byte home area). */
    int i = 0;
    int nreg = arg_reg_count();
    for (ASTNode *p = fn->left; p != NULL; p = p->next, i++)
    {
        if (i < nreg)
        {
            emit_line("\tmovq %s, %d(%%rbp)", arg_reg(i), p->stack_offset);
        }
        else
        {
            emit_line("\tmovq %d(%%rbp), %%rax",
                      stack_arg_base() + 8 * (i - nreg));
            emit_line("\tmovq %%rax, %d(%%rbp)", p->stack_offset);
        }
    }

    if (!gen_stmt(fn->body))
    {
        /* Reaching the closing brace of a function without a return yields 0
         * instead of leaving whatever garbage happened to be in %rax. For
         * main this matches C's implicit "return 0". */
        emit_line("\tmovq $0, %%rax");
        emit_line("\tleave");
        emit_line("\tret");
    }
}

static void gen_data(ASTNode *prog)
{
    int have_data = 0;
    for (ASTNode *d = prog->body; d != NULL; d = d->next)
    {
        if (d->type == AST_VAR_DECL)
        {
            have_data = 1;
            break;
        }
    }
    if (!have_data)
    {
        return;
    }
    emit_line(".data");
    for (ASTNode *d = prog->body; d != NULL; d = d->next)
    {
        if (d->type == AST_VAR_DECL)
        {
            emit_line(".globl %s", sym_name(d->name));
            emit_line("%s:", sym_name(d->name));
            if (d->val_type != NULL &&
                (d->val_type->kind == TYPE_ARRAY ||
                 d->val_type->kind == TYPE_STRUCT))
            {
                /* Aggregates reserve their whole footprint; they are always
                 * zero-initialized (this subset has no aggregate literals). */
                int sz = d->val_type->size;
                emit_line("\t.zero %d", sz > 0 ? sz : 8);
            }
            else if (d->vtype == TYPE_CHAR)
            {
                emit_line("\t.byte %lld", d->value);
            }
            else
            {
                emit_line("\t.quad %lld", d->value);
            }
        }
    }
}

static void gen_program(ASTNode *prog)
{
    emit_line("# Generated by mycc: C-subset compiler for AMD64 AT&T syntax");
    gen_data(prog);
    for (ASTNode *d = prog->body; d != NULL; d = d->next)
    {
        if (d->type == AST_FUNC_DECL && d->body!=NULL)
        {
            gen_func(d);
        }
    }
    /* String literals are discovered while generating functions, so the
     * .rodata section goes out last. */
    gen_strings();
    /* ELF-only: marks the stack as non-executable on Linux. GNU as on
     * Windows (COFF/PE) does not understand this directive, and Mach-O uses
     * its own section naming, so skip it for both. */
    if (target != TARGET_WINDOWS && target != TARGET_DARWIN)
    {
        emit_line(".section .note.GNU-stack,\"\",@progbits");
    }
}

int codegen_generate(ASTNode *root, FILE *output, TargetKind target_kind)
{
    if (root == NULL || root->type != AST_PROGRAM)
    {
        report_error(0, 0, "internal: code generation requires a program node");
        return 1;
    }
    out = output;
    label_counter = 0;
    pushed_count = 0;
    break_sp = 0;
    continue_sp = 0;
    switch_sp = 0;
    target = target_kind;

    /* Remember which functions this file defines: a call to one of them is a
     * direct call, while everything else is an external symbol that has to go
     * through the PLT on ELF. */
    str_count = 0;
    defined_count = 0;
    for (ASTNode *d = root->body; d != NULL; d = d->next)
    {
        if (d->type == AST_FUNC_DECL && d->body != NULL &&
            defined_count < MAX_DEFINED)
        {
            defined_names[defined_count++] = d->name;
        }
    }

    gen_program(root);
    return error_count() > 0 ? 1 : 0;
}
