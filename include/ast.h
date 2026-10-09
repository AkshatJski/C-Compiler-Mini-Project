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
    AST_DO_WHILE,
    AST_FOR,
    AST_SWITCH,
    AST_CASE,
    AST_DEFAULT,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_BLOCK,
    AST_BINARY_OP,
    AST_TERNARY,
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
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_XOR,
    OP_SHL,
    OP_SHR,
    OP_AND,
    OP_OR,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE
} BinaryOp;

/* Value type of a variable, parameter, or function return value. */
typedef enum {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_VOID
} TypeKind;

typedef struct ASTNode {
    ASTNodeType type;
    BinaryOp op;
    TypeKind vtype;     /* int/char; also a function's return type */
    char *name;         /* identifier: variable / function / parameter */
    long long value;    /* integer literal / case label constant */
    int line;
    int col;

    int stack_offset;   /* filled by semantic analysis (-off(%rbp)) */
    int is_global;      /* filled by semantic analysis */
    int frame_size;     /* local frame size for functions, filled by semantic */
    int case_index;     /* filled by codegen: index of a case label */

    struct ASTNode *left;  /* binop lhs / assign target / return expr / params /
                               for-init / postfix operand / ternary then /
                               global initializer */
    struct ASTNode *right; /* binop rhs / assign value / if-else branch /
                               for-step / ternary else */
    struct ASTNode *cond;  /* if / while / do-while / for / switch condition */
    struct ASTNode *body;  /* block statement list / if-then / loop body /
                               switch body */
    struct ASTNode *args;  /* call arguments (chained via next) */
    struct ASTNode *next;  /* sibling chain: statements, params, args */
} ASTNode;

#endif /* AST_H */
