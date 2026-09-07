#include <string.h>

#include "semantic.h"
#include "symtab.h"
#include "util.h"

static void sem_decl_pass(ASTNode *prog);
static void check_function(ASTNode *fn);
static void check_stmt(ASTNode *stmt, int *frame);
static void check_expr(ASTNode *expr);
static void check_call(ASTNode *call);
static Symbol *resolve_var(ASTNode *node);
static int count_params(ASTNode *fn);
static int count_args(ASTNode *call);

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
            Symbol *sym = symbol_insert(d->name, SYMBOL_TYPE_INT, 1);
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
            Symbol *sym = symbol_insert(d->name, SYMBOL_TYPE_INT, 1);
            sym->is_global = 1;
            d->is_global = 1;
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
    int argc = count_args(call);
    if (argc != sym->param_count) {
        report_error(call->line, call->col,
                     "function '%s' expects %d argument(s), got %d",
                     call->name, sym->param_count, argc);
    }
    for (ASTNode *a = call->args; a != NULL; a = a->next) {
        check_expr(a);
    }
}

static void check_expr(ASTNode *expr)
{
    if (expr == NULL) {
        return;
    }
    switch (expr->type) {
    case AST_INT_LIT:
        break;
    case AST_VAR_REF:
        resolve_var(expr);
        break;
    case AST_BINARY_OP:
        check_expr(expr->left);
        check_expr(expr->right);
        break;
    case AST_ASSIGN:
        if (expr->left->type != AST_VAR_REF) {
            report_error(expr->line, expr->col,
                         "left side of '=' must be a variable");
        } else {
            resolve_var(expr->left);
        }
        check_expr(expr->right);
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

static void check_stmt(ASTNode *stmt, int *frame)
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
                Symbol *sym = symbol_insert(n->name, SYMBOL_TYPE_INT, 0);
                *frame += 8;
                sym->stack_offset = -(*frame);
                n->stack_offset = -(*frame);
            }
            break;
        }
        case AST_ASSIGN:
        case AST_BINARY_OP:
        case AST_VAR_REF:
        case AST_INT_LIT:
        case AST_CALL:
        case AST_POSTFIX:
            check_expr(n);
            break;
        case AST_RETURN:
            check_expr(n->left);
            break;
        case AST_IF:
            check_expr(n->cond);
            check_stmt(n->body, frame);
            check_stmt(n->right, frame); /* else branch */
            break;
        case AST_WHILE:
            check_expr(n->cond);
            check_stmt(n->body, frame);
            break;
        case AST_FOR:
            if (n->left != NULL) { /* init: declaration or expression */
                check_stmt(n->left, frame);
            }
            check_expr(n->cond);
            check_expr(n->right); /* step */
            check_stmt(n->body, frame);
            break;
        case AST_BLOCK:
            scope_push();
            check_stmt(n->body, frame);
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
    int frame = 0;
    for (ASTNode *p = fn->left; p != NULL; p = p->next) {
        if (symbol_lookup_current(p->name) != NULL) {
            report_error(p->line, p->col, "duplicate parameter '%s'", p->name);
            continue;
        }
        Symbol *sym = symbol_insert(p->name, SYMBOL_TYPE_INT, 0);
        sym->is_param = 1;
        frame += 8;
        sym->stack_offset = -frame;
        p->stack_offset = -frame;
    }

    check_stmt(fn->body, &frame);
    fn->frame_size = frame;

    scope_pop();
}

int semantic_analyze(ASTNode *root)
{
    if (root == NULL || root->type != AST_PROGRAM) {
        report_error(0, 0, "internal: semantic analysis requires a program node");
        return 1;
    }

    symtab_init();
    sem_decl_pass(root);
    for (ASTNode *d = root->body; d != NULL; d = d->next) {
        if (d->type == AST_FUNC_DECL) {
            check_function(d);
        }
    }
    return error_count() > 0 ? 1 : 0;
}
