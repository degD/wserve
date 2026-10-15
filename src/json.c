
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Parsing JSON in two steps:
// 1. Lexical analysis (tokenizing)
// 2. Syntatic analysis (parsing)

/* 0: string
 * 1: number
 * 2: boolean
 * 3: null
 * 4: syntax
 */
typedef struct JSON_TYPE
{
    int type;
    void *data;
}
JSON_TYPE;


JSON_TYPE *json_lex_string(int *i, char *json_str, size_t json_len)
{
    int j;
    char c, *data = malloc(json_len + 1);
    JSON_TYPE *token = malloc(sizeof(JSON_TYPE));

    if (json_str[*i] == '"') *i += 1;
    else return NULL;

    while ((c = json_str[*i]) != '\0')
    {
        if (c == '"')
        {
            token->type = 0;
            data[j] = '\0';
            data = realloc(data, strlen(data) + 1);
            return token;
        }
        else data[j] = c;
        *i += 1;
        j += 1;
    }

    *i = -1;
    return NULL;
}


char **json_lex(char *json_str)
{
    int i = 0;
    int token_idx = 0;
    char *p;
    size_t json_len = strlen(json_str);
    JSON_TYPE *token, **tokens = NULL;

    while (json_str[i] != '\0')
    {
        token = json_lex_string(&i, json_str, json_len);
        if (i == -1) return NULL;
        else if (token != NULL) 
        {
            tokens = realloc(tokens, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens[token_idx] = token;
            token_idx += 1;
            continue;
        }

        // other lexers...

        if (json_str[i] == ' '  || 
            json_str[i] == '\t' || 
            json_str[i] == '\r' || 
            json_str[i] == '\n'
        )
        i += 1;

        else if (1) // if json syntax (what are they?)
        {
            token = malloc(sizeof(JSON_TYPE));
            p = malloc(sizeof(char));
            *p = json_str[i];
            token->type = 4;
            token->data = p;
            tokens = realloc(tokens, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens[token_idx] = token;
            token_idx += 1;
            continue;
        }

        else 
        {
            printf("Unexpected character '%c' at index %d.\n", json_str[i], i);
            return NULL;
        }

    }
}

