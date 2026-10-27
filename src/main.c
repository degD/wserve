
#include "wserve.h"

int main(void)
{
    init_routes_list();
    build_routing_table();

    init_server_settings(1, "test/static", "6543", 100, 1024, 1024*1024,
        "test/https/server-key.der",
        "test/https/server-cert.der"
    );
    puts("STARTING HTTP WSERVE...");
    wserve_http();
    return 0;
}
