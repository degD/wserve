
#include "wserve.h"

int main(void)
{
    puts("HTTP WSERVE...");
    wserve_http("6600", 100, 1);
    return 0;
}
