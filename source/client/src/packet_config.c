#include <netinet/in.h>
#include "packet_config.h"

int create_window(struct sent_packet **window, uint8_t cmd_line_window_size, struct fsm_error *err)
{
    window_size     = cmd_line_window_size;
    *window         = (struct sent_packet *) malloc(sizeof(struct sent_packet) * window_size + 1);

    if (window == NULL)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    for (int i = 0; i < window_size; i++)
    {
        (*window)[i].is_packet_full = 0;
    }

    first_empty_packet      = 0;
    first_unacked_packet    = 0;
    is_window_available     = TRUE;

    return 0;
}

int window_empty(struct sent_packet *window)
{
    if (!window[first_empty_packet].is_packet_full)
    {
        is_window_available = TRUE;
        return 1;
    }

    is_window_available = FALSE;
    return 0;
}

int is_window_empty(void)
{
    return is_window_available;
}

int first_packet_ring_buffer(struct sent_packet *window)
{
    if (!window[first_empty_packet].is_packet_full)
    {
        return 1;
    }

    if (first_empty_packet + 1 >= window_size)
    {
        if (!window[0].is_packet_full)
        {
            first_empty_packet = 0;
            return 1;
        }

        first_empty_packet = first_unacked_packet;
        return 0;
    }

    if (!window[first_empty_packet + 1].is_packet_full)
    {
        first_empty_packet++;
        return 1;
    }
    else
    {
        first_empty_packet = first_unacked_packet;
        return 0;
    }
}

int first_unacked_ring_buffer(struct sent_packet *window)
{
    if (window[first_unacked_packet].is_packet_full)
    {
        return 1;
    }

    if (first_unacked_packet + 1 >= window_size)
    {
        if (window[0].is_packet_full)
        {
            first_unacked_packet = 0;
            return 1;
        }

        first_unacked_packet = first_empty_packet;
        return 0;
    }

    if (window[first_unacked_packet + 1].is_packet_full)
    {
        first_unacked_packet++;
        return 1;
    }
    else
    {
        first_unacked_packet = first_empty_packet;
        return 0;
    }
}


int send_packet(int sockfd, struct sockaddr_storage *addr, struct sent_packet *window,
                struct packet *pt, FILE *fp, struct fsm_error *err)
{
    ssize_t result;

    result = sendto(sockfd, pt, sizeof(*pt), 0, (struct sockaddr *) addr,
                    size_of_address(addr));

//    printf("\n\nSENDING:\n");
//    printf("seq number: %u\n", pt->hd.seq_number);
//    printf("flags: %u\n", pt->hd.flags);
//    printf("checksum: %u\n", pt->hd.checksum);
//    printf("data: %s\n\n\n\n", pt->data);

    if (result < 0)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    write_stats_to_file(fp, pt);

    return 0;
}

int add_packet_to_window(struct sent_packet *window, struct packet *pt)
{
//    gettimeofday(&pt->hd.tv, NULL);
    window[first_empty_packet].pt                           = *pt;

    if (pt->hd.flags == ACK)
    {
        window[first_empty_packet].is_packet_full           = FALSE;
        first_empty_packet++;
        first_unacked_ring_buffer(window);
    }
    else
    {
        window[first_empty_packet].is_packet_full           = TRUE;
    }

    if (pt->hd.flags == SYN)
    {
        window[first_empty_packet].expected_ack_number      = pt -> hd.seq_number + 1;
    }
    else if (pt->hd.flags == SYNACK)
    {
        window[first_empty_packet].expected_ack_number      = pt -> hd.seq_number + 1;
        window[first_empty_packet].pt.hd.seq_number         = pt -> hd.seq_number + 1;
    }
    else
    {
        window[first_empty_packet].expected_ack_number      = pt->hd.seq_number +
                                                                strlen(pt->data);
    }
    first_packet_ring_buffer(window);
    window_empty(window);

    return 0;
}

int receive_packet(int sockfd, struct sent_packet *window, struct packet *pt, FILE *fp,
                    struct fsm_error *err)
{
    struct sockaddr_storage     client_addr;
    socklen_t                   client_addr_len;
    struct packet               temp_pt;
    ssize_t                     result;

    client_addr_len     = sizeof(client_addr);
    result              = recvfrom(sockfd, &temp_pt, sizeof(temp_pt), 0, (struct sockaddr *) &client_addr, &client_addr_len);

    if (result == -1)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    *pt = temp_pt;

//    printf("\n\nRECEIVED:\n");
//    printf("seq number: %u\n", pt->hd.seq_number);
//    printf("ack number: %u\n", pt->hd.ack_number);
//    printf("flags: %u\n", pt->hd.flags);

    write_stats_to_file(fp, pt);
    window_empty(window);

    return 0;
}

int remove_packet_from_window(struct sent_packet *window, struct packet *pt)
{
    if (window[first_unacked_packet].expected_ack_number == pt->hd.ack_number)
    {
        remove_single_packet(window, pt);

        return 0;
    }
    else if (window[first_unacked_packet].expected_ack_number < pt->hd.ack_number)
    {
        remove_cumulative_packets(window, pt);

        return 0;
    }

    return -1;
}

int remove_single_packet(struct sent_packet *window, struct packet *pt)
{
    if (window[first_unacked_packet].expected_ack_number == pt->hd.ack_number)
    {
        printf("removing packet with expected ack number: %u at index: %u\n",
               window[first_unacked_packet].expected_ack_number, first_unacked_packet);
        window[first_unacked_packet].is_packet_full = FALSE;
        first_unacked_ring_buffer(window);

        return 1;
    }

    return 0;
}

int remove_cumulative_packets(struct sent_packet *window, struct packet *pt)
{
    uint8_t index;

    index = get_ack_number_index(pt->hd.ack_number, window);

    if (index > first_unacked_packet)
    {
        remove_greater_index(window, index);
    }
    else if (index < first_unacked_packet)
    {
        remove_lesser_index(window, index);
    }
    else
    {
        return -1;
    }

    first_unacked_packet = index;
    first_unacked_ring_buffer(window);

    return 0;
}

int remove_lesser_index(struct sent_packet *window, uint8_t index)
{
    for (uint8_t i = 0; i <= index; i++)
    {
        printf("removing packet with expected ack number: %u at index: %u\n", window[i].expected_ack_number, i);
        window[i].is_packet_full = FALSE;
    }

    remove_greater_index(window, window_size - 1);

    return 0;
}
int remove_greater_index(struct sent_packet *window, uint8_t index)
{
   for (uint8_t i = first_unacked_packet; i <= index; i++)
   {
       printf("removing packet with expected ack number: %u at index: %u\n", window[i].expected_ack_number, i);
       window[i].is_packet_full = FALSE;
   }

   return 0;
}

uint32_t get_expected_ack_number(struct sent_packet *window)
{
   return window[first_unacked_packet].expected_ack_number;
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

uint32_t previous_seq_number(struct sent_packet *window)
{
    if (first_empty_packet == 0)
    {
        return window[window_size - 1].pt.hd.seq_number;
    }

    return window[first_empty_packet - 1].pt.hd.seq_number;
}

uint32_t previous_data_size(struct sent_packet *window)
{
    if (first_empty_packet == 0)
    {
        return strlen(window[window_size - 1].pt.data);
    }

    return strlen(window[first_empty_packet - 1].pt.data);
}

uint32_t previous_ack_number(struct sent_packet *window)
{
    if (first_empty_packet == 0)
    {
        return window[window_size - 1].pt.hd.ack_number;
    }

    return window[first_empty_packet - 1].pt.hd.ack_number;
}

int check_ack_number(uint32_t expected_ack_number, uint32_t ack_number, struct sent_packet *window)
{
    if(window[first_unacked_packet].is_packet_full == FALSE)
    {
        return FALSE;
    }

    return  check_ack_number_equal(expected_ack_number, ack_number) ||
            check_ack_number_greater(expected_ack_number, ack_number, window);
}

int check_ack_number_equal(uint32_t expected_ack_number, uint32_t ack_number)
{
    return expected_ack_number == ack_number ? TRUE : FALSE;
}

int check_ack_number_greater(uint32_t expected_ack_number, uint32_t ack_number, struct sent_packet *window)
{
    if (ack_number > expected_ack_number)
    {
        for (int i = 0; i < window_size; i++)
        {
            if (window[i].expected_ack_number == ack_number)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

int get_ack_number_index(uint32_t ack_number, struct sent_packet *window)
{
    for (int i = 0; i < window_size; i++)
    {
        if (window[i].expected_ack_number == ack_number)
        {
            return i;
        }
    }

    return -1;
}

int previous_index(struct sent_packet *window)
{
    if (first_empty_packet == 0)
    {
        return window_size - 1;
    }

    return first_empty_packet - 1;
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
