
#include "../src/wserve.h"


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
        if (name != NULL)
            return build_response_with_body(smart_concat("Nice name, ", params[1]), ".txt");
        else
            return build_response_with_body("I asked you your name?", ".txt");
    }
}


// ###########################################
// # ADD YOUR CALLBACKS TO THE ROUTING TABLE #
// ###########################################

void build_routing_table(void)
{
    add_init_route("/home", "GET", home_route);
    add_init_route("/hello", "GET", say_hello);
}
