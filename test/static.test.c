
#include "../src/wserve.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    {
        assert(is_path("./") == 1);
        assert(is_path("test/static/../../src/") == 1);
        assert(is_path("test/static.test.c") == 2);
        assert(is_path("./test/parser.test.c") == 2);
        assert(is_path("abcde") == 0);
        puts("[DONE] Is path");
    }

    {
        char *p1 = "./src";
        char *p2 = "/main.c";
        assert(strcmp(concat_path(p1, p2), "./src/main.c") == 0);
        assert(concat_path(p2, p1) == NULL);
        puts("[DONE] Concatonate paths");
    }

    {
        char *root = "./test";
        char *target = "/a.txt";
        char *buf;
        ssize_t n = read_static_file(root, target, &buf);
        assert(n == 12);
        assert(strlen(buf) == 12);
        free(buf);
    }
}