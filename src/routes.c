
#include "wserve.h"


HTTP_RESPONSE *home_route(HTTP_REQUEST *hr)
{
}

void set_routes(void)
{
    init_routes_list();

    // home_route
    HTTP_ROUTE *r = init_route("/", "GET", home_route);
    add_route(r);
}
