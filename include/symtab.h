#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

typedef struct Symbol
{
    char *name;
    Type *type;
    /* variable/parameter type, or function return type */
    int stack_offset; /* local and parameter slots: -off(%rbp); globals: 0 */
    int is_global;
    int is_function;
    int is_defined;
    int param_count;
    /* Functions only: the AST_PARAM chain from the declaration, so call sites
     * can be checked against the parameter types (BUG-019). Points into the
     * AST, which outlives the symbol table. */
    ASTNode *param_list;
    /* Functions only: set when a call site resolves to this symbol. Together
     * with the first call location this lets the compiler report a function
     * that was declared and called but never defined (BUG-015). */
    int is_called;
    int call_line;
    int call_col;
    struct Symbol *next;
} Symbol;

typedef struct Scope
{
    Symbol *symbols;
    struct Scope *parent;
} Scope;

void symtab_init(void);
void scope_push(void);
void scope_pop(void);
Symbol *symbol_insert(const char *name, Type *type, int is_global);
Symbol *symbol_lookup(const char *name);
Symbol *symbol_lookup_current(const char *name);
void struct_register(const char *tag, Type *type);
Type *struct_lookup(const char *tag);
#endif /* SYMTAB_H */