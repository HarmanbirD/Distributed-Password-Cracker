#include "command_line.h"
#include "fsm.h"
#include "protocol.h"
#include "server_config.h"
#include <pthread.h>
#include <signal.h>

#define TIMER_TIME 1

enum application_states
{
    STATE_PARSE_ARGUMENTS = FSM_USER_START,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_CREATE_SOCKET,
    STATE_BIND_SOCKET,
    STATE_LISTEN,
    STATE_CREATE_GUI_THREAD,
    STATE_WAIT,
    STATE_COMPARE_CHECKSUM,
    STATE_SEND_SYN_ACK,
    STATE_CHECK_SEQ_NUMBER,
    STATE_CREATE_TIMER_THREAD,
    STATE_WAIT_FOR_ACK,
    STATE_SEND_PACKET,
    STATE_UPDATE_SEQ_NUMBER,
    STATE_CLEANUP,
    STATE_ERROR
};

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int handle_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int convert_address_handler(struct fsm_context *context, struct fsm_error *err);
static int create_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int bind_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int listen_handler(struct fsm_context *context, struct fsm_error *err);
static int cleanup_handler(struct fsm_context *context, struct fsm_error *err);
static int error_handler(struct fsm_context *context, struct fsm_error *err);

// static void sigint_handler(int signum);
// static int  setup_signal_handler(struct fsm_error *err);
int create_file(const char *filepath, FILE **fp, struct fsm_error *err);

static volatile sig_atomic_t exit_flag = 0;

void *init_timer_function(void *ptr);
void *init_gui_function(void *ptr);

// typedef struct arguments
// {
//     int                     sockfd, num_of_threads, is_handshake_ack;
//     int                     server_gui_fd, connected_gui_fd, is_connected_gui;
//     int                     work_size, checkpoint, timeout;
//     char                   *server_addr, *server_port_str, *hash, *work_size_str, *checkpoint_str, *timeout_str;
//     in_port_t               server_port, client_port;
//     struct sockaddr_storage server_addr_struct, client_addr_struct, gui_addr_struct;
//     struct packet           temp_packet;
//     uint32_t                expected_seq_number;
//     pthread_t               accept_gui_thread;
//     pthread_t              *thread_pool;
//     FILE                   *sent_data, *received_data;
// } arguments;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
        .expected_seq_number = 0,
        .is_connected_gui = 0};
    struct fsm_context context = {
        .argc = argc,
        .argv = argv,
        .args = &args};

    static struct client_fsm_transition transitions[] = {
        {FSM_INIT, STATE_PARSE_ARGUMENTS, parse_arguments_handler},
        {STATE_PARSE_ARGUMENTS, STATE_HANDLE_ARGUMENTS, handle_arguments_handler},
        {STATE_HANDLE_ARGUMENTS, STATE_CONVERT_ADDRESS, convert_address_handler},
        {STATE_CONVERT_ADDRESS, STATE_CREATE_SOCKET, create_socket_handler},
        {STATE_CREATE_SOCKET, STATE_BIND_SOCKET, bind_socket_handler},
        {STATE_BIND_SOCKET, STATE_LISTEN, listen_handler},
        {STATE_WAIT, STATE_CLEANUP, cleanup_handler},
        {STATE_WAIT_FOR_ACK, STATE_CLEANUP, cleanup_handler},
        {STATE_ERROR, STATE_CLEANUP, cleanup_handler},
        {STATE_PARSE_ARGUMENTS, STATE_ERROR, error_handler},
        {STATE_HANDLE_ARGUMENTS, STATE_ERROR, error_handler},
        {STATE_CONVERT_ADDRESS, STATE_ERROR, error_handler},
        {STATE_CREATE_SOCKET, STATE_ERROR, error_handler},
        {STATE_BIND_SOCKET, STATE_ERROR, error_handler},
        {STATE_LISTEN, STATE_ERROR, error_handler},
        {STATE_CREATE_GUI_THREAD, STATE_ERROR, error_handler},
        {STATE_WAIT, STATE_ERROR, error_handler},
        {STATE_CREATE_TIMER_THREAD, STATE_ERROR, error_handler},
        {STATE_WAIT_FOR_ACK, STATE_ERROR, error_handler},
        {STATE_CLEANUP, FSM_EXIT, NULL},
    };

    fsm_run(&context, &err, transitions);

    return 0;
}

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in parse arguments handler", "STATE_PARSE_ARGUMENTS");
    if (parse_arguments(ctx->argc, ctx->argv, ctx->args, err) != 0)
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
    if (handle_arguments(ctx->argv[0], ctx->args, err) != 0)
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
    if (convert_address(ctx->args->server_addr, &ctx->args->server_addr_struct,
                        ctx->args->server_port, err) != 0)
    {
        return STATE_ERROR;
    }

    if (convert_address(ctx->args->server_addr, &ctx->args->gui_addr_struct,
                        61000, err) != 0)
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
    ctx->args->sockfd = socket_create(ctx->args->server_addr_struct.ss_family,
                                      SOCK_DGRAM, 0, err);
    if (ctx->args->sockfd == -1)
    {
        return STATE_ERROR;
    }

    ctx->args->server_gui_fd = socket_create(ctx->args->server_addr_struct.ss_family,
                                             SOCK_STREAM, 0, err);
    if (ctx->args->server_gui_fd == -1)
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
    if (socket_bind(ctx->args->sockfd, &ctx->args->server_addr_struct, err))
    {
        return STATE_ERROR;
    }

    if (socket_bind(ctx->args->server_gui_fd, &ctx->args->gui_addr_struct, err))
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
    if (start_listening(ctx->args->server_gui_fd, SOMAXCONN, err))
    {
        return STATE_ERROR;
    }

    return STATE_CREATE_GUI_THREAD;
}

// static int wait_handler(struct fsm_context *context, struct fsm_error *err)
// {
//     struct fsm_context *ctx;
//     ssize_t             result;
//
//     ctx = context;
//     SET_TRACE(context, "", "STATE_LISTEN_SERVER");
//     while (!exit_flag)
//     {
//         result = receive_packet(ctx->args->sockfd, &ctx->args->temp_packet,
//                                 ctx->args->received_data, err);
//
//         if (result == -1)
//         {
//             return STATE_ERROR;
//         }
//
//         if (ctx->args->is_connected_gui)
//         {
//             send_stats_gui(ctx->args->connected_gui_fd, RECEIVED_PACKET);
//         }
//
//         return STATE_COMPARE_CHECKSUM;
//     }
//
//     return STATE_CLEANUP;
// }

// static int create_timer_handler(struct fsm_context *context, struct fsm_error *err)
// {
//     struct fsm_context *ctx;
//     pthread_t          *temp_thread_pool;
//
//     ctx = context;
//     temp_thread_pool = ctx->args->thread_pool;
//     SET_TRACE(context, "", "STATE_CREATE_TIMER_THREAD");
//
//     ctx->args->num_of_threads++;
//
//     temp_thread_pool = (pthread_t *)realloc(temp_thread_pool,
//                                             sizeof(pthread_t) * ctx->args->num_of_threads);
//     if (temp_thread_pool == NULL)
//     {
//         return STATE_ERROR;
//     }
//
//     ctx->args->thread_pool = temp_thread_pool;
//
//     pthread_create(&ctx->args->thread_pool[ctx->args->num_of_threads], NULL, init_timer_function, (void *)ctx);
//
//     return STATE_WAIT_FOR_ACK;
// }

static int cleanup_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in cleanup handler", "STATE_CLEANUP");

    if (ctx->args->sockfd)
    {
        if (socket_close(ctx->args->sockfd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    if (ctx->args->server_gui_fd)
    {
        if (socket_close(ctx->args->server_gui_fd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    if (ctx->args->connected_gui_fd)
    {
        if (socket_close(ctx->args->connected_gui_fd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    fsm_error_clear(err);

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n",
            err->err_msg, err->file_name, err->function_name, err->error_line);

    return STATE_CLEANUP;
}
