#include <stdlib.h>
#include <string.h>

#include "symtab.h"
#include "util.h"

static Scope global_scope;
static Scope *current_scope;

void symtab_init(void)
{
    global_scope.symbols = NULL;
    global_scope.parent = NULL;
    current_scope = &global_scope;
}

void scope_push(void)
{
    Scope *scope = xcalloc(1, sizeof(Scope));
    scope->parent = current_scope;
    current_scope = scope;
}

void scope_pop(void)
{
    if (current_scope->parent == NULL) {
        return; /* never pop the global scope */
    }
    Scope *victim = current_scope;
    current_scope = victim->parent;
    Symbol *sym = victim->symbols;
    while (sym != NULL) {
        Symbol *next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
    free(victim);
}

Symbol *symbol_insert(const char *name, TypeKind type, int is_global)
{
    Symbol *sym = xcalloc(1, sizeof(Symbol));
    sym->name = xstrdup(name);
    sym->type = type;
    sym->is_global = is_global;
    sym->stack_offset = 0;
    sym->is_function = 0;
    sym->param_count = 0;
    sym->next = current_scope->symbols;
    current_scope->symbols = sym;
    return sym;
}

Symbol *symbol_lookup(const char *name)
{
    for (Scope *s = current_scope; s != NULL; s = s->parent) {
        for (Symbol *sym = s->symbols; sym != NULL; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

Symbol *symbol_lookup_current(const char *name)
{
    for (Symbol *sym = current_scope->symbols; sym != NULL; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}
