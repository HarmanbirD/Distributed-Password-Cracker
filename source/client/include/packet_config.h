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

static uint8_t                     first_empty_packet;
static uint8_t                     first_unacked_packet;
static uint8_t                     is_window_available;
static uint8_t                     window_size;

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

typedef struct sent_packet
{
    struct packet   pt;
    uint32_t        expected_ack_number;
    uint8_t         is_packet_full;
} sent_packet;

int                 create_window(struct sent_packet **window, uint8_t window_size, struct fsm_error *err);
int                 window_empty(struct sent_packet *window);
int                 is_window_empty(void);
int                 first_packet_ring_buffer(struct sent_packet *window);
int                 first_unacked_ring_buffer(struct sent_packet *window);
int                 send_packet(int sockfd, struct sockaddr_storage *addr,
                                struct sent_packet *window, struct packet *pt,
                                FILE *fp, struct fsm_error *err);
int                 add_packet_to_window(struct sent_packet *window, struct packet *pt);
int                 receive_packet(int sockfd, struct sent_packet *window,
                                    struct packet *pt, FILE *fp, struct fsm_error *err);
int                 remove_packet_from_window(struct sent_packet *window, struct packet *pt);
int                 remove_single_packet(struct sent_packet *window, struct packet *pt);
int                 remove_cumulative_packets(struct sent_packet *window, struct packet *pt);
int                 remove_lesser_index(struct sent_packet *window, uint8_t index);
int                 remove_greater_index(struct sent_packet *window, uint8_t index);
uint32_t            get_expected_ack_number(struct sent_packet *window);
uint32_t            create_second_handshake_seq_number(void);
uint32_t            create_ack_number(uint32_t previous_ack_number, uint32_t data_size);
uint32_t            create_sequence_number(uint32_t prev_seq_number, uint32_t data_size);
uint32_t            previous_seq_number(struct sent_packet *window);
uint32_t            previous_ack_number(struct sent_packet *window);
uint32_t            previous_data_size(struct sent_packet *window);
int                 check_ack_number(uint32_t expected_ack_number, uint32_t ack_number, struct sent_packet *window);
int                 check_ack_number_equal(uint32_t expected_ack_number, uint32_t ack_number);
int                 check_ack_number_greater(uint32_t expected_ack_number, uint32_t ack_number, struct sent_packet *window);
int                 get_ack_number_index(uint32_t ack_number, struct sent_packet *window);
int                 previous_index(struct sent_packet *window);
int                 write_stats_to_file(FILE *fp, const struct packet *pt);


#endif //CLIENT_PACKET_CONFIG_H
