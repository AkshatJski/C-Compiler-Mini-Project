#include <stdio.h>
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
    /* Return type of the function being checked, so `return expr;` can be
     * compared against it (BUG-019). */
    Type *ret_type;
} CheckCtx;

/* Inserts the `index * sizeof(*ptr)` multiplication that pointer arithmetic
 * needs, so codegen can emit a plain integer add. Called from the ordinary
 * binary case and from the compound-assignment case; each caller reaches a
 * given node exactly once, so the rewrite can never be applied twice
 * (BUG-020). */
static void scale_index_operand(ASTNode *binop, Type *ptr_type)
{
    int scale = (ptr_type->base != NULL && ptr_type->base->size > 0)
                    ? ptr_type->base->size
                    : 8;
    if (scale <= 1)
    {
        return;
    }
    ASTNode *mul = xcalloc(1, sizeof(ASTNode));
    mul->type = AST_BINARY_OP;
    mul->op = OP_MUL;
    mul->val_type = type_int();
    mul->left = binop->right;
    ASTNode *sc = xcalloc(1, sizeof(ASTNode));
    sc->type = AST_INT_LIT;
    sc->value = scale;
    sc->val_type = type_int();
    mul->right = sc;
    binop->right = mul;
}

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
static void fold_literal(ASTNode *expr);
static void check_object_complete(ASTNode *decl);
static int types_compatible(const Type *to, const Type *from, ASTNode *rhs);
static int same_type(const Type *a, const Type *b);
static int stmt_terminates(ASTNode *stmt);
static int list_terminates(ASTNode *stmt);

/* ------------------------------------------------------------------ */
/* Type compatibility                                                 */
/* ------------------------------------------------------------------ */

/* Structural identity, needed because type_ptr() returns a fresh object on
 * every call: `int *` written twice must still compare equal. */
static int same_type(const Type *a, const Type *b)
{
    if (a == b)
    {
        return 1;
    }
    if (a == NULL || b == NULL || a->kind != b->kind)
    {
        return 0;
    }
    switch (a->kind)
    {
    case TYPE_INT:
    case TYPE_CHAR:
        return a->is_unsigned == b->is_unsigned;
    case TYPE_VOID:
        return 1;
    case TYPE_PTR:
        return same_type(a->base, b->base);
    case TYPE_ARRAY:
        return a->array_size == b->array_size && same_type(a->base, b->base);
    case TYPE_STRUCT:
        if (a->struct_tag != NULL && b->struct_tag != NULL)
        {
            return strcmp(a->struct_tag, b->struct_tag) == 0;
        }
        return a->members == b->members;
    }
    return 0;
}

/* Is `rhs` the null pointer constant (a constant expression that folds to 0)?
 * C allows `p = 0;` and nothing else implicit between integers and pointers. */
static int is_null_constant(ASTNode *rhs)
{
    if (rhs == NULL)
    {
        return 0;
    }
    long long v;
    return fold_const(rhs, &v) && v == 0;
}

/* One predicate for the three places a value is converted: assignment,
 * argument passing and `return`. Deliberately permissive for scalar-to-scalar
 * (narrowing is legal C, and `int` is 64-bit here) and strict for the cases
 * that would otherwise reinterpret bytes: aggregate<->scalar and
 * pointer<->integer. A cast is the explicit opt-in for the latter (BUG-006),
 * because by the time the check runs the cast has already produced the
 * target type. */
static int types_compatible(const Type *to, const Type *from, ASTNode *rhs)
{
    if (to == NULL || from == NULL)
    {
        return 1; /* an earlier error already left a type unknown */
    }
    if (to->kind == TYPE_VOID || from->kind == TYPE_VOID)
    {
        /* A void value never reaches here as a real conversion: check_value()
         * already rejects `f()` from a void function. Stay quiet rather than
         * piling a second error on the same expression. */
        return 1;
    }
    if (from->kind == TYPE_ARRAY)
    {
        /* Array-to-pointer decay: `int row[4]` becomes `int *` in every
         * conversion context (initializer, argument, return value). The
         * semantic layer keeps the array type on the node because codegen uses
         * it to avoid loading through the address. */
        return to->kind == TYPE_PTR;
    }
    if (to->kind == TYPE_PTR)
    {
        if (from->kind == TYPE_PTR)
        {
            /* Any pointer to any pointer object pointer: `int *` to `char *`
             * is a warning in C, not an error, and is how such casts are
             * written before casts existed. */
            return 1;
        }
        return is_null_constant(rhs);
    }
    if (to->kind == TYPE_INT || to->kind == TYPE_CHAR)
    {
        return from->kind == TYPE_INT || from->kind == TYPE_CHAR;
    }
    if (to->kind == TYPE_STRUCT)
    {
        return from->kind == TYPE_STRUCT && same_type(to, from);
    }
    return 1;
}

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
    case OP_COMMA:
        /* `(a, b)` is not a constant expression: the left side exists for its
         * side effects. */
        return 0;
    }
    return 0;
}

/* Phase 5: constant folding.
 *
 * Runs at the end of check_expr(), once the node's type is known, and
 * rewrites the node *in place* into an integer literal. In place matters:
 * every parent already holds a pointer to this node, so replacing the node
 * with a fresh one would leave the tree pointing at the old, unfolded copy.
 * Mutating the node means no re-parenting anywhere.
 *
 * Deliberately conservative, because a folding bug produces a silently wrong
 * program, which is the one failure this project refuses to ship:
 *  - only integer results fold; a pointer result (`p + 0`) must stay a
 *    pointer, and there is no floating point yet;
 *  - unsigned operands are left alone: fold_const() works in signed
 *    arithmetic, so `u / 2` with a large `u` would fold to a wrong value;
 *  - division by zero is not folded (fold_const declines), so a trap stays a
 *    trap instead of becoming a number;
 *  - the comma operator is not folded: its left side exists for its effects;
 *  - `(char)x` is not folded, because narrowing has its own semantics that
 *    codegen already implements correctly -- no need to write it twice.
 *
 * Folding a `?:` whose condition is constant is still correct even when the
 * dead arm has side effects: C does not evaluate that arm either.
 */
static void fold_literal(ASTNode *expr)
{
    if (!g_optimize || expr == NULL)
    {
        return;
    }
    if (expr->type != AST_BINARY_OP && expr->type != AST_TERNARY)
    {
        return;
    }
    if (expr->val_type == NULL || expr->val_type->kind != TYPE_INT)
    {
        return;
    }
    if (expr->left != NULL && expr->left->val_type != NULL &&
        binary_is_unsigned(expr->left->val_type,
                           expr->right != NULL ? expr->right->val_type : NULL))
    {
        return;
    }

    long long value;
    if (!fold_const(expr, &value))
    {
        return;
    }

    expr->type = AST_INT_LIT;
    expr->value = value;
    expr->vtype = TYPE_INT;
    expr->val_type = type_int();
    expr->left = NULL;
    expr->right = NULL;
    expr->cond = NULL;
    /* `line`/`col` are kept: the folded literal still points at the token
     * that produced it, which is what a future diagnostic will quote. */
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

            /* Parameter count is no longer capped: arguments beyond the
             * register slots are passed on the stack by the caller and read
             * from above the frame by the callee (ROADMAP Phase 4). */

            /* `main` is special: the C runtime passes argc/argv in registers
             * chosen by the platform ABI. mycc reads parameters from the SysV
             * registers only, so on Windows a parameterised main would read
             * whatever happened to be in %rdi. Rejecting it turns silent
             * garbage into a one-line fix (BUG-014). */
            if (strcmp(d->name, "main") == 0 && params > 0)
            {
                report_error(d->line, d->col,
                             "'main' must take no parameters; declare it as "
                             "'int main(void)'");
            }

            /* Passing a struct by value needs a hidden-pointer ABI (or a
             * per-argument copy) that mycc does not have. Say so here, where
             * the parameter and its name are still at hand, instead of
             * emitting a call that copies 8 bytes of a 16-byte struct
             * (BUG-004). */
            for (ASTNode *p = d->left; p != NULL; p = p->next)
            {
                if (p->val_type != NULL && p->val_type->kind == TYPE_STRUCT)
                {
                    char tbuf[TYPE_DESCRIBE_MAX];
                    type_describe(p->val_type, tbuf, sizeof(tbuf));
                    report_error(p->line, p->col,
                                 "parameter '%s' has type '%s' (by value), "
                                 "which mycc does not support; pass a pointer "
                                 "instead ('%s *%s')",
                                 p->name, tbuf, tbuf, p->name);
                }
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
                /* A prototype seen first has no parameter list to compare
                 * against later; remember the definition's so argument types
                 * can be checked at call sites (BUG-019). */
                if (existing->param_list == NULL)
                {
                    existing->param_list = d->left;
                }
                /* A later `extern` re-declaration of an already-defined
                 * function must not make it look externally supplied. */
                if (existing->is_defined)
                {
                    existing->is_extern = 0;
                }
            }
            else
            {
                Symbol *sym = symbol_insert(d->name, d->val_type, 1);
                sym->is_function = 1;
                sym->param_count = params;
                sym->param_list = d->left;
                sym->is_defined = (d->body != NULL) ? 1 : 0;
                sym->is_extern = (d->body == NULL && d->is_extern) ? 1 : 0;
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

/* ------------------------------------------------------------------ */
/* Built-in I/O lowering                                               */
/* ------------------------------------------------------------------ */

static ASTNode *make_string_node(const char *text)
{
    ASTNode *s = ast_node(AST_STRING, 0, 0);
    s->name = xstrdup(text);
    s->str_len = (int)strlen(text);
    s->value = -1;
    s->vtype = TYPE_PTR;
    s->val_type = type_ptr(type_char());
    return s;
}

static ASTNode *make_int_node(long long value)
{
    ASTNode *n = ast_node(AST_INT_LIT, 0, 0);
    n->value = value;
    n->vtype = TYPE_INT;
    n->val_type = type_int();
    return n;
}

/* print_int(x)   -> printf("%lld\n", x)   (mycc's int is 64-bit)
 * print_str(s)   -> printf("%s", s)
 * print_char(x)  -> putchar(x)
 * print_endl()   -> putchar('\n')
 *
 * The extra argument is prepended as an ordinary AST node, so the regular
 * call path generates it and the platform ABI carries it. */
static void rewrite_builtin_call(ASTNode *call)
{
    if (strcmp(call->name, "print_int") == 0)
    {
        ASTNode *fmt = make_string_node("%lld\n");
        fmt->next = call->args;
        call->args = fmt;
        call->name = "printf";
    }
    else if (strcmp(call->name, "print_str") == 0)
    {
        ASTNode *fmt = make_string_node("%s");
        fmt->next = call->args;
        call->args = fmt;
        call->name = "printf";
    }
    else if (strcmp(call->name, "print_char") == 0)
    {
        call->name = "putchar";
    }
    else if (strcmp(call->name, "print_endl") == 0)
    {
        call->args = make_int_node('\n');
        call->name = "putchar";
    }
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

    /* Remember that this callee is actually used: a prototype that is called
     * but never defined has no body to link against, and reporting it here
     * (at the call site, in the user's file) is far more useful than a linker
     * error naming a temporary .s file (BUG-015). */
    if (!sym->is_called)
    {
        sym->is_called = 1;
        sym->call_line = call->line;
        sym->call_col = call->col;
    }

    int argc = count_args(call);
    if (sym->is_extern)
    {
        /* A variadic extern (`printf`) legitimately takes more arguments than
         * its declaration names; only the declared prefix is type-checked
         * below, and the count must be at least that prefix. */
        if (argc < sym->param_count)
        {
            report_error(call->line, call->col,
                         "function '%s' expects at least %d argument(s), got %d",
                         call->name, sym->param_count, argc);
        }
    }
    else if (argc != sym->param_count)
    {
        report_error(call->line, call->col,
                     "function '%s' expects %d argument(s), got %d",
                     call->name, sym->param_count, argc);
    }

    ASTNode *param = sym->param_list;
    for (ASTNode *a = call->args; a != NULL; a = a->next)
    {
        check_value(a);
        if (param != NULL && !types_compatible(param->val_type, a->val_type, a))
        {
            char tbuf[TYPE_DESCRIBE_MAX];
            char fbuf[TYPE_DESCRIBE_MAX];
            type_describe(param->val_type, tbuf, sizeof(tbuf));
            type_describe(a->val_type, fbuf, sizeof(fbuf));
            report_error(a->line, a->col,
                         "passing '%s' to parameter '%s' of '%s' (expected "
                         "'%s')",
                         fbuf, param->name, call->name, tbuf);
        }
        if (param != NULL)
        {
            param = param->next;
        }
    }

    /* A built-in I/O helper becomes the libc call that implements it. Doing
     * this after the checks means the helper's own signature is what the user
     * sees in diagnostics, and codegen needs no knowledge of the names. */
    if (sym->is_builtin)
    {
        rewrite_builtin_call(call);
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
    case AST_STRING:
        /* A string literal denotes its first byte, i.e. `char *`. */
        expr->vtype = TYPE_PTR;
        expr->val_type = type_ptr(type_char());
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
                scale_index_operand(expr, expr->left->val_type);
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
                scale_index_operand(expr, expr->left->val_type);
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
        else if (expr->op == OP_COMMA)
        {
            /* `a, b` evaluates `a` for its side effects and yields `b`. */
            expr->val_type = expr->right->val_type;
            expr->vtype = expr->right->vtype;
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
    case AST_CAST:
        /* The parser already recorded the target type. A cast is exactly the
         * explicit opt-in for otherwise-incompatible conversions, so only the
         * operand still needs checking (BUG-006). */
        check_value(expr->left);
        if (expr->val_type != NULL)
        {
            expr->vtype = expr->val_type->kind;
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
        /* `x op= e` arrives as `x = x op e` with one shared node for `x`.
         * That node was just checked (and may have been rewritten by the
         * pointer-index scaling above), so walking the whole binop again would
         * scale the index a second time -- `a[i] += 1` used to address
         * a[i*64]. Check only the right operand and type the operation here
         * (BUG-020). */
        if (expr->right->type == AST_BINARY_OP &&
            expr->right->left == expr->left)
        {
            ASTNode *binop = expr->right;
            check_value(binop->right);
            Type *ltype = binop->left->val_type;
            Type *rtype = binop->right->val_type;
            if ((binop->op == OP_ADD || binop->op == OP_SUB) &&
                type_is_pointer(ltype) && type_is_integer(rtype))
            {
                scale_index_operand(binop, ltype);
                binop->val_type = ltype;
                binop->vtype = ltype->kind;
            }
            else if (binop->op == OP_SUB && type_is_pointer(ltype) &&
                     type_is_pointer(rtype))
            {
                /* `p -= q` is a difference in elements, not in bytes. */
                binop->val_type = type_int();
                binop->vtype = TYPE_INT;
            }
            else
            {
                binop->val_type = binary_is_unsigned(ltype, rtype) ? type_uint()
                                                                   : type_int();
                binop->vtype = TYPE_INT;
            }
        }
        else
        {
            check_value(expr->right);
        }
        /* `struct P a; struct P b; a = b;` is a byte copy and legal C.
         * Anything else that would reinterpret bytes -- struct to int, int to
         * pointer -- is now reported instead of silently copying 8 bytes
         * (BUG-019). The `0` literal stays valid as the null pointer. */
        if (expr->left->val_type != NULL &&
            expr->left->val_type->kind != TYPE_ARRAY &&
            !types_compatible(expr->left->val_type, expr->right->val_type,
                              expr->right))
        {
            char tbuf[TYPE_DESCRIBE_MAX];
            char fbuf[TYPE_DESCRIBE_MAX];
            type_describe(expr->left->val_type, tbuf, sizeof(tbuf));
            type_describe(expr->right->val_type, fbuf, sizeof(fbuf));
            report_error(expr->line, expr->col,
                         "assignment to '%s' from incompatible type '%s'", tbuf,
                         fbuf);
        }
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
        else if (expr->left->val_type != NULL &&
                 expr->left->val_type->kind == TYPE_STRUCT)
        {
            /* Incrementing a struct would add 1 to its first 8 bytes. */
            report_error(expr->line, expr->col,
                         "'++'/'--' cannot be applied to a struct");
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

    /* Phase 5, last step of typing a node: if the whole expression is
     * computable now, replace it with its value. Doing this *after* the type
     * is known is what lets the fold respect `unsigned` and pointer
     * arithmetic; doing it *here* is what lets a parent, which called us on
     * its child, see the folded child. */
    fold_literal(expr);
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
        case AST_CAST:
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
            else if (n->left != NULL && ctx->ret_type != NULL &&
                     !types_compatible(ctx->ret_type, n->left->val_type,
                                       n->left))
            {
                char tbuf[TYPE_DESCRIBE_MAX];
                char fbuf[TYPE_DESCRIBE_MAX];
                type_describe(ctx->ret_type, tbuf, sizeof(tbuf));
                type_describe(n->left->val_type, fbuf, sizeof(fbuf));
                report_error(n->line, n->col,
                             "returning '%s' from a function with result type "
                             "'%s'",
                             fbuf, tbuf);
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

/* ------------------------------------------------------------------ */
/* Fall-through analysis (BUG-016)                                     */
/* ------------------------------------------------------------------ */

/* Does a `default:` label appear in this switch body? Nested switches own
 * their own defaults, so the scan stops at AST_SWITCH. */
static int has_default_label(ASTNode *stmt)
{
    for (ASTNode *n = stmt; n != NULL; n = n->next)
    {
        if (n->type == AST_DEFAULT)
        {
            return 1;
        }
        if (n->type == AST_SWITCH)
        {
            continue;
        }
        if ((n->type == AST_BLOCK || n->type == AST_IF ||
             n->type == AST_WHILE || n->type == AST_DO_WHILE ||
             n->type == AST_FOR) &&
            has_default_label(n->body))
        {
            return 1;
        }
        if (n->type == AST_IF && has_default_label(n->right))
        {
            return 1;
        }
    }
    return 0;
}

static ASTNode *last_in_list(ASTNode *stmt)
{
    if (stmt == NULL)
    {
        return NULL;
    }
    while (stmt->next != NULL)
    {
        stmt = stmt->next;
    }
    return stmt;
}

/* Does control *never* reach the end of this statement?
 *
 * This only decides whether to warn about a missing `return`, so it is
 * deliberately conservative: when uncertain it answers 0, which suppresses
 * the warning. A missed warning is a non-event; a bogus one on correct code
 * would train users to ignore the channel. */
static int stmt_terminates(ASTNode *stmt)
{
    if (stmt == NULL)
    {
        return 0;
    }
    switch (stmt->type)
    {
    case AST_RETURN:
        return 1;
    case AST_BLOCK:
        return list_terminates(stmt->body);
    case AST_IF:
        /* Both arms must terminate: without an `else` there is always a path
         * that skips the `then` branch. */
        return stmt->right != NULL && stmt_terminates(stmt->body) &&
               stmt_terminates(stmt->right);
    case AST_WHILE:
    case AST_FOR:
    {
        /* An endless loop never reaches the statement after it. Only a missing
         * condition (`for (;;)`) or a constant non-zero one counts; a `break`
         * inside is missed, which errs towards silence. */
        if (stmt->cond == NULL)
        {
            return 1;
        }
        long long v;
        return fold_const(stmt->cond, &v) && v != 0;
    }
    case AST_DO_WHILE:
    {
        long long v;
        return stmt_terminates(stmt->body) ||
               (fold_const(stmt->cond, &v) && v != 0);
    }
    case AST_SWITCH:
    {
        /* Without a `default:` an unmatched value skips every case, so the
         * switch always falls through. With one, the last case region decides:
         * if it ends in a return, no path reaches the closing brace. */
        ASTNode *body = stmt->body;
        ASTNode *list = (body != NULL && body->type == AST_BLOCK) ? body->body
                                                                  : body;
        ASTNode *last = last_in_list(list);
        return has_default_label(list) &&
               (last != NULL && last->type == AST_RETURN);
    }
    default:
        return 0;
    }
}

/* A statement list terminates when any of its statements does; everything
 * after such a statement is unreachable. */
static int list_terminates(ASTNode *stmt)
{
    for (ASTNode *n = stmt; n != NULL; n = n->next)
    {
        if (stmt_terminates(n))
        {
            return 1;
        }
    }
    return 0;
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
    ctx.ret_type = fn->val_type;
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

    /* Reaching the closing brace of a non-void function is undefined in C;
     * mycc quietly returned 0, which turns "I forgot a return" into a
     * mysterious wrong answer at run time. `main` is exempt because the
     * standard gives it an implicit `return 0` (BUG-016). */
    if (fn->vtype != TYPE_VOID && strcmp(fn->name, "main") != 0 &&
        !(fn->body != NULL && stmt_terminates(fn->body)))
    {
        report_warning(fn->line, fn->col,
                       "control reaches end of non-void function '%s'",
                       fn->name);
    }

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

/* ------------------------------------------------------------------ */
/* Built-in I/O (ROADMAP Phase 4)                                      */
/* ------------------------------------------------------------------ */

/* Four printing helpers are available without any declaration:
 *
 *   print_int(expr)   decimal 64-bit integer, then a newline
 *   print_char(expr)  one character
 *   print_str(expr)   a `char *` string, no newline
 *   print_endl()      one newline
 *
 * They are registered as extern symbols, so a call type-checks like any other
 * function (arity and the declared parameter types). Codegen lowers them to
 * libc calls -- printf with "%lld"/"%s", and putchar -- using the platform
 * calling convention. A program that defines one of these names itself keeps
 * its own definition: sem_decl_pass has already inserted it, and codegen only
 * lowers the builtin when no body exists. */
static void register_builtin(const char *name, Type *ret, Type *arg, int argc)
{
    if (symbol_lookup(name) != NULL)
    {
        return; /* the program declared its own -- that one wins */
    }
    Symbol *sym = symbol_insert(name, ret, 1);
    sym->is_function = 1;
    sym->is_defined = 1; /* nothing to link: codegen emits the calls */
    sym->is_extern = 1;  /* variadic-friendly arity rule */
    sym->is_builtin = 1;
    sym->param_count = argc;

    ASTNode *first = NULL;
    ASTNode **tail = &first;
    for (int i = 0; i < argc; i++)
    {
        ASTNode *p = ast_node(AST_PARAM, 0, 0);
        char buf[16];
        snprintf(buf, sizeof(buf), "arg%d", i);
        p->name = xstrdup(buf);
        p->val_type = arg;
        p->vtype = arg->kind;
        *tail = p;
        tail = &p->next;
    }
    sym->param_list = first;
}

static void register_builtins(void)
{
    Type *int_type = type_int();
    /* Source-level helpers: these are rewritten into libc calls. */
    register_builtin("print_int", int_type, int_type, 1);
    register_builtin("print_char", int_type, int_type, 1);
    register_builtin("print_str", int_type, type_ptr(type_char()), 1);
    register_builtin("print_endl", int_type, NULL, 0);
    /* The two libc functions the rewrites target, and the ones a program is
     * most likely to call directly. Declaring them here means no `extern`
     * line is needed for the common case. */
    register_builtin("printf", int_type, type_ptr(type_char()), 1);
    register_builtin("puts", int_type, type_ptr(type_char()), 1);
    register_builtin("putchar", int_type, int_type, 1);
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
    register_builtins();
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

    /* A prototype that is called but never defined would fail at link time
     * with a message naming a temporary .s file. Report it here, at the call
     * site, while the user's own source coordinates still mean something
     * (BUG-015). Unused prototypes stay legal -- they are how a file declares
     * what it expects a library to provide. */
    for (ASTNode *d = root->body; d != NULL; d = d->next)
    {
        if (d->type != AST_FUNC_DECL)
        {
            continue;
        }
        Symbol *sym = symbol_lookup(d->name);
        if (sym != NULL && sym->is_function && sym->is_called &&
            !sym->is_defined && !sym->is_extern)
        {
            report_error(sym->call_line, sym->call_col,
                         "function '%s' is declared but never defined", d->name);
        }
    }
    return error_count() > 0 ? 1 : 0;
}