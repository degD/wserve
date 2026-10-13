
#include "wserve.h"


HTTP_RESPONSE *home_route(HTTP_REQUEST *hr)
{
    return build_response_with_body("This is my home!", ".txt");
}

void set_routes(void)
{
    init_routes_list();

    // home_route
    HTTP_ROUTE *r = init_route("/home", "GET", home_route);
    add_route(r);
}
