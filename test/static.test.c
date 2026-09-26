
#include "../src/wserve.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    {
        char *root = "./test";
        char *target = "/a.txt";
        char *buf, *ext;
        ssize_t n = read_static_file(root, target, &buf, &ext);
        assert(n == 12);
        assert(strlen(buf) == 12);
        assert(strcmp(ext, ".txt") == 0);
        free(ext);
        free(buf);
        puts("[DONE] Read static file");
    }
}
