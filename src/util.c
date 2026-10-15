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

static Type g_type_int = {TYPE_INT, NULL, 0, NULL, NULL, 8, 8};
static Type g_type_char = {TYPE_CHAR, NULL, 0, NULL, NULL, 1, 1};
static Type g_type_void = {TYPE_VOID, NULL, 0, NULL, NULL, 0, 1};

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