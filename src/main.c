
#include "wserve.h"

int main(void)
{
    puts("HTTP WSERVE...");
    wserve_http(
        "./static",
        "6600",
        100,
        1024
    );
    return 0;
}
