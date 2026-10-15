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
    AST_CALL,
    /* Phase 3 additions */
    AST_ADDR_OF,
    AST_DEREF,
    AST_MEMBER_ACCESS
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

typedef enum {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_STRUCT
} TypeKind;

struct Type;

typedef struct StructMember {
    char *name;
    struct Type *type;
    int offset;
    struct StructMember *next;
} StructMember;

typedef struct Type {
    TypeKind kind;
    struct Type *base;
    int array_size;
    char *struct_tag;
    StructMember *members;
    int size;
    int align;
} Type;

typedef struct ASTNode {
    ASTNodeType type;
    BinaryOp op;
    TypeKind vtype;
    Type *val_type;
    char *name;
    long long value;
    int line;
    int col;
    int stack_offset;
    int is_global;
    int frame_size;
    int case_index;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *cond;
    struct ASTNode *body;
    struct ASTNode *args;
    struct ASTNode *next;
} ASTNode;

#endif /* AST_H */