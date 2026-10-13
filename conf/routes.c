
#include "../src/wserve.h"


// ####################################
// # WRITE YOUR ROUTE CALLBACKS BELOW #
// ####################################

HTTP_RESPONSE *home_route(HTTP_REQUEST *hr)
{
    return build_response_with_body("This is my home!", ".txt");
}


// ###########################################
// # ADD YOUR CALLBACKS TO THE ROUTING TABLE #
// ###########################################

void build_routing_table(void)
{
    add_init_route("/home", "GET", home_route);
}
