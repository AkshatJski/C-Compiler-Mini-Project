#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include "ast.h"

void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

/* Integer-literal hand-off: text from the lexer (digits only, any U/L suffix
 * already dropped) to a value, with C's rules -- base 0, so `0x10` is 16 and
 * `010` is 8. One implementation, used by both front ends: the toy language
 * inherits C's literal syntax from the shared lexer, so it has to inherit the
 * meaning too (BUG-029). Three outcomes, because "too large" and "not a
 * number" deserve different diagnostics. */
typedef enum
{
    INT_LITERAL_OK = 0,
    INT_LITERAL_RANGE,
    INT_LITERAL_MALFORMED
} IntLiteralStatus;

IntLiteralStatus parse_integer_literal(const char *text, long long *out);

void set_source_path(const char *path);
/* Phase 6: the source text, borrowed (not copied) for diagnostics. Passing
 * NULL disables the caret snippet, which is what the token/AST dumps do. */
void set_source_text(const char *text);
void report_error(int line, int col, const char *fmt, ...);
int error_count(void);
void error_reset(void);

/* Warnings use the same file:line:col prefix but never change the exit
 * status: they are advisory only (BUG-016). `set_warnings_enabled(0)` backs
 * the -w command-line switch. */
void report_warning(int line, int col, const char *fmt, ...);
int warning_count(void);
void set_warnings_enabled(int enabled);

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

/* Renders a type into `buf` ("int", "unsigned char", "struct Point", "int **",
 * "int[4]") so diagnostics can quote the type a program actually used. */
void type_describe(const Type *t, char *buf, size_t size);

/* Phase 5 optimizer switches. One global, read by semantic analysis
 * (constant folding) and by codegen (constant conditions), and cleared by
 * `--no-opt`. Keeping one flag rather than a pass list is deliberate: with
 * two passes there is nothing to configure, and a single switch means the
 * unoptimized path is always exactly the pre-Phase-5 compiler, which is what
 * makes an optimizer bug bisectable instead of mysterious. */
extern int g_optimize;

/* Phase 7: set by --sign. When on, codegen puts one provenance string in the
 * program's read-only data so the linked binary can be traced back to this
 * compiler. */
extern int g_sign;

/* Shared nesting limit: the parser stops descending past this depth and the
 * code generator refuses to push past it, so no input can corrupt either
 * stage (BUG-009, BUG-010). */
#define MYCC_MAX_NEST 256

/* Recommended buffer size for type_describe(): deep enough for any type the
 * grammar can spell, plus the NUL. */
#define TYPE_DESCRIBE_MAX 96

/* Collects the AST_CASE / AST_DEFAULT nodes belonging to a single switch
 * body, in source order. Descends through compound statements and nested
 * control flow (blocks, if/while/do/for) but stops at nested switch
 * statements, which own their own case labels. *vec is grown with xrealloc
 * as needed; the caller owns the returned array and must free() it. */
void collect_switch_cases(ASTNode *body, ASTNode ***vec, int *n, int *cap);

#endif /* UTIL_H */
