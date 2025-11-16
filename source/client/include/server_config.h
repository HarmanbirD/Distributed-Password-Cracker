#ifndef CLIENT_SERVER_CONFIG_H
#define CLIENT_SERVER_CONFIG_H

#include "fsm.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

int       socket_create(int domain, int type, int protocol, struct fsm_error *err);
int       start_listening(int sockfd, int backlog, struct fsm_error *err);
int       socket_accept_connection(int sockfd, struct fsm_error *err);
int       socket_close(int sockfd, struct fsm_error *err);
int       socket_bind(int sockfd, struct sockaddr_storage *addr, struct fsm_error *err);
int       convert_address(const char *address, struct sockaddr_storage *addr,
                          in_port_t port, struct fsm_error *err);
int       socket_connect(int sockfd, struct sockaddr_storage *addr, in_port_t port, struct fsm_error *err);
int       receive_hash(int sockfd, worker_state *ws, struct fsm_error *err);
int       wait_for_work(int sockfd, worker_state *ws, struct fsm_error *err);
int       send_checkpoint(worker_state *ws, uint64_t idx);
int       send_done(int sockfd, struct fsm_error *err);
int       send_found(int sockfd, const char *password);
socklen_t size_of_address(struct sockaddr_storage *addr);
int       get_sockaddr_info(struct sockaddr_storage *addr, char **ip_address, char **port, struct fsm_error *err);

#endif // CLIENT_SERVER_CONFIG_H
