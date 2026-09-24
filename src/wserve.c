
#include "wserve.h"
#include <ctype.h>
#include <stddef.h>
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
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/syscall.h>
#include <unistd.h>


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

int set_socket_timeouts(int fd)
{
    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };

    if (
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1
    ) {
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
    struct timeval tv;

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
        freeaddrinfo(ai);
        return -1;
    }

    // timeout after 10 seconds of no operation
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(listenfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Enable socket port reuse.
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // Bind the socket to the local address and port.
    r = bind(listenfd, ai->ai_addr, ai->ai_addrlen);
    if (r == -1) {
        close(listenfd);
        perror("source: bind");
        freeaddrinfo(ai);
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
        if (n <= 0) break;
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
    size_t len_str = strlen(str);
    size_t len_substr = strlen(substr);
    int count = 0;

    if (len_substr == 0 || len_substr > len_str) return 0;
    for (size_t i = 0; i <= len_str - len_substr; i++)
    {
        if (strncmp(str + i, substr, len_substr) == 0) count++;
    }

    return count;
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
    size_t start = 0;
    size_t end = strlen(str);

    while (start < end && isspace((unsigned char)str[start]))
        start++;

    while (end > start && isspace((unsigned char)str[end - 1]))
        end--;

    char *trimmed = malloc(end - start + 1);
    if (trimmed == NULL)
        return NULL;

    memcpy(trimmed, str + start, end - start);
    trimmed[end - start] = '\0';
    return trimmed;
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
        str[i] = toupper((unsigned char)str[i]);
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
    if (len < 4) return NULL;
    for (int i = 0; i < len-3; i++)
    {
        if (
            http_msg[i] == '\r'   &&
            http_msg[i+1] == '\n' &&
            http_msg[i+2] == '\r' &&
            http_msg[i+3] == '\n'
        ) {
            return http_msg + i + 4;
        }
    }
    return NULL;
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
    HTTP_HEADER_FIELD *hhf;
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
    if (saveptr != NULL)
        hr->num_of_headers = count_substring(saveptr, "\r\n") + 1;
    else
        hr->num_of_headers = 0;
    hr->headers = malloc(hr->num_of_headers * sizeof(HTTP_HEADER_FIELD));
    hr->body = body;
    hr->bodylen = http_msg_len - (body - http_msg);
    line = split_str(NULL, "\r\n", &saveptr);

    for (int i = 0; i < hr->num_of_headers; i++)
    {
        hhf = parse_http_header_line(line);
        if (hhf == NULL) return NULL;
        hr->headers[i] = hhf;
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
    HTTP_HEADER_FIELD *hhf;
    char *saveptr;
    char *start_line;
    char *head;
    char *line;
    char *body;

    body = get_http_body(http_msg, http_msg_len);
    if (body == NULL) return NULL;

    hr = malloc(sizeof(HTTP_RESPONSE));
    head = split_str(http_msg, "\r\n\r\n", &saveptr);
    start_line = split_str(head, "\r\n", &saveptr);
    hr->hsl = parse_http_status_line(start_line);
    if (saveptr != NULL)
        hr->num_of_headers = count_substring(saveptr, "\r\n") + 1;
    else
        hr->num_of_headers = 0;
    hr->headers = malloc(hr->num_of_headers * sizeof(HTTP_HEADER_FIELD));
    hr->body = body;
    hr->bodylen = http_msg_len - (body - http_msg);
    line = split_str(NULL, "\r\n", &saveptr);

    for (int i = 0; i < hr->num_of_headers; i++)
    {
        hhf = parse_http_header_line(line);
        if (hhf == NULL) return NULL;
        hr->headers[i] = hhf;
        line = split_str(NULL, "\r\n", &saveptr);
    }

    return hr;
}

void free_http_request(HTTP_REQUEST *hr)
{
    free(hr->hrl);
    for (int i = 0; i < hr->num_of_headers; i++)
        free(hr->headers[i]);
    free(hr->headers);
    free(hr);
}

void free_http_response(HTTP_RESPONSE *hr)
{
    free(hr->hsl);
    for (int i = 0; i < hr->num_of_headers; i++)
        free(hr->headers[i]);
    free(hr->headers);
    free(hr);
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
    msglen += 2;

    // body
    msglen += hr->bodylen;

    return msglen;
}

ssize_t tostring_http_response(HTTP_RESPONSE *hr, char **msg)
{
    char *p;
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
    memcpy(p, "\r\n", 2);
    p += 2;
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
    new_headers[2*i] = malloc((strlen(key)+1) * sizeof(char));
    strcpy(new_headers[2*i], key);
    new_headers[2*i+1] = malloc((strlen(val)+1) * sizeof(char));
    strcpy(new_headers[2*i+1], val);

    return new_headers;
}


// ###############
// # HTTP SERVER #
// ###############

// shared global variable for server settings
SERVER_SETTINGS *_settings = NULL;

void init_server_settings(
    char *root,
    char *port,
    int backlog,
    size_t max_recv_size,
    size_t total_req_size
) {
    _settings = malloc(sizeof(SERVER_SETTINGS));
    _settings->root = root;
    _settings->port = port;
    _settings->backlog = backlog;
    _settings->max_recv_size = max_recv_size;
    _settings->total_req_size = total_req_size;
}

int is_server_settings_set()
{
    if (_settings != NULL) return 1;
    else
    {
        puts("settings: Server settings must be initialized");
        return 0;
    }
}

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
    size_t *bodylen
) {
    char *req, *tmp;
    char *p;
    size_t reqsize = 0;
    size_t maxrecvsize;
    size_t totalreqsize;
    ssize_t n;

    if (!is_server_settings_set()) return -1;
    maxrecvsize = _settings->max_recv_size;
    totalreqsize = _settings->total_req_size;

    req = malloc((1 + maxrecvsize) * sizeof(char));
    n = recv(newfd, req, maxrecvsize, 0);
    while (n > 0)
    {
        reqsize += n;
        if (reqsize > totalreqsize)
        {
            puts("wserve: Received exceeded total size");
            free(req);
            return -1;
        }

        tmp = req;
        req = realloc(req, reqsize + maxrecvsize);
        if (req == NULL)
        {
            free(tmp);
            puts("wserve: Memory allocation error");
            return -1;
        }

        if ((p = get_http_body(req, reqsize)) != NULL)
        {
            req[reqsize] = '\0';
            *head = req;
            *body = p;
            *headlen = p - req;
            *bodylen = reqsize - *headlen;
            return reqsize;
        }

        n = recv(newfd, req+reqsize, maxrecvsize, 0);
    }
    if (n == 0)
    {
        free(req);
        puts("wserve: Connection closed by client before headers completed");
        return -1;
    }

    free(req);
    perror("recv");
    return -1;
}

// HTTP server core loop. Runs indefinitely and
// returns nothing.
//
// char *port: Port number that server will use.
// int backlog: Max length of connection queue.
// size_t maxrecvsize: Max number of bytes received at each "recv()".
void wserve_http()
{
    char *root, *port;
    int backlog;
    int listenfd;

    if (!is_server_settings_set()) exit(1);
    root = _settings->root;
    port = _settings->port;
    backlog = _settings->backlog;

    listenfd = create_listen_socket(port, backlog);
    if (listenfd == -1) exit(2);

    install_sigchld_handler();
    while (1)
    {
        int newfd = accept_connection(listenfd);
        if (newfd == -1) continue;
        set_socket_timeouts(newfd);

        printf("Connection to socket %d\n", newfd);

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

            msglen = http_recv(newfd, &http_msg, &http_body, &headlen, &bodylen);
            if (msglen > 0)
            {
                hr = parse_http_request(http_msg, msglen);
                if (hr != NULL && hr->hrl != NULL)
                {
                    response = process_http_requests(hr, root);
                    n = tostring_http_response(response, &response_str);
                    if (n > 0) _send(newfd, response_str, n);
                    free_http_response(response);
                    free_http_request(hr);
                }
                free(http_msg);
            }

            close(newfd);
            exit(0);
        }
        close(newfd);
    }
}

ssize_t recv_http_body_content_length(
    int newfd,
    char **body, size_t bodylen,
    size_t contentlength
) {
    char *newbody;
    char *p;
    ssize_t n;

    if (bodylen > contentlength)
    {
        puts("recv: body: Body length different than content length specified");
        return -1;
    }

    newbody = malloc(contentlength * sizeof(char));
    memcpy(newbody, *body, bodylen);
    p = newbody + bodylen;

    n = _recv(newfd, p, (contentlength - bodylen));
    if (n == -1)
    {
        perror("recv");
        free(newbody);
        return -1;
    }
    else if (n == (contentlength - bodylen))
    {
        *body = newbody;
        return n;
    }
    else
    {
        puts("recv: body: Body length different than content length specified");
        free(newbody);
        return -1;
    }
}

HTTP_RESPONSE *process_http_requests(HTTP_REQUEST *hr, char *root)
{
    if (hr == NULL || hr->hrl == NULL) return NULL;

    char *buf;
    char *ext = NULL, *mime = NULL;
    ssize_t n;

    if (strcmp(hr->hrl->method, "GET") == 0)
    {
        n = read_static_file(root, hr->hrl->target, &buf, &ext);
        mime = mime_type(ext);
        if (n == -1 || mime == NULL)
            return init_http_response("404", "Not found", NULL, 0, NULL, 0);
        else
        {
            int nh = 0;
            char **h = NULL;
            char val[65];

            sprintf(val, "%ld", n);
            h = append_to_headers_list(h, &nh, "content-length", val);
            h = append_to_headers_list(h, &nh, "content-type", mime);

            return init_http_response("200", "OK", buf, n, h, 2);
        }
    }
    else if (strcmp(hr->hrl->method, "HEAD") == 0)
    {
        n = read_static_file(root, hr->hrl->target, &buf, &ext);
        mime = mime_type(ext);
        if (n == -1 || mime == NULL)
            return init_http_response("404", "Not found", NULL, 0, NULL, 0);
        else
        {
            int nh = 0;
            char **h = NULL;
            char val[65];

            sprintf(val, "%ld", n);
            h = append_to_headers_list(h, &nh, "content-length", val);
            h = append_to_headers_list(h, &nh, "content-type", mime);

            return init_http_response("200", "OK", NULL, 0, h, 2);
        }
    }
    else
        return init_http_response("418", "I'm a teapod", NULL, 0, NULL, 0);
}


// ##################
// # STATIC ROUTING #
// ##################

int validate_target_path(char *target)
{
    // target NULL, empty, not starting with "/",
    // or including "./", ".." or "%".
    if (
        target == NULL                  ||
        strlen(target) == 0             ||
        target[0] != '/'                ||
        strstr(target, "..") != NULL    ||
        strstr(target, "./") != NULL    ||
        strstr(target, "%") != NULL
    )
        return 0;
    return 1;
}

long openat2(int dirfd, const char *path, struct open_how *how, size_t size)
{
    return syscall(SYS_openat2, dirfd, path, how, size);
}

int read_static_file(char *root, char *target, char **buf, char **extension)
{
    int c;
    char *ext = NULL;
    long i, len;
    int dirfd, fd, tmp;
    struct stat s;
    struct open_how f;
    FILE *fp;

    // fail if target is NULL, empty, not starting with "/",
    // or including "./", ".." or "%".
    if (!validate_target_path(target))
    {
        puts("validate: Target path invalid");
        return -1;
    }

    // fail if root not dir
    dirfd = open(root, O_DIRECTORY);
    if (dirfd == -1)
    {
        perror("open: root");
        return -1;
    }

    // consider root as / and resolve accordingly
    // do not resolve symlinks in path
    f.flags = 0;
    f.mode = 0;
    f.resolve = RESOLVE_IN_ROOT | RESOLVE_NO_SYMLINKS;
    fd = openat2(dirfd, target, &f, sizeof(struct open_how));
    close(dirfd);
    if (fd == -1)
    {
        perror("open: target");
        return -1;
    }

    // if path directory, try opening an "index.html"
    if (fstat(fd, &s) == 0)
    {
        if (S_ISDIR(s.st_mode)) {
            tmp = fd;
            fd = openat(fd, "index.html", O_NOFOLLOW);
            close(tmp);
            if (fd == -1)
            {
                perror("open: index");
                return -1;
            }

            if (fstat(fd, &s) == 0 && S_ISREG(s.st_mode))
            {
                ext = malloc((strlen(".html") + 1) * sizeof(char));
                strcpy(ext, ".html");
            }
            else
            {
                perror("open: index");
                close(fd);
                return -1;
            }
        }
        else if (S_ISREG(s.st_mode)) {
            ext = malloc((strlen(get_extension(target)) + 1) * sizeof(char));
            strcpy(ext, get_extension(target));
        }
        else
        {
            close(fd);
            perror("fstat");
            return -1;
        }
    }
    else
    {
        close(fd);
        perror("fstat");
        return -1;
    }

    // create a new stream from file descriptor
    fp = fdopen(fd, "rb");
    if (fp == NULL) {
        close(fd);
        perror("fdopen");
        return -1;
    }

    // get file content length
    len = 0;
    while (fgetc(fp) != EOF) len++;
    *buf = malloc((len + 1) * sizeof(char));

    // read content to a buffer
    rewind(fp);
    for (i = 0; i < len; i++)
    {
        c = fgetc(fp);
        (*buf)[i] = c;
    }
    (*buf)[i] = '\0';

    *extension = ext;
    fclose(fp);
    return len;
}

char *get_extension(char *path)
{
    char *p = strrchr(path, '.');
    if (p == NULL) return path;
    return p;
}

char *mime_type(char *extension)
{
    if (extension == NULL) return NULL;
    else if (extension[0] != '.') return "application/octet-stream";

    else if (strcmp(extension, ".aac") == 0) return "audio/aac";
    else if (strcmp(extension, ".abw") == 0) return "application/x-abiword";
    else if (strcmp(extension, ".apng") == 0) return "image/apng";
    else if (strcmp(extension, ".arc") == 0) return "application/x-freearc";
    else if (strcmp(extension, ".avif") == 0) return "image/avif";
    else if (strcmp(extension, ".avi") == 0) return "video/x-msvideo";
    else if (strcmp(extension, ".azw") == 0) return "application/vnd.amazon.ebook";
    else if (strcmp(extension, ".bin") == 0) return "application/octet-stream";
    else if (strcmp(extension, ".bmp") == 0) return "image/bmp";
    else if (strcmp(extension, ".bz") == 0) return "application/x-bzip";
    else if (strcmp(extension, ".bz2") == 0) return "application/x-bzip2";
    else if (strcmp(extension, ".cda") == 0) return "application/x-cdf";
    else if (strcmp(extension, ".csh") == 0) return "application/x-csh";
    else if (strcmp(extension, ".css") == 0) return "text/css";
    else if (strcmp(extension, ".csv") == 0) return "text/csv";
    else if (strcmp(extension, ".doc") == 0) return "application/msword";
    else if (strcmp(extension, ".docx") == 0) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    else if (strcmp(extension, ".eot") == 0) return "application/vnd.ms-fontobject";
    else if (strcmp(extension, ".epub") == 0) return "application/epub+zip";
    else if (strcmp(extension, ".gz") == 0) return "application/gzip";
    else if (strcmp(extension, ".gif") == 0) return "image/gif";
    else if (strcmp(extension, ".htm") == 0) return "text/html";
    else if (strcmp(extension, ".html") == 0) return "text/html";
    else if (strcmp(extension, ".ico") == 0) return "image/vnd.microsoft.icon";
    else if (strcmp(extension, ".ics") == 0) return "text/calendar";
    else if (strcmp(extension, ".jar") == 0) return "application/java-archive";
    else if (strcmp(extension, ".jpeg") == 0) return "image/jpeg";
    else if (strcmp(extension, ".jpg") == 0) return "image/jpeg";
    else if (strcmp(extension, ".js") == 0) return "text/javascript";
    else if (strcmp(extension, ".json") == 0) return "application/json";
    else if (strcmp(extension, ".jsonld") == 0) return "application/ld+json";
    else if (strcmp(extension, ".md") == 0) return "text/markdown";
    else if (strcmp(extension, ".mid") == 0) return "audio/midi";
    else if (strcmp(extension, ".midi") == 0) return "audio/midi";
    else if (strcmp(extension, ".mjs") == 0) return "text/javascript";
    else if (strcmp(extension, ".mp3") == 0) return "audio/mpeg";
    else if (strcmp(extension, ".mp4") == 0) return "video/mp4";
    else if (strcmp(extension, ".mpeg") == 0) return "video/mpeg";
    else if (strcmp(extension, ".mpkg") == 0) return "application/vnd.apple.installer+xml";
    else if (strcmp(extension, ".odp") == 0) return "application/vnd.oasis.opendocument.presentation";
    else if (strcmp(extension, ".ods") == 0) return "application/vnd.oasis.opendocument.spreadsheet";
    else if (strcmp(extension, ".odt") == 0) return "application/vnd.oasis.opendocument.text";
    else if (strcmp(extension, ".oga") == 0) return "audio/ogg";
    else if (strcmp(extension, ".ogv") == 0) return "video/ogg";
    else if (strcmp(extension, ".ogx") == 0) return "application/ogg";
    else if (strcmp(extension, ".opus") == 0) return "audio/ogg";
    else if (strcmp(extension, ".otf") == 0) return "font/otf";
    else if (strcmp(extension, ".pdf") == 0) return "application/pdf";
    else if (strcmp(extension, ".php") == 0) return "application/x-httpd-php";
    else if (strcmp(extension, ".png") == 0) return "image/png";
    else if (strcmp(extension, ".ppt") == 0) return "application/vnd.ms-powerpoint";
    else if (strcmp(extension, ".pptx") == 0) return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    else if (strcmp(extension, ".rar") == 0) return "application/vnd.rar";
    else if (strcmp(extension, ".rtf") == 0) return "application/rtf";
    else if (strcmp(extension, ".sh") == 0) return "application/x-sh";
    else if (strcmp(extension, ".svg") == 0) return "image/svg+xml";
    else if (strcmp(extension, ".tar") == 0) return "application/x-tar";
    else if (strcmp(extension, ".tif") == 0) return "image/tiff";
    else if (strcmp(extension, ".tiff") == 0) return "image/tiff";
    else if (strcmp(extension, ".ts") == 0) return "video/mp2t";
    else if (strcmp(extension, ".ttf") == 0) return "font/ttf";
    else if (strcmp(extension, ".txt") == 0) return "text/plain";
    else if (strcmp(extension, ".vsd") == 0) return "application/vnd.visio";
    else if (strcmp(extension, ".wav") == 0) return "audio/wav";
    else if (strcmp(extension, ".weba") == 0) return "audio/webm";
    else if (strcmp(extension, ".webm") == 0) return "video/webm";
    else if (strcmp(extension, ".webmanifest") == 0) return "application/manifest+json";
    else if (strcmp(extension, ".webp") == 0) return "image/webp";
    else if (strcmp(extension, ".woff") == 0) return "font/woff";
    else if (strcmp(extension, ".woff2") == 0) return "font/woff2";
    else if (strcmp(extension, ".xhtml") == 0) return "application/xhtml+xml";
    else if (strcmp(extension, ".xls") == 0) return "application/vnd.ms-excel";
    else if (strcmp(extension, ".xlsx") == 0) return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    else if (strcmp(extension, ".xml") == 0) return "application/xml";
    else if (strcmp(extension, ".xul") == 0) return "application/vnd.mozilla.xul+xml";
    else if (strcmp(extension, ".zip") == 0) return "application/zip";
    else if (strcmp(extension, ".3gp") == 0) return "video/3gpp";
    else if (strcmp(extension, ".3g2") == 0) return "video/3gpp2";
    else if (strcmp(extension, ".7z") == 0) return "application/x-7z-compressed";

    else return "application/octet-stream";
}
