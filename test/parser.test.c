
#include "../src/wserve.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    assert(count_substring("Quick;;:fox;;:jumped;;:over;;:something;;:something", ";;:") == 5);
    assert(strcmp(trim("   aa bbbccD  "), "aa bbbccD") == 0);
    puts("[DONE] Parser helper functions");

    char line[] = "Content-Type: text/html";
    parse_http_header_line(line);
    puts("[DONE] Header line parsing");

    char http_msg[] = "\
HTTP/1.1 301 Moved Permanently\r\n\
Server: nginx\r\n\
Date: Wed, 09 Sep 2026 18:11:58 GMT\r\n\
Content-Type: text/html\r\n\
Content-Length: 162\r\n\
Connection: keep-alive\r\n\
Location: https://wiki.archlinux.org/\r\n\r\n\
BODYasdfasdfasdf";
    char *http_body = get_http_body(http_msg, 212);
    printf("%s", http_body);
    HTTP_HEAD hh = parse_head(http_msg);
    print_http_head(hh);
    puts("[INSPECT] HTTP message parsing");

    return 0;
}
