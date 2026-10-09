#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "util.h"

typedef struct {
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
static ASTNode *parse_var_decl(TypeKind type);
static ASTNode *parse_function_after_name(char *name, TypeKind ret, int line, int col);

static ASTNode *ast_node(ASTNodeType type, int line, int col)
{
    ASTNode *node = xcalloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->col = col;
    node->vtype = TYPE_INT;
    return node;
}

static void parser_advance(void)
{
    if (parser.previous.value != NULL) {
        free(parser.previous.value);
        parser.previous.value = NULL;
    }
    parser.previous = parser.current;
    parser.current = lexer_next_token(parser.lexer);
}

static int parser_match(TokenType type)
{
    if (parser.current.type == type) {
        parser_advance();
        return 1;
    }
    return 0;
}

static int parser_expect(TokenType type, const char *message)
{
    if (parser.current.type == type) {
        parser_advance();
        return 1;
    }
    report_error(parser.current.line, parser.current.col, "%s (found %s)",
                 message, token_type_to_string(parser.current.type));
    return 0;
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
    if (left == NULL) {
        return NULL;
    }
    int plain = 0;
    BinaryOp comp_op = OP_ADD;
    switch (parser.current.type) {
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
    if (left->type != AST_VAR_REF) {
        report_error(parser.current.line, parser.current.col,
                     "left side of assignment must be a variable");
        return NULL;
    }
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *right = parse_assignment();
    if (right == NULL) {
        return NULL;
    }
    if (!plain) {
        /* Desugar "x op= y" into "x = x op y". */
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
    if (cond == NULL) {
        return NULL;
    }
    if (parser.current.type != TOKEN_QUESTION) {
        return cond;
    }
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance();
    ASTNode *then_expr = parse_expression();
    if (then_expr == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_COLON, "expected ':' in conditional expression")) {
        return NULL;
    }
    ASTNode *else_expr = parse_assignment();
    if (else_expr == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        if (parser.current.type != TOKEN_PIPE_PIPE) {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_and();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        if (parser.current.type != TOKEN_AMPERSAND_AMPERSAND) {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_bit_or();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    while (parser.current.type == TOKEN_PIPE) {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_bit_xor();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    while (parser.current.type == TOKEN_CARET) {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_bit_and();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    while (parser.current.type == TOKEN_AMPERSAND) {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_equality();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        BinaryOp op;
        if (parser.current.type == TOKEN_EQUALS_EQUALS) {
            op = OP_EQ;
        } else if (parser.current.type == TOKEN_BANG_EQUALS) {
            op = OP_NE;
        } else {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_relational();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        BinaryOp op;
        if (parser.current.type == TOKEN_LESS) {
            op = OP_LT;
        } else if (parser.current.type == TOKEN_LESS_EQUAL) {
            op = OP_LE;
        } else if (parser.current.type == TOKEN_GREATER) {
            op = OP_GT;
        } else if (parser.current.type == TOKEN_GREATER_EQUAL) {
            op = OP_GE;
        } else {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_shift();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        BinaryOp op;
        if (parser.current.type == TOKEN_LESS_LESS) {
            op = OP_SHL;
        } else if (parser.current.type == TOKEN_GREATER_GREATER) {
            op = OP_SHR;
        } else {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_additive();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        BinaryOp op;
        if (parser.current.type == TOKEN_PLUS) {
            op = OP_ADD;
        } else if (parser.current.type == TOKEN_MINUS) {
            op = OP_SUB;
        } else {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_multiplicative();
        if (right == NULL) {
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
    if (left == NULL) {
        return NULL;
    }
    for (;;) {
        BinaryOp op;
        if (parser.current.type == TOKEN_STAR) {
            op = OP_MUL;
        } else if (parser.current.type == TOKEN_SLASH) {
            op = OP_DIV;
        } else if (parser.current.type == TOKEN_PERCENT) {
            op = OP_MOD;
        } else {
            return left;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *right = parse_unary();
        if (right == NULL) {
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
    if (parser.current.type == TOKEN_MINUS) {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL) {
            return NULL;
        }
        /* Desugar -x into 0 - x. */
        ASTNode *zero = ast_node(AST_INT_LIT, line, col);
        zero->value = 0;
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_SUB;
        node->left = zero;
        node->right = operand;
        return node;
    }
    if (parser.current.type == TOKEN_PLUS) {
        parser_advance();
        return parse_unary();
    }
    if (parser.current.type == TOKEN_BANG) {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL) {
            return NULL;
        }
        /* Desugar !x into (x == 0). */
        ASTNode *zero = ast_node(AST_INT_LIT, line, col);
        zero->value = 0;
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_EQ;
        node->left = operand;
        node->right = zero;
        return node;
    }
    if (parser.current.type == TOKEN_TILDE) {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *operand = parse_unary();
        if (operand == NULL) {
            return NULL;
        }
        /* Desugar ~x into (x ^ -1). */
        ASTNode *minus_one = ast_node(AST_INT_LIT, line, col);
        minus_one->value = -1;
        ASTNode *node = ast_node(AST_BINARY_OP, line, col);
        node->op = OP_BIT_XOR;
        node->left = operand;
        node->right = minus_one;
        return node;
    }
    if (parser.current.type == TOKEN_PLUS_PLUS ||
        parser.current.type == TOKEN_MINUS_MINUS) {
        /* Prefix ++/-- desugar to x = x +- 1. */
        int line = parser.current.line;
        int col = parser.current.col;
        int is_inc = parser.current.type == TOKEN_PLUS_PLUS;
        parser_advance();
        ASTNode *target = parse_unary();
        if (target == NULL) {
            return NULL;
        }
        if (target->type != AST_VAR_REF) {
            report_error(line, col, "'++'/'--' operand must be a variable");
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
    return parse_primary();
}

static ASTNode *parse_primary(void)
{
    ASTNode *node = NULL;

    if (parser.current.type == TOKEN_INT_LIT ||
        parser.current.type == TOKEN_CHAR_LIT) {
        int line = parser.current.line;
        int col = parser.current.col;
        node = ast_node(AST_INT_LIT, line, col);
        errno = 0;
        /* base 0 lets the lexer's 0x/0 prefixes work as hex/octal */
        long long value = strtoll(parser.current.value, NULL, 0);
        if (errno == ERANGE) {
            report_error(line, col, "integer literal '%s' out of range",
                         parser.current.value);
            return NULL;
        }
        node->value = value;
        parser_advance();
    } else if (parser.current.type == TOKEN_IDENT) {
        int line = parser.current.line;
        int col = parser.current.col;
        char *name = xstrdup(parser.current.value);
        parser_advance();

        if (parser.current.type == TOKEN_LPAREN) {
            parser_advance();
            ASTNode *call = ast_node(AST_CALL, line, col);
            call->name = name;
            ASTNode **tail = &call->args;
            if (parser.current.type != TOKEN_RPAREN) {
                for (;;) {
                    ASTNode *arg = parse_expression();
                    if (arg == NULL) {
                        free(name);
                        return NULL;
                    }
                    *tail = arg;
                    tail = &arg->next;
                    if (!parser_match(TOKEN_COMMA)) {
                        break;
                    }
                }
            }
            if (!parser_expect(TOKEN_RPAREN, "expected ')' after arguments")) {
                free(name);
                return NULL;
            }
            node = call;
        } else {
            ASTNode *ref = ast_node(AST_VAR_REF, line, col);
            ref->name = name;
            node = ref;
        }
    } else if (parser_match(TOKEN_LPAREN)) {
        node = parse_expression();
        if (node == NULL) {
            return NULL;
        }
        if (!parser_expect(TOKEN_RPAREN, "expected ')' after expression")) {
            return NULL;
        }
    } else {
        report_error(parser.current.line, parser.current.col,
                     "expected expression (found %s)",
                     token_type_to_string(parser.current.type));
        return NULL;
    }

    /* Postfix ++ / --: keep the old value, then update the variable. */
    while (parser.current.type == TOKEN_PLUS_PLUS ||
           parser.current.type == TOKEN_MINUS_MINUS) {
        if (node->type != AST_VAR_REF) {
            report_error(parser.current.line, parser.current.col,
                         "'++'/'--' operand must be a variable");
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
    return node;
}

/* ------------------------------------------------------------------ */
/* Statements                                                          */
/* ------------------------------------------------------------------ */

static ASTNode *parse_var_decl(TypeKind type)
{
    ASTNode *first = NULL;
    ASTNode **tail = &first;
    for (;;) {
        if (parser.current.type != TOKEN_IDENT) {
            report_error(parser.current.line, parser.current.col,
                         "expected variable name (found %s)",
                         token_type_to_string(parser.current.type));
            return NULL;
        }
        ASTNode *decl = ast_node(AST_VAR_DECL, parser.current.line,
                                 parser.current.col);
        decl->vtype = type;
        decl->name = xstrdup(parser.current.value);
        parser_advance();

        if (parser_match(TOKEN_ASSIGN)) {
            ASTNode *init = parse_expression();
            if (init == NULL) {
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
        while (*tail != NULL) { /* walk past an initializer assignment */
            tail = &(*tail)->next;
        }

        if (!parser_match(TOKEN_COMMA)) {
            break;
        }
    }

    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after declaration")) {
        return NULL;
    }
    return first;
}

static ASTNode *parse_block(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    if (!parser_expect(TOKEN_LBRACE, "expected '{'")) {
        return NULL;
    }
    ASTNode *block = ast_node(AST_BLOCK, line, col);
    ASTNode **tail = &block->body;
    while (parser.current.type != TOKEN_RBRACE &&
           parser.current.type != TOKEN_EOF) {
        ASTNode *stmt;
        if (parser.current.type == TOKEN_KEYWORD_INT ||
            parser.current.type == TOKEN_KEYWORD_CHAR) {
            TypeKind type = parser.current.type == TOKEN_KEYWORD_INT
                                ? TYPE_INT
                                : TYPE_CHAR;
            parser_advance();
            stmt = parse_var_decl(type);
        } else {
            stmt = parse_statement();
        }
        if (stmt == NULL) {
            return NULL;
        }
        *tail = stmt;
        tail = &stmt->next;
        while (*tail != NULL) { /* declarator chains may carry more nodes */
            tail = &(*tail)->next;
        }
    }
    if (!parser_expect(TOKEN_RBRACE, "expected '}' after block")) {
        return NULL;
    }
    return block;
}

static ASTNode *parse_if(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'if' */
    ASTNode *node = ast_node(AST_IF, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'if'")) {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after if condition")) {
        return NULL;
    }
    node->body = parse_statement();
    if (node->body == NULL) {
        return NULL;
    }
    if (parser_match(TOKEN_KEYWORD_ELSE)) {
        node->right = parse_statement();
        if (node->right == NULL) {
            return NULL;
        }
    }
    return node;
}

static ASTNode *parse_while(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'while' */
    ASTNode *node = ast_node(AST_WHILE, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'while'")) {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after while condition")) {
        return NULL;
    }
    node->body = parse_statement();
    if (node->body == NULL) {
        return NULL;
    }
    return node;
}

static ASTNode *parse_do_while(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'do' */
    ASTNode *node = ast_node(AST_DO_WHILE, line, col);
    node->body = parse_statement();
    if (node->body == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_KEYWORD_WHILE, "expected 'while' after 'do' body")) {
        return NULL;
    }
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'while'")) {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after do-while condition")) {
        return NULL;
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after do-while")) {
        return NULL;
    }
    return node;
}

static ASTNode *parse_for(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'for' */
    ASTNode *node = ast_node(AST_FOR, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'for'")) {
        return NULL;
    }
    /* init clause: declaration, expression, or empty */
    if (parser.current.type == TOKEN_SEMICOLON) {
        parser_advance();
    } else if (parser.current.type == TOKEN_KEYWORD_INT ||
               parser.current.type == TOKEN_KEYWORD_CHAR) {
        TypeKind type = parser.current.type == TOKEN_KEYWORD_INT
                            ? TYPE_INT
                            : TYPE_CHAR;
        parser_advance();
        node->left = parse_var_decl(type); /* consumes trailing ';' */
        if (node->left == NULL) {
            return NULL;
        }
    } else {
        node->left = parse_expr_stmt(); /* consumes trailing ';' */
        if (node->left == NULL) {
            return NULL;
        }
    }
    /* condition clause */
    if (parser.current.type != TOKEN_SEMICOLON) {
        node->cond = parse_expression();
        if (node->cond == NULL) {
            return NULL;
        }
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after for condition")) {
        return NULL;
    }
    /* step clause */
    if (parser.current.type != TOKEN_RPAREN) {
        node->right = parse_expression();
        if (node->right == NULL) {
            return NULL;
        }
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after for clauses")) {
        return NULL;
    }
    node->body = parse_statement();
    if (node->body == NULL) {
        return NULL;
    }
    return node;
}

static ASTNode *parse_switch(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'switch' */
    ASTNode *node = ast_node(AST_SWITCH, line, col);
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after 'switch'")) {
        return NULL;
    }
    node->cond = parse_expression();
    if (node->cond == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_RPAREN, "expected ')' after switch expression")) {
        return NULL;
    }
    if (!parser_expect(TOKEN_LBRACE, "expected '{' after switch")) {
        return NULL;
    }

    ASTNode **tail = &node->body;
    while (parser.current.type != TOKEN_RBRACE &&
           parser.current.type != TOKEN_EOF) {
        ASTNode *item = NULL;
        if (parser.current.type == TOKEN_KEYWORD_CASE) {
            int cline = parser.current.line;
            int ccol = parser.current.col;
            parser_advance();
            ASTNode *expr = parse_expression();
            if (expr == NULL) {
                return NULL;
            }
            if (!parser_expect(TOKEN_COLON, "expected ':' after case label")) {
                return NULL;
            }
            item = ast_node(AST_CASE, cline, ccol);
            item->left = expr; /* constant, folded by semantic analysis */
        } else if (parser.current.type == TOKEN_KEYWORD_DEFAULT) {
            int dline = parser.current.line;
            int dcol = parser.current.col;
            parser_advance();
            if (!parser_expect(TOKEN_COLON, "expected ':' after 'default'")) {
                return NULL;
            }
            item = ast_node(AST_DEFAULT, dline, dcol);
        } else if (parser.current.type == TOKEN_KEYWORD_INT ||
                   parser.current.type == TOKEN_KEYWORD_CHAR) {
            TypeKind type = parser.current.type == TOKEN_KEYWORD_INT
                                ? TYPE_INT
                                : TYPE_CHAR;
            parser_advance();
            item = parse_var_decl(type);
        } else {
            item = parse_statement();
        }
        if (item == NULL) {
            return NULL;
        }
        *tail = item;
        tail = &item->next;
        while (*tail != NULL) {
            tail = &(*tail)->next;
        }
    }
    if (!parser_expect(TOKEN_RBRACE, "expected '}' after switch body")) {
        return NULL;
    }
    return node;
}

static ASTNode *parse_jump(const char *keyword)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'break' / 'continue' */
    ASTNode *node = ast_node(strcmp(keyword, "break") == 0 ? AST_BREAK
                                                           : AST_CONTINUE,
                             line, col);
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after statement")) {
        return NULL;
    }
    return node;
}

static ASTNode *parse_return(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    parser_advance(); /* consume 'return' */
    ASTNode *node = ast_node(AST_RETURN, line, col);
    if (parser.current.type != TOKEN_SEMICOLON) {
        node->left = parse_expression();
        if (node->left == NULL) {
            return NULL;
        }
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after return")) {
        return NULL;
    }
    return node;
}

static ASTNode *parse_expr_stmt(void)
{
    ASTNode *expr = parse_expression();
    if (expr == NULL) {
        return NULL;
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after expression")) {
        return NULL;
    }
    return expr;
}

static ASTNode *parse_statement(void)
{
    switch (parser.current.type) {
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
    case TOKEN_KEYWORD_CASE: {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        ASTNode *n = ast_node(AST_CASE, line, col);
        n->left = parse_expression();
        if (n->left == NULL) {
            return NULL;
        }
        if (!parser_expect(TOKEN_COLON, "expected ':' after case label")) {
            return NULL;
        }
        return n;
    }
    case TOKEN_KEYWORD_DEFAULT: {
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance();
        if (!parser_expect(TOKEN_COLON, "expected ':' after 'default'")) {
            return NULL;
        }
        return ast_node(AST_DEFAULT, line, col);
    }
    case TOKEN_KEYWORD_RETURN:
        return parse_return();
    case TOKEN_SEMICOLON:
        parser_advance(); /* empty statement */
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
    if (parser.current.type == TOKEN_RPAREN) {
        parser_advance();
        return 1;
    }
    if (parser.current.type == TOKEN_KEYWORD_VOID) {
        parser_advance();
        return parser_expect(TOKEN_RPAREN,
                             "expected ')' after 'void' parameter list");
    }
    ASTNode **tail = &fn->left;
    for (;;) {
        TypeKind ptype;
        if (parser.current.type == TOKEN_KEYWORD_INT) {
            ptype = TYPE_INT;
        } else if (parser.current.type == TOKEN_KEYWORD_CHAR) {
            ptype = TYPE_CHAR;
        } else {
            report_error(parser.current.line, parser.current.col,
                         "expected parameter type (found %s)",
                         token_type_to_string(parser.current.type));
            return 0;
        }
        parser_advance();
        if (parser.current.type != TOKEN_IDENT) {
            report_error(parser.current.line, parser.current.col,
                         "expected parameter name");
            return 0;
        }
        ASTNode *param = ast_node(AST_PARAM, parser.current.line,
                                  parser.current.col);
        param->vtype = ptype;
        param->name = xstrdup(parser.current.value);
        parser_advance();
        *tail = param;
        tail = &param->next;
        if (!parser_match(TOKEN_COMMA)) {
            break;
        }
    }
    return parser_expect(TOKEN_RPAREN, "expected ')' after parameters");
}

static ASTNode *parse_function_after_name(char *name, TypeKind ret, int line,
                                          int col)
{
    ASTNode *fn = ast_node(AST_FUNC_DECL, line, col);
    fn->name = name;
    fn->vtype = ret;
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after function name")) {
        return NULL;
    }
    if (!parse_param_list(fn)) {
        return NULL;
    }
    fn->body = parse_block();
    if (fn->body == NULL) {
        return NULL;
    }
    return fn;
}

/* Parse the comma-separated global variable declarators that follow the
 * first name (already consumed). Each becomes an AST_VAR_DECL; an optional
 * initializer is attached via ->left (evaluated at compile time). */
static ASTNode *parse_global_vars(char *first_name, TypeKind type, int line,
                                  int col)
{
    ASTNode *first = NULL;
    ASTNode **tail = &first;
    char *name = first_name;
    for (;;) {
        ASTNode *decl = ast_node(AST_VAR_DECL, line, col);
        decl->vtype = type;
        decl->name = name;
        decl->is_global = 1;
        if (parser_match(TOKEN_ASSIGN)) {
            decl->left = parse_expression();
            if (decl->left == NULL) {
                return NULL;
            }
        }
        *tail = decl;
        tail = &decl->next;

        if (!parser_match(TOKEN_COMMA)) {
            break;
        }
        if (parser.current.type != TOKEN_IDENT) {
            report_error(parser.current.line, parser.current.col,
                         "expected variable name after ','");
            return NULL;
        }
        name = xstrdup(parser.current.value);
        parser_advance();
    }
    if (!parser_expect(TOKEN_SEMICOLON, "expected ';' after global declaration")) {
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

    while (parser.current.type != TOKEN_EOF) {
        TypeKind type;
        if (parser.current.type == TOKEN_KEYWORD_INT) {
            type = TYPE_INT;
        } else if (parser.current.type == TOKEN_KEYWORD_CHAR) {
            type = TYPE_CHAR;
        } else if (parser.current.type == TOKEN_KEYWORD_VOID) {
            type = TYPE_VOID;
        } else {
            report_error(parser.current.line, parser.current.col,
                         "expected top-level declaration starting with a type "
                         "(found %s)",
                         token_type_to_string(parser.current.type));
            return NULL;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance(); /* consume type */

        if (parser.current.type != TOKEN_IDENT) {
            report_error(parser.current.line, parser.current.col,
                         "expected name after type");
            return NULL;
        }
        char *name = xstrdup(parser.current.value);
        parser_advance(); /* consume identifier */

        ASTNode *decl = NULL;
        if (parser.current.type == TOKEN_LPAREN) {
            if (type == TYPE_CHAR) {
                report_error(line, col,
                             "function return type 'char' is not supported");
            }
            decl = parse_function_after_name(name, type, line, col);
            if (decl == NULL) {
                free(name);
                return NULL;
            }
        } else {
            if (type == TYPE_VOID) {
                report_error(line, col, "variable cannot have type 'void'");
                free(name);
                return NULL;
            }
            decl = parse_global_vars(name, type, line, col);
            if (decl == NULL) {
                free(name);
                return NULL;
            }
        }

        *tail = decl;
        tail = &decl->next;
        while (*tail != NULL) {
            tail = &(*tail)->next;
        }
    }

    free(parser.current.value);
    free(parser.previous.value);
    return prog;
}
