#include "command_line.h"
#include "fsm.h"
#include "linked_list.h"
#include "protocol.h"
#include "server_config.h"
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

static void sigint_handler(int signum);
static int  setup_signal_handler(struct fsm_error *err);
int         create_file(const char *filepath, FILE **fp, struct fsm_error *err);

static volatile sig_atomic_t exit_flag = 0;

void *init_recv_function(void *ptr);
void *init_timer_function(void *ptr);
void *init_window_checker_function(void *ptr);
void *init_gui_function(void *ptr);

pthread_mutex_t num_of_threads_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
        .threads = 0,
    };
    struct fsm_context context = {
        .argc = argc,
        .argv = argv,
        .args = &args};

    static struct fsm_transition transitions[] = {
        {FSM_INIT,                     STATE_PARSE_ARGUMENTS,        parse_arguments_handler       },
        {STATE_PARSE_ARGUMENTS,        STATE_HANDLE_ARGUMENTS,       handle_arguments_handler      },
        {STATE_HANDLE_ARGUMENTS,       STATE_CONVERT_ADDRESS,        convert_address_handler       },
        {STATE_CONVERT_ADDRESS,        STATE_CREATE_SOCKET,          create_socket_handler         },
        {STATE_CREATE_SOCKET,          STATE_BIND_SOCKET,            bind_socket_handler           },
        {STATE_BIND_SOCKET,            STATE_LISTEN,                 listen_handler                },
        {STATE_LISTEN,                 STATE_CREATE_GUI_THREAD,      create_gui_thread_handler     },
        {STATE_CREATE_GUI_THREAD,      STATE_CREATE_WINDOW,          create_window_handler         },
        {STATE_CREATE_WINDOW,          STATE_START_HANDSHAKE,        start_handshake_handler       },
        {STATE_START_HANDSHAKE,        STATE_CREATE_HANDSHAKE_TIMER, create_handshake_timer_handler},
        {STATE_CREATE_HANDSHAKE_TIMER, STATE_WAIT_FOR_SYN_ACK,       wait_for_syn_ack_handler      },
        {STATE_WAIT_FOR_SYN_ACK,       STATE_SEND_HANDSHAKE_ACK,     send_handshake_ack_handler    },
        {STATE_WAIT_FOR_SYN_ACK,       STATE_CLEANUP,                cleanup_handler               },
        {STATE_SEND_HANDSHAKE_ACK,     STATE_CREATE_RECV_THREAD,     create_recv_thread_handler    },
        {STATE_CREATE_RECV_THREAD,     STATE_READ_FROM_KEYBOARD,     read_from_keyboard_handler    },
        {STATE_READ_FROM_KEYBOARD,     STATE_CHECK_WINDOW,           check_window_handler          },
        {STATE_CHECK_WINDOW,           STATE_ADD_PACKET_TO_WINDOW,   add_packet_to_window_handler  },
        {STATE_CHECK_WINDOW,           STATE_ADD_PACKET_TO_BUFFER,   add_packet_to_buffer_handler  },
        {STATE_ADD_PACKET_TO_BUFFER,   STATE_READ_FROM_KEYBOARD,     read_from_keyboard_handler    },
        {STATE_ADD_PACKET_TO_BUFFER,   STATE_CHECK_WINDOW_THREAD,    check_window_thread_handler   },
        {STATE_ADD_PACKET_TO_WINDOW,   STATE_SEND_MESSAGE,           send_message_handler          },
        //            {STATE_ADD_PACKET_TO_WINDOW,    STATE_CHECK_WINDOW_THREAD,  check_window_thread_handler},
        {STATE_CHECK_WINDOW_THREAD,    STATE_READ_FROM_KEYBOARD,     read_from_keyboard_handler    },
        {STATE_SEND_MESSAGE,           STATE_CREATE_TIMER_THREAD,    create_timer_thread_handler   },
        {STATE_CREATE_TIMER_THREAD,    STATE_READ_FROM_KEYBOARD,     read_from_keyboard_handler    },
        {STATE_READ_FROM_KEYBOARD,     STATE_CLEANUP,                cleanup_handler               },
        {STATE_ERROR,                  STATE_CLEANUP,                cleanup_handler               },
        {STATE_PARSE_ARGUMENTS,        STATE_ERROR,                  error_handler                 },
        {STATE_HANDLE_ARGUMENTS,       STATE_ERROR,                  error_handler                 },
        {STATE_CONVERT_ADDRESS,        STATE_ERROR,                  error_handler                 },
        {STATE_CREATE_SOCKET,          STATE_ERROR,                  error_handler                 },
        {STATE_BIND_SOCKET,            STATE_ERROR,                  error_handler                 },
        {STATE_CREATE_WINDOW,          STATE_ERROR,                  error_handler                 },
        {STATE_CREATE_RECV_THREAD,     STATE_ERROR,                  error_handler                 },
        {STATE_START_HANDSHAKE,        STATE_ERROR,                  error_handler                 },
        {STATE_SEND_MESSAGE,           STATE_ERROR,                  error_handler                 },
        {STATE_CLEANUP,                FSM_EXIT,                     NULL                          },
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
    if (convert_address(ctx->args->server_addr, &ctx->args->server_addr_struct, ctx->args->server_port, err) != 0)
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
    ctx->args->sockfd = socket_create(ctx->args->client_addr_struct.ss_family, SOCK_DGRAM, 0, err);
    if (ctx->args->sockfd == -1)
    {
        return STATE_ERROR;
    }

    return STATE_BIND_SOCKET;
}

static int cleanup_handler(struct fsm_context *context, struct fsm_error *err)
{
    exit_flag++;
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in cleanup handler", "STATE_CLEANUP");

    for (int i = 0; i < ctx->args->num_of_threads; i++)
    {
        pthread_join(ctx->args->thread_pool[i], NULL);
    }

    if (ctx->args->sockfd)
    {
        if (socket_close(ctx->args->sockfd, err) == -1)
        {
            printf("close socket error\n");
        }
    }

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n",
            err->err_msg, err->file_name, err->function_name, err->error_line);

    return STATE_CLEANUP;
}
