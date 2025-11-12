#ifndef CLIENT_SERVER_CONFIG_H
#define CLIENT_SERVER_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netdb.h>
#include "fsm.h"
#include "protocol.h"

int             socket_create(int domain, int type, int protocol, struct fsm_error *err);
int             start_listening(int sockfd, int backlog, struct fsm_error *err);
int             socket_accept_connection(int sockfd, struct fsm_error *err);
int             socket_close(int sockfd, struct fsm_error *err);
int             read_keyboard(char **buffer);
int             socket_bind(int sockfd, struct sockaddr_storage *addr, struct fsm_error *err);
int             convert_address(const char *address, struct sockaddr_storage *addr,
                                in_port_t port, struct fsm_error *err);
socklen_t       size_of_address(struct sockaddr_storage *addr);
int             get_sockaddr_info(struct sockaddr_storage *addr, char **ip_address, char **port, struct fsm_error *err);
void            *safe_malloc(uint32_t size, struct fsm_error *err);
int             send_stats_gui(int sockfd, int stat);

#endif //CLIENT_SERVER_CONFIG_H
