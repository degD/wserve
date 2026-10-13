
#include "wserve.h"
#include "routes.c"

int main(void)
{
    set_routes();
    
    init_server_settings("test/static", "6543", 100, 1024, 1024*1024);
    puts("STARTING HTTP WSERVE...");
    wserve_http();
    return 0;
}
