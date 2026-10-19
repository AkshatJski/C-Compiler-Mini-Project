#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

static int g_error_count = 0;
static const char *g_source_path = NULL;

void set_source_path(const char *path)
{
    g_source_path = path;
}

void *xmalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
    {
        fprintf(stderr, "fatal: out of memory allocating %lu bytes\n",
                (unsigned long)size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count, size);
    if (p == NULL)
    {
        fprintf(stderr, "fatal: out of memory allocating %lu bytes\n",
                (unsigned long)(count * size));
        exit(EXIT_FAILURE);
    }
    return p;
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s);
    char *copy = xmalloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL)
    {
        fprintf(stderr, "fatal: out of memory allocating %lu bytes\n",
                (unsigned long)size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void report_error(int line, int col, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (g_source_path != NULL)
    {
        fprintf(stderr, "%s:", g_source_path);
    }
    if (line > 0 && col > 0)
    {
        fprintf(stderr, "%d:%d: ", line, col);
    }
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    g_error_count++;
}

int error_count(void)
{
    return g_error_count;
}

void error_reset(void)
{
    g_error_count = 0;
}

/* ------------------------------------------------------------------ */
/* Phase 3 Type System Constructors & Helpers                         */
/* ------------------------------------------------------------------ */

static Type g_type_int = {TYPE_INT, NULL, 0, NULL, NULL, 8, 8, 0};
static Type g_type_char = {TYPE_CHAR, NULL, 0, NULL, NULL, 1, 1, 0};
static Type g_type_void = {TYPE_VOID, NULL, 0, NULL, NULL, 0, 1, 0};
/* Same layout/size as the signed pair above; only is_unsigned differs.
 * Both are 64-bit (resp. 1 byte) so `unsigned` never changes how much
 * storage an object gets, only how its value is interpreted. */
static Type g_type_uint = {TYPE_INT, NULL, 0, NULL, NULL, 8, 8, 1};
static Type g_type_uchar = {TYPE_CHAR, NULL, 0, NULL, NULL, 1, 1, 1};

Type *type_int(void)
{
    return &g_type_int;
}

Type *type_char(void)
{
    return &g_type_char;
}

Type *type_void(void)
{
    return &g_type_void;
}

Type *type_uint(void)
{
    return &g_type_uint;
}

Type *type_uchar(void)
{
    return &g_type_uchar;
}

Type *type_ptr(Type *base)
{
    Type *t = xcalloc(1, sizeof(Type));
    t->kind = TYPE_PTR;
    t->base = base;
    t->size = 8;
    t->align = 8;
    return t;
}

Type *type_array(Type *base, int array_size)
{
    Type *t = xcalloc(1, sizeof(Type));
    t->kind = TYPE_ARRAY;
    t->base = base;
    t->array_size = array_size;
    t->align = base ? base->align : 8;
    t->size = base ? (base->size * array_size) : 0;
    return t;
}

int type_is_pointer(const Type *t)
{
    return t != NULL && (t->kind == TYPE_PTR || t->kind == TYPE_ARRAY);
}

int type_is_integer(const Type *t)
{
    return t != NULL && (t->kind == TYPE_INT || t->kind == TYPE_CHAR);
}

int binary_is_unsigned(const Type *left, const Type *right)
{
    /* A narrow `unsigned char` operand does not make the operation
     * unsigned: it is zero-extended into the 64-bit domain, so signed
     * div/mod/shift/compare give the same answers C's integer promotion
     * would. `unsigned int` (64-bit, can hold values above 2^63) genuinely
     * does, and so do pointers -- addresses order as unsigned. An
     * array-typed operand counts as a pointer too, because this compiler
     * treats arrays in binary expressions as already decayed (see
     * type_is_pointer); that is what makes `a[i] < a[j]` on a 2-D array
     * compare correctly. NULL operands never qualify. */
    if (left != NULL && left->kind == TYPE_INT && left->is_unsigned)
    {
        return 1;
    }
    if (right != NULL && right->kind == TYPE_INT && right->is_unsigned)
    {
        return 1;
    }
    if (type_is_pointer(left) || type_is_pointer(right))
    {
        return 1;
    }
    return 0;
}

/* "Complete" = the type reserves storage. Incomplete types (void, and
 * struct tags that were only forward-declared / have no members yet) have
 * size 0 and cannot back an object; zero-length arrays also count as
 * incomplete here. Objects of incomplete type previously received a
 * 0-byte slot at stack_offset 0 (the saved %rbp) and crashed at runtime. */
int type_is_complete(const Type *t)
{
    if (t == NULL)
    {
        return 0;
    }
    switch (t->kind)
    {
    case TYPE_INT:
    case TYPE_CHAR:
    case TYPE_PTR:
        return 1;
    case TYPE_ARRAY:
        return t->array_size > 0 && type_is_complete(t->base);
    case TYPE_STRUCT:
        return t->members != NULL && t->size > 0;
    case TYPE_VOID:
    default:
        return 0;
    }
}

Type *type_struct(const char *tag)
{
    Type *t = xcalloc(1, sizeof(Type));
    t->kind = TYPE_STRUCT;
    t->struct_tag = tag ? xstrdup(tag) : NULL;
    t->size = 0;
    t->align = 1;
    t->members = NULL;
    return t;
}

void struct_add_member(Type *struct_type, const char *name, Type *member_type)
{
    int m_align = member_type ? member_type->align : 8;
    int m_size = member_type ? member_type->size : 8;
    if (m_align <= 0)
        m_align = 8;
    if (m_size <= 0)
        m_size = 8;

    /* Align current struct size to member's alignment requirement */
    int offset = (struct_type->size + m_align - 1) / m_align * m_align;

    StructMember *m = xcalloc(1, sizeof(StructMember));
    m->name = xstrdup(name);
    m->type = member_type;
    m->offset = offset;
    m->next = NULL;

    /* Append to members list */
    if (struct_type->members == NULL)
    {
        struct_type->members = m;
    }
    else
    {
        StructMember *cur = struct_type->members;
        while (cur->next != NULL)
        {
            cur = cur->next;
        }
        cur->next = m;
    }

    struct_type->size = offset + m_size;
    if (m_align > struct_type->align)
    {
        struct_type->align = m_align;
    }
}

void collect_switch_cases(ASTNode *body, ASTNode ***vec, int *n, int *cap)
{
    for (ASTNode *s = body; s != NULL; s = s->next)
    {
        switch (s->type)
        {
        case AST_CASE:
        case AST_DEFAULT:
            if (*n == *cap)
            {
                *cap = (*cap > 0) ? (*cap * 2) : 8;
                *vec = xrealloc(*vec, sizeof(ASTNode *) * (size_t)(*cap));
            }
            (*vec)[(*n)++] = s;
            break;
        case AST_BLOCK:
            collect_switch_cases(s->body, vec, n, cap);
            break;
        case AST_IF:
            collect_switch_cases(s->body, vec, n, cap);
            collect_switch_cases(s->right, vec, n, cap);
            break;
        case AST_WHILE:
        case AST_DO_WHILE:
            collect_switch_cases(s->body, vec, n, cap);
            break;
        case AST_FOR:
            collect_switch_cases(s->left, vec, n, cap);
            collect_switch_cases(s->body, vec, n, cap);
            collect_switch_cases(s->right, vec, n, cap);
            break;
        case AST_SWITCH:
            break; /* nested switch owns its own case labels */
        default:
            break;
        }
    }
}
