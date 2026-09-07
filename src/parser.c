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
static ASTNode *parse_or(void);
static ASTNode *parse_and(void);
static ASTNode *parse_equality(void);
static ASTNode *parse_relational(void);
static ASTNode *parse_additive(void);
static ASTNode *parse_multiplicative(void);
static ASTNode *parse_unary(void);
static ASTNode *parse_primary(void);
static ASTNode *parse_statement(void);
static ASTNode *parse_block(void);
static ASTNode *parse_if(void);
static ASTNode *parse_while(void);
static ASTNode *parse_for(void);
static ASTNode *parse_return(void);
static ASTNode *parse_expr_stmt(void);
static ASTNode *parse_var_decl(void);
static ASTNode *parse_function_after_name(char *name, int line, int col);

static ASTNode *ast_node(ASTNodeType type, int line, int col)
{
    ASTNode *node = xcalloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->col = col;
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
    ASTNode *left = parse_or();
    if (left == NULL) {
        return NULL;
    }
    BinaryOp comp_op = -1;
    if (parser.current.type == TOKEN_ASSIGN) {
        comp_op = OP_ADD; /* sentinel: plain assignment */
    } else if (parser.current.type == TOKEN_PLUS_EQUAL) {
        comp_op = OP_ADD;
    } else if (parser.current.type == TOKEN_MINUS_EQUAL) {
        comp_op = OP_SUB;
    } else if (parser.current.type == TOKEN_STAR_EQUAL) {
        comp_op = OP_MUL;
    } else if (parser.current.type == TOKEN_SLASH_EQUAL) {
        comp_op = OP_DIV;
    } else if (parser.current.type == TOKEN_PERCENT_EQUAL) {
        comp_op = OP_MOD;
    } else {
        return left;
    }
    if (left->type != AST_VAR_REF) {
        report_error(parser.current.line, parser.current.col,
                     "left side of assignment must be a variable");
        return NULL;
    }
    int plain = parser.current.type == TOKEN_ASSIGN;
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
    ASTNode *left = parse_equality();
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
        ASTNode *right = parse_equality();
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
    ASTNode *left = parse_additive();
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

    if (parser.current.type == TOKEN_INT_LIT) {
        int line = parser.current.line;
        int col = parser.current.col;
        node = ast_node(AST_INT_LIT, line, col);
        errno = 0;
        long long value = strtoll(parser.current.value, NULL, 10);
        if (errno == ERANGE) {
            report_error(line, col, "integer literal '%s' out of range", parser.current.value);
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

static ASTNode *parse_var_decl(void)
{
    int line = parser.current.line;
    int col = parser.current.col;
    if (!parser_expect(TOKEN_KEYWORD_INT, "expected 'int' in declaration")) {
        return NULL;
    }
    ASTNode *first = NULL;
    ASTNode **tail = &first;
    for (;;) {
        if (parser.current.type != TOKEN_IDENT) {
            report_error(parser.current.line, parser.current.col,
                         "expected variable name after 'int'");
            return NULL;
        }
        ASTNode *decl = ast_node(AST_VAR_DECL, line, col);
        decl->name = xstrdup(parser.current.value);
        parser_advance();

        if (parser_match(TOKEN_ASSIGN)) {
            ASTNode *init = parse_expression();
            if (init == NULL) {
                return NULL;
            }
            ASTNode *ref = ast_node(AST_VAR_REF, line, col);
            ref->name = xstrdup(decl->name);
            ASTNode *assign = ast_node(AST_ASSIGN, line, col);
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
        if (parser.current.type == TOKEN_KEYWORD_INT) {
            stmt = parse_var_decl();
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
    } else if (parser.current.type == TOKEN_KEYWORD_INT) {
        node->left = parse_var_decl(); /* consumes trailing ';' */
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
    case TOKEN_KEYWORD_FOR:
        return parse_for();
    case TOKEN_KEYWORD_RETURN:
        return parse_return();
    default:
        return parse_expr_stmt();
    }
}

/* ------------------------------------------------------------------ */
/* Top level                                                           */
/* ------------------------------------------------------------------ */

static ASTNode *parse_function_after_name(char *name, int line, int col)
{
    ASTNode *fn = ast_node(AST_FUNC_DECL, line, col);
    fn->name = name;
    if (!parser_expect(TOKEN_LPAREN, "expected '(' after function name")) {
        return NULL;
    }
    ASTNode **tail = &fn->left;
    if (parser.current.type == TOKEN_RPAREN) {
        parser_advance();
    } else if (parser.current.type == TOKEN_KEYWORD_VOID) {
        parser_advance();
        if (!parser_expect(TOKEN_RPAREN,
                           "expected ')' after 'void' parameter list")) {
            return NULL;
        }
    } else {
        for (;;) {
            if (parser.current.type != TOKEN_KEYWORD_INT) {
                report_error(parser.current.line, parser.current.col,
                             "expected 'int' parameter type (found %s)",
                             token_type_to_string(parser.current.type));
                return NULL;
            }
            parser_advance();
            if (parser.current.type != TOKEN_IDENT) {
                report_error(parser.current.line, parser.current.col,
                             "expected parameter name");
                return NULL;
            }
            ASTNode *param = ast_node(AST_PARAM,
                                      parser.current.line, parser.current.col);
            param->name = xstrdup(parser.current.value);
            parser_advance();
            *tail = param;
            tail = &param->next;
            if (!parser_match(TOKEN_COMMA)) {
                break;
            }
        }
        if (!parser_expect(TOKEN_RPAREN, "expected ')' after parameters")) {
            return NULL;
        }
    }
    fn->body = parse_block();
    if (fn->body == NULL) {
        return NULL;
    }
    return fn;
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
        if (parser.current.type != TOKEN_KEYWORD_INT) {
            report_error(parser.current.line, parser.current.col,
                         "expected top-level declaration starting with 'int' (found %s)",
                         token_type_to_string(parser.current.type));
            return NULL;
        }
        int line = parser.current.line;
        int col = parser.current.col;
        parser_advance(); /* consume 'int' */

        if (parser.current.type != TOKEN_IDENT) {
            report_error(parser.current.line, parser.current.col,
                         "expected name after 'int'");
            return NULL;
        }
        char *name = xstrdup(parser.current.value);
        parser_advance(); /* consume identifier */

        ASTNode *decl = NULL;
        if (parser.current.type == TOKEN_LPAREN) {
            decl = parse_function_after_name(name, line, col);
            if (decl == NULL) {
                free(name);
                return NULL;
            }
        } else if (parser.current.type == TOKEN_SEMICOLON) {
            decl = ast_node(AST_VAR_DECL, line, col);
            decl->name = name;
            parser_advance(); /* consume ';' */
        } else {
            report_error(parser.current.line, parser.current.col,
                         "expected '(' or ';' after '%s'", name);
            free(name);
            return NULL;
        }

        *tail = decl;
        tail = &decl->next;
    }

    free(parser.current.value);
    free(parser.previous.value);
    return prog;
}
