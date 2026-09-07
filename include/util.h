#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/* Memory helpers that abort with a diagnostic instead of returning NULL. */
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

/* Centralized error reporting with file:line:col tracking. */
void set_source_path(const char *path);
void report_error(int line, int col, const char *fmt, ...);
int error_count(void);
void error_reset(void);

#endif /* UTIL_H */
