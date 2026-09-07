#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "codegen.h"
#include "util.h"

static const char *const ARG_REGS[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

static FILE *out;
static int label_counter;
static int pushed_count; /* number of 8-byte temporaries currently on %rsp */
static int target_windows; /* when set, omit ELF-only directives */

static void gen_expr(ASTNode *node);
static int gen_stmt(ASTNode *node);

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
    if (node->is_global) {
        emit_line("\tmovq %s(%%rip), %%rax", node->name);
    } else {
        emit_line("\tmovq %d(%%rbp), %%rax", node->stack_offset);
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
        emit_line("\tmovq %%rax, %%rbx"); /* %rbx = divisor */
        emit_line("\tmovq %%rcx, %%rax"); /* %rax = dividend */
        emit_line("\tcqto");               /* sign-extend %rax into %rdx:%rax */
        emit_line("\tidivq %%rbx");
        break;
    case OP_MOD:
        emit_line("\tmovq %%rax, %%rbx"); /* %rbx = divisor */
        emit_line("\tmovq %%rcx, %%rax"); /* %rax = dividend */
        emit_line("\tcqto");
        emit_line("\tidivq %%rbx");
        emit_line("\tmovq %%rdx, %%rax"); /* %rdx = remainder */
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

static void emit_var_store(ASTNode *target)
{
    if (target->is_global) {
        emit_line("\tmovq %%rax, %s(%%rip)", target->name);
    } else {
        emit_line("\tmovq %%rax, %d(%%rbp)", target->stack_offset);
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
    emit_line(".L_loop_start_%d:", id);
    gen_expr(node->cond);
    emit_line("\tcmpq $0, %%rax");
    emit_line("\tje .L_loop_end_%d", id);
    gen_stmt(node->body);
    emit_line("\tjmp .L_loop_start_%d", id);
    emit_line(".L_loop_end_%d:", id);
}

static void gen_for(ASTNode *node)
{
    int id = label_counter++;
    if (node->left != NULL) { /* init */
        gen_stmt(node->left);
    }
    emit_line(".L_for_start_%d:", id);
    if (node->cond != NULL) {
        gen_expr(node->cond);
        emit_line("\tcmpq $0, %%rax");
        emit_line("\tje .L_for_end_%d", id);
    }
    gen_stmt(node->body);
    if (node->right != NULL) { /* step */
        gen_stmt(node->right);
    }
    emit_line("\tjmp .L_for_start_%d", id);
    emit_line(".L_for_end_%d:", id);
}

static void gen_return(ASTNode *node)
{
    if (node->left != NULL) {
        gen_expr(node->left);
    }
    emit_line("\tleave");
    emit_line("\tret");
}

static int gen_stmt(ASTNode *node)
{
    int ends_in_return = 0;
    for (ASTNode *n = node; n != NULL; n = n->next) {
        switch (n->type) {
        case AST_BLOCK:
            ends_in_return = gen_stmt(n->body);
            break;
        case AST_VAR_DECL:
            /* space reserved by the function prologue */
            ends_in_return = 0;
            break;
        case AST_IF:
            gen_if(n);
            ends_in_return = 0;
            break;
        case AST_WHILE:
            gen_while(n);
            ends_in_return = 0;
            break;
        case AST_FOR:
            gen_for(n);
            ends_in_return = 0;
            break;
        case AST_RETURN:
            gen_return(n);
            ends_in_return = 1;
            break;
        default:
            gen_expr(n); /* expression statement */
            ends_in_return = 0;
            break;
        }
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
            emit_line("\t.quad 0");
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
    target_windows = windows_target;
    gen_program(root);
    return error_count() > 0 ? 1 : 0;
}
