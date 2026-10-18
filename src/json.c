
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
    {
        *i += true_len;
        token->type = 2;
    }
    else if (json_len >= false_len && strncmp(&json_str[*i], "false", false_len) == 0)
    {
        *i += false_len;
        token->type = 3;
    }
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
    {
        *i += null_len;
        token->type = 4;
    }
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
            continue;
        }

        token = json_lex_bool(&i, json_str, json_len);
        if (token != NULL) 
        {
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
            continue;
        }

        token = json_lex_null(&i, json_str, json_len);
        if (token != NULL) 
        {
            tokens_ = realloc(tokens_, sizeof(JSON_TYPE) * (token_idx + 1));
            tokens_[token_idx] = token;
            token_idx += 1;
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
    if (tokens == NULL) return;

    JSON_TYPE *token;
    for (int i = 0; i < tokens_len; i++) 
    {
        token = tokens[i];
        free(token->data);
        free(token);
    }
    free(tokens);
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


JSON_TYPE *json_array_init()
{
    JSON_ARR *array_data = malloc(sizeof(JSON_ARR));
    JSON_TYPE *array = malloc(sizeof(JSON_TYPE));
    array->type = JSON_TYPE_ARR;
    array->data = array_data;
    array_data->len = 0;
    array_data->items = NULL;
    return array;
}


JSON_TYPE *json_object_init()
{
    JSON_OBJ *obj_data = malloc(sizeof(JSON_OBJ));
    JSON_TYPE *object = malloc(sizeof(JSON_TYPE));
    object->type = JSON_TYPE_OBJ;
    object->data = obj_data;
    obj_data->len = 0;
    obj_data->keys = NULL;
    obj_data->vals = NULL;
    return object;
}


int json_array_push(JSON_TYPE *array, JSON_TYPE *item)
{
    if (array == NULL || array->type != JSON_TYPE_ARR || array->data == NULL) return 0;

    JSON_ARR *data = array->data;
    
    data->len += 1;
    data->items = realloc(data->items, sizeof(JSON_TYPE) * data->len);
    data->items[data->len - 1] = item;

    return 1;
}


int json_object_push(JSON_TYPE *object, char *key, JSON_TYPE *val)
{
    if (object == NULL || object->type != JSON_TYPE_OBJ || object->data == NULL) return 0;

    JSON_OBJ *data = object->data;
    
    data->len += 1;
    data->keys = realloc(data->keys, sizeof(char*) * data->len);
    data->vals = realloc(data->vals, sizeof(JSON_TYPE*) * data->len);
    data->keys[data->len - 1] = key;
    data->vals[data->len - 1] = val;

    return 1;
}


int json_token_syntax_char_cmp(JSON_TYPE *t, char c)
{
    if (t->type == JSON_TYPE_SYN && *((char*)t->data) == c) 
        return 1;
    else
        return 0;
}


JSON_TYPE *json_parse_array(JSON_TYPE **tokens, int tokens_len, int *i)
{
    JSON_TYPE *t;
    JSON_TYPE *array = json_array_init();

    // empty array
    t = tokens[*i];
    if (json_token_syntax_char_cmp(t, ']'))
    {
        *i += 1;
        return array;
    }

    while (*i < tokens_len)
    {
        t = json_parse(tokens, tokens_len, i);
        if (t == NULL) return NULL;
        json_array_push(array, t);

        if (*i >= tokens_len) break;

        t = tokens[*i];
        if (json_token_syntax_char_cmp(t, ']'))
        {
            *i += 1;
            return array;
        }
        else if (!json_token_syntax_char_cmp(t, ','))
        {
            printf("Token %d: Expected comma (,) after item in array.\n", *i);
            return NULL;
        }
        else
            *i += 1;
    }

    printf("EOL: End of array ']' not found.\n");
    return NULL;
}


JSON_TYPE *json_parse_object(JSON_TYPE **tokens, int tokens_len, int *i)
{
    JSON_TYPE *t, *t_key, *t_val;
    JSON_TYPE *object = json_object_init();

    t = tokens[*i];
    if (json_token_syntax_char_cmp(t, '}'))
    {
        *i += 1;
        return object;
    }

    while (*i < tokens_len)
    {
        t_key = tokens[*i];
        if (t_key->type != JSON_TYPE_STR)
        {
            printf("Token %d: Expected string key.\n", *i);
            return NULL;
        }
        *i += 1;

        if (*i >= tokens_len) break;        

        t = tokens[*i];
        if (!json_token_syntax_char_cmp(t, ':'))
        {
            printf("Token %d: Expected colon after key.\n", *i);
            return NULL;
        }
        *i += 1;
        
        if (*i >= tokens_len) break;

        t_val = json_parse(tokens, tokens_len, i);
        if (t_val == NULL) return NULL;
        json_object_push(object, t_key->data, t_val);

        if (*i >= tokens_len) break;

        t = tokens[*i];
        if (json_token_syntax_char_cmp(t, '}'))
        {
            *i += 1;
            return object;
        }
        else if (!(json_token_syntax_char_cmp(t, ',')))
        {
            printf("Token %d: Expected comma after a key-value pair.\n", *i);
            return NULL;
        }
        *i += 1;
    }

    printf("EOL: End of object '}' not found.\n");
    return NULL;
}


JSON_TYPE *json_parse(JSON_TYPE **tokens, int tokens_len, int *i)
{
    JSON_TYPE *t = tokens[*i];
    if (t->type == JSON_TYPE_SYN)
    {
        if (json_token_syntax_char_cmp(t, '['))
        {
            *i += 1;
            return json_parse_array(tokens, tokens_len, i);
        }
        else if (json_token_syntax_char_cmp(t, '{'))
        {
            *i += 1;
            return json_parse_object(tokens, tokens_len, i);
        }
    }
    *i += 1;
    return t;
}

void json_free(JSON_TYPE *t)
{
    if (t == NULL) return;
    switch (t->type) 
    {
        case 0: case 1: case 5: free(t->data); break;
        case 2: case 3: case 4: break;
        case 6:
        {
            JSON_ARR *data = t->data;
            for (int i = 0; i < data->len; i++)
                json_free(data->items[i]);
            free(data);
            break;
        }
        case 7:
        {
            JSON_OBJ *data = t->data;
            for (int i = 0; i < data->len; i++)
            {
                free(data->keys[i]);
                json_free(data->vals[i]);
            }
            free(data);
            break;
        }
        default: return;
    }
    if (t == NULL) free(t);
}

void json_print_type(JSON_TYPE *t)
{
    if (t == NULL) return;

    switch (t->type) {
        case 0: printf("\"%s\"", (char*)t->data); break;
        case 1: printf("%.2lf", *((double*)t->data)); break;
        case 2: printf("true"); break;
        case 3: printf("false"); break;
        case 4: printf("null"); break;
    }

    if (t->type == 6)
    {
        JSON_ARR *data;
        data = t->data;
        printf("[");
        for (int i = 0; i < data->len-1; i++)
        {
            json_print_type(data->items[i]);
            printf(",");
        }
        json_print_type(data->items[data->len-1]);
        printf("]");
    }
    else if (t->type == 7)
    {
        JSON_OBJ *data;
        data = t->data;
        printf("{");
        for (int i = 0; i < data->len-1; i++)
        {
            printf("\"%s\":", data->keys[i]);
            json_print_type(data->vals[i]);
            printf(",");
        }
        printf("\"%s\":", data->keys[data->len-1]);
        json_print_type(data->vals[data->len-1]);
        printf("}");
    }

    return;
}

int main(void)
{
    int tokens_len, i = 0;
    char *s = "{\"first_name\":\"John\",\"last_name\":\"Smith\",\"is_alive\":true,\"age\":27,\"address\":{\"street_address\":\"21 2nd Street\",\"city\":\"New York\",\"state\":\"NY\",\"postal_code\":\"10021-3100\"},\"phone_numbers\":[{\"type\":\"home\",\"number\":\"212 555-1234\"},{\"type\":\"office\",\"number\":\"646 555-4567\"}],\"children\":[\"Catherine\",\"Thomas\",\"Trevor\"],\"spouse\":null}";
    JSON_TYPE **tokens, *t;

    tokens_len = json_lex(s, &tokens);
    // print_tokens(tokens, tokens_len);
    t = json_parse(tokens, tokens_len, &i);
    json_print_type(t);

    // free_tokens(tokens, tokens_len);

    return 0;
}
