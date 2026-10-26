/* opt.c -- the AST optimizer: dead local stores (ROADMAP Phase 5).
 *
 * The pass answers one question per function: "which local variables are never
 * read?" A store to such a variable is unobservable, so it can go. Everything
 * else is left exactly as it was.
 *
 * Why so narrow
 * -------------
 * Two bigger optimizations are deliberately absent, and both for the same
 * reason: on this back end they would buy nothing but risk.
 *
 *  - *Copy propagation* (`t = x; ... use t ...` -> use `x`). In a stack machine
 *    both spellings compile to the same single frame load, so replacing one with
 *    the other changes no instruction. It becomes worthwhile only once values
 *    live in registers across statements, which is the liveness-allocator work
 *    that needs an IR (see ROADMAP Phase 5 note).
 *  - *Reordering or eliminating loads/stores of a variable that IS read*. That
 *    is dataflow over statements, and a mistake there is silent wrong code.
 *
 * The safe half is worth doing, though, and it is a real analysis: it needs a
 * use-site classification (read vs write vs address-taken) over the whole
 * function, which is the same shape of analysis the bigger passes would need.
 *
 * Safety argument, in full
 * ------------------------
 * A store to local `v` may be deleted when all of these hold:
 *
 *  1. `v` is read nowhere in the function (no occurrence in value position);
 *  2. `v`'s address is never taken (`&v`, or `v.member`, which needs the
 *     object's address) -- otherwise a pointer could read the stored value;
 *  3. `v` is a scalar (not an array, not a struct) -- arrays decay to pointers
 *     implicitly and structs are copied whole, so both leak their address;
 *  4. the stored expression is pure (a literal, a variable, or arithmetic on
 *     those) -- so deleting the store cannot delete a side effect.
 *
 * Condition 4 is why `t = printf("%d", 1);` keeps its call: only the *store* is
 * dropped, and this pass refuses to touch the statement at all rather than
 * rewrite it, because rewriting is where a pass starts growing new node types.
 *
 * Locals are keyed by stack_offset, not by name. Two different locals may share
 * a name in different scopes; the offset is unique per symbol, so shadowing
 * cannot make one variable's reads pay for another's stores. Globals are
 * skipped entirely (`is_global`): another function may read them.
 */

#include <stdlib.h>
#include <string.h>

#include "opt.h"
#include "util.h"

/* One entry per distinct local symbol in the function being analysed. A
 * function with more locals than this simply skips the pass (overflow flag),
 * which is always safe: doing nothing is a valid optimizer. */
#define OPT_MAX_LOCALS 512

typedef struct
{
    int offset; /* stack_offset, unique per local symbol */
    int reads;
    int address_taken;
} OptLocal;

typedef struct
{
    OptLocal items[OPT_MAX_LOCALS];
    int count;
    int overflow;
} OptTable;

static OptLocal *table_find(OptTable *t, int offset)
{
    for (int i = 0; i < t->count; i++)
    {
        if (t->items[i].offset == offset)
        {
            return &t->items[i];
        }
    }
    return NULL;
}

static OptLocal *table_get(OptTable *t, int offset)
{
    OptLocal *l = table_find(t, offset);
    if (l != NULL)
    {
        return l;
    }
    if (t->count >= OPT_MAX_LOCALS)
    {
        t->overflow = 1;
        return NULL;
    }
    l = &t->items[t->count++];
    l->offset = offset;
    l->reads = 0;
    l->address_taken = 0;
    return l;
}

static void note_ref(OptTable *t, ASTNode *ref, int as_read, int as_address)
{
    if (ref == NULL || ref->type != AST_VAR_REF || ref->is_global)
    {
        return;
    }
    OptLocal *l = table_get(t, ref->stack_offset);
    if (l == NULL)
    {
        return;
    }
    if (as_read)
    {
        l->reads++;
    }
    if (as_address)
    {
        l->address_taken = 1;
    }
}

/* ------------------------------------------------------------------------- */
/* Use-site classification                                                   */
/* ------------------------------------------------------------------------- */

static void scan_value(OptTable *t, ASTNode *n);
static void scan_lvalue(OptTable *t, ASTNode *n);

/* Walks the operand of `&` or of a member access: the object's *address* is
 * used, so a plain variable here is address-taken rather than written. This is
 * the case that makes the whole pass safe: without it, `int *p = &b;` followed
 * by `*p` would leave `b` looking like a dead store, and the pointer would read
 * whatever the slot happened to contain (caught by tests/12_pointers.c). */
static void note_address_use(OptTable *t, ASTNode *n)
{
    if (n == NULL)
    {
        return;
    }
    if (n->type == AST_VAR_REF)
    {
        note_ref(t, n, 0, 1);
        return;
    }
    if (n->type == AST_MEMBER_ACCESS)
    {
        note_address_use(t, n->left);
        return;
    }
    /* e.g. `&a[i]`: the base and the index are ordinary reads. */
    scan_value(t, n);
}

/* Walks a subtree that is being *read*. Every variable reference found here is
 * a read, which is what keeps a variable alive. */
static void scan_value(OptTable *t, ASTNode *n)
{
    if (n == NULL)
    {
        return;
    }
    switch (n->type)
    {
    case AST_VAR_REF:
        note_ref(t, n, 1, 0);
        break;
    case AST_BINARY_OP:
        scan_value(t, n->left);
        scan_value(t, n->right);
        break;
    case AST_TERNARY:
        scan_value(t, n->cond);
        scan_value(t, n->left);
        scan_value(t, n->right);
        break;
    case AST_CAST:
        scan_value(t, n->left);
        break;
    case AST_DEREF:
        /* `*p` reads the *pointer*, not the object, so a plain variable under
         * a dereference is a read of that variable. */
        scan_value(t, n->left);
        break;
    case AST_POSTFIX:
        /* `v++` is a read followed by a write; counting the read is the
         * conservative side and keeps `v` alive. */
        scan_value(t, n->left);
        break;
    case AST_ADDR_OF:
        /* `&v`: the value of `v` is not read, but its address escapes, so the
         * variable must be treated as address-taken. */
        note_address_use(t, n->left);
        break;
    case AST_MEMBER_ACCESS:
        /* `v.m` needs the address of `v`. */
        note_address_use(t, n->left);
        break;
    case AST_CALL:
        for (ASTNode *a = n->args; a != NULL; a = a->next)
        {
            scan_value(t, a);
        }
        break;
    case AST_ASSIGN:
        /* An assignment used as a value: the target is written, the right side
         * is read. */
        scan_lvalue(t, n->left);
        scan_value(t, n->right);
        break;
    default:
        /* literals, string literals: nothing to classify */
        break;
    }
}

/* Walks a subtree that denotes an object being *written*. A plain variable here
 * is a write, not a read -- that is the whole point of the pass. Anything more
 * complicated (an element, a field, a dereference) needs addresses and is
 * therefore treated as reading whatever it is built from. */
static void scan_lvalue(OptTable *t, ASTNode *n)
{
    if (n == NULL)
    {
        return;
    }
    switch (n->type)
    {
    case AST_VAR_REF:
        note_ref(t, n, 0, 0);
        break;
    case AST_MEMBER_ACCESS:
        if (n->left != NULL && n->left->type == AST_VAR_REF)
        {
            note_address_use(t, n->left);
        }
        else
        {
            scan_value(t, n->left);
        }
        break;
    case AST_DEREF:
        scan_value(t, n->left);
        break;
    default:
        scan_value(t, n);
        break;
    }
}

static void scan_stmt(OptTable *t, ASTNode *n)
{
    for (ASTNode *s = n; s != NULL; s = s->next)
    {
        switch (s->type)
        {
        case AST_ASSIGN:
            scan_lvalue(t, s->left);
            scan_value(t, s->right);
            break;
        case AST_IF:
            scan_value(t, s->cond);
            scan_stmt(t, s->body);
            scan_stmt(t, s->right);
            break;
        case AST_WHILE:
            scan_value(t, s->cond);
            scan_stmt(t, s->body);
            break;
        case AST_DO_WHILE:
            scan_stmt(t, s->body);
            scan_value(t, s->cond);
            break;
        case AST_FOR:
            scan_stmt(t, s->left);  /* init  */
            scan_value(t, s->cond); /* test  */
            scan_stmt(t, s->body);
            scan_stmt(t, s->right); /* step  */
            break;
        case AST_SWITCH:
            scan_value(t, s->cond);
            scan_stmt(t, s->body);
            break;
        case AST_CASE:
        case AST_DEFAULT:
            /* A label that is the controlled body of an if/loop carries the
             * statement it names; flat switch labels have no body. */
            scan_stmt(t, s->body);
            break;
        case AST_BLOCK:
            scan_stmt(t, s->body);
            break;
        case AST_RETURN:
            scan_value(t, s->left);
            break;
        case AST_VAR_DECL:
            /* The declaration itself holds no value; its initialiser follows
             * as a separate AST_ASSIGN in the same chain. */
            break;
        default:
            scan_value(t, s);
            break;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Purity                                                                    */
/* ------------------------------------------------------------------------- */

/* True when evaluating the expression has no effect other than producing a
 * value. Deliberately strict: a dereference may fault, a call may do anything,
 * an assignment and `++` both write. */
static int expr_is_pure(ASTNode *n)
{
    if (n == NULL)
    {
        return 1;
    }
    switch (n->type)
    {
    case AST_INT_LIT:
    case AST_STRING:
    case AST_VAR_REF:
        return 1;
    case AST_CAST:
        return expr_is_pure(n->left);
    case AST_BINARY_OP:
        return expr_is_pure(n->left) && expr_is_pure(n->right);
    case AST_TERNARY:
        return expr_is_pure(n->cond) && expr_is_pure(n->left) &&
               expr_is_pure(n->right);
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------------- */
/* Rewriting                                                                 */
/* ------------------------------------------------------------------------- */

static int is_dead_store(OptTable *t, ASTNode *n)
{
    if (n->type != AST_ASSIGN || n->left == NULL || n->right == NULL)
    {
        return 0;
    }
    if (n->left->type != AST_VAR_REF || n->left->is_global)
    {
        return 0;
    }
    const Type *ty = n->left->val_type;
    if (ty == NULL || ty->kind == TYPE_STRUCT || ty->kind == TYPE_ARRAY)
    {
        return 0;
    }
    const OptLocal *l = table_find(t, n->left->stack_offset);
    if (l == NULL || l->reads > 0 || l->address_taken)
    {
        return 0;
    }
    return expr_is_pure(n->right);
}

/* Rebuilds a statement chain, recursing into the blocks it contains. Returns the
 * (possibly different) head of the chain. */
static ASTNode *prune(OptTable *t, ASTNode *n)
{
    ASTNode *head = NULL;
    ASTNode **link = &head;

    for (ASTNode *cur = n; cur != NULL;)
    {
        ASTNode *next = cur->next;
        cur->next = NULL;

        switch (cur->type)
        {
        case AST_IF:
            cur->body = prune(t, cur->body);
            cur->right = prune(t, cur->right);
            break;
        case AST_WHILE:
        case AST_DO_WHILE:
        case AST_SWITCH:
        case AST_BLOCK:
        case AST_CASE:
        case AST_DEFAULT:
            cur->body = prune(t, cur->body);
            break;
        case AST_FOR:
            cur->left = prune(t, cur->left);
            cur->right = prune(t, cur->right);
            cur->body = prune(t, cur->body);
            break;
        default:
            break;
        }

        if (!is_dead_store(t, cur))
        {
            *link = cur;
            link = &cur->next;
        }
        cur = next;
    }
    return head;
}

/* ------------------------------------------------------------------------- */
/* Driver                                                                    */
/* ------------------------------------------------------------------------- */

void opt_run(ASTNode *program)
{
    if (program == NULL)
    {
        return;
    }
    for (ASTNode *fn = program->body; fn != NULL; fn = fn->next)
    {
        if (fn->type != AST_FUNC_DECL || fn->body == NULL || fn->is_extern)
        {
            continue;
        }

        OptTable t;
        memset(&t, 0, sizeof(t));
        scan_stmt(&t, fn->body);
        if (t.overflow)
        {
            /* Too many locals to analyse: skip the function. Doing nothing is
             * always a correct optimization. */
            continue;
        }
        fn->body = prune(&t, fn->body);
    }
}
