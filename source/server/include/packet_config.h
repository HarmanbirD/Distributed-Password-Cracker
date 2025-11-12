#ifndef CLIENT_PACKET_CONFIG_H
#define CLIENT_PACKET_CONFIG_H

#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <printf.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include "protocol.h"
#include "server_config.h"

#define DATA_SIZE 512

typedef struct header
{
    uint32_t                    seq_number;
    uint32_t                    ack_number;
    uint8_t                     flags;
    uint8_t                     window_size;
    uint16_t                    checksum;
} header;

typedef struct packet
{
    struct header   hd;
    char            data[DATA_SIZE];
} packet;

int                 send_packet(int sockfd, struct sockaddr_storage *addr,
                                struct packet *pt, FILE *fp, struct fsm_error *err);
int                 receive_packet(int sockfd, struct packet *temp_packet, FILE *fp,
                                    struct fsm_error *err);
uint32_t            create_second_handshake_seq_number(void);
uint32_t            create_ack_number(uint32_t previous_ack_number, uint32_t data_size);
uint32_t            create_sequence_number(uint32_t prev_seq_number, uint32_t data_size);
int                 check_seq_number(uint32_t seq_number, uint32_t expected_seq_number);
uint32_t            update_expected_seq_number(uint32_t seq_number, uint32_t data_size);
int                 check_if_equal(uint32_t seq_number, uint32_t expected_seq_number);
int                 check_if_less(uint32_t seq_number, uint32_t expected_seq_number);
int                 write_stats_to_file(FILE *fp, const struct packet *pt);

#endif //CLIENT_PACKET_CONFIG_H
