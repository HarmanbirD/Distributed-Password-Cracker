#ifndef CLIENT_FSM_H
#define CLIENT_FSM_H

#include <glob.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

typedef enum
{
    FSM_IGNORE = -1,
    FSM_INIT,
    FSM_EXIT,
    FSM_USER_START
} fsm_state;

typedef struct arguments
{
    int                     sockfd, num_of_threads, is_handshake_ack;
    int                     server_gui_fd, connected_gui_fd, is_connected_gui;
    int                     work_size, checkpoint, timeout;
    char                   *server_addr, *server_port_str, *hash, *work_size_str, *checkpoint_str, *timeout_str;
    in_port_t               server_port, client_port;
    struct sockaddr_storage server_addr_struct, client_addr_struct, gui_addr_struct;
    uint32_t                expected_seq_number;
    pthread_t               accept_gui_thread;
    pthread_t              *thread_pool;
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
    e->err_msg = NULL;
    e->error_line = 0;
    e->function_name = NULL;
    e->file_name = NULL;
}

static inline void fsm_error_clear(struct fsm_error *e)
{
    if (!e)
        return;
    free(e->err_msg);
    e->err_msg = NULL;
    e->error_line = 0;
    e->function_name = NULL;
    e->file_name = NULL;
}

static inline char *fsm_strdup_or_null(const char *s)
{
    if (!s)
        return NULL;
    char *d = strdup(s);
    return d;
}

static fsm_state_func fsm_transition(const struct fsm_context *context,
                                     int from_id, int to_id, const struct client_fsm_transition transitions[]);
int                   fsm_run(struct fsm_context *context, struct fsm_error *err,
                              const struct client_fsm_transition transitions[]);

#define SET_ERROR(err, msg)                             \
    do                                                  \
    {                                                   \
        if (err)                                        \
        {                                               \
            free((err)->err_msg);                       \
            (err)->err_msg = fsm_strdup_or_null((msg)); \
            err->error_line = __LINE__;                 \
            err->function_name = __func__;              \
            err->file_name = __FILENAME__;              \
        }                                               \
    } while (0)

#define SET_TRACE(ctx, msg, curr_state)                     \
    do                                                      \
    {                                                       \
        printf("TRACE: %s \nEntered state at line %d.\n\n", \
               curr_state, __LINE__);                       \
        fflush(stdout);                                     \
    } while (0)

#endif // CLIENT_FSM_H
