#include "command_line.h"
#include "cracker.h"
#include "fsm.h"
#include "linked_list.h"
#include "protocol.h"
#include "server_config.h"
#include <bits/time.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#define TIMER_TIME 1

enum main_application_states
{
    STATE_PARSE_ARGUMENTS = FSM_USER_START,
    STATE_HANDLE_ARGUMENTS,
    STATE_CONVERT_ADDRESS,
    STATE_CREATE_SOCKET,
    STATE_CONNECT_SOCKET,
    STATE_WAIT_HASH,
    STATE_WAIT_WORK,
    STATE_START_TIMER,
    STATE_START_CRACKING,
    STATE_SEND_DONE,
    STATE_STOP_TIMER,
    STATE_CLEANUP,
    STATE_ERROR
};

static int parse_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int handle_arguments_handler(struct fsm_context *context, struct fsm_error *err);
static int convert_address_handler(struct fsm_context *context, struct fsm_error *err);
static int create_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int connect_socket_handler(struct fsm_context *context, struct fsm_error *err);
static int wait_hash_handler(struct fsm_context *context, struct fsm_error *err);
static int wait_work_handler(struct fsm_context *context, struct fsm_error *err);
static int start_timer_handler(struct fsm_context *context, struct fsm_error *err);
static int start_cracking_handler(struct fsm_context *context, struct fsm_error *err);
static int send_done_handler(struct fsm_context *context, struct fsm_error *err);
static int stop_timer_handler(struct fsm_context *context, struct fsm_error *err);
static int cleanup_handler(struct fsm_context *context, struct fsm_error *err);
static int error_handler(struct fsm_context *context, struct fsm_error *err);

static void sigint_handler(int signum);
static int  setup_signal_handler(struct fsm_error *err);
static void ignore_sigpipe(void);

static volatile sig_atomic_t exit_flag = 0;

int main(int argc, char **argv)
{
    struct fsm_error err;
    struct arguments args = {
        .threads       = 0,
        .timer_started = 0,
        .ws            = NULL,
    };
    struct fsm_context context = {
        .argc = argc,
        .argv = argv,
        .args = &args,
    };
    context.args->ws = calloc(1, sizeof(worker_state));

    static struct fsm_transition transitions[] = {
        {FSM_INIT,               STATE_PARSE_ARGUMENTS,  parse_arguments_handler },
        {STATE_PARSE_ARGUMENTS,  STATE_HANDLE_ARGUMENTS, handle_arguments_handler},
        {STATE_HANDLE_ARGUMENTS, STATE_CONVERT_ADDRESS,  convert_address_handler },
        {STATE_CONVERT_ADDRESS,  STATE_CREATE_SOCKET,    create_socket_handler   },
        {STATE_CREATE_SOCKET,    STATE_CONNECT_SOCKET,   connect_socket_handler  },
        {STATE_CONNECT_SOCKET,   STATE_WAIT_HASH,        wait_hash_handler       },
        {STATE_WAIT_HASH,        STATE_WAIT_WORK,        wait_work_handler       },
        {STATE_WAIT_WORK,        STATE_START_TIMER,      start_timer_handler     },
        {STATE_WAIT_WORK,        STATE_START_CRACKING,   start_cracking_handler  },
        {STATE_WAIT_WORK,        STATE_CLEANUP,          cleanup_handler         },
        {STATE_START_TIMER,      STATE_START_CRACKING,   start_cracking_handler  },
        {STATE_START_CRACKING,   STATE_SEND_DONE,        send_done_handler       },
        {STATE_START_CRACKING,   STATE_STOP_TIMER,       stop_timer_handler      },
        {STATE_SEND_DONE,        STATE_WAIT_WORK,        wait_work_handler       },
        {STATE_STOP_TIMER,       STATE_CLEANUP,          cleanup_handler         },
        {STATE_ERROR,            STATE_CLEANUP,          cleanup_handler         },
        {STATE_PARSE_ARGUMENTS,  STATE_ERROR,            error_handler           },
        {STATE_HANDLE_ARGUMENTS, STATE_ERROR,            error_handler           },
        {STATE_CONVERT_ADDRESS,  STATE_ERROR,            error_handler           },
        {STATE_CREATE_SOCKET,    STATE_ERROR,            error_handler           },
        {STATE_CONNECT_SOCKET,   STATE_ERROR,            error_handler           },
        {STATE_WAIT_HASH,        STATE_ERROR,            error_handler           },
        {STATE_WAIT_WORK,        STATE_ERROR,            error_handler           },
        {STATE_START_TIMER,      STATE_ERROR,            error_handler           },
        {STATE_START_CRACKING,   STATE_ERROR,            error_handler           },
        {STATE_SEND_DONE,        STATE_ERROR,            error_handler           },
        {STATE_STOP_TIMER,       STATE_ERROR,            error_handler           },
        {STATE_CLEANUP,          FSM_EXIT,               NULL                    },
    };
    ignore_sigpipe();
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
    ctx->args->ws->sockfd = socket_create(ctx->args->server_addr_struct.ss_family, SOCK_STREAM, 0, err);
    if (ctx->args->ws->sockfd == -1)
    {
        return STATE_ERROR;
    }

    return STATE_CONNECT_SOCKET;
}

static int connect_socket_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in connect socket", "STATE_CONNECT_SOCKET");
    if (socket_connect(ctx->args->ws->sockfd, &ctx->args->server_addr_struct, ctx->args->server_port, err) == -1)
    {
        return STATE_ERROR;
    }

    return STATE_WAIT_HASH;
}

static int wait_hash_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in wait hash", "STATE_WAIT_HASH");
    if (receive_hash(ctx->args->ws->sockfd, ctx->args->ws, err) == -1)
    {
        return STATE_ERROR;
    }

    return STATE_WAIT_WORK;
}

static int wait_work_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in wait work", "STATE_WAIT_WORK");

    int val = wait_for_work(ctx->args->ws->sockfd, ctx->args->ws, err);

    if (val == -1)
        return STATE_ERROR;
    else if (val == 1)
        return STATE_CLEANUP;

    if (ctx->args->timer_started)
        return STATE_START_CRACKING;
    else
        return STATE_START_TIMER;
}

static int start_timer_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in create socket", "STATE_START_TIMER");
    ctx->args->start_cpu = clock();
    clock_gettime(CLOCK_MONOTONIC, &ctx->args->start_wall);

    ctx->args->timer_started = 1;

    return STATE_START_CRACKING;
}

static int start_cracking_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in start cracking", "STATE_START_CRACKING");
    if (create_threads(ctx->args->threads, ctx->args->ws) == -1)
    {
        return STATE_SEND_DONE;
    }

    return STATE_STOP_TIMER;
}

static int send_done_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in send done", "STATE_SEND_DONE");
    if (send_done(ctx->args->ws->sockfd, err) == -1)
    {
        return STATE_ERROR;
    }

    return STATE_WAIT_WORK;
}

static int stop_timer_handler(struct fsm_context *context, struct fsm_error *err)
{
    struct fsm_context *ctx;
    ctx = context;
    SET_TRACE(context, "in send file handler", "STATE_STOP_TIMER");

    clock_gettime(CLOCK_MONOTONIC, &ctx->args->end_wall);

    ctx->args->end_cpu = clock();
    double wall        = (ctx->args->end_wall.tv_sec - ctx->args->start_wall.tv_sec) + (ctx->args->end_wall.tv_nsec - ctx->args->start_wall.tv_nsec) / 1e9;
    double cpu_seconds = (double)(ctx->args->end_cpu - ctx->args->start_cpu) / (double)CLOCKS_PER_SEC;

    printf("Wall: %.6f s\nCPU:  %.6f s\n", wall, cpu_seconds);

    return STATE_CLEANUP;
}

static int cleanup_handler(struct fsm_context *context, struct fsm_error *err)
{
    exit_flag++;
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

    fsm_error_clear(err);

    if (ctx->args->ws)
        if (ctx->args->ws->hash)
            free(ctx->args->ws->hash);

    free(ctx->args->ws);

    return FSM_EXIT;
}

static int error_handler(struct fsm_context *context, struct fsm_error *err)
{
    fprintf(stderr, "ERROR %s\nIn file %s in function %s on line %d\n",
            err->err_msg, err->file_name, err->function_name, err->error_line);

    return STATE_CLEANUP;
}

static void ignore_sigpipe(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}
