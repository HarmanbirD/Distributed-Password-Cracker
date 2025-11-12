#include <netinet/in.h>
#include "packet_config.h"

int send_packet(int sockfd, struct sockaddr_storage *addr, struct packet *pt,
        FILE *fp, struct fsm_error *err)
{
    ssize_t result;

    result = sendto(sockfd, pt, sizeof(*pt), 0, (struct sockaddr *) addr,
                    size_of_address(addr));

    if (result == -1)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    printf("SENDING:\n");
//    printf("bytes: %zd\n", result);
//    printf("seq number: %u\n", pt->hd.seq_number);
    printf("ack number: %u\n", pt->hd.ack_number);
//    printf("window number: %u\n", pt->hd.window_size);
//    printf("flags: %u\n", pt->hd.flags);
//    printf("time: %ld\n", pt->hd.tv.tv_sec);
//    printf("data: %s\n\n", pt->data);

    write_stats_to_file(fp, pt);

    return 0;
}

int receive_packet(int sockfd, struct packet *temp_packet, FILE *fp, struct fsm_error *err)
{
    struct sockaddr_storage     client_addr;
    socklen_t                   client_addr_len;
    struct packet               pt;
    ssize_t                     result;

    client_addr_len = sizeof(client_addr);
    result = recvfrom(sockfd, &pt, sizeof(pt), 0, (struct sockaddr *) &client_addr, &client_addr_len);

    if (result == -1)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    printf("RECEIVED:\n");
//    printf("bytes: %zd\n", result);
    printf("seq number: %u ", pt.hd.seq_number);
//    printf("ack number: %u\n", pt.hd.ack_number);
//    printf("flags: %u\n", pt.hd.flags);
    printf("data: %s\n", pt.data);

    *temp_packet = pt;

    write_stats_to_file(fp, &pt);

    return 0;
}

uint32_t create_second_handshake_seq_number(void)
{
    return 100;
}

uint32_t create_sequence_number(uint32_t prev_seq_number, uint32_t data_size)
{
    return prev_seq_number + data_size;
}

uint32_t create_ack_number(uint32_t recv_seq_number, uint32_t data_size)
{
    return recv_seq_number + data_size;
}

int check_seq_number(uint32_t seq_number, uint32_t expected_seq_number)
{
    printf("expected: %u got: %u\n", expected_seq_number, seq_number);
    return check_if_equal(seq_number, expected_seq_number) || check_if_less(seq_number, expected_seq_number);
}

int check_if_equal(uint32_t seq_number, uint32_t expected_seq_number)
{
    return seq_number == expected_seq_number ? TRUE : FALSE;
}

int check_if_less(uint32_t seq_number, uint32_t expected_seq_number)
{
    return seq_number < expected_seq_number ? TRUE : FALSE;
}

uint32_t update_expected_seq_number(uint32_t seq_number, uint32_t data_size)
{
    printf("expected: %u\n", seq_number + data_size);
    return seq_number + data_size;
}

int write_stats_to_file(FILE *fp, const struct packet *pt)
{
    fprintf(fp, "%u,%u,%u,%u,%u,%s\n",
            pt -> hd.seq_number,
            pt -> hd.ack_number,
            pt -> hd.flags,
            pt -> hd.window_size,
            pt -> hd.checksum,
            pt -> data);
    fflush(fp);

    return 0;
}
