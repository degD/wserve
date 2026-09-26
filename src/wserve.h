
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
int set_socket_timeouts(int fd);
int create_listen_socket(char *port, int backlog);
int accept_connection(int listenfd);
ssize_t _send(int newfd, void *buf, size_t nbytes);
ssize_t _recv(int newfd, void *buf, size_t nbytes);


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
    HTTP_REQUEST_LINE *hrl;
    int num_of_headers;
    HTTP_HEADER_FIELD **headers;
    char *body;
    size_t bodylen;
}
HTTP_REQUEST;

typedef struct HTTP_RESPONSE
{
    HTTP_STATUS_LINE *hsl;
    int num_of_headers;
    HTTP_HEADER_FIELD **headers;
    char *body;
    size_t bodylen;
}
HTTP_RESPONSE;

int count_substring(char *str, char *substr);
char *split_str(char *str, char *substr, char **saveptr);
char *trim(char *str);
void toupper_str(char *str);
char *get_http_body(char *http_msg, size_t len);
HTTP_HEADER_FIELD *parse_http_header_line(char *line);
HTTP_REQUEST_LINE *parse_http_request_line(char *start_line);
HTTP_STATUS_LINE *parse_http_status_line(char *start_line);
HTTP_REQUEST *parse_http_request(char *http_msg, size_t http_msg_len);
HTTP_RESPONSE *parse_http_response(char *http_msg, size_t http_msg_len);
ssize_t calc_http_response_size(HTTP_RESPONSE *hr);
ssize_t tostring_http_response(HTTP_RESPONSE *hr, char **msg);
HTTP_RESPONSE *init_http_response(
    char *status_code,
    char *resp_text,
    char *body, size_t bodylen,
    char **headers, int nheaders
);


// ###############
// # HTTP SERVER #
// ###############

typedef struct SERVER_SETTINGS
{
    char *root;
    char *port;
    int backlog;
    size_t max_recv_size;
    size_t total_req_size;
}
SERVER_SETTINGS;

void init_server_settings(
    char *root,
    char *port,
    int backlog,
    size_t max_recv_size,
    size_t total_req_size
);
ssize_t http_recv(
    int newfd,
    char **head,
    char **body,
    size_t *headlen,
    size_t *bodylen
);
void wserve_http();
HTTP_RESPONSE *process_http_requests(HTTP_REQUEST *hr, char *root);


// ##################
// # STATIC ROUTING #
// ##################

int validate_target_path(char *target);
int read_static_file(char *root, char *target, char **buf, char **extension);
char *get_extension(char *path);
char *mime_type(char *extension);


#endif
