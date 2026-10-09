#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "util.h"

void lexer_init(Lexer *lexer, const char *source)
{
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 1;
}

static int at_end(const Lexer *lexer)
{
    return lexer->source[lexer->pos] == '\0';
}

static char peek(const Lexer *lexer)
{
    return lexer->source[lexer->pos];
}

static char peek_next(const Lexer *lexer)
{
    if (lexer->source[lexer->pos + 1] == '\0') {
        return '\0';
    }
    return lexer->source[lexer->pos + 1];
}

static char advance(Lexer *lexer)
{
    char c = lexer->source[lexer->pos++];
    if (c == '\n') {
        lexer->line++;
        lexer->col = 1;
    } else {
        lexer->col++;
    }
    return c;
}

static Token make_token(TokenType type, char *value, int line, int col)
{
    Token token;
    token.type = type;
    token.value = value;
    token.line = line;
    token.col = col;
    return token;
}

static void skip_trivia(Lexer *lexer)
{
    for (;;) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lexer);
        } else if (c == '/' && peek_next(lexer) == '/') {
            while (!at_end(lexer) && peek(lexer) != '\n') {
                advance(lexer);
            }
        } else if (c == '/' && peek_next(lexer) == '*') {
            int start_line = lexer->line;
            int start_col = lexer->col;
            advance(lexer); /* '/' */
            advance(lexer); /* '*' */
            int closed = 0;
            while (!at_end(lexer)) {
                if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                    advance(lexer);
                    advance(lexer);
                    closed = 1;
                    break;
                }
                advance(lexer);
            }
            if (!closed) {
                report_error(start_line, start_col, "unterminated block comment");
                return;
            }
        } else {
            return;
        }
    }
}

static Token lex_ident_or_keyword(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    size_t start = lexer->pos;
    while (isalnum((unsigned char)peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }
    size_t len = lexer->pos - start;
    char *text = xmalloc(len + 1);
    memcpy(text, lexer->source + start, len);
    text[len] = '\0';

    struct Keyword {
        const char *word;
        TokenType type;
    };
    static const struct Keyword keywords[] = {
        {"int", TOKEN_KEYWORD_INT},
        {"char", TOKEN_KEYWORD_CHAR},
        {"void", TOKEN_KEYWORD_VOID},
        {"if", TOKEN_KEYWORD_IF},
        {"else", TOKEN_KEYWORD_ELSE},
        {"while", TOKEN_KEYWORD_WHILE},
        {"do", TOKEN_KEYWORD_DO},
        {"for", TOKEN_KEYWORD_FOR},
        {"switch", TOKEN_KEYWORD_SWITCH},
        {"case", TOKEN_KEYWORD_CASE},
        {"default", TOKEN_KEYWORD_DEFAULT},
        {"break", TOKEN_KEYWORD_BREAK},
        {"continue", TOKEN_KEYWORD_CONTINUE},
        {"return", TOKEN_KEYWORD_RETURN},
    };

    TokenType type = TOKEN_IDENT;
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strcmp(text, keywords[i].word) == 0) {
            type = keywords[i].type;
            break;
        }
    }
    return make_token(type, text, start_line, start_col);
}

static int is_num_suffix(char c)
{
    return c == 'u' || c == 'U' || c == 'l' || c == 'L';
}

static Token lex_number(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    size_t start = lexer->pos;

    if (peek(lexer) == '0' &&
        (peek_next(lexer) == 'x' || peek_next(lexer) == 'X')) {
        advance(lexer); /* '0' */
        advance(lexer); /* 'x' */
        if (!isxdigit((unsigned char)peek(lexer))) {
            report_error(start_line, start_col,
                         "malformed hexadecimal literal");
        }
        while (isxdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
    } else {
        while (isdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
    }
    /* Optional integer suffix (U / L / UL / LL / ULL in any reasonably
     * ordered spelling); the suffix is dropped from the stored text. */
    size_t digits_end = lexer->pos;
    while (is_num_suffix(peek(lexer))) {
        advance(lexer);
    }

    size_t len = digits_end - start;
    char *text = xmalloc(len + 1);
    memcpy(text, lexer->source + start, len);
    text[len] = '\0';
    return make_token(TOKEN_INT_LIT, text, start_line, start_col);
}

/* Decode one escape sequence after the backslash; returns the byte value
 * and advances past it. */
static int read_escape(Lexer *lexer)
{
    char c = advance(lexer);
    switch (c) {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'v':
        return '\v';
    case '\\':
        return '\\';
    case '\'':
        return '\'';
    case '"':
        return '"';
    case 'x': {
        int value = 0;
        int digits = 0;
        while (isxdigit((unsigned char)peek(lexer))) {
            char h = advance(lexer);
            value = value * 16 +
                    (isdigit((unsigned char)h) ? h - '0'
                                               : (tolower((unsigned char)h) - 'a' + 10));
            digits++;
        }
        if (digits == 0) {
            report_error(lexer->line, lexer->col, "\\x used with no digits");
        }
        return value;
    }
    default:
        report_error(lexer->line, lexer->col, "unknown escape sequence '\\%c'", c);
        return c;
    }
}

static Token lex_char_literal(Lexer *lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->col;
    advance(lexer); /* opening ' */

    int value;
    if (peek(lexer) == '\'') {
        report_error(start_line, start_col, "empty character literal");
        value = 0;
    } else if (peek(lexer) == '\\') {
        advance(lexer); /* backslash */
        value = read_escape(lexer);
    } else {
        value = (unsigned char)advance(lexer);
    }

    if (peek(lexer) == '\'') {
        advance(lexer); /* closing ' */
    } else {
        report_error(start_line, start_col, "unterminated character literal");
    }

    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    return make_token(TOKEN_CHAR_LIT, xstrdup(text), start_line, start_col);
}

Token lexer_next_token(Lexer *lexer)
{
    skip_trivia(lexer);
    if (at_end(lexer)) {
        return make_token(TOKEN_EOF, NULL, lexer->line, lexer->col);
    }

    int line = lexer->line;
    int col = lexer->col;
    char c = peek(lexer);

    if (isalpha((unsigned char)c) || c == '_') {
        return lex_ident_or_keyword(lexer);
    }
    if (isdigit((unsigned char)c)) {
        return lex_number(lexer);
    }
    if (c == '\'') {
        return lex_char_literal(lexer);
    }

    advance(lexer);
    switch (c) {
    case '+':
        if (peek(lexer) == '+') {
            advance(lexer);
            return make_token(TOKEN_PLUS_PLUS, NULL, line, col);
        }
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_PLUS_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_PLUS, NULL, line, col);
    case '-':
        if (peek(lexer) == '-') {
            advance(lexer);
            return make_token(TOKEN_MINUS_MINUS, NULL, line, col);
        }
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_MINUS_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_MINUS, NULL, line, col);
    case '*':
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_STAR_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_STAR, NULL, line, col);
    case '/':
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_SLASH_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_SLASH, NULL, line, col);
    case '%':
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_PERCENT_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_PERCENT, NULL, line, col);
    case '&':
        if (peek(lexer) == '&') {
            advance(lexer);
            return make_token(TOKEN_AMPERSAND_AMPERSAND, NULL, line, col);
        }
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_AMPERSAND_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_AMPERSAND, NULL, line, col);
    case '|':
        if (peek(lexer) == '|') {
            advance(lexer);
            return make_token(TOKEN_PIPE_PIPE, NULL, line, col);
        }
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_PIPE_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_PIPE, NULL, line, col);
    case '^':
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_CARET_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_CARET, NULL, line, col);
    case '~':
        return make_token(TOKEN_TILDE, NULL, line, col);
    case '<':
        if (peek(lexer) == '<') {
            advance(lexer);
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(TOKEN_LESS_LESS_EQUAL, NULL, line, col);
            }
            return make_token(TOKEN_LESS_LESS, NULL, line, col);
        }
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_LESS_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_LESS, NULL, line, col);
    case '>':
        if (peek(lexer) == '>') {
            advance(lexer);
            if (peek(lexer) == '=') {
                advance(lexer);
                return make_token(TOKEN_GREATER_GREATER_EQUAL, NULL, line, col);
            }
            return make_token(TOKEN_GREATER_GREATER, NULL, line, col);
        }
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_GREATER_EQUAL, NULL, line, col);
        }
        return make_token(TOKEN_GREATER, NULL, line, col);
    case '(':
        return make_token(TOKEN_LPAREN, NULL, line, col);
    case ')':
        return make_token(TOKEN_RPAREN, NULL, line, col);
    case '{':
        return make_token(TOKEN_LBRACE, NULL, line, col);
    case '}':
        return make_token(TOKEN_RBRACE, NULL, line, col);
    case ',':
        return make_token(TOKEN_COMMA, NULL, line, col);
    case ';':
        return make_token(TOKEN_SEMICOLON, NULL, line, col);
    case '?':
        return make_token(TOKEN_QUESTION, NULL, line, col);
    case ':':
        return make_token(TOKEN_COLON, NULL, line, col);
    case '=':
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_EQUALS_EQUALS, NULL, line, col);
        }
        return make_token(TOKEN_ASSIGN, NULL, line, col);
    case '!':
        if (peek(lexer) == '=') {
            advance(lexer);
            return make_token(TOKEN_BANG_EQUALS, NULL, line, col);
        }
        return make_token(TOKEN_BANG, NULL, line, col);
    default:
        report_error(line, col, "unexpected character '%c'", c);
        return make_token(TOKEN_EOF, NULL, line, col);
    }
}

const char *token_type_to_string(TokenType type)
{
    switch (type) {
    case TOKEN_EOF:
        return "end of file";
    case TOKEN_INT_LIT:
        return "integer literal";
    case TOKEN_CHAR_LIT:
        return "character literal";
    case TOKEN_IDENT:
        return "identifier";
    case TOKEN_KEYWORD_INT:
        return "'int'";
    case TOKEN_KEYWORD_CHAR:
        return "'char'";
    case TOKEN_KEYWORD_VOID:
        return "'void'";
    case TOKEN_KEYWORD_IF:
        return "'if'";
    case TOKEN_KEYWORD_ELSE:
        return "'else'";
    case TOKEN_KEYWORD_WHILE:
        return "'while'";
    case TOKEN_KEYWORD_DO:
        return "'do'";
    case TOKEN_KEYWORD_FOR:
        return "'for'";
    case TOKEN_KEYWORD_SWITCH:
        return "'switch'";
    case TOKEN_KEYWORD_CASE:
        return "'case'";
    case TOKEN_KEYWORD_DEFAULT:
        return "'default'";
    case TOKEN_KEYWORD_BREAK:
        return "'break'";
    case TOKEN_KEYWORD_CONTINUE:
        return "'continue'";
    case TOKEN_KEYWORD_RETURN:
        return "'return'";
    case TOKEN_PLUS:
        return "'+'";
    case TOKEN_MINUS:
        return "'-'";
    case TOKEN_STAR:
        return "'*'";
    case TOKEN_SLASH:
        return "'/'";
    case TOKEN_PERCENT:
        return "'%'";
    case TOKEN_PLUS_PLUS:
        return "'++'";
    case TOKEN_MINUS_MINUS:
        return "'--'";
    case TOKEN_PLUS_EQUAL:
        return "'+='";
    case TOKEN_MINUS_EQUAL:
        return "'-='";
    case TOKEN_STAR_EQUAL:
        return "'*='";
    case TOKEN_SLASH_EQUAL:
        return "'/='";
    case TOKEN_PERCENT_EQUAL:
        return "'%='";
    case TOKEN_AMPERSAND_EQUAL:
        return "'&='";
    case TOKEN_PIPE_EQUAL:
        return "'|='";
    case TOKEN_CARET_EQUAL:
        return "'^='";
    case TOKEN_LESS_LESS_EQUAL:
        return "'<<='";
    case TOKEN_GREATER_GREATER_EQUAL:
        return "'>>='";
    case TOKEN_AMPERSAND:
        return "'&'";
    case TOKEN_AMPERSAND_AMPERSAND:
        return "'&&'";
    case TOKEN_PIPE:
        return "'|'";
    case TOKEN_PIPE_PIPE:
        return "'||'";
    case TOKEN_CARET:
        return "'^'";
    case TOKEN_TILDE:
        return "'~'";
    case TOKEN_LESS_LESS:
        return "'<<'";
    case TOKEN_GREATER_GREATER:
        return "'>>'";
    case TOKEN_EQUALS_EQUALS:
        return "'=='";
    case TOKEN_BANG_EQUALS:
        return "'!='";
    case TOKEN_BANG:
        return "'!'";
    case TOKEN_LESS:
        return "'<'";
    case TOKEN_LESS_EQUAL:
        return "'<='";
    case TOKEN_GREATER:
        return "'>'";
    case TOKEN_GREATER_EQUAL:
        return "'>='";
    case TOKEN_QUESTION:
        return "'?'";
    case TOKEN_COLON:
        return "':'";
    case TOKEN_ASSIGN:
        return "'='";
    case TOKEN_LPAREN:
        return "'('";
    case TOKEN_RPAREN:
        return "')'";
    case TOKEN_LBRACE:
        return "'{'";
    case TOKEN_RBRACE:
        return "'}'";
    case TOKEN_COMMA:
        return "','";
    case TOKEN_SEMICOLON:
        return "';'";
    }
    return "unknown token";
}
