
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


// #######################
// # FUNCTION PROTOTYPES #
// #######################

void sigchld_handler(int s);
int install_sigchld_handler(void);
int create_listen_socket(char *port, int backlog);
int accept_connection(int listenfd);
ssize_t _send(int newfd, void *buf, size_t nbytes);
ssize_t _recv(int newfd, void *buf, size_t nbytes);
void wserve(char *port, int backlog);


// #####################
// # RUNTIME FUNCTIONS #
// #####################

int main(void)
{
    wserve("6666", 100);
    return 0;
}


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

typedef struct HTTP_HEADER
{
    char *key;
    char *val;
}
HTTP_HEADER;

typedef struct HTTP_HEAD
{
    char *start_line;
    int num_of_headers;
    HTTP_HEADER *headers;
}
HTTP_HEAD;

int count_substring(char *str, char *substr)
{
    int len_str = strlen(str);
    int len_substr = strlen(substr);
    int i = len_substr;
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

char *trim(char *str)
{
    int len = strlen(str);
    char *tstr = malloc(len * sizeof(char));
    int j = 0;

    for (int i = 0; i < len; i++)
    {
        if (!isspace(str[i]))
        {
            tstr[j] = str[i];
        }
    }

    return tstr;
}

char *get_http_body(char *http_msg)
{
    char *http_body = strstr(http_msg, "/r/n/r/n");
    if (http_body != NULL)
    {
        http_body += 4 * sizeof(char);
    }
    return http_body;
}

HTTP_HEADER parse_http_header_line(char *line)
{
    char *saveptr;
    char *val;
    HTTP_HEADER hh;

    hh.key = strtok_r(line, ":", &saveptr);
    val = strtok_r(NULL, ":", &saveptr);
    hh.val = trim(val);

    return hh;
}

HTTP_HEAD parse_headers(char *http_msg)
{
    HTTP_HEAD http_head;
    char *saveptr;
    char *head;
    char *line;
    char *token;

    head = strtok_r(http_msg, "/r/n/r/n", &saveptr);
    http_head.num_of_headers = count_substring(head, "/r/n") - 1;
    http_head.start_line = strtok_r(head, "/r/n", &saveptr);
    http_head.headers = malloc(http_head.num_of_headers * sizeof(HTTP_HEADER));
    line = strtok_r(NULL, "/r/n", &saveptr);

    for (int i = 0; i < http_head.num_of_headers; i++)
    {
        http_head.headers[i] = parse_http_header_line(line);
        line = strtok_r(NULL, "/r/n", &saveptr);
    }

    return http_head;
}

void print_http_head(HTTP_HEAD http_head)
{
    printf("\nHTTP HEAD:\n");
    printf("%s\n", http_head.start_line);
    for(int i = 0; i < http_head.num_of_headers; i++)
    {
        printf("%s: %s\n", http_head.headers[i].key, http_head.headers[i].val);
    }
}
