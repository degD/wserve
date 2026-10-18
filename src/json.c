#include "json.h"
#include <string.h>

/**
 * @brief Lex a JSON string token.
 *
 * This function parses a JSON string starting at the given index `*i` in
 * `json_str`.  The string must begin with a double‑quote character (`"`).
 * It allocates a new `JSON_TYPE` token whose `type` is set to
 * `JSON_TYPE_STR` (0) and whose `data` points to a newly allocated,
 * NUL‑terminated C string containing the string contents (without the
 * surrounding quotes).  The index `*i` is advanced to the character
 * after the closing quote.  If the string is not properly terminated,
 * the function prints an error, frees allocated memory, sets `*i` to `-1`,
 * and returns `NULL`.
 *
 * @param[in,out] i         Pointer to the current character index in the
 *                          input string; updated on success or failure.
 * @param[in]     json_str  The full JSON text being lexed.
 * @param[in]     json_len  Length of the JSON text (as returned by `strlen`).
 *
 * @return Pointer to a freshly allocated `JSON_TYPE` representing the
 *         string token, or `NULL` on failure.
 */
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

/**
 * @brief Lex a JSON number token.
 *
 * This function parses a numeric literal (including integer, fractional,
 * exponent, and sign characters) starting at `*i`.  It collects the raw
 * characters into a temporary string, converts it to a `double` with
 * `strtod`, and stores the result in a newly allocated `JSON_TYPE` whose
 * `type` is `JSON_TYPE_NUM` (1).  The token’s `data` points to a dynamically
 * allocated `double`.  The index `*i` is left positioned at the first
 * character after the number.
 *
 * @param[in,out] i         Pointer to the current index; advanced past the
 *                          number on success.
 * @param[in]     json_str  The full JSON text being lexed.
 * @param[in]     json_len  Length of the JSON text.
 *
 * @return Pointer to a `JSON_TYPE` holding the parsed number, or `NULL` if
 *         no number characters were found.
 */
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

/**
 * @brief Lex a JSON boolean token (`true` or `false`).
 *
 * The function checks whether the substring at the current index matches
 * `"true"` or `"false"`.  On a match it advances the index past the keyword,
 * sets the token’s `type` to `JSON_TYPE_TRUE` (2) or `JSON_TYPE_FALSE` (3),
 * and returns the newly allocated token.  No additional data is stored.
 *
 * @param[in,out] i         Pointer to the current index; advanced on success.
 * @param[in]     json_str  The full JSON text.
 * @param[in]     json_len  Length of the JSON text.
 *
 * @return Pointer to a `JSON_TYPE` representing the boolean, or `NULL` if the
 *         text does not start with a valid boolean literal.
 */
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

/**
 * @brief Lex a JSON `null` token.
 *
 * Detects the literal `"null"` at the current index, advances the index,
 * and returns a token whose `type` is `JSON_TYPE_NULL` (4).  No payload
 * data is stored.
 *
 * @param[in,out] i         Pointer to the current index; advanced on success.
 * @param[in]     json_str  The full JSON text.
 * @param[in]     json_len  Length of the JSON text.
 *
 * @return Pointer to a `JSON_TYPE` representing `null`, or `NULL` if the
 *         text does not match `"null"`.
 */
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

/**
 * @brief Tokenise an entire JSON string.
 *
 * Iterates through `json_str`, repeatedly invoking the specialised lexer
 * functions for strings, numbers, booleans, and `null`.  It also creates
 * tokens for structural characters (`{`, `}`, `[`, `]`, `:`, `,`).  The
 * resulting array of token pointers is allocated dynamically and returned
 * via the `tokens` out‑parameter.  On success the function returns the
 * number of tokens produced; on failure it frees any partial allocations
 * and returns `-1`.
 *
 * @param[in]  json_str   Null‑terminated JSON text to lex.
 * @param[out] tokens     Address of a pointer that will receive the allocated
 *                        token array (`JSON_TYPE **`).
 *
 * @return Number of tokens on success, or `-1` on error.
 */
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

/**
 * @brief Free an array of tokens created by `json_lex`.
 *
 * Iterates over the token array, frees the per‑token data (if any) and the
 * token structures themselves, then frees the array container.
 *
 * @param[in] tokens      Array of `JSON_TYPE *` pointers.
 * @param[in] tokens_len  Number of elements in the array.
 */
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

/**
 * @brief Print a compact representation of the token stream.
 *
 * First prints the numeric `type` of each token, then prints a human‑readable
 * representation of each token’s value (string, number, boolean, null,
 * or syntax character).  Useful for debugging the lexer.
 *
 * @param[in] tokens      Array of token pointers.
 * @param[in] tokens_len  Number of tokens.
 */
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

/**
 * @brief Allocate and initialise a new JSON array node.
 *
 * Creates a `JSON_TYPE` whose `type` is `JSON_TYPE_ARR` (6) and whose
 * `data` points to an empty `JSON_ARR` structure ready to receive items.
 *
 * @return Pointer to the newly created array token.
 */
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

/**
 * @brief Allocate and initialise a new JSON object node.
 *
 * Creates a `JSON_TYPE` whose `type` is `JSON_TYPE_OBJ` (7) and whose
 * `data` points to an empty `JSON_OBJ` structure prepared for key/value
 * pairs.
 *
 * @return Pointer to the newly created object token.
 */
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

/**
 * @brief Append an item to a JSON array.
 *
 * Validates that `array` is a properly initialised JSON array token,
 * then grows its internal `items` buffer and stores the new element.
 *
 * @param[in] array  The JSON array token (`type` == JSON_TYPE_ARR).
 * @param[in] item   The JSON token to append to the array.
 *
 * @return `1` on success, `0` if `array` is not a valid array token.
 */
int json_array_push(JSON_TYPE *array, JSON_TYPE *item)
{
    if (array == NULL || array->type != JSON_TYPE_ARR || array->data == NULL) return 0;

    JSON_ARR *data = array->data;
    
    data->len += 1;
    data->items = realloc(data->items, sizeof(JSON_TYPE) * data->len);
    data->items[data->len - 1] = item;

    return 1;
}

/**
 * @brief Insert a key/value pair into a JSON object.
 *
 * The function expands the object's internal key and value arrays and stores
 * the supplied `key` (ownership transferred to the object) and `val`.
 *
 * @param[in] object  The JSON object token (`type` == JSON_TYPE_OBJ).
 * @param[in] key     Null‑terminated string containing the member name.
 * @param[in] val     JSON token representing the member's value.
 *
 * @return `1` on success, `0` if `object` is not a valid object token.
 */
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

/**
 * @brief Compare a syntax token with a given character.
 *
 * Returns 1 if the supplied token is of type `JSON_TYPE_SYN` (5) and its
 * stored character matches `c`; otherwise returns 0.
 *
 * @param[in] t  Token to test.
 * @param[in] c  Character to compare against.
 *
 * @return `1` if the token is the specified syntax character, else `0`.
 */
int json_token_syntax_char_cmp(JSON_TYPE *t, char c)
{
    if (t->type == JSON_TYPE_SYN && *((char*)t->data) == c) 
        return 1;
    else
        return 0;
}

/**
 * @brief Parse a JSON array from a token stream.
 *
 * Consumes tokens beginning at position `*i` (which should be the first
 * element after the opening `'['`).  Elements are parsed recursively via
 * `json_parse`.  The function stops when it encounters the matching closing
 * `']'`, returning a fully populated array token.  On syntax errors it prints
 * an explanatory message and returns `NULL`.
 *
 * @param[in]  tokens      Array of token pointers produced by the lexer.
 * @param[in]  tokens_len  Number of tokens in the array.
 * @param[in,out] i        Current token index; updated to point just after the
 *                         closing `']'` on success.
 *
 * @return Pointer to a JSON array token, or `NULL` on error.
 */
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

/**
 * @brief Parse a JSON object from a token stream.
 *
 * Consumes tokens starting at `*i` (which should be the first token after
 * the opening `'{'`).  Keys must be string tokens; values are parsed
 * recursively.  The function builds an object token populated with key/value
 * pairs until it encounters the matching closing `'}'`.  Errors are reported
 * via `printf` and result in a `NULL` return.
 *
 * @param[in]  tokens      Token array from the lexer.
 * @param[in]  tokens_len  Number of tokens.
 * @param[in,out] i        Current token index; advanced past the closing `}`.
 *
 * @return Pointer to a JSON object token, or `NULL` on syntax error.
 */
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

/**
 * @brief Parse any JSON value from the token stream.
 *
 * Dispatches based on the current token: if it is a syntax token `'['` or
 * `'{'` it delegates to `json_parse_array` or `json_parse_object`,
 * respectively.  Otherwise the token itself (string, number, boolean,
 * null, or syntax character) is returned.  The index `*i` is advanced
 * past the consumed token(s).
 *
 * @param[in]  tokens      Array of tokens produced by the lexer.
 * @param[in]  tokens_len  Number of tokens.
 * @param[in,out] i        Current token position; updated on return.
 *
 * @return Pointer to a JSON value token, or `NULL` on error.
 */
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

/**
 * @brief Recursively free a parsed JSON structure.
 *
 * Traverses the JSON value tree, releasing dynamically allocated memory for
 * all compound types (arrays and objects) and their children.  Primitive
 * tokens are freed directly.  After freeing the internal data, the token
 * itself is freed.  The function safely tolerates a `NULL` argument.
 *
 * @param[in] t  Root token of the JSON value to free.
 */
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

/**
 * @brief Print a JSON value in its textual representation.
 *
 * Emits a compact JSON string to `stdout` that corresponds to the data
 * represented by token `t`.  Primitive types are printed directly, while
 * arrays and objects are rendered recursively with proper delimiters.
 *
 * @param[in] t  Token representing the JSON value to print.
 */
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

JSON_TYPE *json_parse2(char *data)
{
    JSON_TYPE **tokens;
    int i = 0, n = json_lex(data, &tokens);
    if (n == -1) return NULL;
    return json_parse(tokens, n, &i);
}

JSON_TYPE *json_object_get(JSON_TYPE *t, char *key)
{
    if (t == NULL || t->data == NULL || t->type != JSON_TYPE_OBJ) return NULL;

    int i = 0;
    JSON_OBJ *data = t->data;
    for (i = 0; i < data->len; i++)
        if (strcmp(key, data->keys[i]) == 0) return data->vals[i];
}

// int main(void)
// {
//     int tokens_len, i = 0;
//     char *s = "{\"first_name\":\"John\",\"last_name\":\"Smith\",\"is_alive\":true,\"age\":27,\"address\":{\"street_address\":\"21 2nd Street\",\"city\":\"New York\",\"state\":\"NY\",\"postal_code\":\"10021-3100\"},\"phone_numbers\":[{\"type\":\"home\",\"number\":\"212 555-1234\"},{\"type\":\"office\",\"number\":\"646 555-4567\"}],\"children\":[\"Catherine\",\"Thomas\",\"Trevor\"],\"spouse\":null}";
//     JSON_TYPE **tokens, *t;

//     tokens_len = json_lex(s, &tokens);
//     // print_tokens(tokens, tokens_len);
//     t = json_parse(tokens, tokens_len, &i);
//     json_print_type(t);

//     // free_tokens(tokens, tokens_len);

//     return 0;
// }
