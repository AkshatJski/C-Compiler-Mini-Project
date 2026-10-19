#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "symtab.h"
#include "util.h"

typedef struct
{
    int *frame;
    int loop_depth;
    int switch_depth;
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
static void check_object_complete(ASTNode *decl);

/* ------------------------------------------------------------------ */
/* Constant folding                                                   */
/* ------------------------------------------------------------------ */

/* Rejects objects whose type reserves no storage: incomplete struct types
 * (only forward-declared) and zero-length arrays. Such a local used to get
 * stack_offset 0, i.e. the saved %rbp slot, which crashed on the first
 * store. Every declared object must own at least one real frame slot. */
static void check_object_complete(ASTNode *decl)
{
    Type *t = decl->val_type;
    if (t == NULL)
    {
        return;
    }
    if (t->kind == TYPE_ARRAY && t->array_size <= 0)
    {
        report_error(decl->line, decl->col,
                     "array '%s' must have a positive size", decl->name);
    }
    else if (!type_is_complete(t))
    {
        report_error(decl->line, decl->col,
                     "storage size of '%s' isn't known", decl->name);
    }
}

static int fold_const(ASTNode *node, long long *out)
{
    if (node == NULL)
    {
        return 0;
    }
    if (node->type == AST_INT_LIT)
    {
        *out = node->value;
        return 1;
    }
    if (node->type == AST_TERNARY)
    {
        long long c;
        if (!fold_const(node->cond, &c))
        {
            return 0;
        }
        return c ? fold_const(node->left, out) : fold_const(node->right, out);
    }
    if (node->type != AST_BINARY_OP)
    {
        return 0;
    }
    long long l, r;
    if (!fold_const(node->left, &l) || !fold_const(node->right, &r))
    {
        return 0;
    }
    switch (node->op)
    {
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
        if (r == 0)
        {
            return 0;
        }
        *out = l / r;
        return 1;
    case OP_MOD:
        if (r == 0)
        {
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

static int count_params(ASTNode *fn)
{
    int n = 0;
    for (ASTNode *p = fn->left; p != NULL; p = p->next)
    {
        n++;
    }
    return n;
}

static int count_args(ASTNode *call)
{
    int n = 0;
    for (ASTNode *a = call->args; a != NULL; a = a->next)
    {
        n++;
    }
    return n;
}

static void sem_decl_pass(ASTNode *prog)
{
    for (ASTNode *d = prog->body; d != NULL; d = d->next)
    {
        if (d->type == AST_FUNC_DECL)
        {
            Symbol *existing = symbol_lookup_current(d->name);
            int params = count_params(d);

            if (params > 6)
            {
                report_error(d->line, d->col,
                             "function '%s' has %d parameters; the AMD64 "
                             "register limit is 6",
                             d->name, params);
            }

            if (existing != NULL)
            {
                if (!existing->is_function)
                {
                    report_error(d->line, d->col,
                                 "'%s' redeclared as different kind of symbol",
                                 d->name);
                    continue;
                }
                if (existing->param_count != params)
                {
                    report_error(d->line, d->col,
                                 "conflicting parameter count for '%s'",
                                 d->name);
                    continue;
                }
                if (d->body != NULL)
                {
                    if (existing->is_defined)
                    {
                        report_error(d->line, d->col,
                                     "duplicate definition of '%s'", d->name);
                        continue;
                    }
                    existing->is_defined = 1;
                }
            }
            else
            {
                Symbol *sym = symbol_insert(d->name, d->val_type, 1);
                sym->is_function = 1;
                sym->param_count = params;
                sym->is_defined = (d->body != NULL) ? 1 : 0;
            }
        }
        else if (d->type == AST_VAR_DECL)
        {
            if (symbol_lookup_current(d->name) != NULL)
            {
                report_error(d->line, d->col, "duplicate definition of '%s'",
                             d->name);
                continue;
            }
            Symbol *sym = symbol_insert(d->name, d->val_type, 1);
            (void)sym;
            d->is_global = 1;
            check_object_complete(d);
            if (d->left != NULL)
            {
                long long v;
                if (!fold_const(d->left, &v))
                {
                    report_error(d->line, d->col,
                                 "initializer for global '%s' is not a "
                                 "constant expression",
                                 d->name);
                }
                else
                {
                    d->value = v;
                }
                d->left = NULL;
            }
        }
        else
        {
            report_error(d->line, d->col, "unexpected node at top level");
        }
    }
}

static Symbol *resolve_var(ASTNode *node)
{
    Symbol *sym = symbol_lookup(node->name);
    if (sym == NULL)
    {
        report_error(node->line, node->col, "undeclared variable '%s'",
                     node->name);
        return NULL;
    }
    if (sym->is_function)
    {
        report_error(node->line, node->col,
                     "'%s' is a function and cannot be used as a variable",
                     node->name);
        return NULL;
    }
    node->stack_offset = sym->stack_offset;
    node->is_global = sym->is_global;
    node->vtype = sym->type->kind;
    node->val_type = sym->type;
    return sym;
}

static void check_call(ASTNode *call)
{
    Symbol *sym = symbol_lookup(call->name);
    if (sym == NULL || !sym->is_function)
    {
        report_error(call->line, call->col, "call to undeclared function '%s'",
                     call->name);
        return;
    }
    call->vtype = sym->type->kind;
    call->val_type = sym->type;
    int argc = count_args(call);
    if (argc != sym->param_count)
    {
        report_error(call->line, call->col,
                     "function '%s' expects %d argument(s), got %d",
                     call->name, sym->param_count, argc);
    }
    for (ASTNode *a = call->args; a != NULL; a = a->next)
    {
        check_value(a);
    }
}

static void check_value(ASTNode *expr)
{
    check_expr(expr);
    if (expr != NULL && expr->type == AST_CALL && expr->vtype == TYPE_VOID)
    {
        report_error(expr->line, expr->col,
                     "void function '%s' used where a value is required",
                     expr->name);
    }
}

static void check_expr(ASTNode *expr)
{
    if (expr == NULL)
    {
        return;
    }
    switch (expr->type)
    {
    case AST_INT_LIT:
        expr->vtype = TYPE_INT;
        expr->val_type = type_int();
        break;
    case AST_VAR_REF:
        resolve_var(expr);
        if (expr->val_type && expr->val_type->kind == TYPE_ARRAY)
        {
            /* Array-to-pointer decay */
            expr->val_type = type_ptr(expr->val_type->base);
            expr->vtype = TYPE_ARRAY;
        }
        break;
    case AST_ADDR_OF:
        check_expr(expr->left);
        if (expr->left->type != AST_VAR_REF && expr->left->type != AST_DEREF && expr->left->type != AST_MEMBER_ACCESS)
        {
            report_error(expr->line, expr->col, "cannot take address of non-lvalue");
        }
        expr->val_type = type_ptr(expr->left->val_type ? expr->left->val_type : type_int());
        expr->vtype = TYPE_PTR;
        break;
    case AST_DEREF:
        check_expr(expr->left);
        /* The operand may be a pointer *or* an array expression: `a[i][j]`
         * desugars to `*(a[i] + j)` where the inner `a[i]` is itself a
         * dereference of array type (`int[3]`). Such a dereference denotes
         * the row's address -- the load below is what would be wrong. */
        if (expr->left->val_type == NULL ||
            (expr->left->val_type->kind != TYPE_PTR &&
             expr->left->val_type->kind != TYPE_ARRAY))
        {
            report_error(expr->line, expr->col, "cannot dereference non-pointer type");
            expr->val_type = type_int();
            expr->vtype = TYPE_INT;
        }
        else
        {
            expr->val_type = expr->left->val_type->base;
            if (expr->val_type == NULL || expr->val_type->kind == TYPE_VOID)
            {
                /* e.g. `void *p; *p;` — nothing to load/store through it. */
                report_error(expr->line, expr->col,
                             "cannot dereference a 'void *' pointer");
                expr->val_type = type_int();
                expr->vtype = TYPE_INT;
            }
            else
            {
                expr->vtype = expr->val_type->kind;
            }
        }
        break;
    case AST_BINARY_OP:
        check_value(expr->left);
        check_value(expr->right);

        if (expr->op == OP_ADD)
        {
            if (type_is_pointer(expr->left->val_type) && type_is_integer(expr->right->val_type))
            {
                int scale = expr->left->val_type->base ? expr->left->val_type->base->size : 8;
                if (scale > 1)
                {
                    ASTNode *mul = xcalloc(1, sizeof(ASTNode));
                    mul->type = AST_BINARY_OP;
                    mul->op = OP_MUL;
                    mul->val_type = type_int();
                    mul->left = expr->right;
                    ASTNode *sc = xcalloc(1, sizeof(ASTNode));
                    sc->type = AST_INT_LIT;
                    sc->value = scale;
                    sc->val_type = type_int();
                    mul->right = sc;
                    expr->right = mul;
                }
                expr->val_type = expr->left->val_type;
                expr->vtype = expr->left->vtype;
            }
            else if (type_is_integer(expr->left->val_type) && type_is_pointer(expr->right->val_type))
            {
                int scale = expr->right->val_type->base ? expr->right->val_type->base->size : 8;
                if (scale > 1)
                {
                    ASTNode *mul = xcalloc(1, sizeof(ASTNode));
                    mul->type = AST_BINARY_OP;
                    mul->op = OP_MUL;
                    mul->val_type = type_int();
                    mul->left = expr->left;
                    ASTNode *sc = xcalloc(1, sizeof(ASTNode));
                    sc->type = AST_INT_LIT;
                    sc->value = scale;
                    sc->val_type = type_int();
                    mul->right = sc;
                    expr->left = mul;
                }
                expr->val_type = expr->right->val_type;
                expr->vtype = expr->right->vtype;
            }
            else
            {
                expr->val_type = type_int();
                expr->vtype = TYPE_INT;
            }
        }
        else if (expr->op == OP_SUB)
        {
            if (type_is_pointer(expr->left->val_type) && type_is_integer(expr->right->val_type))
            {
                int scale = expr->left->val_type->base ? expr->left->val_type->base->size : 8;
                if (scale > 1)
                {
                    ASTNode *mul = xcalloc(1, sizeof(ASTNode));
                    mul->type = AST_BINARY_OP;
                    mul->op = OP_MUL;
                    mul->val_type = type_int();
                    mul->left = expr->right;
                    ASTNode *sc = xcalloc(1, sizeof(ASTNode));
                    sc->type = AST_INT_LIT;
                    sc->value = scale;
                    sc->val_type = type_int();
                    mul->right = sc;
                    expr->right = mul;
                }
                expr->val_type = expr->left->val_type;
                expr->vtype = expr->left->vtype;
            }
            else if (type_is_pointer(expr->left->val_type) && type_is_pointer(expr->right->val_type))
            {
                int scale = expr->left->val_type->base ? expr->left->val_type->base->size : 8;
                expr->val_type = type_int();
                expr->vtype = TYPE_INT;
                if (scale > 1)
                {
                    /* Rewrite `p - q` (pointer difference, in elements) into
                     * `(p - q) / scale`. The original subtraction must be
                     * copied into a fresh node: wrapping `expr` around itself
                     * would make expr->left point back at expr and recurse
                     * forever in codegen. The statement-chain `next` link is
                     * preserved because expr is rewritten in place. */
                    ASTNode *saved_next = expr->next;
                    ASTNode *sub = xcalloc(1, sizeof(ASTNode));
                    *sub = *expr;
                    sub->next = NULL;

                    ASTNode *div = xcalloc(1, sizeof(ASTNode));
                    div->type = AST_BINARY_OP;
                    div->op = OP_DIV;
                    div->val_type = type_int();
                    div->left = sub;
                    ASTNode *sc = xcalloc(1, sizeof(ASTNode));
                    sc->type = AST_INT_LIT;
                    sc->value = scale;
                    sc->val_type = type_int();
                    div->right = sc;
                    *expr = *div;
                    expr->next = saved_next;
                }
            }
            else
            {
                expr->val_type = type_int();
                expr->vtype = TYPE_INT;
            }
        }
        else
        {
            /* Simplified usual arithmetic conversions: the result is
             * unsigned when either operand is `unsigned int` or a pointer.
             * (Both operands are already 64-bit here, so rank reduces to
             * "unsigned int wins"; `unsigned char` deliberately does not
             * qualify -- see binary_is_unsigned().) */
            expr->val_type = binary_is_unsigned(expr->left->val_type,
                                                expr->right->val_type)
                                 ? type_uint()
                                 : type_int();
            expr->vtype = TYPE_INT;
        }
        break;
    case AST_TERNARY:
        check_value(expr->cond);
        check_value(expr->left);
        check_value(expr->right);
        expr->val_type = expr->left->val_type ? expr->left->val_type : type_int();
        expr->vtype = expr->val_type->kind;
        break;
    case AST_ASSIGN:
        if (expr->left->type == AST_VAR_REF)
        {
            resolve_var(expr->left);
        }
        else if (expr->left->type == AST_DEREF ||
                 expr->left->type == AST_MEMBER_ACCESS)
        {
            check_expr(expr->left);
        }
        else
        {
            report_error(expr->line, expr->col,
                         "left side of '=' must be an lvalue");
        }
        if (expr->left->val_type != NULL &&
            expr->left->val_type->kind == TYPE_ARRAY)
        {
            /* `a = 5;` where a is an array would store one element over the
             * array storage; C forbids assigning to an array. */
            report_error(expr->line, expr->col,
                         "cannot assign to an array");
        }
        check_value(expr->right);
        expr->val_type = expr->left->val_type;
        expr->vtype = expr->left->vtype;
        break;
    case AST_CALL:
        check_call(expr);
        break;
    case AST_POSTFIX:
        if (expr->left->type != AST_VAR_REF && expr->left->type != AST_DEREF &&
            expr->left->type != AST_MEMBER_ACCESS)
        {
            report_error(expr->line, expr->col,
                         "'++'/'--' operand must be an lvalue");
        }
        else if (expr->left->type == AST_VAR_REF)
        {
            resolve_var(expr->left);
        }
        else
        {
            check_expr(expr->left);
        }
        if (expr->left->val_type != NULL &&
            expr->left->val_type->kind == TYPE_ARRAY)
        {
            report_error(expr->line, expr->col,
                         "'++'/'--' cannot be applied to an array");
        }
        expr->val_type = expr->left->val_type;
        expr->vtype = expr->left->vtype;
        break;
    case AST_MEMBER_ACCESS:
    {
        check_expr(expr->left);
        Type *st_type = expr->left->val_type;
        if (st_type == NULL || st_type->kind != TYPE_STRUCT)
        {
            report_error(expr->line, expr->col, "left side of '.' must be a struct");
            expr->val_type = type_int();
            expr->vtype = TYPE_INT;
            break;
        }
        /* Look up member in struct definition */
        StructMember *m = st_type->members;
        while (m != NULL)
        {
            if (strcmp(m->name, expr->name) == 0)
            {
                break;
            }
            m = m->next;
        }
        if (m == NULL)
        {
            report_error(expr->line, expr->col,
                         "struct has no member named '%s'", expr->name);
            expr->val_type = type_int();
            expr->vtype = TYPE_INT;
            break;
        }
        expr->stack_offset = m->offset; /* save member offset in node */
        expr->val_type = m->type;
        expr->vtype = m->type->kind;
        break;
    }
    default:
        report_error(expr->line, expr->col, "invalid expression");
        break;
    }
}

static void check_switch(ASTNode *node, CheckCtx *ctx)
{
    check_value(node->cond);

    /* Labels may be nested inside blocks / if / loops within the switch
     * body. They all belong to this switch (a nested switch owns its own),
     * so collect them recursively instead of only scanning the top level. */
    ASTNode **cases = NULL;
    int ncases = 0;
    int cap = 0;
    collect_switch_cases(node->body, &cases, &ncases, &cap);

    long long *seen = ncases > 0 ? xmalloc(sizeof(long long) * (size_t)ncases)
                                 : NULL;
    int nseen = 0;
    int saw_default = 0;
    int index = 0;
    for (int i = 0; i < ncases; i++)
    {
        ASTNode *item = cases[i];
        if (item->type == AST_CASE)
        {
            item->case_index = index++;
            long long v;
            if (!fold_const(item->left, &v))
            {
                report_error(item->line, item->col,
                             "case label is not a constant expression");
            }
            else
            {
                for (int j = 0; j < nseen; j++)
                {
                    if (seen[j] == v)
                    {
                        report_error(item->line, item->col,
                                     "duplicate case value %lld", v);
                    }
                }
                seen[nseen++] = v;
                item->value = v;
            }
            item->left = NULL;
        }
        else if (item->type == AST_DEFAULT)
        {
            if (saw_default)
            {
                report_error(item->line, item->col,
                             "multiple 'default' labels in one switch");
            }
            saw_default = 1;
        }
    }
    free(seen);
    free(cases);

    scope_push();
    ctx->switch_depth++;
    check_stmt(node->body, ctx);
    ctx->switch_depth--;
    scope_pop();
}

static void check_stmt(ASTNode *stmt, CheckCtx *ctx)
{
    for (ASTNode *n = stmt; n != NULL; n = n->next)
    {
        switch (n->type)
        {
        case AST_VAR_DECL:
        {
            if (symbol_lookup_current(n->name) != NULL)
            {
                report_error(n->line, n->col,
                             "duplicate declaration of '%s' in scope",
                             n->name);
            }
            else
            {
                check_object_complete(n);
                Symbol *sym = symbol_insert(n->name, n->val_type, 0);
                int sz = n->val_type ? n->val_type->size : 8;
                if (sz <= 0)
                {
                    /* Error already reported by check_object_complete(); keep
                     * the frame layout sane by reserving one real slot so this
                     * variable never aliases the saved %rbp at offset 0. */
                    sz = 8;
                }
                sz = (sz + 7) / 8 * 8;
                *ctx->frame += sz;
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
        case AST_ADDR_OF:
        case AST_DEREF:
        case AST_MEMBER_ACCESS:
            check_expr(n);
            break;
        case AST_CALL:
            check_call(n);
            break;
        case AST_RETURN:
            check_value(n->left);
            if (n->left != NULL && n->left->val_type != NULL &&
                (n->left->val_type->kind == TYPE_STRUCT ||
                 n->left->val_type->kind == TYPE_ARRAY))
            {
                report_error(n->line, n->col,
                             "cannot return an aggregate by value");
            }
            break;
        case AST_IF:
            check_value(n->cond);
            check_stmt(n->body, ctx);
            check_stmt(n->right, ctx);
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
            scope_push();
            if (n->left != NULL)
            {
                check_stmt(n->left, ctx);
            }
            check_value(n->cond);
            check_value(n->right);
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
            if (ctx->switch_depth == 0)
            {
                report_error(n->line, n->col,
                             "'case'/'default' label not within a switch");
            }
            break;
        case AST_BREAK:
            if (ctx->loop_depth == 0 && ctx->switch_depth == 0)
            {
                report_error(n->line, n->col,
                             "'break' statement not within a loop or switch");
            }
            break;
        case AST_CONTINUE:
            if (ctx->loop_depth == 0)
            {
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
    CheckCtx ctx;
    ctx.frame = NULL;
    int frame = 0;
    ctx.frame = &frame;
    ctx.loop_depth = 0;
    ctx.switch_depth = 0;
    for (ASTNode *p = fn->left; p != NULL; p = p->next)
    {
        if (symbol_lookup_current(p->name) != NULL)
        {
            report_error(p->line, p->col, "duplicate parameter '%s'", p->name);
            continue;
        }
        Symbol *sym = symbol_insert(p->name, p->val_type, 0);
        frame += 8;
        sym->stack_offset = -frame;
        p->stack_offset = -frame;
    }
    check_stmt(fn->body, &ctx);
    check_returns(fn, fn->body);
    fn->frame_size = frame;
    scope_pop();
}

static void check_returns(ASTNode *fn, ASTNode *stmt)
{
    for (ASTNode *n = stmt; n != NULL; n = n->next)
    {
        switch (n->type)
        {
        case AST_RETURN:
            if (fn->vtype == TYPE_VOID && n->left != NULL)
            {
                report_error(n->line, n->col,
                             "void function '%s' should not return a value",
                             fn->name);
            }
            else if (fn->vtype != TYPE_VOID && n->left == NULL)
            {
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
    if (root == NULL || root->type != AST_PROGRAM)
    {
        report_error(0, 0, "internal: semantic analysis requires a program node");
        return 1;
    }
    symtab_init();
    sem_decl_pass(root);
    int have_main = 0;
    for (ASTNode *d = root->body; d != NULL; d = d->next)
    {
        if (d->type == AST_FUNC_DECL)
        {
            if (strcmp(d->name, "main") == 0)
            {
                have_main = 1;
            }
            if (d->body != NULL)
            {
                check_function(d);
            }
        }
    }
    if (!have_main)
    {
        report_error(0, 0, "no 'main' function defined");
    }
    return error_count() > 0 ? 1 : 0;
}