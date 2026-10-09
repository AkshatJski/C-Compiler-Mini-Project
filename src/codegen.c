#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "codegen.h"
#include "util.h"

#define MAX_NEST 256

static const char *const ARG_REGS[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

static FILE *out;
static int label_counter;
static int pushed_count;  /* number of 8-byte temporaries currently on %rsp */
static int target_windows; /* when set, omit ELF-only directives */

/* Break/continue target stacks. Loops push both; a switch pushes only a
 * break target, so 'continue' inside a switch still finds the loop. Each
 * frame remembers the kind of construct so the right label name is used. */
typedef enum {
    CTX_WHILE,
    CTX_DO,
    CTX_FOR,
    CTX_SWITCH
} CtxKind;

static CtxKind break_kind[MAX_NEST];
static int break_id[MAX_NEST];
static CtxKind continue_kind[MAX_NEST];
static int continue_id[MAX_NEST];
static int break_sp;
static int continue_sp;

static const char *break_label_fmt(CtxKind kind)
{
    switch (kind) {
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
    switch (kind) {
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
static int gen_stmt(ASTNode *node);
static int gen_stmt_one(ASTNode *node);

static void emit_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

/* ------------------------------------------------------------------ */
/* Expressions                                                        */
/* ------------------------------------------------------------------ */

static void emit_var_load(ASTNode *node)
{
    const char *op = node->vtype == TYPE_CHAR ? "movsbq" : "movq";
    if (node->is_global) {
        emit_line("\t%s %s(%%rip), %%rax", op, node->name);
    } else {
        emit_line("\t%s %d(%%rbp), %%rax", op, node->stack_offset);
    }
}

static void emit_var_store(ASTNode *target)
{
    const char *reg = target->vtype == TYPE_CHAR ? "%al" : "%rax";
    const char *mnemonic = target->vtype == TYPE_CHAR ? "movb" : "movq";
    if (target->is_global) {
        emit_line("\t%s %s, %s(%%rip)", mnemonic, reg, target->name);
    } else {
        emit_line("\t%s %s, %d(%%rbp)", mnemonic, reg, target->stack_offset);
    }
}

static void gen_binary(ASTNode *node)
{
    /* Short-circuit logical operators never evaluate the right operand
     * when the left operand decides the result. */
    if (node->op == OP_AND || node->op == OP_OR) {
        int id = label_counter++;
        if (node->op == OP_AND) {
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
        } else {
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

    gen_expr(node->left);
    emit_line("\tpushq %%rax");
    pushed_count++;
    gen_expr(node->right);
    emit_line("\tpopq %%rcx"); /* %rcx = left operand, %rax = right */
    pushed_count--;

    switch (node->op) {
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
        /* %r10 is caller-saved scratch, so no callee-saved register is
         * clobbered (the old code used %rbx, which the ABI requires a
         * function to preserve). */
        emit_line("\tmovq %%rax, %%r10"); /* %r10 = divisor */
        emit_line("\tmovq %%rcx, %%rax"); /* %rax = dividend */
        emit_line("\tcqto");              /* sign-extend %rax into %rdx:%rax */
        emit_line("\tidivq %%r10");
        break;
    case OP_MOD:
        emit_line("\tmovq %%rax, %%r10"); /* %r10 = divisor */
        emit_line("\tmovq %%rcx, %%rax"); /* %rax = dividend */
        emit_line("\tcqto");
        emit_line("\tidivq %%r10");
        emit_line("\tmovq %%rdx, %%rax"); /* %rdx = remainder */
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
    case OP_SHR:
        /* %rcx = value (left), %rax = shift count (right). */
        emit_line("\tmovq %%rcx, %%r11");
        emit_line("\tmovq %%rax, %%rcx");
        emit_line("\tmovq %%r11, %%rax");
        emit_line(node->op == OP_SHL ? "\tshlq %%cl, %%rax"
                                     : "\tsarq %%cl, %%rax");
        break;
    case OP_AND:
    case OP_OR:
        break; /* handled above */
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
        emit_line("\tsetl %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_LE:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line("\tsetle %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_GT:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line("\tsetg %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    case OP_GE:
        emit_line("\tcmpq %%rax, %%rcx");
        emit_line("\tsetge %%al");
        emit_line("\tmovzbq %%al, %%rax");
        break;
    }
}

static void gen_assign(ASTNode *node)
{
    gen_expr(node->right);
    emit_var_store(node->left);
}

static void gen_postfix(ASTNode *node)
{
    gen_expr(node->left);                 /* %rax = old value */
    emit_line("\tpushq %%rax");           /* keep old value */
    pushed_count++;
    if (node->op == OP_ADD) {
        emit_line("\taddq $1, %%rax");
    } else {
        emit_line("\tsubq $1, %%rax");
    }
    emit_var_store(node->left);           /* variable gets new value */
    emit_line("\tpopq %%rax");            /* result is the old value */
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
    ASTNode *args[6];
    int n = 0;
    for (ASTNode *a = node->args; a != NULL && n < 6; a = a->next) {
        args[n++] = a;
    }

    /* Keep %rsp 16-byte aligned at the point of the call (System V ABI).
     * pushed_count tracks 8-byte units currently on the stack, so it must
     * be even at the call instruction. Pad BEFORE evaluating the arguments:
     * the pad then sits below the pushed args and is never popped as an arg.
     * Tracking the pad in pushed_count keeps nested calls aligned too. */
    int padded = 0;
    if ((pushed_count & 1) != 0) {
        emit_line("\tsubq $8, %%rsp");
        pushed_count++;
        padded = 1;
    }

    /* Evaluate arguments right-to-left so that nested calls do not clobber
     * argument registers already filled, pushing each result on the stack. */
    for (int i = n - 1; i >= 0; i--) {
        gen_expr(args[i]);
        emit_line("\tpushq %%rax");
        pushed_count++;
    }

    for (int i = 0; i < n; i++) {
        emit_line("\tpopq %s", ARG_REGS[i]);
        pushed_count--;
    }
    emit_line("\tcall %s", node->name);

    if (padded) {
        emit_line("\taddq $8, %%rsp");
        pushed_count--;
    }
}

static void gen_expr(ASTNode *node)
{
    switch (node->type) {
    case AST_INT_LIT:
        emit_line("\tmovq $%lld, %%rax", node->value);
        break;
    case AST_VAR_REF:
        emit_var_load(node);
        break;
    case AST_BINARY_OP:
        gen_binary(node);
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
    if (node->right != NULL) {
        emit_line("\tje .L_else_%d", id);
        gen_stmt(node->body);
        emit_line("\tjmp .L_end_%d", id);
        emit_line(".L_else_%d:", id);
        gen_stmt(node->right);
        emit_line(".L_end_%d:", id);
    } else {
        emit_line("\tje .L_end_%d", id);
        gen_stmt(node->body);
        emit_line(".L_end_%d:", id);
    }
}

static void gen_while(ASTNode *node)
{
    int id = label_counter++;
    break_kind[break_sp] = CTX_WHILE;
    break_id[break_sp++] = id;          /* break -> .L_loop_end */
    continue_kind[continue_sp] = CTX_WHILE;
    continue_id[continue_sp++] = id;    /* continue -> .L_loop_start */
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
    int id = label_counter++;
    break_kind[break_sp] = CTX_DO;
    break_id[break_sp++] = id;       /* break -> .L_do_end */
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
    int id = label_counter++;
    if (node->left != NULL) { /* init */
        gen_stmt(node->left);
    }
    break_kind[break_sp] = CTX_FOR;
    break_id[break_sp++] = id;       /* break -> .L_for_end */
    continue_kind[continue_sp] = CTX_FOR;
    continue_id[continue_sp++] = id; /* continue -> .L_for_cont (step) */
    emit_line(".L_for_start_%d:", id);
    if (node->cond != NULL) {
        gen_expr(node->cond);
        emit_line("\tcmpq $0, %%rax");
        emit_line("\tje .L_for_end_%d", id);
    }
    gen_stmt(node->body);
    emit_line(".L_for_cont_%d:", id);
    if (node->right != NULL) { /* step */
        gen_stmt(node->right);
    }
    emit_line("\tjmp .L_for_start_%d", id);
    emit_line(".L_for_end_%d:", id);
    break_sp--;
    continue_sp--;
}

static void gen_switch(ASTNode *node)
{
    int id = label_counter++;
    int index = 0;
    int has_default = 0;

    /* Number the case labels and remember whether a default exists. */
    for (ASTNode *item = node->body; item != NULL; item = item->next) {
        if (item->type == AST_CASE) {
            item->case_index = index++;
        } else if (item->type == AST_DEFAULT) {
            has_default = 1;
        }
    }

    gen_expr(node->cond);

    break_kind[break_sp] = CTX_SWITCH;
    break_id[break_sp++] = id; /* break -> .L_switch_end */
    for (ASTNode *item = node->body; item != NULL; item = item->next) {
        if (item->type == AST_CASE) {
            emit_line("\tmovq $%lld, %%r11", item->value);
            emit_line("\tcmpq %%r11, %%rax");
            emit_line("\tje .L_case_%d_%d", id, item->case_index);
        }
    }
    if (has_default) {
        emit_line("\tjmp .L_default_%d", id);
    } else {
        emit_line("\tjmp .L_switch_end_%d", id);
    }

    for (ASTNode *item = node->body; item != NULL; item = item->next) {
        if (item->type == AST_CASE) {
            emit_line(".L_case_%d_%d:", id, item->case_index);
        } else if (item->type == AST_DEFAULT) {
            emit_line(".L_default_%d:", id);
        } else {
            gen_stmt_one(item);
        }
    }
    emit_line(".L_switch_end_%d:", id);
    break_sp--;
}

static void gen_return(ASTNode *node)
{
    if (node->left != NULL) {
        gen_expr(node->left);
    }
    emit_line("\tleave");
    emit_line("\tret");
}

static int gen_stmt_one(ASTNode *n)
{
    switch (n->type) {
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
    case AST_DEFAULT:
        return 0;
    default:
        gen_expr(n); /* expression statement */
        return 0;
    }
}

static int gen_stmt(ASTNode *node)
{
    int ends_in_return = 0;
    for (ASTNode *n = node; n != NULL; n = n->next) {
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
    emit_line(".globl %s", fn->name);
    emit_line("%s:", fn->name);
    emit_line("\tpushq %%rbp");
    emit_line("\tmovq %%rsp, %%rbp");
    if (aligned_frame > 0) {
        emit_line("\tsubq $%d, %%rsp", aligned_frame);
    }

    /* Copy incoming argument registers into their stack slots. */
    int i = 0;
    for (ASTNode *p = fn->left; p != NULL && i < 6; p = p->next, i++) {
        emit_line("\tmovq %s, %d(%%rbp)", ARG_REGS[i], p->stack_offset);
    }

    if (!gen_stmt(fn->body)) {
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
    for (ASTNode *d = prog->body; d != NULL; d = d->next) {
        if (d->type == AST_VAR_DECL) {
            have_data = 1;
            break;
        }
    }
    if (!have_data) {
        return;
    }
    emit_line(".data");
    for (ASTNode *d = prog->body; d != NULL; d = d->next) {
        if (d->type == AST_VAR_DECL) {
            emit_line(".globl %s", d->name);
            emit_line("%s:", d->name);
            if (d->vtype == TYPE_CHAR) {
                emit_line("\t.byte %lld", d->value);
            } else {
                emit_line("\t.quad %lld", d->value);
            }
        }
    }
}

static void gen_program(ASTNode *prog)
{
    emit_line("# Generated by mycc: C-subset compiler for AMD64 AT&T syntax");
    gen_data(prog);
    for (ASTNode *d = prog->body; d != NULL; d = d->next) {
        if (d->type == AST_FUNC_DECL) {
            gen_func(d);
        }
    }
    /* ELF-only: marks the stack as non-executable on Linux. GNU as on
     * Windows (COFF/PE) does not understand this directive, so skip it. */
    if (!target_windows) {
        emit_line(".section .note.GNU-stack,\"\",@progbits");
    }
}

int codegen_generate(ASTNode *root, FILE *output, int windows_target)
{
    if (root == NULL || root->type != AST_PROGRAM) {
        report_error(0, 0, "internal: code generation requires a program node");
        return 1;
    }
    out = output;
    label_counter = 0;
    pushed_count = 0;
    break_sp = 0;
    continue_sp = 0;
    target_windows = windows_target;
    gen_program(root);
    return error_count() > 0 ? 1 : 0;
}
