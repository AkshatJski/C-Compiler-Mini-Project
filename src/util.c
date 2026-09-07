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
    if (p == NULL) {
        fprintf(stderr, "fatal: out of memory allocating %lu bytes\n",
                (unsigned long)size);
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count, size);
    if (p == NULL) {
        fprintf(stderr, "fatal: out of memory allocating %lu bytes\n",
                (unsigned long)(count * size));
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "fatal: out of memory reallocating %lu bytes\n",
                (unsigned long)size);
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
    if (g_source_path != NULL) {
        fprintf(stderr, "%s:", g_source_path);
    }
    if (line > 0 && col > 0) {
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
