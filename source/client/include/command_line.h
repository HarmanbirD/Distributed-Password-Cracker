#ifndef CLIENT_COMMAND_LINE_H
#define CLIENT_COMMAND_LINE_H

#include <netinet/in.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <inttypes.h>
#include "fsm.h"

int                 parse_arguments(int argc, char *argv[], char **server_addr,
                                    char **client_addr, char **server_port_str,
                                    char **client_port_str, uint8_t *window_size,
                                    struct fsm_error *err);
int                 handle_arguments(const char *binary_name, const char *server_addr,
                                     const char *client_addr, const char *server_port_str,
                                     const char *client_port_str, in_port_t *server_port,
                                     in_port_t *client_port, uint8_t window_size,
                                     struct fsm_error *err);
void                usage(const char *program_name);
int                 parse_in_port_t(const char *binary_name, const char *str, in_port_t *port, struct fsm_error *err);
int                 convert_to_int(const char *binary_name, char *string, uint8_t *value, struct fsm_error *err);

#endif //CLIENT_COMMAND_LINE_H
