#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "symtab.h"
#include "util.h"

typedef struct
{
    Token current;
    Token previous;
    Lexer *lexer;
} Parser;

static Parser parser;

static ASTNode *parse_expression(void);
static ASTNode *parse_assignment(void);
static ASTNode *parse_ternary(void);
static ASTNode *parse_or(void);
static ASTNode *parse_and(void);
static ASTNode *parse_bit_or(void);
static ASTNode *parse_bit_xor(void);
static ASTNode *parse_bit_and(void);
static ASTNode *parse_equality(void);
static ASTNode *parse_relational(void);
static ASTNode *parse_shift(void);
static ASTNode *parse_additive(void);
static ASTNode *parse_multiplicative(void);
static ASTNode *parse_unary(void);
static ASTNode *parse_primary(void);
static ASTNode *parse_statement(void);
static ASTNode *parse_block(void);
static ASTNode *parse_if(void);
static ASTNode *parse_while(void);
static ASTNode *parse_do_while(void);
static ASTNode *parse_for(void);
static ASTNode *parse_switch(void);
static ASTNode *parse_jump(const char *keyword);
static ASTNode *parse_return(void);
static ASTNode *parse_expr_stmt(void);
static ASTNode *parse_var_decl(Type *base_type);
static ASTNode *parse_function_after_name(char *name, Type *ret, int line, int col);

static ASTNode *ast_node(ASTNodeType type, int line, int col)
{
    ASTNode *node = xcalloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->col = col;
    node->vtype = TYPE_INT;
    node->val_type = type_int();
    return node;
}

static void parser_advance(void)
{
    if (parser.previous.value != NULL)
    {
        free(parser.previous.value);
        parser.previous.value = NULL;
    }
    parser.previous = parser.current;
    parser.current = lexer_next_token(parser.lexer);
}

static int parser_match(TokenType type)
{
    if (parser.current.type == type)
    {
        parser_advance();
        return 1;
    }
    return 0;
}

static int parser_expect(TokenType type, const char *message)
{
    if (parser.current.type == type)
    {
        parser_advance();
        return 1;
    }
    report_error(parser.current.line, parser.current.col, "%s (found %s)",
                 message, token_type_to_string(parser.current.type));
    return 0;
}

static int is_type_start(TokenType type)
{
    return type == TOKEN_KEYWORD_INT || type == TOKEN_KEYWORD_CHAR ||
           type == TOKEN_KEYWORD_SIGNED || type == TOKEN_KEYWORD_UNSIGNED ||
           type == TOKEN_KEYWORD_VOID || type == TOKEN_KEYWORD_STRUCT;
}

static Type *parse_type_specifier(void)
{
    /* `signed` / `unsigned`, optionally followed by `int` or `char`.
     * A bare `unsigned` means `unsigned int` (C's rule), and `unsigned`
     * with no base type before a declarator or `*` must still yield a
     * usable type, so the base type is resolved here rather than by the
     * caller. */
    if (parser.current.type == TOKEN_KEYWORD_SIGNED ||
        parser.current.type == TOKEN_KEYWORD_UNSIGNED)
    {
        int uns = parser.current.type == TOKEN_KEYWORD_UNSIGNED;
        parser_advance();
        if (parser.current.type == TOKEN_KEYWORD_INT)
        {
            parser_advance();
            return uns ? type_uint() : type_int();
        }
        if (parser.current.type == TOKEN_KEYWORD_CHAR)
        {
            parser_advance();
            return uns ? type_uchar() : type_char();
        }
        /* Bare `unsigned` == `unsigned int`; `signed` == `int`. */
        return uns ? type_uint() : type_int();
    }
    if (parser.current.type == TOKEN_KEYWORD_INT)
    {
        parser_advance();
        return type_int();
    }
    if (parser.current.type == TOKEN_KEYWORD_CHAR)
    {
        parser_advance();
        return type_char();
    }
    if (parser.current.type == TOKEN_KEYWORD_VOID)
    {
        parser_advance();
        return type_void();
    }
    if (parser.current.type == TOKEN_KEYWORD_STRUCT)
    {
        int sline = parser.current.line;
        int scol = parser.current.col;
        parser_advance();
        if (parser.current.type != TOKEN_IDENT)
        {
            report_error(parser.current.line, parser.current.col,
                         "expected struct tag name");
            return NULL;
        }
        char *tag = xstrdup(parser.current.value);
        parser_advance();

        /* If followed by '{', this is a struct definition */
        if (parser_match(TOKEN_LBRACE))
        {
            /* Reuse a forward-declared (or self-referenced) tag if it has no
             * members yet; otherwise this is a fresh (re)definition. The type
             * is registered *before* its members are parsed so that a
             * self-referential member such as `struct Node *next;` resolves to
             * this same type instead of an empty shadow copy. */
            Type *st = struct_lookup(tag);
            if (st != NULL && st->members != NULL)
            {
                /* A struct type can only be defined once; silently re-binding
                 * the tag to a fresh type would leave earlier objects on the
                 * old layout while later ones use the new one. */
                report_error(sline, scol, "redefinition of 'struct %s'", tag);
                st = type_struct(tag);
                struct_register(tag, st);
            }
            else if (st == NULL)
            {
                st = type_struct(tag);
                struct_register(tag, st);
            }
            while (parser.current.type != TOKEN_RBRACE && parser.current.type != TOKEN_EOF)
            {
                Type *base = parse_type_specifier();
                if (base == NULL)
                {
                    free(tag);
                    return NULL;
                }
                while (parser_match(TOKEN_STAR))
                {
                    base = type_ptr(base);
                }
                if (parser.current.type != TOKEN_IDENT)
                {
                    report_error(parser.current.line, parser.current.col,
                                 "expected member name");
                    free(tag);
                    return NULL;
                }
                char *mname = xstrdup(parser.current.value);
                parser_advance();
                if (base == st || !type_is_complete(base))
                {
                    /* Rejects `struct A { struct A a; }` (direct containment,
                     * even after earlier members made the size non-zero) and
                     * members of an undefined struct type. */
                    report_error(parser.previous.line, parser.previous.col,
                                 "member '%s' has incomplete type", mname);
                }
                struct_add_member(st, mname, base);
                free(mname);
                if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after struct member"))
                {
                    free(tag);
                    return NULL;
                }
            }
            if (!parser_expect(TOKEN_RBRACE, "expected '}' after struct body"))
            {
                free(tag);
                return NULL;
            }
            /* Align total struct size to struct->align */
            if (st->align > 0)
            {
                st->size = (st->size + st->align - 1) / st->align * st->align;
            }
            free(tag);
            return st;
        }
        else
        {
            /* Struct declaration reference (e.g. struct Point p;) */
            Type *st = struct_lookup(tag);
            if (st == NULL)
            {
                /* Forward/new struct type */
                st = type_struct(tag);
                struct_register(tag, st);
            }
            free(tag);
            return st;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Expressions                                                         */
/* ------------------------------------------------------------------ */

static ASTNode *parse_expression(void)
{
    return parse_assignment();
}

static ASTNode *parse_assignment(void)
{
    ASTNode *left = parse_ternary();
    if (left == NULL)
    {
        return NULL;
    }
    int plain = 0;
    BinaryOp comp_op = OP_ADD;
    switch (parser.current.type)
    {
    case TOKEN_ASSIGN:
        plain = 1;
        break;
    case TOKEN_PLUS_EQUAL:
        comp_op = OP_ADD;
        break;
    case TOKEN_MINUS_EQUAL:
        comp_op = OP_SUB;
        break;
    case TOKEN_STAR_EQUAL:
        comp_op = OP_MUL;
        break;
    case TOKEN_SLASH_EQUAL:
        comp_op = OP_DIV;
        break;
    case TOKEN_PERCENT_EQUAL:
        comp_op = OP_MOD;
        break;
    case TOKEN_AMPERSAND_EQUAL:
        comp_op = OP_BIT_AND;
        break;
    case TOKEN_PIPE_EQUAL:
        comp_op = OP_BIT_OR;
        break;
    case TOKEN_CARET_EQUAL:
        comp_op = OP_BIT_XOR;
        break;
    case TOKEN_LESS_LESS_EQUAL:
        comp_op = OP_SHL;
        break;
    case TOKEN_GREATER_GREATER_EQUAL:
        comp_op = OP_SHR;
        break;
    default:
        return left;
    }

    if (left->type != AST_VAR_REF && left->type != AST_DEREF && left->type != AST_MEMBER_ACCESS)
    {
        report_error(parser.current.line, parser.current.col,
                     "left side of assignment must be an lvalue");
        return NULL;
    }

    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *right = parse_assignment();
    if (right == NULL)
    {
        return NULL;
    }

    if (!plain)
    {
        ASTNode *binop = ast_node(AST_BINARY_OP, line, col);
        binop->op = comp_op;
        binop->left = left;
        binop->right = right;
        right = binop;
    }

    ASTNode *node = ast_node(AST_ASSIGN, left->line, left->col);
    node->left = left;
    node->right = right;
    return node;
}

static ASTNode *parse_ternary(void)
{
    ASTNode *cond = parse_or();
    if (cond == NULL)
    {
        return NULL;
    }
    if (parser.current.type != TOKEN_QUESTION)
    {
        return cond;
    }
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *then_expr = parse_expression();
    if (then_expr == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_COLON, "expected ':' in conditional expression"))
    {
        return NULL;
    }
    ASTNode *else_expr = parse_assignment();
    if (else_expr == NULL)
    {
        return NULL;
    }
    ASTNode *node = ast_node(AST_TERNARY, line, col);
    node->cond = cond;
    node->left = then_expr;
    node->right = else_expr;
    return node;
}

static ASTNode *parse_or(void)
{
    ASTNode *left = parse_and();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        if (parser.current.type != TOKEN_PIPE_PIPE)
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_and();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_OR;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_and(void)
{
    ASTNode *left = parse_bit_or();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        if (parser.current.type != TOKEN_AMPERSAND_AMPERSAND)
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_bit_or();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_AND;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_bit_or(void)
{
    ASTNode *left = parse_bit_xor();
    if (left == NULL)
    {
        return NULL;
    }
    while (parser.current.type == TOKEN_PIPE)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_bit_xor();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_BIT_OR;
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_bit_xor(void)
{
    ASTNode *left = parse_bit_and();
    if (left == NULL)
    {
        return NULL;
    }
    while (parser.current.type == TOKEN_CARET)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_bit_and();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_BIT_XOR;
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_bit_and(void)
{
    ASTNode *left = parse_equality();
    if (left == NULL)
    {
        return NULL;
    }
    while (parser.current.type == TOKEN_AMPERSAND)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_equality();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_BIT_AND;
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

static ASTNode *parse_equality(void)
{
    ASTNode *left = parse_relational();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (parser.current.type == TOKEN_EQUALS_EQUALS)
        {
            op = OP_EQ;
        }
        else if (parser.current.type == TOKEN_BANG_EQUALS)
        {
            op = OP_NE;
        }
        else
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_relational();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = op;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_relational(void)
{
    ASTNode *left = parse_shift();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (parser.current.type == TOKEN_LESS)
        {
            op = OP_LT;
        }
        else if (parser.current.type == TOKEN_LESS_EQUAL)
        {
            op = OP_LE;
        }
        else if (parser.current.type == TOKEN_GREATER)
        {
            op = OP_GT;
        }
        else if (parser.current.type == TOKEN_GREATER_EQUAL)
        {
            op = OP_GE;
        }
        else
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_shift();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = op;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_shift(void)
{
    ASTNode *left = parse_additive();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (parser.current.type == TOKEN_LESS_LESS)
        {
            op = OP_SHL;
        }
        else if (parser.current.type == TOKEN_GREATER_GREATER)
        {
            op = OP_SHR;
        }
        else
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_additive();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = op;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_additive(void)
{
    ASTNode *left = parse_multiplicative();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (parser.current.type == TOKEN_PLUS)
        {
            op = OP_ADD;
        }
        else if (parser.current.type == TOKEN_MINUS)
        {
            op = OP_SUB;
        }
        else
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_multiplicative();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = op;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_multiplicative(void)
{
    ASTNode *left = parse_unary();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (parser.current.type == TOKEN_STAR)
        {
            op = OP_MUL;
        }
        else if (parser.current.type == TOKEN_SLASH)
        {
            op = OP_DIV;
        }
        else if (parser.current.type == TOKEN_PERCENT)
        {
            op = OP_MOD;
        }
        else
        {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_unary();
        if (right == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = op;
        node->left = left;
        node->right = right;
        left = node;
    }
}

static ASTNode *parse_unary(void)
{
    if (parser.current.type == TOKEN_MINUS)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL)
        {
            return NULL;
        }
        ASTNode *zero = ast_node(AST_INT_LIT, line, col);
        zero->value = 0;
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_SUB;
        node->left = zero;
        node->right = operand;
        return node;
    }
    if (parser.current.type == TOKEN_PLUS)
    {
        parser_advance();
        return parse_unary();
    }
    if (parser.current.type == TOKEN_BANG)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL)
        {
            return NULL;
        }
        ASTNode *zero = ast_node(AST_INT_LIT, line, col);
        zero->value = 0;
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_EQ;
        node->left = operand;
        node->right = zero;
        return node;
    }
    if (parser.current.type == TOKEN_TILDE)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL)
        {
            return NULL;
        }
        ASTNode *minus_one = ast_node(AST_INT_LIT, line, col);
        minus_one->value = -1;
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_BIT_XOR;
        node->left = operand;
        node->right = minus_one;
        return node;
    }
    if (parser.current.type == TOKEN_PLUS_PLUS ||
        parser.current.type == TOKEN_MINUS_MINUS)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        int is_inc = parser.current.type == TOKEN_PLUS_PLUS;
        parser_advance();
        ASTNode *target = parse_unary();
        if (target == NULL)
        {
            return NULL;
        }
        if (target->type != AST_VAR_REF && target->type != AST_DEREF &&
            target->type != AST_MEMBER_ACCESS)
        {
            report_error(line, col, "'++'/'--' operand must be an lvalue");
            return NULL;
        }
        ASTNode *one = ast_node(AST_INT_LIT, line, col);
        one->value = 1;
        ASTNode *binop = ast_node(AST_BINARY_OP, line, col);
        binop->op = is_inc ? OP_ADD : OP_SUB;
        binop->left = target;
        binop->right = one;
        ASTNode *assign = ast_node(AST_ASSIGN, line, col);
        assign->left = target;
        assign->right = binop;
        return assign;
    }
    if (parser.current.type == TOKEN_AMPERSAND)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_ADDR_OF, line, col);
        node->left = operand;
        return node;
    }
    if (parser.current.type == TOKEN_STAR)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL)
        {
            return NULL;
        }
        ASTNode *node = ast_node(AST_DEREF, line, col);
        node->left = operand;
        return node;
    }
    return parse_primary();
}

static ASTNode *parse_primary(void)
{
    ASTNode *node = NULL;
    if (parser.current.type == TOKEN_INT_LIT ||
        parser.current.type == TOKEN_CHAR_LIT)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        node = ast_node(AST_INT_LIT, line, col);
        errno = 0;
        long long value = strtoll(parser.current.value, NULL, 0);
        if (errno == ERANGE)
        {
            report_error(line, col, "integer literal '%s' out of range",
                         parser.current.value);
            return NULL;
        }
        node->value = value;
        parser_advance();
    }
    else if (parser.current.type == TOKEN_IDENT)
    {
        int line = parser.current.line;
        int col = parser.current.col;
        char *name = xstrdup(parser.current.value);
        parser_advance();
        if (parser.current.type == TOKEN_LPAREN)
        {
            parser_advance();
            ASTNode *call = ast_node(AST_CALL, line, col);
            call->name = name;
            ASTNode **tail = &call->args;
            if (parser.current.type != TOKEN_RPAREN)
            {
                for (;;)
                {
                    ASTNode *arg = parse_expression();
                    if (arg == NULL)
                    {
                        free(name);
                        return NULL;
                    }
                    *tail = arg;
                    tail = &arg->next;
                    if (!parser_match(TOKEN_COMMA))
                    {
                        break;
                    }
                }
            }
            if (!parser_expect(TOKEN_RPAREN, "expected ')' after arguments"))
            {
                free(name);
                return NULL;
            }
            node = call;
        }
        else
        {
            ASTNode *ref = ast_node(AST_VAR_REF, line, col);
            ref->name = name;
            node = ref;
        }
    }
    else if (parser_match(TOKEN_LPAREN))
    {
        node = parse_expression();
        if (node == NULL)
        {
            return NULL;
        }
        if (!parser_expect(TOKEN_RPAREN, "expected ')' after expression"))
        {
            return NULL;
        }
    }
    else
    {
        report_error(parser.current.line, parser.current.col,
                     "expected expression (found %s)",
                     token_type_to_string(parser.current.type));
        return NULL;
    }

    for (;;)
    {
        if (parser.current.type == TOKEN_PLUS_PLUS ||
            parser.current.type == TOKEN_MINUS_MINUS)
        {
            if (node->type != AST_VAR_REF && node->type != AST_DEREF &&
                node->type != AST_MEMBER_ACCESS)
            {
                report_error(parser.current.line, parser.current.col,
                             "'++'/'--' operand must be an lvalue");
                return NULL;
            }
            int line = parser.current.line;
            int col = parser.current.col;
            int is_inc = parser.current.type == TOKEN_PLUS_PLUS;
            parser_advance();
            ASTNode *post = ast_node(AST_POSTFIX, line, col);
            post->op = is_inc ? OP_ADD : OP_SUB;
            post->left = node;
            node = post;
        }
        else if (parser_match(TOKEN_LBRACKET))
        {
            int line = parser.previous.line;
            int col = parser.previous.col;
            ASTNode *idx = parse_expression();
            if (idx == NULL)
                return NULL;
            if (!parser_expect(TOKEN_RBRACKET, "expected ']' after index"))
            {
                return NULL;
            }
            /* Desugar arr[i] -> *(arr + i) */
            ASTNode *add = ast_node(AST_BINARY_OP, line, col);
            add->op = OP_ADD;
            add->left = node;
            add->right = idx;

            ASTNode *deref = ast_node(AST_DEREF, line, col);
            deref->left = add;
            node = deref;
        }
        else if (parser_match(TOKEN_DOT))
        {
            int line = parser.previous.line;
            int col = parser.previous.col;
            if (parser.current.type != TOKEN_IDENT)
            {
                report_error(parser.current.line, parser.current.col,
                             "expected member name after '.'");
                return NULL;
            }
            char *mname = xstrdup(parser.current.value);
            parser_advance();
            ASTNode *mem = ast_node(AST_MEMBER_ACCESS, line, col);
            mem->name = mname;
            mem->left = node;
            node = mem;
        }
        else if (parser_match(TOKEN_ARROW))
        {
            int line = parser.previous.line;
            int col = parser.previous.col;
            if (parser.current.type != TOKEN_IDENT)
            {
                report_error(parser.current.line, parser.current.col,
                             "expected member name after '->'");
                return NULL;
            }
            char *mname = xstrdup(parser.current.value);
            parser_advance();

            /* Desugar ptr->field into (*ptr).field */
            ASTNode *deref = ast_node(AST_DEREF, line, col);
            deref->left = node;

            ASTNode *mem = ast_node(AST_MEMBER_ACCESS, line, col);
            mem->name = mname;
            mem->left = deref;
            node = mem;
        }
        else
        {
            break;
        }
    }
    return node;
}

/* ------------------------------------------------------------------ */
/* Statements                                                          */
/* ------------------------------------------------------------------ */

/* Reads zero or more `[N]` dimensions following a declarator name and
 * returns the type with every dimension applied. Returns NULL if a
 * dimension is malformed (the diagnostic is reported here), and sets
 * *is_array when at least one dimension was consumed.
 *
 * C reads dimensions left to right but nests them inside out: `int a[2][3]`
 * is "2 rows of 3 ints", so the type must be built as
 * array[2] of (array[3] of int). Applying them in source order instead
 * would produce array[3] of array[2] of int -- the same total size but a
 * 16-byte row stride where 24 is required, silently corrupting a[i][j].
 * The dimensions are therefore collected first and applied in reverse. */
static Type *parse_array_dimensions(Type *base, int *is_array)
{
    enum { MAX_DIMS = 8 };
    int dims[MAX_DIMS];
    int ndims = 0;

    while (parser_match(TOKEN_LBRACKET))
    {
        if (parser.current.type != TOKEN_INT_LIT)
        {
            report_error(parser.current.line, parser.current.col,
                         "array size must be an integer constant");
            return NULL;
        }
        if (ndims == MAX_DIMS)
        {
            report_error(parser.current.line, parser.current.col,
                         "too many array dimensions (limit is %d)", MAX_DIMS);
            return NULL;
        }
        dims[ndims++] = (int)strtoll(parser.current.value, NULL, 0);
        parser_advance();
        if (!parser_expect(TOKEN_RBRACKET, "expected ']' after array size"))
        {
            return NULL;
        }
    }
    if (ndims == 0)
    {
        return base;
    }
    for (int i = ndims - 1; i >= 0; i--)
    {
        base = type_array(base, dims[i]);
    }
    *is_array = 1;
    return base;
}

static ASTNode *parse_var_decl(Type *base_type)
{
    ASTNode *first = NULL;
    ASTNode **tail = &first;
    for (;;)
    {
        Type *decl_type = base_type;
        while (parser_match(TOKEN_STAR))
        {
            decl_type = type_ptr(decl_type);
        }
        if (parser.current.type != TOKEN_IDENT)
        {
            report_error(parser.current.line, parser.current.col,
                         "expected variable name (found %s)",
                         token_type_to_string(parser.current.type));
            return NULL;
        }
        ASTNode *decl = ast_node(AST_VAR_DECL, parser.current.line,
                                 parser.current.col);
        decl->vtype = decl_type->kind;
        decl->val_type = decl_type;
        decl->name = xstrdup(parser.current.value);
        parser_advance();
        /* Array dimensions: int arr[10], matrix[2][3] */
        int is_array = 0;
        decl_type = parse_array_dimensions(decl_type, &is_array);
        if (decl_type == NULL)
        {
            return NULL;
        }
        decl->vtype = decl_type->kind;
        decl->val_type = decl_type;
        if (parser_match(TOKEN_ASSIGN))
        {
            if (is_array)
            {
                /* This subset has no initializer lists, so a scalar cannot
                 * initialize an array. Reject rather than silently emitting a
                 * store that clobbers part of the array. */
                report_error(parser.previous.line, parser.previous.col,
                             "array '%s' cannot be initialized with a scalar "
                             "(initializer lists are not supported)",
                             decl->name);
                return NULL;
            }
            ASTNode *init = parse_expression();
            if (init == NULL)
            {
                return NULL;
            }
            ASTNode *ref = ast_node(AST_VAR_REF, decl->line, decl->col);
            ref->name = xstrdup(decl->name);
            ASTNode *assign = ast_node(AST_ASSIGN, decl->line, decl->col);
            assign->left = ref;
            assign->right = init;
            decl->next = assign;
        }
        *tail = decl;
        tail = &decl->next;
        while (*tail != NULL)
        {
            tail = &(*tail)->next;
        }
        if (!parser_match(TOKEN_COMMA))
        {
            break;
        }
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after declaration"))
    {
        return NULL;
    }
    return first;
}

static ASTNode *parse_block(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    if (!parser_expect(TOKEN_LBRACE, "expected '{'"))
    {
        return NULL;
    }
    ASTNode *block = ast_node(AST_BLOCK, line, col);
    ASTNode **tail = &block->body;
    while (parser.current.type != TOKEN_RBRACE &&
           parser.current.type != TOKEN_EOF)
    {
        ASTNode *stmt;
        if (is_type_start(parser.current.type))
        {
            Type *type = parse_type_specifier();
            if (type == NULL)
            {
                return NULL;
            }
            stmt = parse_var_decl(type);
        }
        else
        {
            stmt = parse_statement();
        }
        if (stmt == NULL)
        {
            return NULL;
        }
        *tail = stmt;
        tail = &stmt->next;
        while (*tail != NULL)
        {
            tail = &(*tail)->next;
        }
    }
    if (!parser_expect(TOKEN_RBRACE, "expected '}' after block"))
    {
        return NULL;
    }
    return block;
}

static ASTNode *parse_if(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(AST_IF, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'if'"))
    {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after if condition"))
    {
        return NULL;
    }
    node->body = parse_statement();
    if (node->body == NULL)
    {
        return NULL;
    }
    if (parser_match(TOKEN_KEYWORD_ELSE))
    {
        node->right = parse_statement();
        if (node->right == NULL)
        {
            return NULL;
        }
    }
    return node;
}

static ASTNode *parse_while(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(AST_WHILE, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'while'"))
    {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after while condition"))
    {
        return NULL;
    }
    node->body = parse_statement();
    if (node->body == NULL)
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_do_while(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(AST_DO_WHILE, line, col);
    node->body = parse_statement();
    if (node->body == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_KEYWORD_WHILE, "expected 'while' after 'do' body"))
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'while'"))
    {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after do-while condition"))
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after do-while"))
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_for(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(AST_FOR, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'for'"))
    {
        return NULL;
    }
    if (parser.current.type == TOKEN_SEMICOLON)
    {
        parser_advance();
    }
    else if (is_type_start(parser.current.type))
    {
        Type *type = parse_type_specifier();
        if (type == NULL)
        {
            return NULL;
        }
        node->left = parse_var_decl(type);
        if (node->left == NULL)
        {
            return NULL;
        }
    }
    else
    {
        node->left = parse_expr_stmt();
        if (node->left == NULL)
        {
            return NULL;
        }
    }
    if (parser.current.type != TOKEN_SEMICOLON)
    {
        node->cond = parse_expression();
        if (node->cond == NULL)
        {
            return NULL;
        }
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after for condition"))
    {
        return NULL;
    }
    if (parser.current.type != TOKEN_RPAREN)
    {
        node->right = parse_expression();
        if (node->right == NULL)
        {
            return NULL;
        }
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after for clauses"))
    {
        return NULL;
    }
    node->body = parse_statement();
    if (node->body == NULL)
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_switch(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(AST_SWITCH, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'switch'"))
    {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after switch expression"))
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_LBRACE, "expected '{' after switch"))
    {
        return NULL;
    }
    ASTNode **tail = &node->body;
    while (parser.current.type != TOKEN_RBRACE &&
           parser.current.type != TOKEN_EOF)
    {
        ASTNode *item = NULL;
        if (parser.current.type == TOKEN_KEYWORD_CASE)
        {
            int cline = parser.current.line;
            int ccol = parser.current.col;
            parser_advance();
            ASTNode *expr = parse_expression();
            if (expr == NULL)
            {
                return NULL;
            }
            if (!parser_expect(TOKEN_COLON, "expected ':' after case label"))
            {
                return NULL;
            }
            item = ast_node(AST_CASE, cline, ccol);
            item->left = expr;
        }
        else if (parser.current.type == TOKEN_KEYWORD_DEFAULT)
        {
            int dline = parser.current.line;
            int dcol = parser.current.col;
            parser_advance();
            if (!parser_expect(TOKEN_COLON, "expected ':' after 'default'"))
            {
                return NULL;
            }
            item = ast_node(AST_DEFAULT, dline, dcol);
        }
        else if (is_type_start(parser.current.type))
        {
            Type *type = parse_type_specifier();
            if (type == NULL)
            {
                return NULL;
            }
            item = parse_var_decl(type);
        }
        else
        {
            item = parse_statement();
        }
        if (item == NULL)
        {
            return NULL;
        }
        *tail = item;
        tail = &item->next;
        while (*tail != NULL)
        {
            tail = &(*tail)->next;
        }
    }
    if (!parser_expect(TOKEN_RBRACE, "expected '}' after switch body"))
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_jump(const char *keyword)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(strcmp(keyword, "break") == 0 ? AST_BREAK
                                                           : AST_CONTINUE,
                             line, col);
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after statement"))
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_return(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *node = ast_node(AST_RETURN, line, col);
    if (parser.current.type != TOKEN_SEMICOLON)
    {
        node->left = parse_expression();
        if (node->left == NULL)
        {
            return NULL;
        }
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after return"))
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_expr_stmt(void)
{
    ASTNode *expr = parse_expression();
    if (expr == NULL)
    {
        return NULL;
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after expression"))
    {
        return NULL;
    }
    return expr;
}

static ASTNode *parse_statement(void)
{
    switch (parser.current.type)
    {
    case TOKEN_LBRACE:
        return parse_block();
    case TOKEN_KEYWORD_IF:
        return parse_if();
    case TOKEN_KEYWORD_WHILE:
        return parse_while();
    case TOKEN_KEYWORD_DO:
        return parse_do_while();
    case TOKEN_KEYWORD_FOR:
        return parse_for();
    case TOKEN_KEYWORD_SWITCH:
        return parse_switch();
    case TOKEN_KEYWORD_BREAK:
        return parse_jump("break");
    case TOKEN_KEYWORD_CONTINUE:
        return parse_jump("continue");
    case TOKEN_KEYWORD_CASE:
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *n = ast_node(AST_CASE, line, col);
        n->left = parse_expression();
        if (n->left == NULL)
        {
            return NULL;
        }
        if (!parser_expect(TOKEN_COLON, "expected ':' after case label"))
        {
            return NULL;
        }
        return n;
    }
    case TOKEN_KEYWORD_DEFAULT:
    {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        if (!parser_expect(TOKEN_COLON, "expected ':' after 'default'"))
        {
            return NULL;
        }
        return ast_node(AST_DEFAULT, line, col);
    }
    case TOKEN_KEYWORD_RETURN:
        return parse_return();
    case TOKEN_SEMICOLON:
        parser_advance();
        return ast_node(AST_BLOCK, parser.previous.line, parser.previous.col);
    default:
        return parse_expr_stmt();
    }
}

/* ------------------------------------------------------------------ */
/* Top level                                                           */
/* ------------------------------------------------------------------ */

static int parse_param_list(ASTNode *fn)
{
    if (parser.current.type == TOKEN_RPAREN)
    {
        parser_advance();
        return 1;
    }
    if (parser.current.type == TOKEN_KEYWORD_VOID)
    {
        parser_advance();
        return parser_expect(TOKEN_RPAREN,
                             "expected ')' after 'void' parameter list");
    }
    ASTNode **tail = &fn->left;
    for (;;)
    {
        if (!is_type_start(parser.current.type))
        {
            report_error(parser.current.line, parser.current.col,
                         "expected parameter type (found %s)",
                         token_type_to_string(parser.current.type));
            return 0;
        }
        Type *ptype = parse_type_specifier();
        if (ptype == NULL)
        {
            return 0;
        }
        while (parser_match(TOKEN_STAR))
        {
            ptype = type_ptr(ptype);
        }
        if (!type_is_complete(ptype))
        {
            /* e.g. an incomplete struct passed by value, or a non-pointer
             * `void` parameter after the first. */
            report_error(parser.current.line, parser.current.col,
                         "parameter has incomplete type");
        }
        if (parser.current.type != TOKEN_IDENT)
        {
            report_error(parser.current.line, parser.current.col,
                         "expected parameter name");
            return 0;
        }
        ASTNode *param = ast_node(AST_PARAM, parser.current.line,
                                  parser.current.col);
        param->vtype = ptype->kind;
        param->val_type = ptype;
        param->name = xstrdup(parser.current.value);
        parser_advance();

        *tail = param;
        tail = &param->next;
        if (!parser_match(TOKEN_COMMA))
        {
            break;
        }
    }
    return parser_expect(TOKEN_RPAREN, "expected ')' after parameters");
}

static ASTNode *parse_function_after_name(char *name, Type *ret, int line, int col)
{
    ASTNode *fn = ast_node(AST_FUNC_DECL, line, col);
    fn->name = name;
    fn->vtype = ret->kind;
    fn->val_type = ret;

    if (!parser_expect(TOKEN_LPAREN, "expected '(' after function name"))
    {
        return NULL;
    }
    if (!parse_param_list(fn))
    {
        return NULL;
    }

    /* Check if this is a prototype (ends with ';') or a definition (starts with '{') */
    if (parser_match(TOKEN_SEMICOLON))
    {
        fn->body = NULL; /* Prototype has no body */
        return fn;
    }

    fn->body = parse_block();
    if (fn->body == NULL)
    {
        return NULL;
    }
    return fn;
}

static ASTNode *parse_global_vars(char *first_name, Type *base_type, int line,
                                  int col)
{
    ASTNode *first = NULL;
    ASTNode **tail = &first;
    char *name = first_name;
    for (;;)
    {
        Type *decl_type = base_type;
        while (parser_match(TOKEN_STAR))
        {
            decl_type = type_ptr(decl_type);
        }
        int is_array = 0;
        decl_type = parse_array_dimensions(decl_type, &is_array);
        if (decl_type == NULL)
        {
            return NULL;
        }
        ASTNode *decl = ast_node(AST_VAR_DECL, line, col);
        decl->vtype = decl_type->kind;
        decl->val_type = decl_type;
        decl->name = name;
        decl->is_global = 1;
        if (parser_match(TOKEN_ASSIGN))
        {
            if (decl_type->kind == TYPE_ARRAY || decl_type->kind == TYPE_STRUCT)
            {
                /* gen_data() emits .zero for aggregates and drops any parsed
                 * initializer, so accepting one here would silently discard it.
                 * This subset has no aggregate initializer lists. */
                report_error(parser.previous.line, parser.previous.col,
                             "aggregate '%s' cannot be initialized with a "
                             "scalar (initializer lists are not supported)",
                             name);
                return NULL;
            }
            decl->left = parse_expression();
            if (decl->left == NULL)
            {
                return NULL;
            }
        }
        *tail = decl;
        tail = &decl->next;
        if (!parser_match(TOKEN_COMMA))
        {
            break;
        }
        if (parser.current.type != TOKEN_IDENT)
        {
            report_error(parser.current.line, parser.current.col,
                         "expected variable name after ','");
            return NULL;
        }
        name = xstrdup(parser.current.value);
        parser_advance();
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after global declaration"))
    {
        return NULL;
    }
    return first;
}
ASTNode *parse_program(Lexer *lexer)
{
    parser.lexer = lexer;
    parser.previous.type = TOKEN_EOF;
    parser.previous.value = NULL;
    parser.current = lexer_next_token(lexer);
    ASTNode *prog = ast_node(AST_PROGRAM, 1, 1);
    ASTNode **tail = &prog->body;
    while (parser.current.type != TOKEN_EOF)
    {
        if (!is_type_start(parser.current.type))
        {
            report_error(parser.current.line, parser.current.col,
                         "expected top-level declaration starting with a type "
                         "(found %s)",
                         token_type_to_string(parser.current.type));
            return NULL;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        Type *type = parse_type_specifier();
        if (type == NULL)
        {
            return NULL;
        }
        /* A standalone struct definition or forward declaration, e.g.
         *   struct Point { int x; int y; };
         * introduces a type but declares no variable, so there is no node to
         * add to the program. */
        if (parser_match(TOKEN_SEMICOLON))
        {
            continue;
        }
        if (parser.current.type != TOKEN_IDENT)
        {
            report_error(parser.current.line, parser.current.col,
                         "expected name after type");
            return NULL;
        }
        char *name = xstrdup(parser.current.value);
        parser_advance();
        ASTNode *decl = NULL;
        if (parser.current.type == TOKEN_LPAREN)
        {
            if (type->kind == TYPE_CHAR)
            {
                report_error(line, col,
                             "function return type 'char' is not supported");
            }
            else if (type->kind == TYPE_STRUCT)
            {
                /* Returning a struct by value needs a hidden-pointer ABI that
                 * this compiler does not implement; reject it with a clear
                 * message instead of failing during code generation. */
                report_error(line, col,
                             "function return type 'struct' is not supported");
            }
            decl = parse_function_after_name(name, type, line, col);
            if (decl == NULL)
            {
                free(name);
                return NULL;
            }
        }
        else
        {
            if (type->kind == TYPE_VOID)
            {
                report_error(line, col, "variable cannot have type 'void'");
                free(name);
                return NULL;
            }
            decl = parse_global_vars(name, type, line, col);
            if (decl == NULL)
            {
                free(name);
                return NULL;
            }
        }
        *tail = decl;
        tail = &decl->next;
        while (*tail != NULL)
        {
            tail = &(*tail)->next;
        }
    }
    free(parser.current.value);
    free(parser.previous.value);
    return prog;
}