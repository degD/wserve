
#ifndef _JSONPARSER_H_
#define _JSONPARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* 0: string
 * 1: number
 * 2: true
 * 3: false
 * 4: null
 * 5: syntax
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
char **json_lex(char *json_str);


#endif