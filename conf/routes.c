
#include "../src/wserve.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>


// ####################################
// # WRITE YOUR ROUTE CALLBACKS BELOW #
// ####################################

HTTP_RESPONSE *home_route(HTTP_REQUEST *hr)
{
    return build_response_with_body("This is my home!", ".txt");
}

HTTP_RESPONSE *say_hello(HTTP_REQUEST *hr)
{
    char **params, *target;
    ssize_t n;

    n = extract_query_parameters(hr, &params, &target);
    if (n < 0) 
        return build_status_response("400");
    else if (n == 0)
        return build_response_with_body("You have no name?!", ".txt");
    else 
    {
        char *name = get_params_val_by_key(params, n, "name");
        for (int i = 0; i < 2*n; i++) printf("%s\n", params[i]);

        if (name != NULL)
            return build_response_with_body(smart_concat("Nice name, ", name), ".txt");
        else
            return build_response_with_body("I asked you your name?", ".txt");
    }
}

HTTP_RESPONSE *save_name(HTTP_REQUEST *hr)
{
    char *body;
    JSON_TYPE *t;

    body = malloc(sizeof(char) * (hr->bodylen+1));
    memcpy(body, hr->body, hr->bodylen);
    body[hr->bodylen] = '\0';

    printf("%zu, %s\n", hr->bodylen, hr->body);
    // Expects a JSON in the following format:
    // {
    //    "name": "JohnSmith",
    //    "age": 50
    // }
    
    t = json_parse2(body);
    if (t == NULL || t->data == NULL || t->type != JSON_TYPE_OBJ) 
    {
        free(body);
        return build_status_response("400");
    }

    json_print_type(t);

    char *name = json_object_get(t, "name")->data;
    double *age = json_object_get(t, "age")->data;
    char p[100];
    size_t n;
    sprintf(p, "%s is %.0lf years old.", name, *age);
    
    n = write_file("/post/name.txt", p, strlen(p));
    if (n == -1)
    {
        return build_response_with_body("POST failed. Do you know why?\n", ".txt");
        json_free(t);
    }

    return build_status_response("200");
}


// ###########################################
// # ADD YOUR CALLBACKS TO THE ROUTING TABLE #
// ###########################################

void build_routing_table(void)
{
    add_init_route("/home", "GET", home_route);
    add_init_route("/hello", "GET", say_hello);
    add_init_route("/savename", "POST", save_name);
}
