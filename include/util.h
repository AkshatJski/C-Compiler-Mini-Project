#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include "ast.h"

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
char *xstrdup(const char *s);

void set_source_path(const char *path);
void report_error(int line, int col, const char *fmt, ...);
int error_count(void);
void error_reset(void);

/* Phase 3 Type constructors and queries */
Type *type_int(void);
Type *type_char(void);
Type *type_void(void);
Type *type_ptr(Type *base);
Type *type_array(Type *base, int array_size);
int type_is_pointer(const Type *t);
int type_is_integer(const Type *t);
Type *type_struct(const char *tag);
void struct_add_member(Type *struct_type, const char *name, Type *member_type);

#endif /* UTIL_H */