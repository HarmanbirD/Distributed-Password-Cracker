#ifndef CLIENT_PROTOCOL_H
#define CLIENT_PROTOCOL_H

#include "packet_config.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

enum bools
{
    FALSE = 0,
    TRUE = 1
};

enum flags
{
    SYN = 1,
    ACK = 2,
    PSH = 4,
    FIN = 8,
    URG = 16,
    RST = 32,
    SYNACK = SYN + ACK,
    PSHACK = PSH + ACK,
    FINACK = FIN + ACK,
    RSTACK = RST + ACK
};

enum next_state_for_packet
{
    SEND_HANDSHAKE_ACK,
    SEND_ACK,
    ESTABLISH_HANDSHAKE,
    RECV_ACK,
    END_CONNECTION,
    RECV_RST,
    UNKNOWN_FLAG
};

int                 read_flags(uint8_t flags);
int                 read_received_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 send_syn_packet(int sockfd, struct sockaddr_storage *addr, FILE *fp, struct fsm_error *err);
int                 send_syn_ack_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 create_syn_ack_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 finish_handshake_ack(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 send_handshake_ack_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 send_data_packet(int sockfd, struct sockaddr_storage *addr, char *data, FILE *fp, struct fsm_error *err);
int                 send_data_ack_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 recv_ack_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 recv_termination_request(int sockfd, struct sockaddr_storage *addr, struct packet *pt, FILE *fp, struct fsm_error *err);
int                 initiate_termination(int sockfd, struct sockaddr_storage *addr, FILE *fp, struct fsm_error *err);
int                 create_flags(uint8_t flags);
int                 calculate_checksum(uint16_t *checksum, const char *data, size_t length);
unsigned char       checksum_one(const char *data, size_t length);
unsigned char       checksum_two(const char *data, size_t length);
int                 compare_checksum(uint16_t checksum, const char *data, size_t length);

#endif //CLIENT_PROTOCOL_H
