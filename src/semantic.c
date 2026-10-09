#include <stdlib.h>
#include <string.h>

#include "semantic.h"
#include "symtab.h"
#include "util.h"

typedef struct {
    int *frame;
    int loop_depth;    /* enclosing loops (for 'continue') */
    int switch_depth;  /* enclosing switches (for 'case'/'default') */
} CheckCtx;

static void sem_decl_pass(ASTNode *prog);
static void check_function(ASTNode *fn);
static void check_returns(ASTNode *fn, ASTNode *stmt);
static void check_stmt(ASTNode *stmt, CheckCtx *ctx);
static void check_switch(ASTNode *node, CheckCtx *ctx);
static void check_expr(ASTNode *expr);
static void check_value(ASTNode *expr);
static void check_call(ASTNode *call);
static Symbol *resolve_var(ASTNode *node);
static int count_params(ASTNode *fn);
static int count_args(ASTNode *call);
static int fold_const(ASTNode *node, long long *out);

/* ------------------------------------------------------------------ */
/* Constant folding (used for global initializers and case labels)     */
/* ------------------------------------------------------------------ */

static int fold_const(ASTNode *node, long long *out)
{
    if (node == NULL) {
        return 0;
    }
    if (node->type == AST_INT_LIT) {
        *out = node->value;
        return 1;
    }
    if (node->type == AST_TERNARY) {
        long long c;
        if (!fold_const(node->cond, &c)) {
            return 0;
        }
        return c ? fold_const(node->left, out) : fold_const(node->right, out);
    }
    if (node->type != AST_BINARY_OP) {
        return 0;
    }
    long long l, r;
    if (!fold_const(node->left, &l) || !fold_const(node->right, &r)) {
        return 0;
    }
    switch (node->op) {
    case OP_ADD:
        *out = l + r;
        return 1;
    case OP_SUB:
        *out = l - r;
        return 1;
    case OP_MUL:
        *out = l * r;
        return 1;
    case OP_DIV:
        if (r == 0) {
            return 0;
        }
        *out = l / r;
        return 1;
    case OP_MOD:
        if (r == 0) {
            return 0;
        }
        *out = l % r;
        return 1;
    case OP_BIT_AND:
        *out = l & r;
        return 1;
    case OP_BIT_OR:
        *out = l | r;
        return 1;
    case OP_BIT_XOR:
        *out = l ^ r;
        return 1;
    case OP_SHL:
        *out = l << r;
        return 1;
    case OP_SHR:
        *out = l >> r;
        return 1;
    case OP_AND:
        *out = (l != 0) && (r != 0);
        return 1;
    case OP_OR:
        *out = (l != 0) || (r != 0);
        return 1;
    case OP_EQ:
        *out = l == r;
        return 1;
    case OP_NE:
        *out = l != r;
        return 1;
    case OP_LT:
        *out = l < r;
        return 1;
    case OP_LE:
        *out = l <= r;
        return 1;
    case OP_GT:
        *out = l > r;
        return 1;
    case OP_GE:
        *out = l >= r;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int count_params(ASTNode *fn)
{
    int n = 0;
    for (ASTNode *p = fn->left; p != NULL; p = p->next) {
        n++;
    }
    return n;
}

static int count_args(ASTNode *call)
{
    int n = 0;
    for (ASTNode *a = call->args; a != NULL; a = a->next) {
        n++;
    }
    return n;
}

static void sem_decl_pass(ASTNode *prog)
{
    for (ASTNode *d = prog->body; d != NULL; d = d->next) {
        if (d->type == AST_FUNC_DECL) {
            if (symbol_lookup_current(d->name) != NULL) {
                report_error(d->line, d->col, "duplicate definition of '%s'",
                             d->name);
                continue;
            }
            Symbol *sym = symbol_insert(d->name, d->vtype, 1);
            sym->is_function = 1;
            sym->param_count = count_params(d);
            if (sym->param_count > 6) {
                report_error(d->line, d->col,
                             "function '%s' has %d parameters; the AMD64 "
                             "register limit is 6",
                             d->name, sym->param_count);
            }
        } else if (d->type == AST_VAR_DECL) {
            if (symbol_lookup_current(d->name) != NULL) {
                report_error(d->line, d->col, "duplicate definition of '%s'",
                             d->name);
                continue;
            }
            Symbol *sym = symbol_insert(d->name, d->vtype, 1);
            (void)sym;
            d->is_global = 1;
            if (d->left != NULL) {
                long long v;
                if (!fold_const(d->left, &v)) {
                    report_error(d->line, d->col,
                                 "initializer for global '%s' is not a "
                                 "constant expression",
                                 d->name);
                } else {
                    d->value = v;
                }
                d->left = NULL;
            }
        } else {
            report_error(d->line, d->col, "unexpected node at top level");
        }
    }
}

static Symbol *resolve_var(ASTNode *node)
{
    Symbol *sym = symbol_lookup(node->name);
    if (sym == NULL) {
        report_error(node->line, node->col, "undeclared variable '%s'",
                     node->name);
        return NULL;
    }
    if (sym->is_function) {
        report_error(node->line, node->col,
                     "'%s' is a function and cannot be used as a variable",
                     node->name);
        return NULL;
    }
    node->stack_offset = sym->stack_offset;
    node->is_global = sym->is_global;
    node->vtype = sym->type;
    return sym;
}

static void check_call(ASTNode *call)
{
    Symbol *sym = symbol_lookup(call->name);
    if (sym == NULL || !sym->is_function) {
        report_error(call->line, call->col, "call to undeclared function '%s'",
                     call->name);
        return;
    }
    call->vtype = sym->type;
    int argc = count_args(call);
    if (argc != sym->param_count) {
        report_error(call->line, call->col,
                     "function '%s' expects %d argument(s), got %d",
                     call->name, sym->param_count, argc);
    }
    for (ASTNode *a = call->args; a != NULL; a = a->next) {
        check_value(a);
    }
}

static void check_value(ASTNode *expr)
{
    check_expr(expr);
    if (expr != NULL && expr->type == AST_CALL && expr->vtype == TYPE_VOID) {
        report_error(expr->line, expr->col,
                     "void function '%s' used where a value is required",
                     expr->name);
    }
}

static void check_expr(ASTNode *expr)
{
    if (expr == NULL) {
        return;
    }
    switch (expr->type) {
    case AST_INT_LIT:
        expr->vtype = TYPE_INT;
        break;
    case AST_VAR_REF:
        resolve_var(expr);
        break;
    case AST_BINARY_OP:
        check_value(expr->left);
        check_value(expr->right);
        expr->vtype = TYPE_INT;
        break;
    case AST_TERNARY:
        check_value(expr->cond);
        check_value(expr->left);
        check_value(expr->right);
        expr->vtype = TYPE_INT;
        break;
    case AST_ASSIGN:
        if (expr->left->type != AST_VAR_REF) {
            report_error(expr->line, expr->col,
                         "left side of '=' must be a variable");
        } else {
            resolve_var(expr->left);
        }
        check_value(expr->right);
        expr->vtype = expr->left->vtype;
        break;
    case AST_CALL:
        check_call(expr);
        break;
    case AST_POSTFIX:
        if (expr->left->type != AST_VAR_REF) {
            report_error(expr->line, expr->col,
                         "'++'/'--' operand must be a variable");
        } else {
            resolve_var(expr->left);
        }
        break;
    default:
        report_error(expr->line, expr->col, "invalid expression");
        break;
    }
}

static void check_switch(ASTNode *node, CheckCtx *ctx)
{
    check_value(node->cond);

    int count = 0;
    for (ASTNode *item = node->body; item != NULL; item = item->next) {
        if (item->type == AST_CASE) {
            count++;
        }
    }
    long long *seen = count > 0 ? xmalloc(sizeof(long long) * (size_t)count)
                                : NULL;
    int nseen = 0;
    int saw_default = 0;

    for (ASTNode *item = node->body; item != NULL; item = item->next) {
        if (item->type == AST_CASE) {
            long long v;
            if (!fold_const(item->left, &v)) {
                report_error(item->line, item->col,
                             "case label is not a constant expression");
            } else {
                for (int i = 0; i < nseen; i++) {
                    if (seen[i] == v) {
                        report_error(item->line, item->col,
                                     "duplicate case value %lld", v);
                    }
                }
                seen[nseen++] = v;
                item->value = v;
            }
            item->left = NULL; /* folded away */
        } else if (item->type == AST_DEFAULT) {
            if (saw_default) {
                report_error(item->line, item->col,
                             "multiple 'default' labels in one switch");
            }
            saw_default = 1;
        }
    }
    free(seen);

    scope_push();
    ctx->switch_depth++;
    check_stmt(node->body, ctx);
    ctx->switch_depth--;
    scope_pop();
}

static void check_stmt(ASTNode *stmt, CheckCtx *ctx)
{
    for (ASTNode *n = stmt; n != NULL; n = n->next) {
        switch (n->type) {
        case AST_VAR_DECL: {
            /* Register the local and allocate its stack slot in one pass so
             * that scoped symbols stay alive for the enclosing block. */
            if (symbol_lookup_current(n->name) != NULL) {
                report_error(n->line, n->col,
                             "duplicate declaration of '%s' in scope",
                             n->name);
            } else {
                Symbol *sym = symbol_insert(n->name, n->vtype, 0);
                *ctx->frame += 8;
                sym->stack_offset = -(*ctx->frame);
                n->stack_offset = -(*ctx->frame);
            }
            break;
        }
        case AST_ASSIGN:
        case AST_BINARY_OP:
        case AST_TERNARY:
        case AST_VAR_REF:
        case AST_INT_LIT:
        case AST_POSTFIX:
            check_expr(n);
            break;
        case AST_CALL:
            check_call(n);
            break;
        case AST_RETURN:
            check_value(n->left);
            break;
        case AST_IF:
            check_value(n->cond);
            check_stmt(n->body, ctx);
            check_stmt(n->right, ctx); /* else branch */
            break;
        case AST_WHILE:
            check_value(n->cond);
            ctx->loop_depth++;
            check_stmt(n->body, ctx);
            ctx->loop_depth--;
            break;
        case AST_DO_WHILE:
            ctx->loop_depth++;
            check_stmt(n->body, ctx);
            ctx->loop_depth--;
            check_value(n->cond);
            break;
        case AST_FOR:
            /* C gives the for-statement its own scope, so a declaration in the
             * init clause is loop-local. */
            scope_push();
            if (n->left != NULL) { /* init: declaration or expression */
                check_stmt(n->left, ctx);
            }
            check_value(n->cond);
            check_value(n->right); /* step */
            ctx->loop_depth++;
            check_stmt(n->body, ctx);
            ctx->loop_depth--;
            scope_pop();
            break;
        case AST_SWITCH:
            check_switch(n, ctx);
            break;
        case AST_CASE:
        case AST_DEFAULT:
            if (ctx->switch_depth == 0) {
                report_error(n->line, n->col,
                             "'case'/'default' label not within a switch");
            }
            break;
        case AST_BREAK:
            if (ctx->loop_depth == 0 && ctx->switch_depth == 0) {
                report_error(n->line, n->col,
                             "'break' statement not within a loop or switch");
            }
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0) {
                report_error(n->line, n->col,
                             "'continue' statement not within a loop");
            }
            break;
        case AST_BLOCK:
            scope_push();
            check_stmt(n->body, ctx);
            scope_pop();
            break;
        default:
            report_error(n->line, n->col, "invalid statement");
            break;
        }
    }
}

static void check_function(ASTNode *fn)
{
    scope_push();

    /* Parameters and locals both live at negative offsets within this
     * function's own frame. Spilling register arguments into the caller's
     * frame (the classic +16(%rbp) layout) is unsafe because the caller
     * may have a zero-sized frame or live temporaries at the call %rsp. */
    CheckCtx ctx;
    ctx.frame = NULL;
    int frame = 0;
    ctx.frame = &frame;
    ctx.loop_depth = 0;
    ctx.switch_depth = 0;

    for (ASTNode *p = fn->left; p != NULL; p = p->next) {
        if (symbol_lookup_current(p->name) != NULL) {
            report_error(p->line, p->col, "duplicate parameter '%s'", p->name);
            continue;
        }
        Symbol *sym = symbol_insert(p->name, p->vtype, 0);
        frame += 8;
        sym->stack_offset = -frame;
        p->stack_offset = -frame;
    }

    check_stmt(fn->body, &ctx);
    check_returns(fn, fn->body);
    fn->frame_size = frame;

    scope_pop();
}

/* Walk the function body checking that return statements agree with the
 * declared return type. */
static void check_returns(ASTNode *fn, ASTNode *stmt)
{
    for (ASTNode *n = stmt; n != NULL; n = n->next) {
        switch (n->type) {
        case AST_RETURN:
            if (fn->vtype == TYPE_VOID && n->left != NULL) {
                report_error(n->line, n->col,
                             "void function '%s' should not return a value",
                             fn->name);
            } else if (fn->vtype != TYPE_VOID && n->left == NULL) {
                report_error(n->line, n->col,
                             "non-void function '%s' must return a value",
                             fn->name);
            }
            break;
        case AST_IF:
            check_returns(fn, n->body);
            check_returns(fn, n->right);
            break;
        case AST_WHILE:
        case AST_DO_WHILE:
        case AST_FOR:
            check_returns(fn, n->body);
            break;
        case AST_SWITCH:
        case AST_BLOCK:
            check_returns(fn, n->body);
            break;
        default:
            break;
        }
    }
}

int semantic_analyze(ASTNode *root)
{
    if (root == NULL || root->type != AST_PROGRAM) {
        report_error(0, 0, "internal: semantic analysis requires a program node");
        return 1;
    }

    symtab_init();
    sem_decl_pass(root);

    int have_main = 0;
    for (ASTNode *d = root->body; d != NULL; d = d->next) {
        if (d->type == AST_FUNC_DECL) {
            if (strcmp(d->name, "main") == 0) {
                have_main = 1;
            }
            check_function(d);
        }
    }
    if (!have_main) {
        report_error(0, 0, "no 'main' function defined");
    }
    return error_count() > 0 ? 1 : 0;
}
