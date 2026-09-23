
#include "wserve.h"

int main(void)
{
    init_server_settings("test/static", "6666", 100, 1024, 1024*1024);
    puts("STARTING HTTP WSERVE...");
    wserve_http();
    return 0;
}
