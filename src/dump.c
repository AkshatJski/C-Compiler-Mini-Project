#include <stdio.h>
#include <stdlib.h>

#include "dump.h"
#include "tokens.h"

/* ------------------------------------------------------------------ */
/* Token dump                                                          */
/* ------------------------------------------------------------------ */

void dump_tokens(Lexer *lexer, FILE *out)
{
    int index = 0;
    for (;;) {
        Token tok = lexer_next_token(lexer);
        fprintf(out, "%3d  %3d:%-3d  %-22s", index, tok.line, tok.col,
                token_type_to_string(tok.type));
        if (tok.value != NULL) {
            fprintf(out, "  '%s'", tok.value);
        }
        fprintf(out, "\n");
        int done = tok.type == TOKEN_EOF;
        free(tok.value);
        if (done) {
            break;
        }
        index++;
    }
}

/* ------------------------------------------------------------------ */
/* AST dump                                                            */
/* ------------------------------------------------------------------ */

static const char *op_name(int op)
{
    switch (op) {
    case OP_ADD:
        return "+";
    case OP_SUB:
        return "-";
    case OP_MUL:
        return "*";
    case OP_DIV:
        return "/";
    case OP_MOD:
        return "%";
    case OP_BIT_AND:
        return "&";
    case OP_BIT_OR:
        return "|";
    case OP_BIT_XOR:
        return "^";
    case OP_SHL:
        return "<<";
    case OP_SHR:
        return ">>";
    case OP_AND:
        return "&&";
    case OP_OR:
        return "||";
    case OP_EQ:
        return "==";
    case OP_NE:
        return "!=";
    case OP_LT:
        return "<";
    case OP_LE:
        return "<=";
    case OP_GT:
        return ">";
    case OP_GE:
        return ">=";
    default:
        return "?";
    }
}

static void dump_node(const ASTNode *node, int depth, FILE *out);

static const char *type_name(int vtype)
{
    switch (vtype) {
    case TYPE_INT:
        return "int";
    case TYPE_CHAR:
        return "char";
    case TYPE_VOID:
        return "void";
    }
    return "?";
}

static void dump_chain(const ASTNode *node, int depth, FILE *out)
{
    for (; node != NULL; node = node->next) {
        dump_node(node, depth, out);
    }
}

static void dump_label(const char *label, const ASTNode *child, int depth,
                       FILE *out)
{
    if (child == NULL) {
        return;
    }
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }
    fprintf(out, "[%s]\n", label);
    dump_node(child, depth + 1, out);
}

static void dump_node(const ASTNode *node, int depth, FILE *out)
{
    for (int i = 0; i < depth; i++) {
        fprintf(out, "  ");
    }

    switch (node->type) {
    case AST_PROGRAM:
        fprintf(out, "PROGRAM");
        break;
    case AST_FUNC_DECL:
        fprintf(out, "FUNCTION %s : %s  (frame %d bytes)", node->name,
                type_name(node->vtype), node->frame_size);
        break;
    case AST_PARAM:
        fprintf(out, "PARAM %s : %s  @ %d(%%rbp)", node->name,
                type_name(node->vtype), node->stack_offset);
        break;
    case AST_VAR_DECL:
        if (node->is_global) {
            fprintf(out, "GLOBAL %s : %s  (.data, init %lld)", node->name,
                    type_name(node->vtype), node->value);
        } else {
            fprintf(out, "LOCAL %s : %s  @ %d(%%rbp)", node->name,
                    type_name(node->vtype), node->stack_offset);
        }
        break;
    case AST_ASSIGN:
        fprintf(out, "ASSIGN");
        break;
    case AST_IF:
        fprintf(out, "IF");
        break;
    case AST_WHILE:
        fprintf(out, "WHILE");
        break;
    case AST_DO_WHILE:
        fprintf(out, "DO-WHILE");
        break;
    case AST_FOR:
        fprintf(out, "FOR");
        break;
    case AST_SWITCH:
        fprintf(out, "SWITCH");
        break;
    case AST_CASE:
        fprintf(out, "CASE %lld", node->value);
        break;
    case AST_DEFAULT:
        fprintf(out, "DEFAULT");
        break;
    case AST_BREAK:
        fprintf(out, "BREAK");
        break;
    case AST_CONTINUE:
        fprintf(out, "CONTINUE");
        break;
    case AST_RETURN:
        fprintf(out, "RETURN");
        break;
    case AST_BLOCK:
        fprintf(out, "BLOCK");
        break;
    case AST_BINARY_OP:
        fprintf(out, "BINOP %s", op_name(node->op));
        break;
    case AST_TERNARY:
        fprintf(out, "TERNARY ?:");
        break;
    case AST_POSTFIX:
        fprintf(out, "POSTFIX %s", node->op == OP_ADD ? "++" : "--");
        break;
    case AST_VAR_REF:
        fprintf(out, "VAR %s : %s", node->name, type_name(node->vtype));
        break;
    case AST_INT_LIT:
        fprintf(out, "INT %lld", node->value);
        break;
    case AST_CALL:
        fprintf(out, "CALL %s", node->name);
        break;
    }
    fprintf(out, "  [%d:%d]\n", node->line, node->col);

    int d = depth + 1;
    switch (node->type) {
    case AST_PROGRAM:
        dump_chain(node->body, d, out);
        break;
    case AST_FUNC_DECL:
        dump_chain(node->left, d, out); /* parameters */
        dump_label("body", node->body, d, out);
        break;
    case AST_PARAM:
    case AST_VAR_DECL:
        break;
    case AST_ASSIGN:
        dump_label("lhs", node->left, d, out);
        dump_label("rhs", node->right, d, out);
        break;
    case AST_IF:
        dump_label("cond", node->cond, d, out);
        dump_label("then", node->body, d, out);
        dump_label("else", node->right, d, out);
        break;
    case AST_WHILE:
        dump_label("cond", node->cond, d, out);
        dump_label("body", node->body, d, out);
        break;
    case AST_DO_WHILE:
        dump_label("body", node->body, d, out);
        dump_label("cond", node->cond, d, out);
        break;
    case AST_FOR:
        dump_label("init", node->left, d, out);
        dump_label("cond", node->cond, d, out);
        dump_label("step", node->right, d, out);
        dump_label("body", node->body, d, out);
        break;
    case AST_SWITCH:
        dump_label("expr", node->cond, d, out);
        dump_chain(node->body, d, out);
        break;
    case AST_CASE:
    case AST_DEFAULT:
    case AST_BREAK:
    case AST_CONTINUE:
        break;
    case AST_RETURN:
        dump_label("expr", node->left, d, out);
        break;
    case AST_BLOCK:
        dump_chain(node->body, d, out);
        break;
    case AST_BINARY_OP:
        dump_node(node->left, d, out);
        dump_node(node->right, d, out);
        break;
    case AST_TERNARY:
        dump_label("cond", node->cond, d, out);
        dump_label("then", node->left, d, out);
        dump_label("else", node->right, d, out);
        break;
    case AST_POSTFIX:
        dump_node(node->left, d, out);
        break;
    case AST_CALL:
        dump_chain(node->args, d, out);
        break;
    case AST_VAR_REF:
    case AST_INT_LIT:
        break;
    }
}

void dump_ast(const ASTNode *root, FILE *out)
{
    if (root != NULL) {
        dump_node(root, 0, out);
    }
}
