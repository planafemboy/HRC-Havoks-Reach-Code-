#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include our lexer */
#include "lexer.c"

/* Node Types - what kind of statement is this */
typedef enum
{
    NODE_ASSIGN,   /* h = 100 */
    NODE_OUTPUT,   /* >> h */
    NODE_IF,       /* |> { } */
    NODE_ELSE,     /* |< { } */
    NODE_LOOP,     /* <-> { } */
    NODE_FUNCTION, /* ^ name { } */
    NODE_CHAIN,    /* ->->-> { } */
    NODE_MATH,     /* ad su mu di */
    NODE_NUMBER,   /* 100 */
    NODE_VARIABLE, /* h */
    NODE_TEXT,     /* "hello" */
    NODE_BLOCK,    /* { } */
    NODE_TRUE,     /* /|\ */
    NODE_FALSE     /* /\ */
} NodeType;

/* AST Node - represents one piece of HRC code */
typedef struct Node
{
    NodeType type;
    char value[256];
    struct Node *left;
    struct Node *right;
    struct Node *body;
    struct Node *next;
} Node;

/* Parser Structure */
typedef struct
{
    Lexer lexer;
    Token current_token;
} Parser;

/* Create a new node */
Node *make_node(NodeType type, char *value)
{
    Node *node = malloc(sizeof(Node));
    node->type = type;
    strncpy(node->value, value, 255);
    node->left = NULL;
    node->right = NULL;
    node->body = NULL;
    node->next = NULL;
    return node;
}

/* Move to next token */
void eat(Parser *parser)
{
    parser->current_token = next_token(&parser->lexer);
}

/* Initialize parser */
Parser init_parser(char *source)
{
    Parser parser;
    parser.lexer = init_lexer(source);
    parser.current_token = next_token(&parser.lexer);
    return parser;
}

/* Forward declaration */
Node *parse_statement(Parser *parser);
Node *parse_block(Parser *parser);
Node *parse_expression(Parser *parser);

/* Parse expression - numbers, variables, math */
Node *parse_expression(Parser *parser)
{
    Token token = parser->current_token;

    /* Math operations ad su mu di */
    if (token.type == TOKEN_ADD || token.type == TOKEN_SUB ||
        token.type == TOKEN_MUL || token.type == TOKEN_DIV)
    {
        char op[256];
        strncpy(op, token.value, 255);
        eat(parser);

        /* Expect { */
        eat(parser); /* skip { */

        Node *left = parse_expression(parser);
        Node *right = parse_expression(parser);

        /* Expect } */
        eat(parser); /* skip } */

        Node *node = make_node(NODE_MATH, op);
        node->left = left;
        node->right = right;
        return node;
    }

    /* Number */
    if (token.type == TOKEN_NUMBER || token.type == TOKEN_DECIMAL)
    {
        eat(parser);
        return make_node(NODE_NUMBER, token.value);
    }

    /* Variable */
    if (token.type == TOKEN_VARIABLE)
    {
        eat(parser);
        return make_node(NODE_VARIABLE, token.value);
    }

    /* Text */
    if (token.type == TOKEN_TEXT)
    {
        eat(parser);
        return make_node(NODE_TEXT, token.value);
    }

    /* True /|\ */
    if (token.type == TOKEN_TRUE)
    {
        eat(parser);
        return make_node(NODE_TRUE, "/|\\");
    }

    /* False /\ */
    if (token.type == TOKEN_FALSE)
    {
        eat(parser);
        return make_node(NODE_FALSE, "/\\");
    }

    eat(parser);
    return make_node(NODE_NUMBER, "0");
}

/* Parse block { statements } */
Node *parse_block(Parser *parser)
{
    /* Skip { */
    eat(parser);

    Node *block = make_node(NODE_BLOCK, "block");
    Node *current = NULL;

    while (parser->current_token.type != TOKEN_RBRACE &&
           parser->current_token.type != TOKEN_EOF)
    {
        Node *stmt = parse_statement(parser);
        if (block->body == NULL)
        {
            block->body = stmt;
            current = stmt;
        }
        else
        {
            current->next = stmt;
            current = stmt;
        }
    }

    /* Skip } */
    eat(parser);
    return block;
}

/* Parse one statement */
Node *parse_statement(Parser *parser)
{
    Token token = parser->current_token;

    /* Skip comments */
    if (token.type == TOKEN_COMMENT)
    {
        eat(parser);
        return parse_statement(parser);
    }

    /* Assignment h = something */
    if (token.type == TOKEN_VARIABLE)
    {
        char varname[256];
        strncpy(varname, token.value, 255);
        eat(parser);

        if (parser->current_token.type == TOKEN_EQUALS)
        {
            eat(parser); /* skip = */
            Node *value = parse_expression(parser);
            Node *node = make_node(NODE_ASSIGN, varname);
            node->right = value;
            return node;
        }
        return make_node(NODE_VARIABLE, varname);
    }

    /* Output >> */
    if (token.type == TOKEN_OUTPUT)
    {
        eat(parser);
        Node *value = parse_expression(parser);
        Node *node = make_node(NODE_OUTPUT, ">>");
        node->right = value;
        return node;
    }

    /* If |> { } */
    if (token.type == TOKEN_IF)
    {
        eat(parser);
        Node *body = parse_block(parser);
        Node *node = make_node(NODE_IF, "|>");
        node->body = body;
        return node;
    }

    /* Else |< { } */
    if (token.type == TOKEN_ELSE)
    {
        eat(parser);
        Node *body = parse_block(parser);
        Node *node = make_node(NODE_ELSE, "|<");
        node->body = body;
        return node;
    }

    /* Loop <-> { } */
    if (token.type == TOKEN_LOOP)
    {
        eat(parser);
        Node *body = parse_block(parser);
        Node *node = make_node(NODE_LOOP, "<->");
        node->body = body;
        return node;
    }

    /* Chain ->->-> { } */
    if (token.type == TOKEN_CHAIN)
    {
        eat(parser);
        Node *body = parse_block(parser);
        Node *node = make_node(NODE_CHAIN, "->->->");
        node->body = body;
        return node;
    }

    /* Function ^ name { } */
    if (token.type == TOKEN_FUNCTION)
    {
        eat(parser);
        char fname[256];
        strncpy(fname, parser->current_token.value, 255);
        eat(parser);
        Node *body = parse_block(parser);
        Node *node = make_node(NODE_FUNCTION, fname);
        node->body = body;
        return node;
    }

    eat(parser);
    return make_node(NODE_NUMBER, "0");
}

/* Parse entire program */
Node *parse(Parser *parser)
{
    Node *program = make_node(NODE_BLOCK, "program");
    Node *current = NULL;

    while (parser->current_token.type != TOKEN_EOF)
    {
        Node *stmt = parse_statement(parser);
        if (program->body == NULL)
        {
            program->body = stmt;
            current = stmt;
        }
        else
        {
            current->next = stmt;
            current = stmt;
        }
    }
    return program;
}

/* Print AST for debugging */
void print_node(Node *node, int depth)
{
    if (node == NULL)
        return;

    for (int i = 0; i < depth; i++)
        printf("  ");

    switch (node->type)
    {
    case NODE_ASSIGN:
        printf("ASSIGN: %s\n", node->value);
        break;
    case NODE_OUTPUT:
        printf("OUTPUT\n");
        break;
    case NODE_IF:
        printf("IF\n");
        break;
    case NODE_ELSE:
        printf("ELSE\n");
        break;
    case NODE_LOOP:
        printf("LOOP\n");
        break;
    case NODE_FUNCTION:
        printf("FUNCTION: %s\n", node->value);
        break;
    case NODE_CHAIN:
        printf("CHAIN\n");
        break;
    case NODE_MATH:
        printf("MATH: %s\n", node->value);
        break;
    case NODE_NUMBER:
        printf("NUMBER: %s\n", node->value);
        break;
    case NODE_VARIABLE:
        printf("VARIABLE: %s\n", node->value);
        break;
    case NODE_TEXT:
        printf("TEXT: %s\n", node->value);
        break;
    case NODE_BLOCK:
        printf("BLOCK\n");
        break;
    case NODE_TRUE:
        printf("TRUE\n");
        break;
    case NODE_FALSE:
        printf("FALSE\n");
        break;
    default:
        printf("UNKNOWN\n");
        break;
    }

    if (node->left)
        print_node(node->left, depth + 1);
    if (node->right)
        print_node(node->right, depth + 1);
    if (node->body)
        print_node(node->body, depth + 1);
    if (node->next)
        print_node(node->next, depth);
}

