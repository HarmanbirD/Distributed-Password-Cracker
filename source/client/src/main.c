#include "fsm.h"
#include "protocol.h"
#include "server_config.h"
#include "command_line.h"
#include "linked_list.h"
#include <pthread.h>

#define TIMER_TIME 1

enum main_application_states
{
    STATE_PARSE_ARGUMENTS = FSM_USER_START,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_CREATE_SOCKET,
    STATE_BIND_SOCKET,
    STATE_LISTEN,
    STATE_CREATE_GUI_THREAD,
    STATE_CREATE_WINDOW,
    STATE_START_HANDSHAKE,
    STATE_CREATE_HANDSHAKE_TIMER,
    STATE_WAIT_FOR_SYN_ACK,
    STATE_SEND_HANDSHAKE_ACK,
    STATE_CREATE_RECV_THREAD,
    STATE_READ_FROM_KEYBOARD,
    STATE_CHECK_WINDOW,
    STATE_ADD_PACKET_TO_BUFFER,
    STATE_ADD_PACKET_TO_WINDOW,
    STATE_CHECK_WINDOW_THREAD,
    STATE_SEND_MESSAGE,
    STATE_CREATE_TIMER_THREAD,
    STATE_CLEANUP,
    STATE_ERROR
};

enum receiving_thread_application_states
{
    STATE_WAIT = FSM_USER_START,
    STATE_CHECK_ACK_NUMBER,
    STATE_REMOVE_FROM_WINDOW,
    STATE_SEND_PACKET,
    STATE_TERMINATION
};

enum gui_stats
{
    SENT_PACKET,
    RECEIVED_PACKET,
    RECEIVED_ACK,
    RESENT_PACKET,
    DROPPED_CLIENT_PACKET,
    DELAYED_CLIENT_PACKET,
    DROPPED_SERVER_PACKET,
    DELAYED_SERVER_PACKET,
    CORRUPTED_DATA
};

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int handle_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int convert_address_handler(struct fsm_context *context, struct fsm_error *err);
static int create_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int bind_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int listen_handler(struct fsm_context *context, struct fsm_error *err);
static int create_gui_thread_handler(struct fsm_context *context, struct fsm_error *err);
static int create_window_handler(struct fsm_context *context, struct fsm_error *err);
static int start_handshake_handler(struct fsm_context *context, struct fsm_error *err);
static int create_handshake_timer_handler(struct fsm_context *context, struct fsm_error *err);
static int wait_for_syn_ack_handler(struct fsm_context *context, struct fsm_error *err);
static int send_handshake_ack_handler(struct fsm_context *context, struct fsm_error *err);
static int create_recv_thread_handler(struct fsm_context *context, struct fsm_error *err);
static int read_from_keyboard_handler(struct fsm_context *context, struct fsm_error *err);
static int check_window_handler(struct fsm_context *context, struct fsm_error *err);
static int add_packet_to_buffer_handler(struct fsm_context *context, struct fsm_error *err);
static int add_packet_to_window_handler(struct fsm_context *context, struct fsm_error *err);
static int check_window_thread_handler(struct fsm_context *context, struct fsm_error *err);
static int send_message_handler(struct fsm_context *context, struct fsm_error *err);
static int create_timer_thread_handler(struct fsm_context *context, struct fsm_error *err);
static int cleanup_handler(struct fsm_context *context, struct fsm_error *err);
static int error_handler(struct fsm_context *context, struct fsm_error *err);

static int wait_handler(struct fsm_context *context, struct fsm_error *err);
static int check_ack_number_handler(struct fsm_context *context, struct fsm_error *err);
static int remove_packet_from_window_handler(struct fsm_context *context, struct fsm_error *err);
static int send_packet_handler(struct fsm_context *context, struct fsm_error *err);
static int termination_handler(struct fsm_context *context, struct fsm_error *err);

static void                     sigint_handler(int signum);
static int                      setup_signal_handler(struct fsm_error *err);
int                             create_file(const char *filepath, FILE **fp, struct fsm_error *err);

static volatile sig_atomic_t exit_flag = 0;

void *init_recv_function(void *ptr);
void *init_timer_function(void *ptr);
void *init_window_checker_function(void *ptr);
void *init_gui_function(void *ptr);

typedef struct arguments
{
    int                     sockfd, num_of_threads, is_buffered;
    int                     client_gui_fd, connected_gui_fd, is_connected_gui;
    uint8_t                 window_size;
    char                    *server_addr, *client_addr, *server_port_str, *client_port_str;
    in_port_t               server_port, client_port;
    struct sockaddr_storage server_addr_struct, client_addr_struct, gui_addr_struct;
    struct sent_packet      *window;
    pthread_t               recv_thread, accept_gui_thread, *thread_pool;
    struct packet           temp_packet, temp_message;
    char                    *temp_buffer;
    struct node             *head;
    FILE                    *sent_data, *received_data;
} arguments;


pthread_mutex_t num_of_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
            .head           = NULL,
            .is_buffered    = 0,
            .window_size    = 101
    };
    struct fsm_context context = {
            .argc           = argc,
            .argv           = argv,
            .args           = &args
    };

    static struct fsm_transition transitions[] = {
            {FSM_INIT,                   STATE_PARSE_ARGUMENTS,      parse_arguments_handler},
            {STATE_PARSE_ARGUMENTS,      STATE_HANDLE_ARGUMENTS,     handle_arguments_handler},
            {STATE_HANDLE_ARGUMENTS,     STATE_CONVERT_ADDRESS,      convert_address_handler},
            {STATE_CONVERT_ADDRESS,      STATE_CREATE_SOCKET,        create_socket_handler},
            {STATE_CREATE_SOCKET,        STATE_BIND_SOCKET,          bind_socket_handler},
            {STATE_BIND_SOCKET,         STATE_LISTEN,                 listen_handler},
            {STATE_LISTEN,         STATE_CREATE_GUI_THREAD,                 create_gui_thread_handler},
            {STATE_CREATE_GUI_THREAD,         STATE_CREATE_WINDOW,                 create_window_handler},
            {STATE_CREATE_WINDOW,        STATE_START_HANDSHAKE,      start_handshake_handler},
            {STATE_START_HANDSHAKE,      STATE_CREATE_HANDSHAKE_TIMER,   create_handshake_timer_handler},
            {STATE_CREATE_HANDSHAKE_TIMER,      STATE_WAIT_FOR_SYN_ACK,   wait_for_syn_ack_handler},
            {STATE_WAIT_FOR_SYN_ACK,      STATE_SEND_HANDSHAKE_ACK,   send_handshake_ack_handler},
            {STATE_WAIT_FOR_SYN_ACK,      STATE_CLEANUP,   cleanup_handler},
            {STATE_SEND_HANDSHAKE_ACK,      STATE_CREATE_RECV_THREAD,   create_recv_thread_handler},
            {STATE_CREATE_RECV_THREAD,   STATE_READ_FROM_KEYBOARD,   read_from_keyboard_handler},
            {STATE_READ_FROM_KEYBOARD,   STATE_CHECK_WINDOW,         check_window_handler},
            {STATE_CHECK_WINDOW,         STATE_ADD_PACKET_TO_WINDOW, add_packet_to_window_handler},
            {STATE_CHECK_WINDOW,         STATE_ADD_PACKET_TO_BUFFER, add_packet_to_buffer_handler},
            {STATE_ADD_PACKET_TO_BUFFER, STATE_READ_FROM_KEYBOARD,   read_from_keyboard_handler},
            {STATE_ADD_PACKET_TO_BUFFER, STATE_CHECK_WINDOW_THREAD,  check_window_thread_handler},
            {STATE_ADD_PACKET_TO_WINDOW, STATE_SEND_MESSAGE,         send_message_handler},
//            {STATE_ADD_PACKET_TO_WINDOW,    STATE_CHECK_WINDOW_THREAD,  check_window_thread_handler},
            {STATE_CHECK_WINDOW_THREAD,  STATE_READ_FROM_KEYBOARD,   read_from_keyboard_handler},
            {STATE_SEND_MESSAGE,         STATE_CREATE_TIMER_THREAD,  create_timer_thread_handler},
            {STATE_CREATE_TIMER_THREAD,  STATE_READ_FROM_KEYBOARD,   read_from_keyboard_handler},
            {STATE_READ_FROM_KEYBOARD,   STATE_CLEANUP,              cleanup_handler},
            {STATE_ERROR,                STATE_CLEANUP,              cleanup_handler},
            {STATE_PARSE_ARGUMENTS,      STATE_ERROR,                error_handler},
            {STATE_HANDLE_ARGUMENTS,     STATE_ERROR,                error_handler},
            {STATE_CONVERT_ADDRESS,      STATE_ERROR,                error_handler},
            {STATE_CREATE_SOCKET,        STATE_ERROR,                error_handler},
            {STATE_BIND_SOCKET,          STATE_ERROR,                error_handler},
            {STATE_CREATE_WINDOW,        STATE_ERROR,                error_handler},
            {STATE_CREATE_RECV_THREAD,   STATE_ERROR,                error_handler},
            {STATE_START_HANDSHAKE,      STATE_ERROR,                error_handler},
            {STATE_SEND_MESSAGE,         STATE_ERROR,                error_handler},
            {STATE_CLEANUP,              FSM_EXIT,                   NULL},
    };

    fsm_run(&context, &err, transitions);

    return 0;
}

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in parse arguments handler", "STATE_PARSE_ARGUMENTS");
    if (parse_arguments(ctx -> argc, ctx -> argv, &ctx -> args -> server_addr,
                        &ctx -> args -> client_addr, &ctx -> args -> server_port_str,
                        &ctx -> args -> client_port_str, &ctx -> args -> window_size,
                        err) != 0)

    {
        return STATE_ERROR;
    }

    return STATE_HANDLE_ARGUMENTS;
}
static int handle_arguments_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in handle arguments", "STATE_HANDLE_ARGUMENTS");
    if (handle_arguments(ctx -> argv[0], ctx -> args -> server_addr,
                         ctx -> args -> client_addr, ctx -> args -> server_port_str,
                         ctx -> args -> client_port_str, &ctx -> args -> server_port,
                         &ctx -> args -> client_port, ctx -> args -> window_size, err) != 0)
    {
        return STATE_ERROR;
    }

    if (create_file("../client_received_data.csv", &ctx -> args -> received_data, err) == -1)
    {
        return STATE_ERROR;
    }

    if (create_file("../client_sent_data.csv", &ctx -> args -> sent_data, err) == -1)
    {
        return STATE_ERROR;
    }

    return STATE_CONVERT_ADDRESS;
}

static int convert_address_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in convert server_addr", "STATE_CONVERT_ADDRESS");
    if (convert_address(ctx -> args -> server_addr, &ctx -> args -> server_addr_struct, ctx -> args -> server_port, err) != 0)
    {
        return STATE_ERROR;
    }

    if (convert_address(ctx -> args -> client_addr, &ctx -> args -> client_addr_struct, ctx -> args -> client_port, err) != 0)
    {
        return STATE_ERROR;
    }

    if (convert_address(ctx -> args -> client_addr, &ctx -> args -> gui_addr_struct, 61001, err) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_CREATE_SOCKET;
}

static int create_socket_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in create socket", "STATE_CREATE_SOCKET");
    ctx -> args -> sockfd = socket_create(ctx -> args -> client_addr_struct.ss_family, SOCK_DGRAM, 0, err);
    if (ctx -> args -> sockfd == -1)
    {
        return STATE_ERROR;
    }

    ctx -> args -> client_gui_fd = socket_create(ctx -> args -> client_addr_struct.ss_family, SOCK_STREAM, 0, err);
    if (ctx -> args -> client_gui_fd == -1)
    {
        return STATE_ERROR;
    }

    return STATE_BIND_SOCKET;
}

static int bind_socket_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in bind socket", "STATE_BIND_SOCKET");
    if (socket_bind(ctx -> args -> sockfd, &ctx -> args -> client_addr_struct, err))
    {
        return STATE_ERROR;
    }

    if (socket_bind(ctx -> args -> client_gui_fd, &ctx -> args -> gui_addr_struct, err))
    {
        return STATE_ERROR;
    }

    return STATE_LISTEN;
}

static int listen_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in start listening", "STATE_START_LISTENING");
    if (start_listening(ctx -> args -> client_gui_fd, SOMAXCONN, err))
    {
        return STATE_ERROR;
    }

    return STATE_CREATE_GUI_THREAD;
}

static int create_gui_thread_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context      *ctx;
    int                     result;
    ctx = context;
    SET_TRACE(context, "", "STATE_CREATE_GUI_THREAD");
    result = pthread_create(&ctx -> args -> accept_gui_thread, NULL, init_gui_function,
                            (void *) ctx);
    if (result < 0)
    {
        return STATE_ERROR;
    }

    return STATE_CREATE_WINDOW;
}


static int create_window_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in create window", "STATE_CREATE_WINDOW");
    if (create_window(&ctx -> args -> window, ctx -> args -> window_size, err) != 0)
    {
        return STATE_ERROR;
    }

    return STATE_START_HANDSHAKE;
}


static int start_handshake_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in connect socket", "STATE_START_HANDSHAKE");
    if (send_syn_packet(ctx -> args -> sockfd, &ctx -> args -> server_addr_struct,
                        ctx -> args -> window, ctx -> args -> sent_data, err))
    {
        return STATE_ERROR;
    }

    if (ctx -> args -> is_connected_gui)
    {
        send_stats_gui(ctx -> args -> connected_gui_fd, SENT_PACKET);
    }

    return STATE_CREATE_HANDSHAKE_TIMER;
}

static int create_handshake_timer_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    pthread_t *temp_thread_pool;
    ctx = context;
    SET_TRACE(context, "", "STATE_CREATE_HANDSHAKE_TIMER");

    temp_thread_pool = ctx -> args -> thread_pool;
    ctx -> args -> num_of_threads++;
    temp_thread_pool = (pthread_t *) realloc(temp_thread_pool,
                                             sizeof(pthread_t) * ctx -> args -> num_of_threads);
    if (temp_thread_pool == NULL)
    {
        return STATE_ERROR;
    }

    ctx -> args -> thread_pool = temp_thread_pool;

    pthread_create(&ctx->args->thread_pool[ctx->args->num_of_threads], NULL, init_timer_function, (void *) ctx);

    return STATE_WAIT_FOR_SYN_ACK;
}

static int wait_for_syn_ack_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    ssize_t result;
    SET_TRACE(context, "in connect socket", "STATE_WAIT_FOR_SYN_ACK");
    while (!exit_flag)
    {
        result = receive_packet(ctx->args->sockfd, ctx -> args -> window,
                                &ctx -> args -> temp_packet, ctx -> args -> received_data,
                                err);
        if (result == -1)
        {
            return STATE_ERROR;
        }

        if (ctx -> args -> is_connected_gui)
        {
            send_stats_gui(ctx -> args -> connected_gui_fd, RECEIVED_PACKET);
        }

        printf("Server packet with ack number: %u flag: %u received\n", ctx -> args -> temp_packet.hd.ack_number,
               ctx -> args -> temp_packet.hd.flags);

        if (ctx -> args -> temp_packet.hd.flags == SYNACK)
        {
            return STATE_SEND_HANDSHAKE_ACK;
        }
    }

    return STATE_CLEANUP;
}

static int send_handshake_ack_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in connect socket", "STATE_SEND_HANDSHAKE_ACK");
    read_received_packet(ctx -> args -> sockfd, &ctx -> args -> server_addr_struct,
                         ctx -> args -> window, &ctx -> args -> temp_packet,
                         ctx -> args -> sent_data, err);

    if (ctx -> args -> is_connected_gui)
    {
        send_stats_gui(ctx -> args -> connected_gui_fd, SENT_PACKET);
    }

    return STATE_CREATE_RECV_THREAD;
}

static int create_recv_thread_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context      *ctx;
    int                     result;
    ctx = context;
    SET_TRACE(context, "in create receive thread", "STATE_CREATE_RECV_THREAD");
    result = pthread_create(&ctx->args->recv_thread, NULL, init_recv_function,
                            (void *) ctx);
    if (result < 0)
    {
        return STATE_ERROR;
    }

    return STATE_READ_FROM_KEYBOARD;
}

static int read_from_keyboard_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_READ_FROM_KEYBOARD");
    while (!exit_flag)
    {
        if (read_keyboard(&ctx -> args -> temp_buffer) == -1)
        {
            exit_flag++;
            return STATE_CLEANUP;
        }
        return STATE_CHECK_WINDOW;
    }

    return STATE_CLEANUP;
}

static int check_window_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_CHECK_WINDOW");

    if (!is_window_empty() || ctx -> args -> is_buffered)
    {
        return STATE_ADD_PACKET_TO_BUFFER;
    }

    return STATE_ADD_PACKET_TO_WINDOW;
}

static int add_packet_to_buffer_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_ADD_PACKET_TO_BUFFER");

    if (ctx -> args -> head == NULL)
    {
        init_list(&ctx -> args -> head, ctx -> args -> temp_buffer);
        ctx -> args -> is_buffered++;
        return STATE_CHECK_WINDOW_THREAD;
    }
    push(ctx -> args -> head, ctx -> args -> temp_buffer);

    return STATE_READ_FROM_KEYBOARD;
}

static int add_packet_to_window_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_ADD_PACKET_TO_WINDOW");

    create_data_packet(&ctx -> args -> temp_message, ctx -> args -> window, ctx -> args -> temp_buffer);
//    add_packet_to_window(ctx -> args -> window, &ctx -> args -> temp_message);

    return STATE_SEND_MESSAGE;
}

static int check_window_thread_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_CHECK_WINDOW_THREAD");

    init_window_checker_function((void *) ctx);

    return STATE_READ_FROM_KEYBOARD;
}

static int send_message_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_SEND_PACKET");

    if (send_packet(ctx -> args -> sockfd, &ctx -> args -> server_addr_struct,
                    ctx -> args -> window, &ctx -> args -> temp_message,
                    ctx -> args -> sent_data, err) == -1)
    {
        return STATE_ERROR;
    }

    printf("Client packet with SEQ number: %u sent\n", ctx -> args -> temp_message.hd.seq_number);

    if (ctx -> args -> is_connected_gui)
    {
        send_stats_gui(ctx -> args -> connected_gui_fd, SENT_PACKET);
    }

    return STATE_CREATE_TIMER_THREAD;
}

static int create_timer_thread_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    pthread_t *temp_thread_pool;

    ctx = context;

    temp_thread_pool = ctx -> args -> thread_pool;
    SET_TRACE(context, "", "STATE_CREATE_TIMER_THREAD");
    ctx -> args -> num_of_threads++;
    temp_thread_pool = (pthread_t *) realloc(temp_thread_pool,
                                             sizeof(pthread_t) * ctx -> args -> num_of_threads);
    if (temp_thread_pool == NULL)
    {
        return STATE_ERROR;
    }

    ctx -> args -> thread_pool = temp_thread_pool;

    pthread_create(&ctx->args->thread_pool[ctx->args->num_of_threads], NULL, init_timer_function, (void *) ctx);
    usleep(1000);

    return STATE_READ_FROM_KEYBOARD;
}

static int cleanup_handler(struct fsm_context *context, struct fsm_error *err)
{
    exit_flag++;
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in cleanup handler", "STATE_CLEANUP");

    pthread_join(ctx -> args -> recv_thread, NULL);
    pthread_join(ctx -> args -> accept_gui_thread, NULL);

    for (int i = 0; i < ctx -> args -> num_of_threads; i++)
    {
        pthread_join(ctx -> args -> thread_pool[i], NULL);
    }

    if (ctx -> args -> sockfd)
    {
        if (socket_close(ctx -> args -> sockfd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    if (ctx -> args -> client_gui_fd)
    {
        if (socket_close(ctx -> args -> client_gui_fd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    if (ctx -> args -> connected_gui_fd)
    {
        if (socket_close(ctx -> args -> connected_gui_fd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    free(ctx -> args -> thread_pool);
    free(ctx -> args -> window);
    fclose(ctx -> args -> sent_data);
    fclose(ctx -> args -> received_data);

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n",
            err -> err_msg, err -> file_name, err -> function_name, err -> error_line);

    return STATE_CLEANUP;
}

static int wait_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ssize_t result;

    ctx = context;
    SET_TRACE(context, "", "STATE_LISTEN_SERVER");
    while (!exit_flag)
    {
        result = receive_packet(ctx->args->sockfd, ctx -> args -> window,
                                &ctx -> args -> temp_packet, ctx -> args -> received_data,
                                err);
        if (result == -1)
        {
            return STATE_ERROR;
        }

        if (ctx -> args -> is_connected_gui)
        {
            send_stats_gui(ctx -> args -> connected_gui_fd, RECEIVED_PACKET);
        }

        printf("Server packet with ack number: %u flags: %u received\n", ctx -> args -> temp_packet.hd.ack_number, ctx -> args -> temp_packet.hd.flags);

        return STATE_CHECK_ACK_NUMBER;
    }

    return FSM_EXIT;
}

static int check_ack_number_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    int result;

    ctx = context;
    SET_TRACE(context, "", "STATE_CHECK_ACK_NUMBER");

    result = read_flags(ctx -> args -> temp_packet.hd.flags);

    if (result == RECV_ACK)
    {
        printf("received ack\n");
        if (check_ack_number(get_expected_ack_number(ctx -> args -> window),
                             ctx -> args -> temp_packet.hd.ack_number, ctx -> args -> window))
        {
            return STATE_REMOVE_FROM_WINDOW;
        }
        else
        {
            return STATE_WAIT;
        }
    }
    else if (result == END_CONNECTION)
    {
        return STATE_TERMINATION;
    }
    else if (result == SEND_HANDSHAKE_ACK)
    {
        printf("recieved syn ack again\n");
        packet pt = ctx -> args -> temp_packet;
        create_handshake_ack_packet(ctx->args->sockfd, &ctx -> args -> server_addr_struct,
                                    ctx -> args -> window,&ctx -> args -> temp_packet,
                                    ctx -> args -> sent_data, err);

        if (ctx -> args -> is_connected_gui)
        {
            send_stats_gui(ctx -> args -> connected_gui_fd, SENT_PACKET);
        }

        return STATE_WAIT;
    }

    return STATE_SEND_PACKET;
}

static int remove_packet_from_window_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_REMOVE_FROM_WINDOW");

    remove_packet_from_window(ctx -> args -> window, &ctx -> args -> temp_packet);

    if (ctx -> args -> is_connected_gui)
    {
        send_stats_gui(ctx -> args -> connected_gui_fd, RECEIVED_ACK);
    }

    return STATE_WAIT;
}

static int send_packet_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "", "STATE_SEND_PACKET");

    read_received_packet(ctx -> args -> sockfd, &ctx -> args -> server_addr_struct,
                         ctx -> args -> window, &ctx -> args -> temp_packet,
                         ctx -> args -> sent_data, err);

    if (ctx -> args -> is_connected_gui)
    {
        send_stats_gui(ctx -> args -> connected_gui_fd, SENT_PACKET);
    }

    return STATE_WAIT;
}

static int termination_handler(struct fsm_context *context, struct fsm_error *err)
{

}

void *init_recv_function(void *ptr)
{
    struct fsm_context *ctx = (struct fsm_context*) ptr;
    struct fsm_error err;

    static struct fsm_transition transitions[] = {
            {FSM_INIT,                 STATE_WAIT,               wait_handler},
            {STATE_WAIT,               STATE_CHECK_ACK_NUMBER,   check_ack_number_handler},
            {STATE_CHECK_ACK_NUMBER,   STATE_REMOVE_FROM_WINDOW, remove_packet_from_window_handler},
            {STATE_CHECK_ACK_NUMBER,   STATE_SEND_PACKET,        send_packet_handler},
            {STATE_CHECK_ACK_NUMBER,   STATE_TERMINATION,        send_packet_handler},
            {STATE_CHECK_ACK_NUMBER,   STATE_WAIT,               wait_handler},
            {STATE_REMOVE_FROM_WINDOW, STATE_WAIT,               wait_handler},
            {STATE_SEND_PACKET,        STATE_WAIT,               wait_handler},
            {STATE_WAIT,               STATE_ERROR,              error_handler},
            {STATE_WAIT,               FSM_EXIT, NULL},
            {STATE_ERROR,              FSM_EXIT, NULL},
    };

    fsm_run(ctx, &err, transitions);

    return NULL;
}

void *init_timer_function(void *ptr)
{
    struct fsm_context  *ctx;
    struct fsm_error    err;
    int                 index, counter;
    uint32_t            temp_seq_number;

    ctx                 = (struct fsm_context*) ptr;
//    pthread_mutex_lock(&num_of_threads_mutex);
    index               = previous_index(ctx -> args -> window);
    temp_seq_number     = ctx -> args -> window[index].pt.hd.seq_number;
//    pthread_mutex_unlock(&num_of_threads_mutex);
    counter             = 0;

//    printf("created timer for packet at index: %d with seq number: %u and: %u\n", index,ctx -> args -> window[index].pt.hd.seq_number, ctx -> args -> window[index].is_packet_full);
    while (ctx -> args -> window[index].pt.hd.seq_number == temp_seq_number
            && ctx -> args -> window[index].is_packet_full)
    {
        sleep(TIMER_TIME);
//        printf("in timer the window: %d for index: %d\n", ctx -> args -> window[index].is_packet_full, index);
        if (ctx -> args -> window[index].is_packet_full && ctx -> args -> window[index].pt.hd.seq_number == temp_seq_number)
        {
            send_packet(ctx -> args -> sockfd, &ctx -> args -> server_addr_struct,
                        ctx -> args -> window, &ctx -> args -> window[index].pt,
                        ctx -> args -> sent_data, &err);

            if (ctx -> args -> is_connected_gui)
            {
                send_stats_gui(ctx -> args -> connected_gui_fd, RESENT_PACKET);
            }

            printf("Resent packet with seq number: %u\n", ctx -> args -> window[index].pt.hd.seq_number);
            counter++;
        }
    }

    pthread_exit(NULL);
}

void *init_window_checker_function(void *ptr)
{
    struct fsm_context *ctx = (struct fsm_context*) ptr;
    struct fsm_error *err = NULL;

    while (ctx -> args -> head != NULL)
    {
        if (is_window_empty())
        {
            struct packet pt;

            create_data_packet(&pt, ctx -> args -> window, ctx -> args -> head->data);
            send_packet(ctx -> args -> sockfd, &ctx -> args -> server_addr_struct,
                        ctx -> args -> window, &pt, ctx -> args -> sent_data,
                        err);

            create_timer_thread_handler(ctx, err);
            pop(&ctx -> args -> head);

            if (ctx -> args -> is_connected_gui)
            {
                send_stats_gui(ctx -> args -> connected_gui_fd, SENT_PACKET);
            }
        }
    }

    ctx -> args -> is_buffered = 0;

    return NULL;
}

void *init_gui_function(void *ptr)
{
    struct fsm_context *ctx = (struct fsm_context*) ptr;
    struct fsm_error err;

    while(!exit_flag)
    {
        ctx->args->connected_gui_fd = socket_accept_connection(ctx->args->client_gui_fd, &err);
        ctx->args->is_connected_gui++;
    }

    printf("leaving GUI thread\n");
    return NULL;
}

int create_file(const char *filepath, FILE **fp, struct fsm_error *err)
{
    *fp = fopen(filepath, "w");

    if(*fp == NULL)
    {
        SET_ERROR(err, "Error in opening file.");

        return -1;
    }

    return 0;
}
