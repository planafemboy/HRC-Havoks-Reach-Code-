#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include parser which includes lexer */
#include "parser.c"

/* Variable storage */
typedef struct Variable {
    char name[256];
    char value[256];
    int is_number;
    float number_value;
    struct Variable *next;
} Variable;

/* Environment - stores all variables */
typedef struct {
    Variable *variables;
} Environment;

/* Create new environment */
Environment* create_env() {
    Environment *env = malloc(sizeof(Environment));
    env->variables = NULL;
    return env;
}

/* Set a variable */
void set_var(Environment *env, char *name, char *value, float number_value, int is_number) {
    Variable *var = env->variables;
    while (var != NULL) {
        if (strcmp(var->name, name) == 0) {
            strncpy(var->value, value, 255);
            var->number_value = number_value;
            var->is_number = is_number;
            return;
        }
        var = var->next;
    }
    Variable *new_var = malloc(sizeof(Variable));
    strncpy(new_var->name, name, 255);
    strncpy(new_var->value, value, 255);
    new_var->number_value = number_value;
    new_var->is_number = is_number;
    new_var->next = env->variables;
    env->variables = new_var;
}

/* Get a variable */
Variable* get_var(Environment *env, char *name) {
    Variable *var = env->variables;
    while (var != NULL) {
        if (strcmp(var->name, name) == 0)
            return var;
        var = var->next;
    }
    return NULL;
}

/* Result of evaluating a node */
typedef struct {
    char value[256];
    float number_value;
    int is_number;
} Result;

/* Forward declaration */
Result evaluate(Node *node, Environment *env);

/* Evaluate a node */
Result evaluate(Node *node, Environment *env) {
    Result result;
    result.number_value = 0;
    result.is_number = 0;
    strncpy(result.value, "", 255);

    if (node == NULL) return result;

    switch(node->type) {

        case NODE_NUMBER: {
            result.number_value = atof(node->value);
            result.is_number = 1;
            strncpy(result.value, node->value, 255);
            return result;
        }

        case NODE_VARIABLE: {
            Variable *var = get_var(env, node->value);
            if (var != NULL) {
                result.number_value = var->number_value;
                result.is_number = var->is_number;
                strncpy(result.value, var->value, 255);
            }
            return result;
        }

        case NODE_TEXT: {
            strncpy(result.value, node->value, 255);
            result.is_number = 0;
            return result;
        }

        case NODE_TRUE: {
            strncpy(result.value, "true", 255);
            result.number_value = 1;
            result.is_number = 1;
            return result;
        }

        case NODE_FALSE: {
            strncpy(result.value, "false", 255);
            result.number_value = 0;
            result.is_number = 1;
            return result;
        }

        case NODE_ASSIGN: {
            Result val = evaluate(node->right, env);
            char num_str[256];
            if (val.is_number) {
                snprintf(num_str, 255, "%g", val.number_value);
                set_var(env, node->value, num_str, val.number_value, 1);
            } else {
                set_var(env, node->value, val.value, 0, 0);
            }
            return val;
        }

        case NODE_OUTPUT: {
            Result val = evaluate(node->right, env);
            if (val.is_number) {
                printf("%g\n", val.number_value);
            } else {
                printf("%s\n", val.value);
            }
            return val;
        }

        case NODE_MATH: {
            Result left = evaluate(node->left, env);
            Result right = evaluate(node->right, env);
            result.is_number = 1;

            if (strcmp(node->value, "ad") == 0)
                result.number_value = left.number_value + right.number_value;
            else if (strcmp(node->value, "su") == 0)
                result.number_value = left.number_value - right.number_value;
            else if (strcmp(node->value, "mu") == 0)
                result.number_value = left.number_value * right.number_value;
            else if (strcmp(node->value, "di") == 0) {
                if (right.number_value != 0)
                    result.number_value = left.number_value / right.number_value;
                else {
                    printf("{*} ERROR: Division by zero\n");
                    result.number_value = 0;
                }
            }
            char num_str[256];
            snprintf(num_str, 255, "%g", result.number_value);
            strncpy(result.value, num_str, 255);
            return result;
        }

        case NODE_IF: {
            if (node->body != NULL)
                evaluate(node->body, env);
            return result;
        }

        case NODE_ELSE: {
            if (node->body != NULL)
                evaluate(node->body, env);
            return result;
        }

        case NODE_LOOP: {
            if (node->body != NULL)
                evaluate(node->body, env);
            return result;
        }

        case NODE_CHAIN: {
            if (node->body != NULL)
                evaluate(node->body, env);
            return result;
        }

        case NODE_FUNCTION: {
            if (node->body != NULL)
                evaluate(node->body, env);
            return result;
        }

        case NODE_BLOCK: {
            Node *current = node->body;
            while (current != NULL) {
                result = evaluate(current, env);
                current = current->next;
            }
            return result;
        }

        default:
            return result;
    }
}

/* Main */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: hrc <file.hrc>\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        printf("{*} ERROR: Cannot open file %s\n", argv[1]);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = malloc(size + 1);
    fread(source, 1, size, file);
    source[size] = '\0';
    fclose(file);

    Parser parser = init_parser(source);
    Node *ast = parse(&parser);
    Environment *env = create_env();
    evaluate(ast, env);

    free(source);
    return 0;
}