
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

int count_substring(char *str, char *substr);
char *split_str(char *str, char *substr, char **saveptr);
char *trim(char *str);
char *get_http_body(char *http_msg, size_t len);
HTTP_HEADER parse_http_header_line(char *line);
HTTP_HEAD parse_head(char *http_msg);
void print_http_head(HTTP_HEAD http_head);


#endif