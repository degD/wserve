
#ifndef _JSONPARSER_H_
#define _JSONPARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_TYPE_STR 0
#define JSON_TYPE_NUM 1
#define JSON_TYPE_TRU 2
#define JSON_TYPE_FAL 3
#define JSON_TYPE_NUL 4
#define JSON_TYPE_SYN 5
#define JSON_TYPE_ARR 6
#define JSON_TYPE_OBJ 7


typedef struct JSON_OBJ
{
    char **keys;
    void **vals;
    size_t len;
}
JSON_OBJ;


typedef struct JSON_ARR
{
    void **items;
    size_t len;
}
JSON_ARR;


/* 0: string
 * 1: number
 * 2: true
 * 3: false
 * 4: null
 * 5: syntax
 * 6: array
 * 7: object
 */
typedef struct JSON_TYPE
{
    int type;
    void *data;
}
JSON_TYPE;

// Parsing JSON in two steps:
// 1. Lexical analysis (tokenizing)
// 2. Syntatic analysis (parsing)

JSON_TYPE *json_lex_string(int *i, char *json_str, size_t json_len);
JSON_TYPE *json_lex_number(int *i, char *json_str, size_t json_len);
JSON_TYPE *json_lex_bool(int *i, char *json_str, size_t json_len);
JSON_TYPE *json_lex_null(int *i, char *json_str, size_t json_len);
int json_lex(char *json_str, JSON_TYPE ***tokens);
JSON_TYPE *json_parse_array(JSON_TYPE **tokens, int tokens_len, int *i);
JSON_TYPE *json_parse_object(JSON_TYPE **tokens, int tokens_len, int *i);
JSON_TYPE *json_parse(JSON_TYPE **tokens, int tokens_len, int *i);


#endif