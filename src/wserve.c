
#include "wserve.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>


// ########################
// # SERVER TCP FUNCTIONS #
// ########################

// Signal handler to reap zombie child processes. After
// installation, called automatically by OS whenever a
// child process exits.
//
// int s: Signal number (Normally SIGCHLD). Used by OS.
void sigchld_handler(int s)
{
    int saved_errno = errno;
    (void) s;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Install the handler for the process calling sigaction.
// Its signal disposition is inherited by its children.
// Therefore, enables the OS to call sigchld_handler for
// the parent and the children, reaping them when they turn
// zombies.
//
// Returns 0 if works, -1 otherwise.
int install_sigchld_handler(void)
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("wserve: sigaction");
        return -1;
    }
    return 0;
}

// Create and return a socket for listening to incoming
// connection requests. The server uses this socket to
// Accept incoming connections.
//
// char *port: Port number that server will use.
// int backlog: Requested max length of connection queue.
//
// Returns the socket FD. Returns -1 if it fails.
int create_listen_socket(char *port, int backlog)
{
    int listenfd;
    int yes = 1;
    int r;
    struct addrinfo hints, *ai;

    // Get a socket and bind to it.
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    r = getaddrinfo(NULL, port, &hints, &ai);
    if (r != 0) {
        fprintf(stderr, "wserve: %s\n", gai_strerror(r));
        return -1;
    }

    listenfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listenfd == -1) {
        perror("wserve: socket");
        return -1;
    }

    // Enable socket port reuse.
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // Bind the socket to the local address and port.
    r = bind(listenfd, ai->ai_addr, ai->ai_addrlen);
    if (r == -1) {
        close(listenfd);
        perror("source: bind");
        return -1;
    }

    freeaddrinfo(ai);

    r = listen(listenfd, backlog);
    if (r == -1) {
        perror("wserve: listen");
        return -1;
    }

    return listenfd;
}

// Await a connection on socket listenfd.
// Return a socket for communcating with the
// client.
//
// int listenfd: Socket for listening.
//
// Returns the socket for communcation, or -1
// if fails.
int accept_connection(int listenfd)
{
    struct sockaddr_storage ss;
    socklen_t sin_size;
    int newfd;

    sin_size = sizeof(ss);
    newfd = accept(listenfd, (struct sockaddr *) &ss, &sin_size);

    if (newfd == -1) {
        perror("wserve: accept");
        return -1;
    }

    return newfd;
}

// Send N bytes of buffer to socket.
// Can handle partial sends automatically.
//
// int newfd: Socket FD.
// void *buf: Buffer.
// size_t nbytes: N bytes to send from buffer.
//
// Returns number of bytes sent. Returns -1
// if fails.
ssize_t _send(int newfd, void *buf, size_t nbytes)
{
    size_t bytes_sent = 0;
    size_t bytes_left = nbytes;
    ssize_t n = 0;
    char *p = buf;

    while(bytes_sent < nbytes) {
        n = send(newfd, p, bytes_left, 0);
        if (n < 0) break;
        bytes_sent += n;
        bytes_left -= n;
        p += n;
    }

    return n == -1 ? -1 : bytes_sent;
}

// Receive up to N bytes to buffer from socket.
// Up to N, because the client can close the
// connection while data has been received.
// It is an orderly connection shutdown. Can
// handle partial receives automatically.
//
// int newfd: Socket FD.
// void *buf: Buffer.
// size_t nbytes: N bytes to receive to buffer.
//
// Returns number of bytes received. Returns -1
// if fails.
ssize_t _recv(int newfd, void *buf, size_t nbytes)
{
    size_t bytes_recv = 0;
    size_t bytes_left = nbytes;
    ssize_t n = 0;
    char *p = buf;

    while(bytes_recv < nbytes) {
        n = recv(newfd, p, bytes_left, 0);
        if (n <= 0) break;
        bytes_recv += n;
        bytes_left -= n;
        p += n;
    }

    return n == -1 ? -1 : bytes_recv;
}

// Server core loop. Runs indefinitely and
// returns nothing.
//
// char *port: Port number that server will use.
// int backlog: Max length of connection queue.
void wserve(char *port, int backlog)
{
    char request[30];
    char response[] = "Got it! Hello, world!\n";
    int listenfd = create_listen_socket(port, backlog);

    install_sigchld_handler();
    while (1)
    {
        int newfd = accept_connection(listenfd);
        if (newfd == -1) continue;

        if (!fork())
        {
            // Child proocess exit but the parent does not
            // wait. Child turns into a zombie. That is why
            // the signal handler is required.
            close(listenfd);
            _recv(newfd, request, 4);
            printf("REQUEST: %s", request);
            _send(newfd, response, strlen(response));
            printf("RESPONSE: %s", response);
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
}


// #######################
// # HTTP HEADERS PARSER #
// #######################

// All parser functions should be called over a dynamically
// allocated HTTP message. Parser functions will modify the
// HTTP message while parsing it. Parser functions return
// structs, which have fields that point to the message.
// Free the HTTP message alongside freeing structs.

// Count occurances of "substr" inside "str".
//
// char *str: haystack.
// char *substr: needle.
//
// Return number of occurances. 0 if none.
int count_substring(char *str, char *substr)
{
    int len_str = strlen(str);
    int len_substr = strlen(substr);
    int c = 0;
    int f;

    for (int i = len_substr; i < len_str; i++)
    {
        f = 1;
        for (int j = 0; j < len_substr; j++)
        {
            if (str[i - len_substr + j] != substr[j])
            {
                f = 0;
                break;
            }
        }
        c += f;
    }

    return c;
}

// Split "str" into sub-strings by "substr". Works very
// similar to "strtok_r". Except instead of separating
// "str" by multiple delimiters, it splits by a single,
// multi-character "substr". Similar to Python str.split().
// Call with NULL in place of "str" for subsequent calls.
//
// char *str: The string to be splitted.
// char *substr: The string to be used for splitting.
// char **saveptr: Pointer for remaining section after split.
//
// Returns a pointer to the splitted "token". Just like
// "strtok_r", returns NULL when no "token" left.
char *split_str(char *str, char *substr, char **saveptr)
{
    char *p;

    if (str == NULL)
    {
        str = *saveptr;
    }

    if (str == NULL && *saveptr == NULL)
    {
        return NULL;
    }

    p = strstr(str, substr);
    if (p == NULL)
    {
        *saveptr = NULL;
        return str;
    }

    p[0] = '\0';
    *saveptr = p + strlen(substr) * sizeof(char);
    return str;
}

// Removes whitespace from both ends of "str" and
// returns a new malloc'ed string, without modifying
// the original. Because a new string is returned,
// it is up to programmer to "free()" it.
//
// char *str: String to be trimmed.
//
// Returns pointer to the new string.
char *trim(char *str)
{
    int len = strlen(str);
    char *tstr;
    int start, end, i;
    int j = 0;

    i = 0;
    while (i < len && isspace(str[i++]));
    start = i - 1;

    i = 0;
    while (i < len && isspace(str[len - 1 - (i++)]));
    end = len - i;

    tstr = malloc((end - start + 2) * sizeof(char));
    for (i = start; i <= end; i++) tstr[j++] = str[i];
    tstr[j] = '\0';

    return tstr;
}

// Convert a string to uppercase in place.
// Modifies the given string.
//
// char *str: String to be converted.
void toupper_str(char *str)
{
    int i = 0;
    while (str[i] != '\0')
    {
        str[i] = toupper(str[i]);
        i++;
    }
}

// HTTP messages have two main parts, the "head" and
// the "body". They are separated by a "CRLF CRLF"
// separator. This function returns a pointer to the
// message body by finding this "CRLF CRLF" and pointing
// to the first char after it. Looks up to "len" chars.
//
// char *http_msg: HTTP message to be scanned.
// size_t len: Length (size) of HTTP message.
//
// Returns either a pointer to the body, or NULL if body
// "CRLF CRLF" not found.
char *get_http_body(char *http_msg, size_t len)
{
    char *_http_msg = malloc((len + 1) * sizeof(char));
    char *http_body;

    memcpy(_http_msg, http_msg, len);
    _http_msg[len] = '\0';

    http_body = strstr(_http_msg, "\r\n\r\n");
    if (http_body != NULL)
    {
        http_body += 4 * sizeof(char);
        http_body = http_msg + (http_body - _http_msg);
    }

    free(_http_msg);
    return http_body;
}

// Parse a single line of HTTP header and return
// an "HTTP_HEADER_FIELD" representing it.
//
// char *line: Header line.
//
// Returns the "HTTP_HEADER" variable. Returns NULL
// if malformed.
HTTP_HEADER_FIELD *parse_http_header_line(char *line)
{
    char *saveptr;
    char *val;
    HTTP_HEADER_FIELD *hh;

    if (strstr(line, ":") == NULL) return NULL;

    hh = malloc(sizeof(HTTP_HEADER_FIELD));
    val = strstr(line, ":") + sizeof(char);
    hh->key = split_str(line, ":", &saveptr);
    hh->val = trim(val);
    toupper_str(hh->key);
    toupper_str(hh->val);

    return hh;
}

// Parse the request line (start line of an HTTP request) into
// a HTTP_REQUEST_LINE struct.
//
// char *start_line: Request line.
//
// Returns HTTP_REQUEST_LINE. Returns NULL if malformed.
HTTP_REQUEST_LINE *parse_http_request_line(char *start_line)
{
    HTTP_REQUEST_LINE *hrl;
    char *saveptr;
    int n = count_substring(start_line, " ");

    if (n < 2) return NULL;

    hrl = malloc(sizeof(HTTP_REQUEST_LINE));
    hrl->method = split_str(start_line, " ", &saveptr);
    hrl->target = split_str(NULL, " ", &saveptr);
    hrl->http_version = saveptr;

    return hrl;
}

// Parse the status line (start line of an HTTP response) into
// a HTTP_STATUS_LINE struct.
//
// char *start_line: Status line.
//
// Returns HTTP_STATUS_LINE. Returns NULL if malformed.
HTTP_STATUS_LINE *parse_http_status_line(char *start_line)
{
    HTTP_STATUS_LINE *hsl;
    char *saveptr;
    int n = count_substring(start_line, " ");

    if (n < 2) return NULL;

    hsl = malloc(sizeof(HTTP_STATUS_LINE));
    hsl->http_version = split_str(start_line, " ", &saveptr);
    hsl->status_code = split_str(NULL, " ", &saveptr);
    hsl->response_text = saveptr;

    return hsl;
}

// Parse an HTTP request into an HTTP_REQUEST struct.
// Assumes "head" is complete with CRLFCRLF.
//
// char *http_msg: HTTP message to be parsed.
// size_t http_msg_len: Size of http_msg.
//
// Returns the "HTTP_REQUEST" variable. Returns NULL
// if cannot find the CRLFCRLF.
HTTP_REQUEST *parse_http_request(char *http_msg, size_t http_msg_len)
{
    HTTP_REQUEST *hr;
    char *saveptr;
    char *start_line;
    char *head;
    char *line;
    char *body;

    body = get_http_body(http_msg, http_msg_len);
    if (body == NULL) return NULL;

    hr = malloc(sizeof(HTTP_REQUEST));
    head = split_str(http_msg, "\r\n\r\n", &saveptr);
    start_line = split_str(head, "\r\n", &saveptr);
    hr->hrl = parse_http_request_line(start_line);
    hr->num_of_headers = count_substring(saveptr, "\r\n") + 1;
    hr->headers = malloc(hr->num_of_headers * sizeof(HTTP_HEADER_FIELD));
    hr->body = body;
    hr->bodylen = http_msg_len - (body - http_msg);
    line = split_str(NULL, "\r\n", &saveptr);

    for (int i = 0; i < hr->num_of_headers; i++)
    {
        hr->headers[i] = parse_http_header_line(line);
        line = split_str(NULL, "\r\n", &saveptr);
    }

    return hr;
}

// Parse an HTTP response into an HTTP_RESPONSE struct.
// Assumes "head" is complete with CRLFCRLF.
//
// char *http_msg: HTTP message to be parsed.
// size_t http_msg_len: Size of http_msg.
//
// Returns the "HTTP_RESPONSE" variable. Returns NULL
// if cannot find the CRLFCRLF.
HTTP_RESPONSE *parse_http_response(char *http_msg, size_t http_msg_len)
{
    HTTP_RESPONSE *hr;
    char *saveptr;
    char *start_line;
    char *head;
    char *line;
    char *body;

    body = get_http_body(http_msg, http_msg_len);
    if (body == NULL) return NULL;

    hr = malloc(sizeof(HTTP_REQUEST));
    head = split_str(http_msg, "\r\n\r\n", &saveptr);
    start_line = split_str(head, "\r\n", &saveptr);
    hr->hsl = parse_http_status_line(start_line);
    hr->num_of_headers = count_substring(saveptr, "\r\n") + 1;
    hr->headers = malloc(hr->num_of_headers * sizeof(HTTP_HEADER_FIELD));
    hr->body = body;
    hr->bodylen = http_msg_len - (body - http_msg);
    line = split_str(NULL, "\r\n", &saveptr);

    for (int i = 0; i < hr->num_of_headers; i++)
    {
        hr->headers[i] = parse_http_header_line(line);
        line = split_str(NULL, "\r\n", &saveptr);
    }

    return hr;
}

ssize_t calc_http_response_size(HTTP_RESPONSE *hr)
{
    if (hr == NULL) return -1;

    size_t msglen = 0;

    // start line
    msglen += strlen(hr->hsl->http_version) + 1;
    msglen += strlen(hr->hsl->status_code) + 1;
    msglen += strlen(hr->hsl->response_text) + 2;

    // headers
    for (int i = 0; i < hr->num_of_headers; i++)
    {
        msglen += strlen(hr->headers[i]->key) + 2;
        msglen += strlen(hr->headers[i]->val) + 2;
    }

    // body
    msglen += hr->bodylen;
    msglen += 2;

    return msglen;
}

ssize_t tostring_http_response(HTTP_RESPONSE *hr, char **msg)
{
    char *p, *src;
    size_t len;
    ssize_t msglen = calc_http_response_size(hr);
    if (msglen < 0) return -1;

    *msg = malloc(msglen * sizeof(char));
    p = *msg;

    p += sprintf(p, "%s %s %s\r\n",
        hr->hsl->http_version,
        hr->hsl->status_code,
        hr->hsl->response_text
    );
    for (int i = 0; i < hr->num_of_headers; i++)
    {
        p += sprintf(p, "%s: %s\r\n",
            hr->headers[i]->key,
            hr->headers[i]->val
        );
    }
    p += sprintf(p, "\r\n");
    memcpy(p, hr->body, hr->bodylen);

    return msglen;
}

HTTP_RESPONSE *init_http_response(
    char *status_code,
    char *resp_text,
    char *body, size_t bodylen,
    char **headers, int nheaders
) {
    HTTP_RESPONSE *hr = malloc(sizeof(HTTP_RESPONSE));
    HTTP_STATUS_LINE *hsl;
    HTTP_HEADER_FIELD *p;

    hsl = malloc(sizeof(HTTP_STATUS_LINE));
    hsl->http_version = "HTTP/1.1";
    hsl->status_code = status_code;
    hsl->response_text = resp_text;
    hr->hsl = hsl;

    hr->body = body;
    hr->bodylen = bodylen;

    hr->num_of_headers = nheaders;
    hr->headers = malloc(nheaders * sizeof(HTTP_HEADER_FIELD));
    for (int i = 0; i < nheaders; i++)
    {
        p = malloc(sizeof(HTTP_HEADER_FIELD));
        hr->headers[i] = p;
        p->key = headers[2*i];
        p->val = headers[2*i+1];
    }

    return hr;
}

char **append_to_headers_list(
    char **headers, int *nheaders,
    char *key, char *val
) {
    int i;
    char **new_headers;

    *nheaders += 1;
    new_headers = realloc(headers, *nheaders * sizeof(char*) * 2);

    i = *nheaders - 1;
    new_headers[2*i] = malloc(strlen(key) * sizeof(char));
    strcpy(new_headers[2*i], key);
    new_headers[2*i+1] = malloc(strlen(val) * sizeof(char));
    strcpy(new_headers[2*i+1], val);

    return new_headers;
}


// ###############
// # HTTP SERVER #
// ###############

// Receive an HTTP message. Receives until "head" is complete.
// Returns the number of bytes read. Allocates memory for HTTP
// message. Programmer should free "head" after use. Function
// writes "body", "bodylen" and "head", "headlen". No validation
// involved.
//
// int newfd: File descriptor of socket.
// char **head: It will point to the HTTP message (head).
// char **body: It will point to memory after CRLFCRLF.
// size_t *headlen: It will hold head memory size.
// size_t *bodylen: It will hold received body memory size.
// size_t maxrecvsize: Max number of bytes received at each "recv()".
//
// Returns number of bytes received. Returns "-1" if an error
// occurs.
ssize_t http_recv(
    int newfd,
    char **head,
    char **body,
    size_t *headlen,
    size_t *bodylen,
    size_t maxrecvsize
) {
    char *req;
    char *p;
    size_t reqsize = 0;
    ssize_t n;

    req = malloc(maxrecvsize * sizeof(char));
    n = recv(newfd, req, maxrecvsize, 0);
    while (n > 0)
    {
        reqsize += n;
        req = realloc(req, reqsize + maxrecvsize);

        if ((p = get_http_body(req, reqsize)) != NULL)
        {
            *head = req;
            *body = p;
            *headlen = p - req;
            *bodylen = reqsize - *headlen;
            return reqsize;
        }
    }
    if (n == 0)
    {
        printf("wserve: Connection closed by socket %d.\n", newfd);
        return reqsize;
    }

    perror("recv");
    return -1;
}

void http_send_status(int newfd, int status_code)
{
    char response[] = "HTTP/1.1 200 OK\r\n\r\n";
    size_t len = strlen(response);

    sprintf(response, "HTTP/1.1 %d OK\r\n\r\n", status_code);
    send(newfd, response, len, 0);
}

// HTTP server core loop. Runs indefinitely and
// returns nothing.
//
// char *port: Port number that server will use.
// int backlog: Max length of connection queue.
// size_t maxrecvsize: Max number of bytes received at each "recv()".
void wserve_http(
    char *root,
    char *port,
    int backlog,
    size_t maxrecvsize
) {
    int listenfd = create_listen_socket(port, backlog);

    install_sigchld_handler();
    while (1)
    {
        int newfd = accept_connection(listenfd);
        if (newfd == -1) continue;

        printf("Connection to socket %d\n```\n", newfd);

        if (!fork())
        {
            close(listenfd);
            char *http_msg;
            char *http_body;
            char *response_str;
            size_t headlen;
            size_t bodylen;
            ssize_t msglen, n;
            HTTP_REQUEST *hr;
            HTTP_RESPONSE *response;

            msglen = http_recv(newfd, &http_msg, &http_body, &headlen, &bodylen, maxrecvsize);
            if (msglen > 0)
            {
                hr = parse_http_request(http_msg, msglen);
                response = process_http_requests(hr, root);
                n = tostring_http_response(response, &response_str);
                _send(newfd, response_str, n);
            }

            free(hr);
            free(http_msg);
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
}

ssize_t recv_http_body_content_length(
    int newfd,
    char *body, size_t bodylen,
    size_t contentlength
) {
    char *p = body + bodylen;
    ssize_t n;

    n = recv(newfd, p, (contentlength - bodylen), 0);
    while (n > 0)
    {
        p += n;
        bodylen += n;
    }
    if (n == 0)
    {
        printf("wserve: Connection closed by socket %d.\n", newfd);
        return bodylen;
    }

    perror("recv");
    return -1;
}

HTTP_RESPONSE *process_http_requests(HTTP_REQUEST *hr, char *root)
{
    if (hr == NULL) return NULL;

    char *buf;
    ssize_t n;
    HTTP_RESPONSE *response;

    if (strcmp(hr->hrl->method, "GET") == 0)
    {
        n = read_static_txt_file(root, hr->hrl->target, &buf);
        if (n == -1)
            return init_http_response("404", "Not found", NULL, 0, NULL, 0);
        else
        {
            int nh = 0;
            char **h = NULL;
            char val[65];
            sprintf(val, "%ld", n);
            h = append_to_headers_list(h, &nh, "content-length", val);
            return init_http_response("200", "OK", buf, n, h, 1);
        }
    }
    else if (strcmp(hr->hrl->method, "HEAD") == 0)
    {
        n = read_static_txt_file(root, hr->hrl->target, &buf);
        if (n == -1)
            return init_http_response("404", "Not found", NULL, 0, NULL, 0);
        else
        {
            int nh = 0;
            char **h = NULL;
            char val[65];
            sprintf(val, "%ld", n);
            h = append_to_headers_list(h, &nh, "content-length", val);
            return init_http_response("200", "OK", NULL, 0, h, 1);
        }
    }
    else
        return init_http_response("418", "I'm a teapod", NULL, 0, NULL, 0);
}


// ##################
// # STATIC ROUTING #
// ##################

int is_path(char *path)
{
    struct stat s;
    if( stat(path, &s) == 0 )
    {
        if( s.st_mode & S_IFDIR )
        {
            return 1; // directory
        }
        else if( s.st_mode & S_IFREG )
        {
            return 2; // file
        }
        else
        {
            return 3; // something else
        }
    }
    else
        return 0; // error
}

// Trim `/` from the end of a path. From
// `Downloads/sdcard/` to `Downloads/sdcard`.
void trim_path(char *path)
{
    size_t n = strlen(path);
    if (path[n-1] == '/') path[n-1] = '\0';
}

char *concat_path(char *path1, char *path2)
{
    char *path;

    if (is_path(path1) == 1)
    {
        trim_path(path1);
        path = malloc((strlen(path1) + strlen(path2) + 1) * sizeof(char));
        sprintf(path, "%s%s", path1, path2);
    }

    return path;
}

ssize_t read_static_txt_file(char *root, char *target, char **buf)
{
    int i, c, len = 0;
    char *p, *path;
    FILE *fp;

    puts(root);
    puts(target);

    // root should be a directory
    if (is_path(root) != 1) return -1;
    path = concat_path(root, target);

    puts(path);

    // look for index.html if target points to directory
    if (is_path(path) == 1)
    {
        puts("index");
        p = path;
        path = concat_path(path, "/index.html");
        free(p);
        puts(path);
    }
    // path should be a file
    if (is_path(path) != 2) return -1;

    fp = fopen(path, "r");
    if (fp == NULL) return -1;

    // get file content length
    while (fgetc(fp) != EOF) len++;
    *buf = malloc((len + 1) * sizeof(char));

    // read content to a buffer
    i = 0;
    rewind(fp);
    while ((c = fgetc(fp)) != EOF)
    {
        (*buf)[i] = c;
        i++;
    }
    (*buf)[i] = '\0';

    return len;
}
