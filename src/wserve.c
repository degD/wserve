
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

/**
 * @brief Reap terminated child processes.
 *
 * This function is intended to be installed as the `SIGCHLD` handler. It
 * reaps every child that has already terminated without blocking and
 * preserves `errno` for the interrupted code.
 *
 * @param[in] s Signal number supplied by the signal dispatcher. The value is
 *              unused and is normally `SIGCHLD`.
 */
void sigchld_handler(int s)
{
    int saved_errno = errno;
    (void)s;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

/**
 * @brief Install the `SIGCHLD` handler used to reap child processes.
 *
 * The handler uses `SA_RESTART` and the resulting signal disposition is
 * inherited by children created after installation.
 *
 * @return `0` on success, or `-1` if installing the handler fails.
 */
int install_sigchld_handler(void)
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("wserve: sigaction");
        return -1;
    }
    return 0;
}

/**
 * @brief Set ten-second send and receive timeouts on a socket.
 *
 * @param[in] fd Socket file descriptor to configure.
 *
 * @return `0` if both socket options are set, or `-1` if either operation
 *         fails.
 */
int set_socket_timeouts(int fd)
{
    struct timeval tv = {.tv_sec = 10, .tv_usec = 0};

    if (
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)
    {
        return -1;
    }
    return 0;
}

/**
 * @brief Create, bind, and listen on an IPv4 TCP socket.
 *
 * The socket is bound to all local IPv4 addresses, configured for address
 * reuse, and given ten-second send and receive timeouts.
 *
 * @param[in] port NUL-terminated service name or port number.
 * @param[in] backlog Requested length of the pending-connection queue.
 *
 * @return The listening socket descriptor on success, or `-1` on failure.
 */
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
    if (r != 0)
    {
        fprintf(stderr, "wserve: %s\n", gai_strerror(r));
        return -1;
    }

    listenfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listenfd == -1)
    {
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
    if (r == -1)
    {
        close(listenfd);
        perror("source: bind");
        freeaddrinfo(ai);
        return -1;
    }

    freeaddrinfo(ai);

    r = listen(listenfd, backlog);
    if (r == -1)
    {
        perror("wserve: listen");
        return -1;
    }

    return listenfd;
}

/**
 * @brief Accept one pending connection from a listening socket.
 *
 * @param[in] listenfd Listening socket descriptor.
 *
 * @return A connected socket descriptor on success, or `-1` if accepting the
 *         connection fails.
 */
int accept_connection(int listenfd)
{
    struct sockaddr_storage ss;
    socklen_t sin_size;
    int newfd;

    sin_size = sizeof(ss);
    newfd = accept(listenfd, (struct sockaddr *)&ss, &sin_size);

    if (newfd == -1)
    {
        perror("wserve: accept");
        return -1;
    }

    return newfd;
}

/**
 * @brief Send a complete buffer to a socket when possible.
 *
 * The function repeats `send()` calls to handle partial writes. It stops if
 * `send()` returns zero or reports an error.
 *
 * @param[in] newfd Socket file descriptor.
 * @param[in] buf Buffer containing the bytes to send.
 * @param[in] nbytes Number of bytes to send.
 *
 * @return The number of bytes sent if the loop stops without an error, or
 *         `-1` if a `send()` call fails.
 */
ssize_t _send(int newfd, void *buf, size_t nbytes)
{
    size_t bytes_sent = 0;
    size_t bytes_left = nbytes;
    ssize_t n = 0;
    char *p = buf;

    while (bytes_sent < nbytes)
    {
        n = send(newfd, p, bytes_left, 0);
        if (n <= 0)
            break;
        bytes_sent += n;
        bytes_left -= n;
        p += n;
    }

    return n == -1 ? -1 : bytes_sent;
}

/**
 * @brief Receive up to a complete buffer from a socket.
 *
 * The function repeats `recv()` calls to handle partial reads. It stops when
 * the requested length is reached or when the peer closes the connection.
 *
 * @param[in] newfd Socket file descriptor.
 * @param[out] buf Buffer that receives the data.
 * @param[in] nbytes Maximum number of bytes to receive.
 *
 * @return The number of bytes received before an orderly shutdown, or the
 *         requested length, or `-1` if a `recv()` call fails.
 */
ssize_t _recv(int newfd, void *buf, size_t nbytes)
{
    size_t bytes_recv = 0;
    size_t bytes_left = nbytes;
    ssize_t n = 0;
    char *p = buf;

    while (bytes_recv < nbytes)
    {
        n = recv(newfd, p, bytes_left, 0);
        if (n <= 0)
            break;
        bytes_recv += n;
        bytes_left -= n;
        p += n;
    }

    return n == -1 ? -1 : bytes_recv;
}

// #######################
// # HTTP HEADERS PARSER #
// #######################

/**
 * @brief Count occurrences of a substring, including overlapping matches.
 *
 * @param[in] str NUL-terminated string to search.
 * @param[in] substr NUL-terminated substring to count.
 *
 * @return The number of occurrences, or `0` when `substr` is empty or is not
 *         present in `str`.
 */
int count_substring(char *str, char *substr)
{
    size_t len_str = strlen(str);
    size_t len_substr = strlen(substr);
    int count = 0;

    if (len_substr == 0 || len_substr > len_str)
        return 0;
    for (size_t i = 0; i <= len_str - len_substr; i++)
    {
        if (strncmp(str + i, substr, len_substr) == 0)
            count++;
    }

    return count;
}

/**
 * @brief Split a string at the next occurrence of a substring.
 *
 * On the first call, pass the string to split. Pass `NULL` for `str` on
 * subsequent calls to continue from `saveptr`. The delimiter is removed by
 * replacing it with a NUL character.
 *
 * @param[in,out] str String to split on the first call, or `NULL` to continue
 *                    an existing split operation.
 * @param[in] substr Non-empty delimiter substring.
 * @param[in,out] saveptr Caller-owned state pointer used between calls.
 *
 * @return A pointer to the next token within the original string, or `NULL`
 *         when no token remains.
 *
 * @note The input string is modified and no token storage is allocated.
 */
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

/**
 * @brief Copy a string after removing leading and trailing whitespace.
 *
 * @param[in] str NUL-terminated string to trim.
 *
 * @return A newly allocated, NUL-terminated trimmed string. The caller owns
 *         the returned string and must free it, or `NULL` if allocation fails.
 */
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

/**
 * @brief Convert a NUL-terminated string to uppercase in place.
 *
 * @param[in,out] str String to modify.
 */
void toupper_str(char *str)
{
    int i = 0;
    while (str[i] != '\0')
    {
        str[i] = toupper((unsigned char)str[i]);
        i++;
    }
}

/**
 * @brief Find the body of an HTTP message.
 *
 * The body begins immediately after the first `CRLFCRLF` sequence found in
 * the supplied byte range.
 *
 * @param[in] http_msg HTTP message bytes to scan.
 * @param[in] len Number of bytes available in `http_msg`.
 *
 * @return A pointer into `http_msg` immediately after the separator, or
 *         `NULL` if the separator is not present.
 *
 * @note The returned pointer aliases `http_msg`; this function does not
 *       allocate memory and does not require a NUL terminator.
 */
char *get_http_body(char *http_msg, size_t len)
{
    if (len < 4)
        return NULL;
    for (int i = 0; i < len - 3; i++)
    {
        if (
            http_msg[i] == '\r' &&
            http_msg[i + 1] == '\n' &&
            http_msg[i + 2] == '\r' &&
            http_msg[i + 3] == '\n')
        {
            return http_msg + i + 4;
        }
    }
    return NULL;
}

/**
 * @brief Parse one HTTP header field line.
 *
 * The first colon separates the field name from its value. The field name is
 * converted to uppercase and the value is copied after trimming whitespace.
 *
 * @param[in,out] line NUL-terminated, writable header line to parse.
 *
 * @return A newly allocated header-field structure, or `NULL` when `line`
 *         does not contain a colon.
 *
 * @note The first colon in `line` is replaced with `\0`. The returned `key`
 *       aliases `line`, while `val` is separately allocated and must be
 *       released by the caller.
 */
HTTP_HEADER_FIELD *parse_http_header_line(char *line)
{
    char *saveptr;
    char *val;
    HTTP_HEADER_FIELD *hh;

    if (strstr(line, ":") == NULL)
        return NULL;

    hh = malloc(sizeof(HTTP_HEADER_FIELD));
    val = strstr(line, ":") + sizeof(char);
    hh->key = split_str(line, ":", &saveptr);
    hh->val = trim(val);
    toupper_str(hh->key);

    return hh;
}

/**
 * @brief Parse an HTTP request start line.
 *
 * The line is split at its first two spaces into the method, request target,
 * and HTTP version.
 *
 * @param[in,out] start_line NUL-terminated, writable request start line.
 *
 * @return A newly allocated request-line structure, or `NULL` if fewer than
 *         two spaces are present.
 *
 * @note The returned fields alias `start_line`, which is modified in place.
 *       This function performs no further syntax or HTTP-version validation.
 */
HTTP_REQUEST_LINE *parse_http_request_line(char *start_line)
{
    HTTP_REQUEST_LINE *hrl;
    char *saveptr;
    int n = count_substring(start_line, " ");

    if (n < 2)
        return NULL;

    hrl = malloc(sizeof(HTTP_REQUEST_LINE));
    hrl->method = split_str(start_line, " ", &saveptr);
    hrl->target = split_str(NULL, " ", &saveptr);
    hrl->http_version = saveptr;

    return hrl;
}

/**
 * @brief Parse an HTTP response status line.
 *
 * The line is split at its first two spaces into the HTTP version, status
 * code, and response text.
 *
 * @param[in,out] start_line NUL-terminated, writable response status line.
 *
 * @return A newly allocated status-line structure, or `NULL` if fewer than
 *         two spaces are present.
 *
 * @note The returned fields alias `start_line`, which is modified in place.
 *       This function performs no further syntax or status-code validation.
 */
HTTP_STATUS_LINE *parse_http_status_line(char *start_line)
{
    HTTP_STATUS_LINE *hsl;
    char *saveptr;
    int n = count_substring(start_line, " ");

    if (n < 2)
        return NULL;

    hsl = malloc(sizeof(HTTP_STATUS_LINE));
    hsl->http_version = split_str(start_line, " ", &saveptr);
    hsl->status_code = split_str(NULL, " ", &saveptr);
    hsl->response_text = saveptr;

    return hsl;
}

/**
 * @brief Parse an HTTP request message.
 *
 * @param[in,out] http_msg NUL-terminated, writable HTTP message containing a
 *                         complete header section.
 * @param[in] http_msg_len Number of message bytes available in `http_msg`.
 *
 * @return A newly allocated request structure, or `NULL` if the header
 *         separator is missing or a header field cannot be parsed.
 *
 * @note Parsing modifies `http_msg`. The returned start-line fields and body
 *       alias that buffer; retain it until the request is no longer used.
 */
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
    if (body == NULL)
        return NULL;

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
        if (hhf == NULL)
            return NULL;
        hr->headers[i] = hhf;
        line = split_str(NULL, "\r\n", &saveptr);
    }

    return hr;
}

/**
 * @brief Parse an HTTP response message.
 *
 * @param[in,out] http_msg NUL-terminated, writable HTTP message containing a
 *                         complete header section.
 * @param[in] http_msg_len Number of message bytes available in `http_msg`.
 *
 * @return A newly allocated response structure, or `NULL` if the header
 *         separator is missing or a header field cannot be parsed.
 *
 * @note Parsing modifies `http_msg`. The returned status-line fields and body
 *       alias that buffer; retain it until the response is no longer used.
 */
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
    if (body == NULL)
        return NULL;

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
        if (hhf == NULL)
            return NULL;
        hr->headers[i] = hhf;
        line = split_str(NULL, "\r\n", &saveptr);
    }

    return hr;
}

/**
 * @brief Free the allocations associated with a parsed HTTP request.
 *
 * @param[in] hr Request structure to release.
 *
 * @note The original message buffer and strings referenced by the structure
 *       are not freed. The caller must keep them alive until after this
 *       function returns and release any separately allocated field values.
 */
void free_http_request(HTTP_REQUEST *hr)
{
    free(hr->hrl);
    for (int i = 0; i < hr->num_of_headers; i++)
        free(hr->headers[i]);
    free(hr->headers);
    free(hr);
}

/**
 * @brief Free the allocations associated with an HTTP response structure.
 *
 * @param[in] hr Response structure to release.
 *
 * @note The body and strings referenced by the structure are not freed. The
 *       caller owns those buffers and must release any separately allocated
 *       strings.
 */
void free_http_response(HTTP_RESPONSE *hr)
{
    free(hr->hsl);
    for (int i = 0; i < hr->num_of_headers; i++)
        free(hr->headers[i]);
    free(hr->headers);
    free(hr);
}

/**
 * @brief Calculate the serialized size of an HTTP response.
 *
 * The size includes the status line, all header lines, the terminating
 * `CRLFCRLF`, and the response body. It does not include a NUL terminator.
 *
 * @param[in] hr Response to measure.
 *
 * @return The serialized response length in bytes, or `-1` if `hr` is
 *         `NULL`.
 */
ssize_t calc_http_response_size(HTTP_RESPONSE *hr)
{
    if (hr == NULL)
        return -1;

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

/**
 * @brief Serialize an HTTP response into a newly allocated buffer.
 *
 * @param[in] hr Response to serialize.
 * @param[out] msg Receives the allocated serialized response buffer.
 *
 * @return The number of serialized bytes, or `-1` if `hr` is `NULL`.
 *
 * @note The returned buffer is owned by the caller and is not NUL-terminated;
 *       use the returned length when sending it.
 */
ssize_t tostring_http_response(HTTP_RESPONSE *hr, char **msg)
{
    char *p;
    ssize_t msglen = calc_http_response_size(hr);
    if (msglen < 0)
        return -1;

    *msg = malloc(msglen * sizeof(char));
    p = *msg;

    p += sprintf(p, "%s %s %s\r\n",
                 hr->hsl->http_version,
                 hr->hsl->status_code,
                 hr->hsl->response_text);
    for (int i = 0; i < hr->num_of_headers; i++)
    {
        p += sprintf(p, "%s: %s\r\n",
                     hr->headers[i]->key,
                     hr->headers[i]->val);
    }
    memcpy(p, "\r\n", 2);
    p += 2;
    memcpy(p, hr->body, hr->bodylen);

    return msglen;
}

/**
 * @brief Construct an HTTP/1.1 response structure.
 *
 * @param[in] status_code NUL-terminated HTTP status code.
 * @param[in] resp_text NUL-terminated reason phrase.
 * @param[in] body Response body, or `NULL` when `bodylen` is zero.
 * @param[in] bodylen Number of bytes in `body`.
 * @param[in] headers Flat array of header key/value pointers. Each header
 *                    occupies two entries: key followed by value.
 * @param[in] nheaders Number of header fields in `headers`.
 *
 * @return A newly allocated response structure.
 *
 * @note The status code, reason phrase, body, and header strings are not
 *       copied. They must remain valid for the lifetime of the response.
 *       The returned structure should be released with
 *       `free_http_response()`.
 */
HTTP_RESPONSE *init_http_response(
    char *status_code,
    char *resp_text,
    char *body, size_t bodylen,
    char **headers, int nheaders)
{
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
        p->key = headers[2 * i];
        p->val = headers[2 * i + 1];
    }

    return hr;
}

/**
 * @brief Append a copied key/value pair to a flat header list.
 *
 * @param[in,out] headers Existing list of alternating key/value pointers, or
 *                        `NULL` when `*nheaders` is zero.
 * @param[in,out] nheaders Number of header fields in the list. Incremented
 *                         on success.
 * @param[in] key NUL-terminated header name to copy.
 * @param[in] val NUL-terminated header value to copy.
 *
 * @return The resized header list containing the new pair.
 *
 * @note The new strings are separately allocated. The caller owns the list
 *       and every string stored in it.
 */
char **append_to_headers_list(
    char **headers, int *nheaders,
    char *key, char *val)
{
    int i;
    char **new_headers;

    *nheaders += 1;
    new_headers = realloc(headers, *nheaders * sizeof(char *) * 2);

    i = *nheaders - 1;
    new_headers[2 * i] = malloc((strlen(key) + 1) * sizeof(char));
    strcpy(new_headers[2 * i], key);
    new_headers[2 * i + 1] = malloc((strlen(val) + 1) * sizeof(char));
    strcpy(new_headers[2 * i + 1], val);

    return new_headers;
}

// ###############
// # HTTP SERVER #
// ###############

// shared global variable for server settings
SERVER_SETTINGS *_settings = NULL;

/**
 * @brief Validate an HTTP request.
 *
 * Only `GET`, `HEAD`, and `POST` are allowed. The request must not
 * contain a `TRANSFER-ENCODING` header. At most one `CONTENT-LENGTH`
 * header is permitted. Only `HTTP/1.1` is accepted.
 *
 * @param[in] hr Parsed request to be validated.
 *
 * @return `0` if request is valid. `1` if method not supported.
 *         `2` if malformed or unsupported headers. `3` if HTTP
 *         version is unsuported.
 */
int validate_request(HTTP_REQUEST *hr)
{
    // method not supported
    if (
        strcmp(hr->hrl->method, "GET") != 0 ||
        strcmp(hr->hrl->method, "HEAD") != 0 ||
        strcmp(hr->hrl->method, "POST") != 0)
        return 1;

    // malformed/unsupported headers
    int c = 0;
    for (int i = 0; i < hr->num_of_headers; i++)
    {
        if (strcmp(hr->headers[i]->key, "TRANSFER-ENCODING") == 0)
            return 2;

        if (c > 1)
            return 2;
        if (strcmp(hr->headers[i]->key, "CONTENT-LENGTH") == 0)
            c += 1;
    }

    // http version unsupported
    if (strcmp(hr->hrl->http_version, "HTTP/1.1") != 0)
        return 3;

    return 0;
}

/**
 * @brief Initialize the process-wide HTTP server settings.
 *
 * @param[in] root Document-root path used for static files.
 * @param[in] port Service name or port number for the listening socket.
 * @param[in] backlog Requested pending-connection queue length.
 * @param[in] max_recv_size Maximum number of bytes read per `recv()` call.
 * @param[in] total_req_size Maximum total request size accepted by
 *                           `http_recv()`.
 *
 * @note `root` and `port` are stored without copying, so both strings must
 *       remain valid while the server is running.
 */
void init_server_settings(
    char *root,
    char *port,
    int backlog,
    size_t max_recv_size,
    size_t total_req_size)
{
    _settings = malloc(sizeof(SERVER_SETTINGS));
    _settings->root = root;
    _settings->port = port;
    _settings->backlog = backlog;
    _settings->max_recv_size = max_recv_size;
    _settings->total_req_size = total_req_size;
}

/**
 * @brief Check whether process-wide server settings have been initialized.
 *
 * Prints a diagnostic message when settings have not been initialized.
 *
 * @return `1` when settings are available, otherwise `0`.
 */
int is_server_settings_set()
{
    if (_settings != NULL)
        return 1;
    else
    {
        puts("settings: Server settings must be initialized");
        return 0;
    }
}

/**
 * @brief Send a response with only a status code and response text.
 *
 * @param[in] newfd Connected socket file descriptor.
 * @param[in] status_code Three-digit status code.
 * @param[in] response_text Response text explaining status code.
 *
 * @note This function does not validate `status_code` and can therefore
 *       send invalid values.
 */
void send_http_response_status(int newfd, char *status_code, char *response_text)
{
    size_t n = strlen("HTTP/1.1") + 1 + strlen(status_code) + 1 + strlen(response_text) + 4 + 1;
    char *msg = malloc(n * sizeof(char));

    snprintf(msg, n, "HTTP/1.1 %s %s\r\n\r\n", status_code, response_text);
    _send(newfd, msg, n-1);
    free(msg);
}

/**
 * @brief Receive a message through the end of its HTTP header section.
 *
 * Reads chunks until `CRLFCRLF` is received or an error occurs. The receive
 * limits are taken from the process-wide settings.
 *
 * @param[in] newfd Connected socket file descriptor.
 * @param[out] head Receives an allocated, NUL-terminated buffer containing
 *                  all bytes received. Free this buffer when finished.
 * @param[out] body Receives a pointer into `*head` immediately after the
 *                  header separator.
 * @param[out] headlen Receives the header length, including `CRLFCRLF`.
 * @param[out] bodylen Receives the number of body bytes already read after
 *                     the header separator.
 *
 * @return The total number of bytes received, or `-1` if settings are absent,
 *         the request exceeds the configured limit, the peer closes before
 *         the header is complete, or a socket/allocation error occurs.
 *
 * @note This function does not inspect `Content-Length` and may return before
 *       the complete body has been received. Use along with
 *       `recv_http_body_content_length` to receive the the whole request.
 */
ssize_t http_recv(
    int newfd,
    char **head,
    char **body,
    size_t *headlen,
    size_t *bodylen)
{
    char *req, *tmp;
    char *p;
    size_t reqsize = 0;
    size_t maxrecvsize;
    size_t totalreqsize;
    ssize_t n;

    if (!is_server_settings_set())
        return -1;
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

        n = recv(newfd, req + reqsize, maxrecvsize, 0);
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

/**
 * @brief Run the main process-per-connection HTTP server loop.
 *
 * The function creates the listening socket, accepts connections, forks a
 * child for each connection, receives one request, sends one response, and
 * closes the connection.
 *
 * @note Server settings must be initialized with `init_server_settings()`
 *       first. The function does not return during normal operation; it
 *       exits the process when initial setup fails.
 */
void wserve_http()
{
    char *root, *port;
    int backlog;
    int listenfd;

    if (!is_server_settings_set())
        exit(1);
    root = _settings->root;
    port = _settings->port;
    backlog = _settings->backlog;

    listenfd = create_listen_socket(port, backlog);
    if (listenfd == -1)
        exit(2);

    install_sigchld_handler();
    while (1)
    {
        int newfd = accept_connection(listenfd);
        if (newfd == -1)
            continue;
        set_socket_timeouts(newfd);

        printf("Connection to socket %d\n", newfd);

        if (!fork())
        {
            close(listenfd);
            int is_hr;
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
                    is_hr = validate_request(hr);
                    switch (is_hr) {
                        case 1:
                            send_http_response_status(newfd, "501", "Not Implemented");
                            break;
                        case 2:
                            send_http_response_status(newfd, "400", "Bad Request");
                            break;
                        case 3:
                            send_http_response_status(newfd, "505", "HTTP Version Not Supported");
                            break;
                    }
                    if (is_hr != 0)
                    {
                        close(newfd);
                        free(http_msg);
                        free_http_request(hr);
                        exit(0);
                    }

                    response = process_http_requests(hr, root);
                    n = tostring_http_response(response, &response_str);
                    if (n > 0)
                        _send(newfd, response_str, n);
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

/**
 * @brief Receive the remainder of a body with a known content length.
 *
 * @param[in] newfd Connected socket file descriptor.
 * @param[in,out] body Pointer to the bytes already received. On success it
 *                     is replaced with a newly allocated complete body.
 * @param[in] bodylen Number of body bytes already present at `*body`.
 * @param[in] contentlength Expected total body length.
 *
 * @return The number of bytes read from the socket, or `-1` if the supplied
 *         length is inconsistent or the remaining bytes cannot be received.
 *
 * @note The new buffer is exactly `contentlength` bytes and is not
 *       NUL-terminated. The original buffer is not freed. If `bodylen` equals
 *       `contentlength`, the function returns `0` after replacing `*body`.
 */
ssize_t recv_http_body_content_length(
    int newfd,
    char **body, size_t bodylen,
    size_t contentlength)
{
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

/**
 * @brief Create a response for a parsed HTTP request.
 *
 * `GET` and `HEAD` requests are served from the configured static-file root.
 * Missing files and unsupported MIME types produce `404`; methods other than
 * `GET` and `HEAD` produce the server's `418` response.
 *
 * @param[in] hr Parsed request to process.
 * @param[in] root Document-root path for static files.
 *
 * @return A newly allocated response structure, or `NULL` when `hr` or its
 *         request line is `NULL`.
 *
 * @note The caller owns the returned response and should release it with
 *       `free_http_response()`.
 */
HTTP_RESPONSE *process_http_requests(HTTP_REQUEST *hr, char *root)
{
    if (hr == NULL || hr->hrl == NULL)
        return NULL;

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

/**
 * @brief Validate a request target for static-file lookup.
 *
 * The accepted form is a non-empty path beginning with `/` that contains no
 * `..`, `./`, or percent characters. This is a syntactic check only; it does
 * not access the filesystem or normalize the path.
 *
 * @param[in] target NUL-terminated request target to validate.
 *
 * @return `1` when the target satisfies the static-routing policy, otherwise
 *         `0`.
 */
int validate_target_path(char *target)
{
    // target NULL, empty, not starting with "/",
    // or including "./", ".." or "%".
    if (
        target == NULL ||
        strlen(target) == 0 ||
        target[0] != '/' ||
        strstr(target, "..") != NULL ||
        strstr(target, "./") != NULL ||
        strstr(target, "%") != NULL)
        return 0;
    return 1;
}

/**
 * @brief Invoke the Linux `openat2` system call.
 *
 * @param[in] dirfd Directory file descriptor used to resolve relative paths.
 * @param[in] path Path to open.
 * @param[in] how Open and path-resolution options.
 * @param[in] size Size of the `struct open_how` data supplied by `how`.
 *
 * @return The file descriptor returned by the kernel, or `-1` on error with
 *         `errno` set by the system call.
 */
long openat2(int dirfd, const char *path, struct open_how *how, size_t size)
{
    return syscall(SYS_openat2, dirfd, path, how, size);
}

/**
 * @brief Read a static file beneath a document root.
 *
 * The target is resolved relative to `root` without following symbolic links.
 * If the target names a directory, `index.html` is opened from that directory
 * instead. The entire file is copied into a newly allocated buffer.
 *
 * @param[in] root Path to the document-root directory.
 * @param[in] target Validated request target relative to `root`.
 * @param[out] buf Receives the allocated file contents. An extra NUL byte is
 *                 appended after the file data.
 * @param[out] extension Receives an allocated file extension, including the
 *                       leading dot when present.
 *
 * @return The file length in bytes, or `-1` if validation, lookup, opening,
 *         metadata, or reading setup fails.
 *
 * @note On success, the caller owns both `*buf` and `*extension` and must
 *       free them. Binary file data may contain embedded NUL bytes.
 */
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
        if (S_ISDIR(s.st_mode))
        {
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
        else if (S_ISREG(s.st_mode))
        {
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
    if (fp == NULL)
    {
        close(fd);
        perror("fdopen");
        return -1;
    }

    // get file content length
    len = 0;
    while (fgetc(fp) != EOF)
        len++;
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

/**
 * @brief Return the suffix beginning at the last dot in a path.
 *
 * @param[in] path NUL-terminated path to inspect.
 *
 * @return A pointer into `path` at its last `.` character, or `path` itself
 *         when no dot is present.
 *
 * @note The returned pointer aliases `path`; no memory is allocated.
 */
char *get_extension(char *path)
{
    char *p = strrchr(path, '.');
    if (p == NULL)
        return path;
    return p;
}

/**
 * @brief Map a file extension to a MIME type.
 *
 * @param[in] extension Extension to map, normally including its leading dot.
 *
 * @return A pointer to a static MIME-type string. Returns `NULL` when
 *         `extension` is `NULL`; unknown extensions map to
 *         `application/octet-stream`.
 *
 * @note Matching is case-sensitive and the returned string must not be freed
 *       or modified.
 */
char *mime_type(char *extension)
{
    if (extension == NULL)
        return NULL;
    else if (extension[0] != '.')
        return "application/octet-stream";

    else if (strcmp(extension, ".aac") == 0)
        return "audio/aac";
    else if (strcmp(extension, ".abw") == 0)
        return "application/x-abiword";
    else if (strcmp(extension, ".apng") == 0)
        return "image/apng";
    else if (strcmp(extension, ".arc") == 0)
        return "application/x-freearc";
    else if (strcmp(extension, ".avif") == 0)
        return "image/avif";
    else if (strcmp(extension, ".avi") == 0)
        return "video/x-msvideo";
    else if (strcmp(extension, ".azw") == 0)
        return "application/vnd.amazon.ebook";
    else if (strcmp(extension, ".bin") == 0)
        return "application/octet-stream";
    else if (strcmp(extension, ".bmp") == 0)
        return "image/bmp";
    else if (strcmp(extension, ".bz") == 0)
        return "application/x-bzip";
    else if (strcmp(extension, ".bz2") == 0)
        return "application/x-bzip2";
    else if (strcmp(extension, ".cda") == 0)
        return "application/x-cdf";
    else if (strcmp(extension, ".csh") == 0)
        return "application/x-csh";
    else if (strcmp(extension, ".css") == 0)
        return "text/css";
    else if (strcmp(extension, ".csv") == 0)
        return "text/csv";
    else if (strcmp(extension, ".doc") == 0)
        return "application/msword";
    else if (strcmp(extension, ".docx") == 0)
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    else if (strcmp(extension, ".eot") == 0)
        return "application/vnd.ms-fontobject";
    else if (strcmp(extension, ".epub") == 0)
        return "application/epub+zip";
    else if (strcmp(extension, ".gz") == 0)
        return "application/gzip";
    else if (strcmp(extension, ".gif") == 0)
        return "image/gif";
    else if (strcmp(extension, ".htm") == 0)
        return "text/html";
    else if (strcmp(extension, ".html") == 0)
        return "text/html";
    else if (strcmp(extension, ".ico") == 0)
        return "image/vnd.microsoft.icon";
    else if (strcmp(extension, ".ics") == 0)
        return "text/calendar";
    else if (strcmp(extension, ".jar") == 0)
        return "application/java-archive";
    else if (strcmp(extension, ".jpeg") == 0)
        return "image/jpeg";
    else if (strcmp(extension, ".jpg") == 0)
        return "image/jpeg";
    else if (strcmp(extension, ".js") == 0)
        return "text/javascript";
    else if (strcmp(extension, ".json") == 0)
        return "application/json";
    else if (strcmp(extension, ".jsonld") == 0)
        return "application/ld+json";
    else if (strcmp(extension, ".md") == 0)
        return "text/markdown";
    else if (strcmp(extension, ".mid") == 0)
        return "audio/midi";
    else if (strcmp(extension, ".midi") == 0)
        return "audio/midi";
    else if (strcmp(extension, ".mjs") == 0)
        return "text/javascript";
    else if (strcmp(extension, ".mp3") == 0)
        return "audio/mpeg";
    else if (strcmp(extension, ".mp4") == 0)
        return "video/mp4";
    else if (strcmp(extension, ".mpeg") == 0)
        return "video/mpeg";
    else if (strcmp(extension, ".mpkg") == 0)
        return "application/vnd.apple.installer+xml";
    else if (strcmp(extension, ".odp") == 0)
        return "application/vnd.oasis.opendocument.presentation";
    else if (strcmp(extension, ".ods") == 0)
        return "application/vnd.oasis.opendocument.spreadsheet";
    else if (strcmp(extension, ".odt") == 0)
        return "application/vnd.oasis.opendocument.text";
    else if (strcmp(extension, ".oga") == 0)
        return "audio/ogg";
    else if (strcmp(extension, ".ogv") == 0)
        return "video/ogg";
    else if (strcmp(extension, ".ogx") == 0)
        return "application/ogg";
    else if (strcmp(extension, ".opus") == 0)
        return "audio/ogg";
    else if (strcmp(extension, ".otf") == 0)
        return "font/otf";
    else if (strcmp(extension, ".pdf") == 0)
        return "application/pdf";
    else if (strcmp(extension, ".php") == 0)
        return "application/x-httpd-php";
    else if (strcmp(extension, ".png") == 0)
        return "image/png";
    else if (strcmp(extension, ".ppt") == 0)
        return "application/vnd.ms-powerpoint";
    else if (strcmp(extension, ".pptx") == 0)
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    else if (strcmp(extension, ".rar") == 0)
        return "application/vnd.rar";
    else if (strcmp(extension, ".rtf") == 0)
        return "application/rtf";
    else if (strcmp(extension, ".sh") == 0)
        return "application/x-sh";
    else if (strcmp(extension, ".svg") == 0)
        return "image/svg+xml";
    else if (strcmp(extension, ".tar") == 0)
        return "application/x-tar";
    else if (strcmp(extension, ".tif") == 0)
        return "image/tiff";
    else if (strcmp(extension, ".tiff") == 0)
        return "image/tiff";
    else if (strcmp(extension, ".ts") == 0)
        return "video/mp2t";
    else if (strcmp(extension, ".ttf") == 0)
        return "font/ttf";
    else if (strcmp(extension, ".txt") == 0)
        return "text/plain";
    else if (strcmp(extension, ".vsd") == 0)
        return "application/vnd.visio";
    else if (strcmp(extension, ".wav") == 0)
        return "audio/wav";
    else if (strcmp(extension, ".weba") == 0)
        return "audio/webm";
    else if (strcmp(extension, ".webm") == 0)
        return "video/webm";
    else if (strcmp(extension, ".webmanifest") == 0)
        return "application/manifest+json";
    else if (strcmp(extension, ".webp") == 0)
        return "image/webp";
    else if (strcmp(extension, ".woff") == 0)
        return "font/woff";
    else if (strcmp(extension, ".woff2") == 0)
        return "font/woff2";
    else if (strcmp(extension, ".xhtml") == 0)
        return "application/xhtml+xml";
    else if (strcmp(extension, ".xls") == 0)
        return "application/vnd.ms-excel";
    else if (strcmp(extension, ".xlsx") == 0)
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    else if (strcmp(extension, ".xml") == 0)
        return "application/xml";
    else if (strcmp(extension, ".xul") == 0)
        return "application/vnd.mozilla.xul+xml";
    else if (strcmp(extension, ".zip") == 0)
        return "application/zip";
    else if (strcmp(extension, ".3gp") == 0)
        return "video/3gpp";
    else if (strcmp(extension, ".3g2") == 0)
        return "video/3gpp2";
    else if (strcmp(extension, ".7z") == 0)
        return "application/x-7z-compressed";

    else
        return "application/octet-stream";
}
