#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* HRC Lexer - Havoks Reach Code */

/* Token Types */
typedef enum
{
    TOKEN_NUMBER,   /* 100, 200 */
    TOKEN_DECIMAL,  /* 3.14 */
    TOKEN_TEXT,     /* "hello" */
    TOKEN_TRUE,     /* /|\ */
    TOKEN_FALSE,    /* /\ */
    TOKEN_EQUALS,   /* = */
    TOKEN_ADD,      /* ad */
    TOKEN_SUB,      /* su */
    TOKEN_MUL,      /* mu */
    TOKEN_DIV,      /* di */
    TOKEN_IF,       /* |> */
    TOKEN_ELSE,     /* |< */
    TOKEN_LOOP,     /* <-> */
    TOKEN_FUNCTION, /* ^ */
    TOKEN_CHAIN,    /* ->->-> */
    TOKEN_OUTPUT,   /* >> */
    TOKEN_COMMENT,  /* ## */
    TOKEN_GLOBAL,   /* ** */
    TOKEN_DESTROY,  /* ~~ */
    TOKEN_SYSTEM,   /* ;/; */
    TOKEN_STORE,    /* @^ */
    TOKEN_LBRACE,   /* { */
    TOKEN_RBRACE,   /* } */
    TOKEN_LBRACKET, /* [ */
    TOKEN_RBRACKET, /* ] */
    TOKEN_VARIABLE, /* h, m, st etc */
    TOKEN_ERROR,    /* unknown */
    TOKEN_EOF       /* end of file */
} TokenType;

/* Token Structure */
typedef struct
{
    TokenType type;
    char value[256];
    int line;
} Token;

/* Lexer Structure */
typedef struct
{
    char *source;
    int pos;
    int line;
} Lexer;

/* Initialize Lexer */
Lexer init_lexer(char *source)
{
    Lexer lexer;
    lexer.source = source;
    lexer.pos = 0;
    lexer.line = 1;
    return lexer;
}

/* Get current character */
char current(Lexer *lexer)
{
    return lexer->source[lexer->pos];
}

/* Move to next character */
void advance(Lexer *lexer)
{
    if (current(lexer) == '\n')
        lexer->line++;
    lexer->pos++;
}

/* Skip whitespace */
void skip_whitespace(Lexer *lexer)
{
    while (current(lexer) == ' ' ||
           current(lexer) == '\t' ||
           current(lexer) == '\n')
    {
        advance(lexer);
    }
}

/* Create a token */
Token make_token(TokenType type, char *value, int line)
{
    Token token;
    token.type = type;
    strncpy(token.value, value, 255);
    token.line = line;
    return token;
}

/* Main Lexer Function */
Token next_token(Lexer *lexer)
{
    skip_whitespace(lexer);

    if (current(lexer) == '\0')
        return make_token(TOKEN_EOF, "EOF", lexer->line);

    char c = current(lexer);
    int line = lexer->line;

    /* Comments ## */
    if (c == '#')
    {
        advance(lexer);
        if (current(lexer) == '#')
        {
            advance(lexer);
            while (current(lexer) != '\n' && current(lexer) != '\0')
                advance(lexer);
            return make_token(TOKEN_COMMENT, "##", line);
        }
    }

    /* Output >> */
    if (c == '>')
    {
        advance(lexer);
        if (current(lexer) == '>')
        {
            advance(lexer);
            return make_token(TOKEN_OUTPUT, ">>", line);
        }
    }

    /* If |> and Else |< */
    if (c == '|')
    {
        advance(lexer);
        if (current(lexer) == '>')
        {
            advance(lexer);
            return make_token(TOKEN_IF, "|>", line);
        }
        if (current(lexer) == '<')
        {
            advance(lexer);
            return make_token(TOKEN_ELSE, "|<", line);
        }
    }

    /* Chain ->->-> */
    if (c == '-')
    {
        advance(lexer);
        if (current(lexer) == '>')
        {
            advance(lexer);
            if (current(lexer) == '-')
            {
                advance(lexer);
                if (current(lexer) == '>')
                {
                    advance(lexer);
                    if (current(lexer) == '-')
                    {
                        advance(lexer);
                        if (current(lexer) == '>')
                        {
                            advance(lexer);
                            return make_token(TOKEN_CHAIN, "->->->", line);
                        }
                    }
                }
            }
        }
    }

    /* Loop <-> */
    if (c == '<')
    {
        advance(lexer);
        if (current(lexer) == '-')
        {
            advance(lexer);
            if (current(lexer) == '>')
            {
                advance(lexer);
                return make_token(TOKEN_LOOP, "<->", line);
            }
        }
    }

    /* Function ^ */
    if (c == '^')
    {
        advance(lexer);
        return make_token(TOKEN_FUNCTION, "^", line);
    }

    /* Equals = */
    if (c == '=')
    {
        advance(lexer);
        return make_token(TOKEN_EQUALS, "=", line);
    }

    /* Global ** */
    if (c == '*')
    {
        advance(lexer);
        if (current(lexer) == '*')
        {
            advance(lexer);
            return make_token(TOKEN_GLOBAL, "**", line);
        }
    }

    /* Destroy ~~ */
    if (c == '~')
    {
        advance(lexer);
        if (current(lexer) == '~')
        {
            advance(lexer);
            return make_token(TOKEN_DESTROY, "~~", line);
        }
    }

    /* Left Brace { */
    if (c == '{')
    {
        advance(lexer);
        return make_token(TOKEN_LBRACE, "{", line);
    }

    /* Right Brace } */
    if (c == '}')
    {
        advance(lexer);
        return make_token(TOKEN_RBRACE, "}", line);
    }

    /* Left Bracket [ */
    if (c == '[')
    {
        advance(lexer);
        return make_token(TOKEN_LBRACKET, "[", line);
    }

    /* Right Bracket ] */
    if (c == ']')
    {
        advance(lexer);
        return make_token(TOKEN_RBRACKET, "]", line);
    }

    /* Numbers */
    if (isdigit(c))
    {
        char num[256];
        int i = 0;
        int is_decimal = 0;
        while (isdigit(current(lexer)) || current(lexer) == '.')
        {
            if (current(lexer) == '.')
                is_decimal = 1;
            num[i++] = current(lexer);
            advance(lexer);
        }
        num[i] = '\0';
        return make_token(is_decimal ? TOKEN_DECIMAL : TOKEN_NUMBER, num, line);
    }

    /* Text strings "" */
    if (c == '"')
    {
        advance(lexer);
        char text[256];
        int i = 0;
        while (current(lexer) != '"' && current(lexer) != '\0')
        {
            text[i++] = current(lexer);
            advance(lexer);
        }
        advance(lexer);
        text[i] = '\0';
        return make_token(TOKEN_TEXT, text, line);
    }

    /* True /|\ */
    if (c == '/')
    {
        advance(lexer);
        if (current(lexer) == '|')
        {
            advance(lexer);
            if (current(lexer) == '\\')
            {
                advance(lexer);
                return make_token(TOKEN_TRUE, "/|\\", line);
            }
        }
        /* False /\ */
        if (current(lexer) == '\\')
        {
            advance(lexer);
            return make_token(TOKEN_FALSE, "/\\", line);
        }
    }

    /* Keywords and Variables */
    if (isalpha(c))
    {
        char word[256];
        int i = 0;
        while (isalpha(current(lexer)) || current(lexer) == '_')
        {
            word[i++] = current(lexer);
            advance(lexer);
        }
        word[i] = '\0';
        if (strcmp(word, "ad") == 0)
            return make_token(TOKEN_ADD, "ad", line);
        if (strcmp(word, "su") == 0)
            return make_token(TOKEN_SUB, "su", line);
        if (strcmp(word, "mu") == 0)
            return make_token(TOKEN_MUL, "mu", line);
        if (strcmp(word, "di") == 0)
            return make_token(TOKEN_DIV, "di", line);
        /* Everything else is a variable */
        return make_token(TOKEN_VARIABLE, word, line);
    }

    /* Unknown token */
    advance(lexer);
    return make_token(TOKEN_ERROR, "?", line);
}

/* Print token type as string */
char *token_name(TokenType type)
{
    switch (type)
    {
    case TOKEN_NUMBER:
        return "NUMBER";
    case TOKEN_DECIMAL:
        return "DECIMAL";
    case TOKEN_TEXT:
        return "TEXT";
    case TOKEN_TRUE:
        return "TRUE";
    case TOKEN_FALSE:
        return "FALSE";
    case TOKEN_EQUALS:
        return "EQUALS";
    case TOKEN_ADD:
        return "ADD";
    case TOKEN_SUB:
        return "SUB";
    case TOKEN_MUL:
        return "MUL";
    case TOKEN_DIV:
        return "DIV";
    case TOKEN_IF:
        return "IF";
    case TOKEN_ELSE:
        return "ELSE";
    case TOKEN_LOOP:
        return "LOOP";
    case TOKEN_FUNCTION:
        return "FUNCTION";
    case TOKEN_CHAIN:
        return "CHAIN";
    case TOKEN_OUTPUT:
        return "OUTPUT";
    case TOKEN_COMMENT:
        return "COMMENT";
    case TOKEN_GLOBAL:
        return "GLOBAL";
    case TOKEN_DESTROY:
        return "DESTROY";
    case TOKEN_SYSTEM:
        return "SYSTEM";
    case TOKEN_STORE:
        return "STORE";
    case TOKEN_LBRACE:
        return "LBRACE";
    case TOKEN_RBRACE:
        return "RBRACE";
    case TOKEN_LBRACKET:
        return "LBRACKET";
    case TOKEN_RBRACKET:
        return "RBRACKET";
    case TOKEN_VARIABLE:
        return "VARIABLE";
    case TOKEN_ERROR:
        return "ERROR";
    case TOKEN_EOF:
        return "EOF";
    default:
        return "UNKNOWN";
    }
}
