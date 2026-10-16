
#include "json.h"


typedef struct JSON_TYPE JSON_TYPE;


JSON_TYPE *json_lex_string(int *i, char *json_str, size_t json_len)
{
    int j, tmp = *i;
    char c, *data = malloc(json_len + 1);
    JSON_TYPE *token = malloc(sizeof(JSON_TYPE));

    if (json_str[*i] == '"') *i += 1;
    else
    {
        free(data);
        free(token);
        return NULL;
    } 

    j = 0;
    while ((c = json_str[*i]) != '\0')
    {
        if (c == '"')
        {
            token->type = 0;
            data[j] = '\0';
            data = realloc(data, strlen(data) + 1);
            token->data = data;
            return token;
        }
        else data[j] = c;
        *i += 1;
        j += 1;
    }

    *i = -1;
    printf("String at index %d is not terminated\n", tmp);
    free(data);
    free(token);
    return NULL;
}


JSON_TYPE *json_lex_number(int *i, char *json_str, size_t json_len)
{
    // TODO: Number parsing is an oversimplification of the required process. Read into it.

    int j;
    double *data = malloc(sizeof(double));
    char c, *data_str = malloc(json_len + 1);
    JSON_TYPE *token = malloc(sizeof(JSON_TYPE));

    j = 0;
    while ((c = json_str[*i]) != '\0')
    {
        if (strchr("0123456789-.e", c) != NULL)
            data_str[j] = c;
        else
            break;

        *i += 1;
        j += 1;
    }

    if (j == 0)
    {
        free(data);
        free(data_str);
        free(token);
        return NULL;
    }

    token->type = 1;
    data_str[j] = '\0';
    *data = strtod(data_str, NULL);
    token->data = data;
    free(data_str);
    return token;
}


JSON_TYPE *json_lex_bool(int *i, char *json_str, size_t json_len)
{
    int true_len = strlen("true");
    int false_len = strlen("false");
    JSON_TYPE *token = malloc(sizeof(JSON_TYPE));
    token->data = NULL;

    if (json_len >= true_len && strncmp(&json_str[*i], "true", true_len) == 0)
        token->type = 2;
    else if (json_len >= false_len && strncmp(&json_str[*i], "false", false_len) == 0)
        token->type = 3;
    else 
    {
        free(token);
        return NULL;
    }

    return token;
}


JSON_TYPE *json_lex_null(int *i, char *json_str, size_t json_len)
{
    int null_len = strlen("null");
    JSON_TYPE *token = malloc(sizeof(JSON_TYPE));
    token->data = NULL;

    if (json_len >= null_len && strncmp(&json_str[*i], "null", null_len) == 0)
        token->type = 4;
    else 
    {
        free(token);
        return NULL;
    }

    return token;
}


int json_lex(char *json_str, JSON_TYPE ***tokens)
{
    int i = 0;
    int token_idx = 0;
    char *p;
    size_t json_len = strlen(json_str);
    JSON_TYPE *token, **tokens_ = NULL;

    while (json_str[i] != '\0')
    {
        token = json_lex_string(&i, json_str, json_len);
        if (i == -1)
        {
            free(tokens_);
            return -1;
        }
        else if (token != NULL) 
        {
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
            i += 1;
            continue;
        }

        token = json_lex_number(&i, json_str, json_len);
        if (i == -1)
        {
            free(tokens_);
            return -1;
        }
        else if (token != NULL) 
        {
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
            i += 1;
            continue;
        }

        token = json_lex_bool(&i, json_str, json_len);
        if (token != NULL) 
        {
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
            i += 1;
            continue;
        }

        token = json_lex_null(&i, json_str, json_len);
        if (token != NULL) 
        {
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
            i += 1;
            continue;
        }

        if (strchr(" \t\r\n", json_str[i]) != NULL)
            i += 1;
        else if (strchr(",[]{}:", json_str[i]) != NULL)
        {
            token = malloc(sizeof(JSON_TYPE));
            p = malloc(sizeof(char));
            *p = json_str[i];
            token->type = 5;
            token->data = p;
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
            i += 1;
            continue;
        }
        else 
        {
            printf("Unexpected character '%c' at index %d.\n", json_str[i], i);
            free(tokens_);
            return -1;
        }
    }

    *tokens = tokens_;
    return token_idx;
}


void free_tokens(JSON_TYPE **tokens, int tokens_len)
{
    JSON_TYPE *token;
    for (int i = 0; i < tokens_len; i++) 
    {
        free(token->data);
        free(token);
    }
}


void print_tokens(JSON_TYPE **tokens, int tokens_len)
{
    JSON_TYPE *token;

    for (int i = 0; i < tokens_len; i++)
    {
        token = tokens[i];
        printf("%d, ", token->type);
    }
    puts("");

    for (int i = 0; i < tokens_len; i++)
    {
        token = tokens[i];
        switch (token->type) 
        {
            case 0: printf("<\"%s\">, ", (char*)token->data); break;
            case 1: printf("<%lf>, ", *((double*)token->data)); break;
            case 2: printf("<true>, "); break;
            case 3: printf("<false>, "); break;
            case 4: printf("<null>, "); break;
            case 5: printf("<%c>, ", *((char*)token->data)); break;
            default: break;
        }
    }
    puts("");
}


int main(void)
{
    int tokens_len;
    char *s = "{\"name\": \"John Smith\", \"age\": 53}";
    JSON_TYPE **tokens;

    tokens_len = json_lex(s, &tokens);
    print_tokens(tokens, tokens_len);
    free_tokens(tokens, tokens_len);

    return 0;
}