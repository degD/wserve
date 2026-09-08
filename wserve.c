
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
int create_listen_socket(char *port, int backlog);
int accept_connection(int listenfd);
int _send(int newfd, void *buf, size_t nbytes);
int _recv(int newfd, void *buf, size_t nbytes);
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

void sigchld_handler(int s)
{
    int saved_errno = errno;
    (void) s;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

int install_sigchld_handler(void) 
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

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

int _send(int newfd, void *buf, size_t nbytes)
{
    size_t bytes_sent = 0;        
    size_t bytes_left = nbytes;
    size_t n = 0;
    void *p = buf;

    while(bytes_sent < nbytes) {
        n = send(newfd, p, bytes_left, 0);
        if (n < 0) break;
        bytes_sent += n;
        bytes_left -= n;
        p += n;
    }

    return n == -1 ? -1 : bytes_sent;
} 

int _recv(int newfd, void *buf, size_t nbytes)
{
    size_t bytes_recv = 0;        
    size_t bytes_left = nbytes;
    size_t n = 0;
    void *p = buf;

    while(bytes_recv < nbytes) {
        n = recv(newfd, p, bytes_left, 0);
        if (n <= 0) break;
        bytes_recv += n;
        bytes_left -= n;
        p += n;
    }

    return n == -1 ? -1 : bytes_recv;
} 

void wserve(char *port, int backlog)
{
    char msg[] = "Hello, world!\n"; 
    size_t len = strlen(msg);
    int listenfd = create_listen_socket(port, backlog);

    install_sigchld_handler();
    while (1) 
    {
        int newfd = accept_connection(listenfd);
        if (newfd == -1) continue;

        if (!fork()) 
        {
            close(listenfd);
            _send(newfd, msg, len);
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
}