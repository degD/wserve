
#ifndef _WSERVE_H_
#define _WSERVE_H_

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


// ########################
// # SERVER TCP FUNCTIONS #
// ########################

void sigchld_handler(int s);
int install_sigchld_handler(void);
int create_listen_socket(char *port, int backlog);
int accept_connection(int listenfd);
ssize_t _send(int newfd, void *buf, size_t nbytes);
ssize_t _recv(int newfd, void *buf, size_t nbytes);
void wserve(char *port, int backlog);


// #######################
// # HTTP HEADERS PARSER #
// #######################

typedef struct HTTP_REQUEST_LINE
{
    char *method;
    char *target;
    char *http_version;
}
HTTP_REQUEST_LINE;

typedef struct HTTP_STATUS_LINE
{
    char *http_version;
    char *status_code;
    char *response_text;
}
HTTP_STATUS_LINE;

typedef struct HTTP_HEADER_FIELD
{
    char *key;
    char *val;
}
HTTP_HEADER_FIELD;

typedef struct HTTP_REQUEST
{
    HTTP_REQUEST_LINE hrl;
    int num_of_headers;
    HTTP_HEADER_FIELD *headers;
    char *body;
}
HTTP_REQUEST;

typedef struct HTTP_RESPONSE
{
    HTTP_STATUS_LINE hrl;
    int num_of_headers;
    HTTP_HEADER_FIELD *headers;
    char *body;
}
HTTP_RESPONSE;

int count_substring(char *str, char *substr);
char *split_str(char *str, char *substr, char **saveptr);
char *trim(char *str);
char *get_http_body(char *http_msg, size_t len);
HTTP_HEADER parse_http_header_line(char *line);
HTTP_HEAD parse_head(char *http_msg);
void print_http_head(HTTP_HEAD http_head);


// ###############
// # HTTP SERVER #
// ###############

ssize_t http_recv(
    int newfd,
    char **head,
    char **body,
    size_t *headlen,
    size_t *bodylen,
    size_t maxrecvsize
);
void http_send_status(int newfd);
void wserve_http(char *port, int backlog, size_t bufsize);


#endif
