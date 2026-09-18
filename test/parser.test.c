
#include "../src/wserve.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    {
        assert(count_substring("Quick;;:fox;;:jumped;;:over;;:something;;:something", ";;:") == 5);
        puts("[DONE] String counting");
    }

    {
        assert(strcmp(trim("   aa bbbccD  "), "aa bbbccD") == 0);
        puts("[DONE] Trim string");
    }

    {
        char str[] = "Quick;;:fox;;:jumped";
        char substr[] = ";;:";
        char *token;
        char *saveptr;

        token = split_str(str, substr, &saveptr);
        assert(strcmp(token, "Quick") == 0);
        token = split_str(NULL, substr, &saveptr);
        assert(strcmp(token, "fox") == 0);
        token = split_str(NULL, substr, &saveptr);
        assert(strcmp(token, "jumped") == 0);

        puts("[DONE] String splitting");
    }

    {
        char str[] = "mixed CASE strING\r\n";
        toupper_str(str);
        assert(strcmp(str, "MIXED CASE STRING\r\n") == 0);
        puts("[DONE] String to uppercase");
    }

    {
        char line[] = "Content-Type: text/html";
        char line1[] = "Content-Type text/html";
        char line2[] = "Content-Type: ::text/html";

        HTTP_HEADER_FIELD *hhf = parse_http_header_line(line);
        assert(strcmp(hhf->key, "CONTENT-TYPE") == 0);
        assert(strcmp(hhf->val, "TEXT/HTML") == 0);
        free(hhf);

        HTTP_HEADER_FIELD *hhf1 = parse_http_header_line(line1);
        assert(hhf1 == NULL);
        free(hhf1);

        HTTP_HEADER_FIELD *hhf2 = parse_http_header_line(line2);
        assert(strcmp(hhf2->key, "CONTENT-TYPE") == 0);
        assert(strcmp(hhf2->val, "::TEXT/HTML") == 0);
        free(hhf2);

        puts("[DONE] Parse header");
    }

    {
        char nobody[] = "No body\n";
        char body[] = "Yes body\r\n\r\nbody part...";
        assert(get_http_body(nobody, 9) == NULL);
        assert(strcmp(get_http_body(body, 25), "body part...") == 0);
        puts("[DONE] Get HTTP body");
    }

    {
        char reqline[] = "GET / HTTP/1.1";
        char resline[] = "HTTP/1.1 301 Moved Permanently";
        HTTP_REQUEST_LINE *hrl;
        HTTP_STATUS_LINE *hsl;

        hrl = parse_http_request_line(reqline);
        hsl = parse_http_status_line(resline);

        assert(strcmp(hrl->method, "GET") == 0);
        assert(strcmp(hrl->target, "/") == 0);
        assert(strcmp(hrl->http_version, "HTTP/1.1") == 0);
        free(hrl);

        assert(strcmp(hsl->http_version, "HTTP/1.1") == 0);
        assert(strcmp(hsl->status_code, "301") == 0);
        assert(strcmp(hsl->response_text, "MOVED PERMANENTLY") == 0);
        free(hsl);

        puts("[DONE] Parse HTTP start line");
    }

    {
        char msg[] = "GET / HTTP/1.1\r\nHost: google.com\r\nUser-Agent: curl/8.5.0\r\nAccept: */*\r\n\r\nTheReqBody";
        HTTP_REQUEST *hr = parse_http_request(msg, strlen(msg));

        assert(hr->bodylen == strlen("TheReqBody"));
        assert(strcmp(hr->body, "TheReqBody") == 0);
        assert(strcmp(hr->hrl->method, "GET") == 0);
        assert(strcmp(hr->hrl->target, "/") == 0);
        assert(strcmp(hr->hrl->http_version, "HTTP/1.1") == 0);

        assert(hr->num_of_headers == 3);
        assert(strcmp(hr->headers[0]->key, "HOST") == 0);
        assert(strcmp(hr->headers[0]->val, "GOOGLE.COM") == 0);
        assert(strcmp(hr->headers[1]->key, "USER-AGENT") == 0);
        assert(strcmp(hr->headers[1]->val, "CURL/8.5.0") == 0);
        assert(strcmp(hr->headers[2]->key, "ACCEPT") == 0);
        assert(strcmp(hr->headers[2]->val, "*/*") == 0);

        free(hr);
        puts("[DONE] Parse HTTP request");
    }

    {
        char msg[] = "HTTP/1.1 301 Moved Permanently\r\nServer: nginx\r\nLocation: https://wiki.archlinux.org/\r\n\r\nTheReqBody";
        HTTP_RESPONSE *hr = parse_http_response(msg, strlen(msg));

        assert(hr->bodylen == strlen("TheReqBody"));
        assert(strcmp(hr->body, "TheReqBody") == 0);
        assert(strcmp(hr->hsl->http_version, "HTTP/1.1") == 0);
        assert(strcmp(hr->hsl->status_code, "301") == 0);
        assert(strcmp(hr->hsl->response_text, "MOVED PERMANENTLY") == 0);

        assert(hr->num_of_headers == 2);
        assert(strcmp(hr->headers[0]->key, "SERVER") == 0);
        assert(strcmp(hr->headers[0]->val, "NGINX") == 0);
        assert(strcmp(hr->headers[1]->key, "LOCATION") == 0);
        assert(strcmp(hr->headers[1]->val, "HTTPS://WIKI.ARCHLINUX.ORG/") == 0);

        free(hr);
        puts("[DONE] Parse HTTP response");
    }

    {
        HTTP_STATUS_LINE hsl;
        HTTP_RESPONSE hr;
        HTTP_HEADER_FIELD hhf0, hhf1;
        hr.hsl = &hsl;
        hr.hsl->http_version = "HTTP/1.1";
        hr.hsl->status_code = "200";
        hr.hsl->response_text = "OK";
        hr.num_of_headers = 2;
        hr.headers = malloc(2 * sizeof(HTTP_HEADER_FIELD));
        hr.headers[0] = &hhf0;
        hr.headers[1] = &hhf1;
        hr.headers[0]->key = "abc";
        hr.headers[0]->val = "klm";
        hr.headers[1]->key = "xyz";
        hr.headers[1]->val = "prs";
        hr.body = "TheResponseBody!";
        hr.bodylen = 16;

        // start line: 17
        // header 1: 10
        // header 2: 10
        // CRLF: 2
        // body: 16
        assert((17 + 10 + 10 + 2 + 16) == calc_http_response_size(&hr));

        free(hr.headers);
        puts("[DONE] Calculate HTTP response size");
    }

    {
        HTTP_STATUS_LINE hsl;
        HTTP_RESPONSE hr;
        HTTP_HEADER_FIELD hhf0, hhf1;
        hr.hsl = &hsl;
        hr.hsl->http_version = "HTTP/1.1";
        hr.hsl->status_code = "200";
        hr.hsl->response_text = "OK";
        hr.num_of_headers = 2;
        hr.headers = malloc(2 * sizeof(HTTP_HEADER_FIELD));
        hr.headers[0] = &hhf0;
        hr.headers[1] = &hhf1;
        hr.headers[0]->key = "abc";
        hr.headers[0]->val = "klm";
        hr.headers[1]->key = "xyz";
        hr.headers[1]->val = "prs";
        hr.body = "TheResponseBody!";
        hr.bodylen = 16;

        char *msg;
        char *s = "HTTP/1.1 200 OK\r\nabc: klm\r\nxyz: prs\r\n\r\n";
        ssize_t n = build_http_response(&hr, &msg);

        // start line: 17
        // header 1: 10
        // header 2: 10
        // CRLF: 2
        // body: 16
        assert((17 + 10 + 10 + 2 + 16) == n);
        assert(strcmp(s, msg));

        free(hr.headers);
        free(msg);
        puts("[DONE] Build HTTP response");
    }

    return 0;
}
