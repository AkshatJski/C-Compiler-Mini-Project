/* myc.c -- the toy-language front end.
 *
 * myc is a small language in the spirit of C but with its own grammar:
 *
 *     fn main() -> int
 *       let count = 3
 *       let name = "world"
 *       say "hello, "
 *       say name
 *       while count > 0
 *         say count
 *         count = count - 1
 *       end
 *       if count == 0 and count < 1
 *         say "done"
 *       else
 *         say "impossible"
 *       end
 *       return 0
 *     end
 *
 * Design rules, in order of importance:
 *
 *  1. It produces the SAME AST the C front end produces, node for node. That is
 *     what makes `mycc --lang=myc prog.myc -o a.s` and `mycc prog.c -o b.s`
 *     emit identical assembly for equivalent programs -- the back end cannot
 *     tell which language it is compiling.
 *
 *  2. It reuses the C lexer. Tokens are not where the interesting difference
 *     lives, and writing a second lexer would only add a second place for
 *     string escapes and column tracking to disagree.
 *
 *  3. Where the toy language has a construct the AST does not, it desugars the
 *     way the C parser does (unary `-x` becomes `0 - x`, `not x` becomes
 *     `x == 0`), so there is one implementation of each meaning.
 *
 *  4. It refuses to support things it cannot do properly. No compound
 *     assignment (`x += 1`), because the C parser desugars that into an
 *     AST_ASSIGN whose two sides *share* the lvalue node, and reproducing that
 *     aliasing contract here would be a second copy of a subtle rule.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "lexer.h"
#include "myc.h"
#include "util.h"

/* ------------------------------------------------------------------------- */
/* Token stream                                                              */
/* ------------------------------------------------------------------------- */

static struct
{
    Lexer *lexer;
    Token current;
    Token previous;
} p;

static void advance(void)
{
    if (p.previous.value != NULL)
    {
        free(p.previous.value);
        p.previous.value = NULL;
    }
    p.previous = p.current;
    p.current = lexer_next_token(p.lexer);
}

static int check(TokenType type)
{
    return p.current.type == type;
}

static int match(TokenType type)
{
    if (p.current.type == type)
    {
        advance();
        return 1;
    }
    return 0;
}

/* myc words that the C lexer reports as ordinary identifiers: fn, let, say,
 * end, and, or, not. Testing the text is enough -- there is no place in the
 * grammar where these can be anything else. */
static int word(const char *kw)
{
    return p.current.type == TOKEN_IDENT && p.current.value != NULL &&
           strcmp(p.current.value, kw) == 0;
}

static int match_word(const char *kw)
{
    if (word(kw))
    {
        advance();
        return 1;
    }
    return 0;
}

/* What to call the current token in a diagnostic. Identifiers and strings are
 * quoted, because their text is what the reader needs to see; every other token
 * already comes back quoted from token_type_to_string() (or as a readable
 * phrase such as "end of file"). The static buffer is safe here: it is consumed
 * by one formatted message before the next call. */
static const char *current_text(void)
{
    static char buf[120];
    if ((p.current.type == TOKEN_IDENT || p.current.type == TOKEN_STRING) &&
        p.current.value != NULL)
    {
        snprintf(buf, sizeof(buf), "'%.100s'", p.current.value);
        return buf;
    }
    return token_type_to_string(p.current.type);
}

static int expect(TokenType type, const char *what)
{
    if (check(type))
    {
        advance();
        return 1;
    }
    report_error(p.current.line, p.current.col, "expected %s (found %s)", what,
                 current_text());
    return 0;
}

static int expect_word(const char *kw)
{
    if (match_word(kw))
    {
        return 1;
    }
    report_error(p.current.line, p.current.col, "expected '%s' (found %s)",
                 kw, current_text());
    return 0;
}

/* ------------------------------------------------------------------------- */
/* A tiny name table, so `say` can tell a string from a number               */
/* ------------------------------------------------------------------------- */

/* The toy language has no types in its syntax, but `say` must choose between
 * printing a number and printing text. Rather than invent a type system for a
 * teaching language, the parser remembers which `let` names were initialised
 * with a string literal. That is enough for the whole language as defined, and
 * its limits are honest ones: a name is in scope for the rest of the function,
 * so a later `let` of the same name redefines it (there is no shadowing). */

#define MYC_MAX_VARS 128

static struct
{
    char *name;
    int is_string;
} g_vars[MYC_MAX_VARS];
static int g_var_count = 0;

static void vars_reset(void)
{
    for (int i = 0; i < g_var_count; i++)
    {
        free(g_vars[i].name);
        g_vars[i].name = NULL;
    }
    g_var_count = 0;
}

static void vars_declare(const char *name, int is_string)
{
    for (int i = 0; i < g_var_count; i++)
    {
        if (strcmp(g_vars[i].name, name) == 0)
        {
            g_vars[i].is_string = is_string;
            return;
        }
    }
    if (g_var_count >= MYC_MAX_VARS)
    {
        report_error(p.current.line, p.current.col,
                     "too many variables in one function (limit %d)",
                     MYC_MAX_VARS);
        return;
    }
    g_vars[g_var_count].name = xstrdup(name);
    g_vars[g_var_count].is_string = is_string;
    g_var_count++;
}

static int vars_is_string(const char *name)
{
    for (int i = 0; i < g_var_count; i++)
    {
        if (strcmp(g_vars[i].name, name) == 0)
        {
            return g_vars[i].is_string;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Expressions                                                               */
/* ------------------------------------------------------------------------- */

static ASTNode *parse_expr(void);
static ASTNode *parse_primary(void);

static ASTNode *make_int(long long value, int line, int col)
{
    ASTNode *n = ast_node(AST_INT_LIT, line, col);
    n->value = value;
    return n;
}

static ASTNode *make_binary(BinaryOp op, ASTNode *left, ASTNode *right)
{
    ASTNode *n = ast_node(AST_BINARY_OP, left->line, left->col);
    n->op = op;
    n->left = left;
    n->right = right;
    return n;
}

static ASTNode *make_var_ref(const char *name, int line, int col)
{
    ASTNode *n = ast_node(AST_VAR_REF, line, col);
    n->name = xstrdup(name);
    return n;
}

/* unary := ("-" | "!" | "not") unary | primary
 *
 * The two operators are desugared exactly the way the C front end desugars
 * them, so codegen needs no unary-minus or unary-not case and both languages
 * share one implementation of each meaning. */
static ASTNode *parse_unary_or_primary(void)
{
    if (match(TOKEN_MINUS))
    {
        ASTNode *operand = parse_unary_or_primary();
        if (operand == NULL)
        {
            return NULL;
        }
        return make_binary(OP_SUB, make_int(0, operand->line, operand->col),
                           operand);
    }
    if (match(TOKEN_BANG) || match_word("not"))
    {
        ASTNode *operand = parse_unary_or_primary();
        if (operand == NULL)
        {
            return NULL;
        }
        return make_binary(OP_EQ, operand,
                           make_int(0, operand->line, operand->col));
    }
    return parse_primary();
}

static ASTNode *parse_primary(void)
{
    if (check(TOKEN_INT_LIT))
    {
        ASTNode *n = make_int(strtoll(p.current.value, NULL, 10),
                              p.current.line, p.current.col);
        advance();
        return n;
    }

    if (check(TOKEN_STRING))
    {
        /* AST_STRING carries decoded bytes plus a byte count, never a
         * strlen() result: a literal may legally contain NUL. */
        int len = p.current.length > 0 ? p.current.length
                                       : (int)strlen(p.current.value);
        ASTNode *n = ast_node(AST_STRING, p.current.line, p.current.col);
        n->name = xmalloc((size_t)len + 1);
        memcpy(n->name, p.current.value, (size_t)len);
        n->name[len] = '\0';
        n->str_len = len;
        n->val_type = type_ptr(type_char());
        n->vtype = TYPE_PTR;
        advance();
        return n;
    }

    if (match(TOKEN_LPAREN))
    {
        ASTNode *inner = parse_expr();
        if (inner == NULL)
        {
            return NULL;
        }
        if (!expect(TOKEN_RPAREN, "')'"))
        {
            return NULL;
        }
        return inner;
    }

    if (check(TOKEN_IDENT))
    {
        char *name = xstrdup(p.current.value);
        int line = p.current.line;
        int col = p.current.col;
        advance();
        if (match(TOKEN_LPAREN))
        {
            ASTNode *call = ast_node(AST_CALL, line, col);
            call->name = name;
            ASTNode **tail = &call->args;
            if (!check(TOKEN_RPAREN))
            {
                for (;;)
                {
                    ASTNode *arg = parse_expr();
                    if (arg == NULL)
                    {
                        return NULL;
                    }
                    *tail = arg;
                    tail = &arg->next;
                    if (!match(TOKEN_COMMA))
                    {
                        break;
                    }
                }
            }
            if (!expect(TOKEN_RPAREN, "')' after arguments"))
            {
                return NULL;
            }
            return call;
        }
        {
            ASTNode *ref = ast_node(AST_VAR_REF, line, col);
            ref->name = name;
            return ref;
        }
    }

    report_error(p.current.line, p.current.col, "expected a value (found %s)",
                 current_text());
    return NULL;
}

static ASTNode *parse_mul(void)
{
    ASTNode *left = parse_unary_or_primary();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (match(TOKEN_STAR))
        {
            op = OP_MUL;
        }
        else if (match(TOKEN_SLASH))
        {
            op = OP_DIV;
        }
        else if (match(TOKEN_PERCENT))
        {
            op = OP_MOD;
        }
        else
        {
            break;
        }
        ASTNode *right = parse_unary_or_primary();
        if (right == NULL)
        {
            return NULL;
        }
        left = make_binary(op, left, right);
    }
    return left;
}

static ASTNode *parse_add(void)
{
    ASTNode *left = parse_mul();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (match(TOKEN_PLUS))
        {
            op = OP_ADD;
        }
        else if (match(TOKEN_MINUS))
        {
            op = OP_SUB;
        }
        else
        {
            break;
        }
        ASTNode *right = parse_mul();
        if (right == NULL)
        {
            return NULL;
        }
        left = make_binary(op, left, right);
    }
    return left;
}

static ASTNode *parse_rel(void)
{
    ASTNode *left = parse_add();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (match(TOKEN_LESS))
        {
            op = OP_LT;
        }
        else if (match(TOKEN_LESS_EQUAL))
        {
            op = OP_LE;
        }
        else if (match(TOKEN_GREATER))
        {
            op = OP_GT;
        }
        else if (match(TOKEN_GREATER_EQUAL))
        {
            op = OP_GE;
        }
        else
        {
            break;
        }
        ASTNode *right = parse_add();
        if (right == NULL)
        {
            return NULL;
        }
        left = make_binary(op, left, right);
    }
    return left;
}

static ASTNode *parse_eq(void)
{
    ASTNode *left = parse_rel();
    if (left == NULL)
    {
        return NULL;
    }
    for (;;)
    {
        BinaryOp op;
        if (match(TOKEN_EQUALS_EQUALS))
        {
            op = OP_EQ;
        }
        else if (match(TOKEN_BANG_EQUALS))
        {
            op = OP_NE;
        }
        else
        {
            break;
        }
        ASTNode *right = parse_rel();
        if (right == NULL)
        {
            return NULL;
        }
        left = make_binary(op, left, right);
    }
    return left;
}

static ASTNode *parse_and(void)
{
    ASTNode *left = parse_eq();
    if (left == NULL)
    {
        return NULL;
    }
    while (match(TOKEN_AMPERSAND_AMPERSAND) || match_word("and"))
    {
        ASTNode *right = parse_eq();
        if (right == NULL)
        {
            return NULL;
        }
        left = make_binary(OP_AND, left, right);
    }
    return left;
}

static ASTNode *parse_expr(void)
{
    ASTNode *left = parse_and();
    if (left == NULL)
    {
        return NULL;
    }
    while (match(TOKEN_PIPE_PIPE) || match_word("or"))
    {
        ASTNode *right = parse_and();
        if (right == NULL)
        {
            return NULL;
        }
        left = make_binary(OP_OR, left, right);
    }
    return left;
}

/* ------------------------------------------------------------------------- */
/* Statements                                                                */
/* ------------------------------------------------------------------------- */

static ASTNode *parse_block_body(void);

/* A statement may be more than one node: `let` is a declaration followed by an
 * initialising assignment (the same two-node shape the C parser builds for
 * `int x = 5;`), and `say` of a string is a print_str call followed by a
 * print_endl call. Returning a chain keeps the block builder trivial. */
static void append_node(ASTNode **tail, ASTNode *node)
{
    while (*tail != NULL)
    {
        tail = &(*tail)->next;
    }
    *tail = node;
}

static ASTNode *parse_let(void)
{
    int line = p.current.line;
    int col = p.current.col;
    advance(); /* 'let' */

    if (!check(TOKEN_IDENT))
    {
        report_error(p.current.line, p.current.col,
                     "expected a name after 'let' (found %s)",
                     current_text());
        return NULL;
    }
    char *name = xstrdup(p.current.value);
    int name_line = p.current.line;
    int name_col = p.current.col;
    advance();

    if (!check(TOKEN_ASSIGN))
    {
        report_error(p.current.line, p.current.col,
                     "expected '=' after 'let %s' (found %s)", name,
                     current_text());
        free(name);
        return NULL;
    }
    advance();

    /* The declared type is decided by the initialiser's *syntax*: a string
     * literal makes a char pointer, everything else is an int. This is the only
     * place the toy language does inference, and it is documented as such. */
    int is_string = check(TOKEN_STRING);
    Type *decl_type = is_string ? type_ptr(type_char()) : type_int();

    ASTNode *init = parse_expr();
    if (init == NULL)
    {
        free(name);
        return NULL;
    }

    ASTNode *decl = ast_node(AST_VAR_DECL, name_line, name_col);
    decl->name = name;
    decl->val_type = decl_type;
    decl->vtype = decl_type->kind;

    ASTNode *ref = make_var_ref(name, name_line, name_col);
    ASTNode *assign = ast_node(AST_ASSIGN, line, col);
    assign->left = ref;
    assign->right = init;
    decl->next = assign;

    vars_declare(name, is_string);
    return decl;
}

static ASTNode *parse_say(void)
{
    int line = p.current.line;
    int col = p.current.col;
    advance(); /* 'say' */

    ASTNode *value = parse_expr();
    if (value == NULL)
    {
        return NULL;
    }

    int is_string = 0;
    if (value->type == AST_STRING)
    {
        is_string = 1;
    }
    else if (value->type == AST_VAR_REF)
    {
        is_string = vars_is_string(value->name);
    }

    ASTNode *call = ast_node(AST_CALL, line, col);
    call->name = xstrdup(is_string ? "print_str" : "print_int");
    value->next = NULL;
    call->args = value;

    if (!is_string)
    {
        /* print_int already ends its line. */
        return call;
    }

    /* print_str does not, so the newline is a second statement. Two calls also
     * means two ordinary AST nodes -- nothing about `say` needs a new node
     * type, which is the point of lowering it here. */
    ASTNode *newline = ast_node(AST_CALL, line, col);
    newline->name = xstrdup("print_endl");
    call->next = newline;
    return call;
}

static ASTNode *parse_if(void)
{
    ASTNode *node = ast_node(AST_IF, p.current.line, p.current.col);
    advance(); /* 'if' */

    node->cond = parse_expr();
    if (node->cond == NULL)
    {
        return NULL;
    }
    node->body = parse_block_body();
    if (node->body == NULL)
    {
        return NULL;
    }
    if (check(TOKEN_KEYWORD_ELSE))
    {
        advance();
        node->right = parse_block_body();
        if (node->right == NULL)
        {
            return NULL;
        }
    }
    if (!expect_word("end"))
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_while(void)
{
    ASTNode *node = ast_node(AST_WHILE, p.current.line, p.current.col);
    advance(); /* 'while' */

    node->cond = parse_expr();
    if (node->cond == NULL)
    {
        return NULL;
    }
    node->body = parse_block_body();
    if (node->body == NULL)
    {
        return NULL;
    }
    if (!expect_word("end"))
    {
        return NULL;
    }
    return node;
}

static ASTNode *parse_return(void)
{
    ASTNode *node = ast_node(AST_RETURN, p.current.line, p.current.col);
    advance(); /* 'return' */

    /* `return` on its own is legal (a void function), and the tokens that can
     * follow it are exactly the ones that end a block. */
    if (!check(TOKEN_EOF) && !word("end") && !word("else"))
    {
        node->left = parse_expr();
        if (node->left == NULL)
        {
            return NULL;
        }
    }
    return node;
}

static ASTNode *parse_ident_statement(void)
{
    char *name = xstrdup(p.current.value);
    int line = p.current.line;
    int col = p.current.col;
    advance();

    if (match(TOKEN_ASSIGN))
    {
        ASTNode *ref = ast_node(AST_VAR_REF, line, col);
        ref->name = name;
        ASTNode *value = parse_expr();
        if (value == NULL)
        {
            free(name);
            return NULL;
        }
        ASTNode *assign = ast_node(AST_ASSIGN, line, col);
        assign->left = ref;
        assign->right = value;
        return assign;
    }

    if (match(TOKEN_LPAREN))
    {
        ASTNode *call = ast_node(AST_CALL, line, col);
        call->name = name;
        ASTNode **tail = &call->args;
        if (!check(TOKEN_RPAREN))
        {
            for (;;)
            {
                ASTNode *arg = parse_expr();
                if (arg == NULL)
                {
                    return NULL;
                }
                *tail = arg;
                tail = &arg->next;
                if (!match(TOKEN_COMMA))
                {
                    break;
                }
            }
        }
        if (!expect(TOKEN_RPAREN, "')' after arguments"))
        {
            return NULL;
        }
        return call;
    }

    report_error(line, col,
                 "expected '=' or '(' after '%s' (statement expressions are not "
                 "supported in myc)",
                 name);
    free(name);
    return NULL;
}

static ASTNode *parse_stmt(void)
{
    if (word("let"))
    {
        return parse_let();
    }
    if (word("say"))
    {
        return parse_say();
    }
    if (check(TOKEN_KEYWORD_IF))
    {
        return parse_if();
    }
    if (check(TOKEN_KEYWORD_WHILE))
    {
        return parse_while();
    }
    if (check(TOKEN_KEYWORD_RETURN))
    {
        return parse_return();
    }
    if (check(TOKEN_IDENT))
    {
        return parse_ident_statement();
    }

    report_error(p.current.line, p.current.col,
                 "expected a statement (found %s)", current_text());
    return NULL;
}

/* A block stops at `end`, `else` or end-of-file and does NOT consume the
 * terminator: whoever opened the block closes it. That single rule is what
 * makes nested `if`/`while` bodies match the right `end` without indentation
 * or brace counting. */
static ASTNode *parse_block_body(void)
{
    ASTNode *block = ast_node(AST_BLOCK, p.current.line, p.current.col);
    ASTNode *head = NULL;

    while (!check(TOKEN_EOF) && !word("end") && !word("else"))
    {
        ASTNode *stmt = parse_stmt();
        if (stmt == NULL)
        {
            return NULL;
        }
        append_node(&head, stmt);
    }
    block->body = head;
    return block;
}

/* ------------------------------------------------------------------------- */
/* Functions and the program                                                 */
/* ------------------------------------------------------------------------- */

static ASTNode *parse_fn(void)
{
    int line = p.current.line;
    int col = p.current.col;
    advance(); /* 'fn' */

    if (!check(TOKEN_IDENT))
    {
        report_error(p.current.line, p.current.col,
                     "expected a function name after 'fn' (found %s)",
                     current_text());
        return NULL;
    }
    ASTNode *fn = ast_node(AST_FUNC_DECL, line, col);
    fn->name = xstrdup(p.current.value);
    advance();

    if (!expect(TOKEN_LPAREN, "'(' after the function name"))
    {
        return NULL;
    }

    /* Every parameter is an int: myc has no type syntax for parameters, and a
     * toy language that pretends otherwise would need a type parser, which is
     * exactly the C front end's job. */
    ASTNode **tail = &fn->left;
    if (!check(TOKEN_RPAREN))
    {
        for (;;)
        {
            if (!check(TOKEN_IDENT))
            {
                report_error(p.current.line, p.current.col,
                             "expected a parameter name (found %s)",
                             current_text());
                return NULL;
            }
            ASTNode *param = ast_node(AST_PARAM, p.current.line, p.current.col);
            param->name = xstrdup(p.current.value);
            param->val_type = type_int();
            param->vtype = TYPE_INT;
            advance();
            *tail = param;
            tail = &param->next;
            if (!match(TOKEN_COMMA))
            {
                break;
            }
        }
    }
    if (!expect(TOKEN_RPAREN, "')' after the parameters"))
    {
        return NULL;
    }

    /* Return type: `-> int` (default when omitted) or `-> void`. */
    Type *ret = type_int();
    if (match(TOKEN_ARROW))
    {
        if (check(TOKEN_KEYWORD_VOID))
        {
            advance();
            ret = type_void();
        }
        else if (check(TOKEN_KEYWORD_INT))
        {
            advance();
        }
        else
        {
            report_error(p.current.line, p.current.col,
                         "expected 'int' or 'void' after '->' (found %s)",
                         current_text());
            return NULL;
        }
    }
    fn->val_type = ret;
    fn->vtype = ret->kind;

    /* Variables from the previous function must not leak into this one: `say`
     * decides string-vs-number from this table. */
    vars_reset();

    fn->body = parse_block_body();
    if (fn->body == NULL)
    {
        return NULL;
    }
    if (!expect_word("end"))
    {
        return NULL;
    }
    return fn;
}

ASTNode *myc_parse_program(Lexer *lexer)
{
    memset(&p, 0, sizeof(p));
    memset(g_vars, 0, sizeof(g_vars));
    g_var_count = 0;

    p.lexer = lexer;
    p.current = lexer_next_token(lexer);

    ASTNode *prog = ast_node(AST_PROGRAM, 1, 1);
    ASTNode *head = NULL;

    while (!check(TOKEN_EOF))
    {
        if (!word("fn"))
        {
            report_error(p.current.line, p.current.col,
                         "expected 'fn' at the start of a declaration (found "
                         "%s)",
                         current_text());
            break;
        }
        ASTNode *fn = parse_fn();
        if (fn == NULL)
        {
            break;
        }
        append_node(&head, fn);
    }
    prog->body = head;

    vars_reset();
    if (p.current.value != NULL)
    {
        free(p.current.value);
        p.current.value = NULL;
    }
    if (p.previous.value != NULL)
    {
        free(p.previous.value);
        p.previous.value = NULL;
    }
    return prog;
}
