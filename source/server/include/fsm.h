#ifndef CLIENT_FSM_H
#define CLIENT_FSM_H

#include <glob.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

typedef enum
{
    FSM_IGNORE = -1,
    FSM_INIT,
    FSM_EXIT,
    FSM_USER_START
} fsm_state;

#define RECV_BUF_SIZE 2048

typedef struct worker_state
{
    int sockfd;

    uint64_t start_index;
    uint64_t work_size;
    uint64_t end_index;
    uint64_t last_checkpoint_index;
    time_t   started_at;
    time_t   duration_secs;
    time_t   last_heard;
    uint64_t checkpoint_interval;
    uint32_t timeout_seconds;

    int    assigned;
    int    alive;
    char   recv_buf[RECV_BUF_SIZE];
    size_t recv_len;
} worker_state;

typedef struct work_chunk
{
    size_t start;
    size_t len;
} work_chunk;

typedef struct cracking_context
{
    char       *hash;
    uint64_t    index;
    uint64_t    work_size;
    uint64_t    checkpoint;
    uint64_t    timeout;
    int         found;
    char        password[255];
    work_chunk *queue;
    size_t      queue_len;
    time_t      total_secs;
} cracking_context;

typedef struct arguments
{
    int                     sockfd, *client_sockets, num_ready;
    cracking_context        crack_ctx;
    char                   *work_size_str, *checkpoint_str, *timeout_str;
    char                   *server_addr, *server_port_str;
    in_port_t               server_port;
    struct sockaddr_storage server_addr_struct;
    nfds_t                  max_clients;
    worker_state          **client_states;
    struct pollfd          *file_descriptors;
    struct timespec         start_wall, end_wall;
} arguments;

typedef struct fsm_context
{
    int               argc;
    char            **argv;
    struct arguments *args;
} fsm_context;

typedef struct fsm_error
{
    char       *err_msg;
    const char *function_name;
    const char *file_name;
    int         error_line;
} fsm_error;

typedef int (*fsm_state_func)(struct fsm_context *context,
                              struct fsm_error   *err);

struct client_fsm_transition
{
    int            from_id;
    int            to_id;
    fsm_state_func perform;
};

static inline void fsm_error_init(struct fsm_error *e)
{
    if (!e)
        return;
    e->err_msg       = NULL;
    e->error_line    = 0;
    e->function_name = NULL;
    e->file_name     = NULL;
}

static inline void fsm_error_clear(struct fsm_error *e)
{
    if (!e)
        return;
    free(e->err_msg);
    e->err_msg       = NULL;
    e->error_line    = 0;
    e->function_name = NULL;
    e->file_name     = NULL;
}

static inline char *fsm_strdup_or_null(const char *s)
{
    if (!s)
        return NULL;
    char *d = strdup(s);
    return d;
}

int fsm_run(struct fsm_context *context, struct fsm_error *err,
            const struct client_fsm_transition transitions[]);

#define SET_ERROR(err, msg)                                 \
    do                                                      \
    {                                                       \
        if (err)                                            \
        {                                                   \
            free((err)->err_msg);                           \
            (err)->err_msg     = fsm_strdup_or_null((msg)); \
            err->error_line    = __LINE__;                  \
            err->function_name = __func__;                  \
            err->file_name     = __FILENAME__;              \
        }                                                   \
    } while (0)

#define SET_TRACE(ctx, msg, curr_state)                     \
    do                                                      \
    {                                                       \
        printf("TRACE: %s \nEntered state at line %d.\n\n", \
               curr_state, __LINE__);                       \
        fflush(stdout);                                     \
    } while (0)

#endif // CLIENT_FSM_H
