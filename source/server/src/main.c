#include "command_line.h"
#include "fsm.h"
#include "server_config.h"
#include "utils.h"
#include <pthread.h>
#include <signal.h>

enum application_states
{
    STATE_PARSE_ARGUMENTS = FSM_USER_START,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_CREATE_SOCKET,
    STATE_BIND_SOCKET,
    STATE_LISTEN,
    STATE_SETUP_SIGNAL,
    STATE_START_TIMER,
    STATE_START_POLLING,
    STATE_STOP_TIMER,
    STATE_CLEANUP,
    STATE_ERROR
};

static void sigint_handler(int signum);
static int  parse_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int  handle_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int  convert_address_handler(struct fsm_context *context, struct fsm_error *err);
static int  create_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int  bind_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int  listen_handler(struct fsm_context *context, struct fsm_error *err);
static int  setup_signal_handler(struct fsm_context *context, struct fsm_error *err);
static int  start_timer_handler(struct fsm_context *context, struct fsm_error *err);
static int  start_polling_handler(struct fsm_context *context, struct fsm_error *err);
static int  stop_timer_handler(struct fsm_context *context, struct fsm_error *err);
static int  cleanup_handler(struct fsm_context *context, struct fsm_error *err);
static int  error_handler(struct fsm_context *context, struct fsm_error *err);

static volatile sig_atomic_t exit_flag = 0;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
        .crack_ctx.index       = 0,
        .crack_ctx.found       = 0,
        .crack_ctx.queue       = NULL,
        .crack_ctx.queue_len   = 0,
        .crack_ctx.total_secs  = 0,
        .crack_ctx.password[0] = '\0',
        .client_states         = NULL,
    };
    struct fsm_context context = {
        .argc = argc,
        .argv = argv,
        .args = &args,
    };

    static struct client_fsm_transition transitions[] = {
        {FSM_INIT,               STATE_PARSE_ARGUMENTS,  parse_arguments_handler },
        {STATE_PARSE_ARGUMENTS,  STATE_HANDLE_ARGUMENTS, handle_arguments_handler},
        {STATE_HANDLE_ARGUMENTS, STATE_CONVERT_ADDRESS,  convert_address_handler },
        {STATE_CONVERT_ADDRESS,  STATE_CREATE_SOCKET,    create_socket_handler   },
        {STATE_CREATE_SOCKET,    STATE_BIND_SOCKET,      bind_socket_handler     },
        {STATE_BIND_SOCKET,      STATE_LISTEN,           listen_handler          },
        {STATE_LISTEN,           STATE_SETUP_SIGNAL,     setup_signal_handler    },
        {STATE_SETUP_SIGNAL,     STATE_START_TIMER,      start_timer_handler     },
        {STATE_START_TIMER,      STATE_START_POLLING,    start_polling_handler   },
        {STATE_START_POLLING,    STATE_STOP_TIMER,       stop_timer_handler      },
        {STATE_STOP_TIMER,       STATE_CLEANUP,          cleanup_handler         },
        {STATE_ERROR,            STATE_CLEANUP,          cleanup_handler         },
        {STATE_PARSE_ARGUMENTS,  STATE_ERROR,            error_handler           },
        {STATE_HANDLE_ARGUMENTS, STATE_ERROR,            error_handler           },
        {STATE_CONVERT_ADDRESS,  STATE_ERROR,            error_handler           },
        {STATE_CREATE_SOCKET,    STATE_ERROR,            error_handler           },
        {STATE_BIND_SOCKET,      STATE_ERROR,            error_handler           },
        {STATE_LISTEN,           STATE_ERROR,            error_handler           },
        {STATE_START_TIMER,      STATE_ERROR,            error_handler           },
        {STATE_START_POLLING,    STATE_ERROR,            error_handler           },
        {STATE_STOP_TIMER,       STATE_ERROR,            error_handler           },
        {STATE_CLEANUP,          FSM_EXIT,               NULL                    },
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
    if (convert_address(ctx->args->server_addr, &ctx->args->server_addr_struct, ctx->args->server_port,
                        err) != 0)
        return STATE_ERROR;

    return STATE_CREATE_SOCKET;
}

static int create_socket_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in create socket", "STATE_CREATE_SOCKET");
    ctx->args->sockfd = socket_create(ctx->args->server_addr_struct.ss_family, SOCK_STREAM, 0, err);
    if (ctx->args->sockfd == -1)
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

    return STATE_LISTEN;
}

static int listen_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in start listening", "STATE_START_LISTENING");
    if (start_listening(ctx->args->sockfd, SOMAXCONN, err))
    {
        return STATE_ERROR;
    }

    return STATE_SETUP_SIGNAL;
}

static int setup_signal_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        SET_ERROR(err, "sigaction");

        return STATE_ERROR;
    }

    return STATE_START_TIMER;
}

static int start_timer_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in start timer", "STATE_START_TIMER");
    clock_gettime(CLOCK_MONOTONIC, &ctx->args->start_wall);

    return STATE_START_POLLING;
}

static int start_polling_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in start polling", "STATE_START_POLLING");

    while (exit_flag == 0 && ctx->args->crack_ctx.found == 0)
    {
        if (polling(ctx->args->sockfd, &ctx->args->file_descriptors, &ctx->args->max_clients,
                    &ctx->args->client_sockets, &ctx->args->client_states, &ctx->args->crack_ctx,
                    err) != 0)
        {
            return STATE_ERROR;
        }
    }

    return STATE_STOP_TIMER;
}

static int stop_timer_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in send file handler", "STATE_STOP_TIMER");

    clock_gettime(CLOCK_MONOTONIC, &ctx->args->end_wall);

    double wall = (ctx->args->end_wall.tv_sec - ctx->args->start_wall.tv_sec) +
                  (ctx->args->end_wall.tv_nsec - ctx->args->start_wall.tv_nsec) / 1e9;

    printf("Total time workers spent: %ld seconds\n", ctx->args->crack_ctx.total_secs);
    printf("Server ran for:           %.2f seconds\n", wall);

    return STATE_CLEANUP;
}

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

    close_clients(ctx->args->client_sockets, ctx->args->client_states, ctx->args->max_clients, err);

    fsm_error_clear(err);

    free(ctx->args->client_sockets);
    free(ctx->args->client_states);
    free(ctx->args->file_descriptors);

    if (ctx->args->crack_ctx.queue)
        free(ctx->args->crack_ctx.queue);

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n", err->err_msg, err->file_name,
            err->function_name, err->error_line);

    return STATE_CLEANUP;
}

static void sigint_handler(int signum)
{
    exit_flag = 1;
}
