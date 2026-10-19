#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include "ast.h"

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

void set_source_path(const char *path);
void report_error(int line, int col, const char *fmt, ...);
int error_count(void);
void error_reset(void);

/* Phase 3 Type constructors and queries */
Type *type_int(void);
Type *type_char(void);
Type *type_void(void);
/* `unsigned int` and `unsigned char`. Same storage as their signed
 * counterparts; only the sign of the 64-bit value (resp. the byte load
 * width) differs. */
Type *type_uint(void);
Type *type_uchar(void);
Type *type_ptr(Type *base);
Type *type_array(Type *base, int array_size);
int type_is_pointer(const Type *t);
int type_is_integer(const Type *t);
int type_is_complete(const Type *t);
/* 1 if a binary operation over these two operand types must use unsigned
 * semantics (div/mod, right shift, ordered comparison). `unsigned int` and
 * pointers qualify; `unsigned char` deliberately does not, because its
 * values are zero-extended into the 64-bit domain, which is exactly what
 * C's integer promotion does with them. NULL operands never qualify. */
int binary_is_unsigned(const Type *left, const Type *right);
Type *type_struct(const char *tag);
void struct_add_member(Type *struct_type, const char *name, Type *member_type);

/* Collects the AST_CASE / AST_DEFAULT nodes belonging to a single switch
 * body, in source order. Descends through compound statements and nested
 * control flow (blocks, if/while/do/for) but stops at nested switch
 * statements, which own their own case labels. *vec is grown with xrealloc
 * as needed; the caller owns the returned array and must free() it. */
void collect_switch_cases(ASTNode *body, ASTNode ***vec, int *n, int *cap);

#endif /* UTIL_H */
