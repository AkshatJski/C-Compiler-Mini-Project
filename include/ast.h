#ifndef AST_H
#define AST_H

#include "tokens.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_PARAM,
    AST_VAR_DECL,
    AST_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_BLOCK,
    AST_BINARY_OP,
    AST_POSTFIX,
    AST_VAR_REF,
    AST_INT_LIT,
    AST_CALL
} ASTNodeType;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_AND,
    OP_OR,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE
} BinaryOp;

typedef struct ASTNode {
    ASTNodeType type;
    BinaryOp op;
    char *name;         /* identifier: variable / function / parameter */
    long long value;    /* integer literal */
    int line;
    int col;

    int stack_offset;   /* filled by semantic analysis (-off(%rbp)) */
    int is_global;      /* filled by semantic analysis */
    int frame_size;     /* local frame size for functions, filled by semantic */

    struct ASTNode *left;  /* binop lhs / assign target / return expr / params /
                               for-init / postfix operand */
    struct ASTNode *right; /* binop rhs / assign value / if-else branch / for-step */
    struct ASTNode *cond;  /* if / while / for condition */
    struct ASTNode *body;  /* block statement list / if-then / while body / for body */
    struct ASTNode *args;  /* call arguments (chained via next) */
    struct ASTNode *next;  /* sibling chain: statements, params, args */
} ASTNode;

#endif /* AST_H */
