#include "server_config.h"
#include "fsm.h"
#include "utils.h"

int socket_create(int domain, int type, int protocol, struct fsm_error *err)
{
    int sockfd;

    sockfd = socket(domain, type, protocol);

    if (sockfd == -1)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    return sockfd;
}

int socket_connect(int sockfd, struct sockaddr_storage *addr, in_port_t port, struct fsm_error *err)
{
    char      addr_str[INET6_ADDRSTRLEN];
    in_port_t net_port;

    if (inet_ntop(addr->ss_family, addr->ss_family == AF_INET ? (void *)&(((struct sockaddr_in *)addr)->sin_addr) : (void *)&(((struct sockaddr_in6 *)addr)->sin6_addr), addr_str, sizeof(addr_str)) == NULL)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    printf("Connecting to: %s:%u\n", addr_str, port);
    net_port = htons(port);

    if (addr->ss_family == AF_INET)
    {
        struct sockaddr_in *ipv4_addr;
        ipv4_addr           = (struct sockaddr_in *)addr;
        ipv4_addr->sin_port = net_port;
        if (connect(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in)) == -1)
        {
            SET_ERROR(err, strerror(errno));
            return -1;
        }
    }
    else if (addr->ss_family == AF_INET6)
    {
        struct sockaddr_in6 *ipv6_addr;
        ipv6_addr            = (struct sockaddr_in6 *)addr;
        ipv6_addr->sin6_port = net_port;
        if (connect(sockfd, (struct sockaddr *)addr, sizeof(struct sockaddr_in6)) == -1)
        {
            SET_ERROR(err, strerror(errno));
            return -1;
        }
    }
    else
    {
        SET_ERROR(err, "Address family not supported");
        return -1;
    }
    printf("Connected to: %s:%u\n", addr_str, port);

    return 0;
}

int receive_hash(int sockfd, worker_state *ws, struct fsm_error *err)
{
    char    buffer[512];
    ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0)
    {
        SET_ERROR(err, "recv() failed (receive_hash)");

        return -1;
    }

    buffer[n] = '\0';

    if (strncmp(buffer, "HASH ", 5) != 0)
    {
        char message[256];
        snprintf(message, sizeof(message), "Invalid HASH message from server: %s\n", buffer);
        SET_ERROR(err, message);

        return -1;
    }

    const char *hash_start = buffer + 5;

    char *newline = strchr(hash_start, '\n');
    if (newline)
        *newline = '\0';

    ws->hash = strdup(hash_start);
    if (!ws->hash)
    {
        SET_ERROR(err, "strdup failed");

        return -1;
    }

    printf("[WORKER] Received hash: %s\n", ws->hash);

    const char *msg = "READY\n";
    if (send(sockfd, msg, strlen(msg), 0) < 0)
    {
        SET_ERROR(err, "send(READY) failed");

        return -1;
    }

    return 0;
}

int wait_for_work(int sockfd, worker_state *ws)
{
    char    buffer[512];
    ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0)
    {
        perror("[WORKER] recv() failed waiting for WORK");
        return -1; // disconnect or error
    }

    buffer[n] = '\0';

    // ------------------------------------------------------------
    // STOP (server found the password or is shutting down)
    // ------------------------------------------------------------
    if (strncmp(buffer, "STOP", 4) == 0)
    {
        printf("[WORKER] Received STOP from server\n");
        return 1; // special meaning: stop everything
    }

    // ------------------------------------------------------------
    // WORK message expected:
    // WORK <start> <len> <checkpoint> <timeout>
    // ------------------------------------------------------------
    if (strncmp(buffer, "WORK ", 5) != 0)
    {
        fprintf(stderr, "[WORKER] Invalid WORK message: %s\n", buffer);
        return -1;
    }

    uint64_t start = 0, len = 0;
    uint32_t checkpoint = 0, timeout = 0;

    // Parse WORK message
    int parsed = sscanf(buffer + 5,
                        "%" SCNu64 " %" SCNu64 " %" SCNu32 " %" SCNu32,
                        &start, &len, &checkpoint, &timeout);

    if (parsed != 4)
    {
        fprintf(stderr, "[WORKER] Failed to parse WORK message: %s\n", buffer);
        return -1;
    }

    // Store into worker_state
    ws->start_index         = start;
    ws->work_size           = len;
    ws->end_index           = start + len - 1;
    ws->checkpoint_interval = checkpoint;
    ws->timeout_seconds     = timeout;

    printf("[WORKER] Received WORK: start=%" PRIu64
           ", len=%" PRIu64 ", checkpoint=%" PRIu64 ", timeout=%u\n",
           ws->start_index,
           ws->work_size,
           ws->checkpoint_interval,
           ws->timeout_seconds);

    return 0;
}

int send_checkpoint(worker_state *ws, uint64_t idx)
{
    if (!ws || ws->sockfd <= 0)
        return -1;

    char buffer[128];
    int  n = snprintf(buffer, sizeof(buffer), "CHECKPOINT %" PRIu64 "\n", idx);

    if (n <= 0 || n >= (int)sizeof(buffer))
    {
        fprintf(stderr, "send_checkpoint(): snprintf failed\n");
        return -1;
    }

    ssize_t sent = send(ws->sockfd, buffer, n, 0);
    if (sent != n)
    {
        perror("send() checkpoint failed");
        return -1;
    }
    printf("Sent checkpoint %" PRIu64 "\n", idx);

    return 0;
}

int start_listening(int sockfd, int backlog, struct fsm_error *err)
{
    if (listen(sockfd, backlog) == -1)
    {
        SET_ERROR(err, strerror(errno));

        return -1;
    }

    return 0;
}

int socket_accept_connection(int sockfd, struct fsm_error *err)
{
    struct sockaddr client_addr;
    socklen_t       client_addr_len;

    client_addr_len = sizeof(client_addr);
    int client_fd;

    errno     = 0;
    client_fd = accept(sockfd, &client_addr, &client_addr_len);

    if (client_fd == -1)
    {
        if (errno != EINTR)
        {
            perror("Error in connecting to client.");
        }
        SET_ERROR(err, strerror(errno));

        return -1;
    }

    return client_fd;
}

int socket_close(int sockfd, struct fsm_error *err)
{
    if (close(sockfd) == -1)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    return 0;
}

int convert_address(const char *address, struct sockaddr_storage *addr,
                    in_port_t port, struct fsm_error *err)
{
    memset(addr, 0, sizeof(*addr));
    char      addr_str[INET6_ADDRSTRLEN];
    socklen_t addr_len;
    void     *vaddr;
    in_port_t net_port;

    net_port = htons(port);

    if (inet_pton(AF_INET, address, &(((struct sockaddr_in *)addr)->sin_addr)) == 1)
    {
        struct sockaddr_in *ipv4_addr;

        ipv4_addr           = (struct sockaddr_in *)addr;
        addr_len            = sizeof(*ipv4_addr);
        ipv4_addr->sin_port = net_port;
        vaddr               = (void *)&(((struct sockaddr_in *)addr)->sin_addr);
        addr->ss_family     = AF_INET;
    }
    else if (inet_pton(AF_INET6, address, &(((struct sockaddr_in6 *)addr)->sin6_addr)) == 1)
    {
        struct sockaddr_in6 *ipv6_addr;

        ipv6_addr            = (struct sockaddr_in6 *)addr;
        addr_len             = sizeof(*ipv6_addr);
        ipv6_addr->sin6_port = net_port;
        vaddr                = (void *)&(((struct sockaddr_in6 *)addr)->sin6_addr);
        addr->ss_family      = AF_INET6;
    }
    else
    {
        char message[90];
        snprintf(message, sizeof(message), "Address family not supported for IP address: %s", address);

        SET_ERROR(err, message);
        return -1;
    }

    return 0;
}

int socket_bind(int sockfd, struct sockaddr_storage *addr, struct fsm_error *err)
{
    char *ip_address;
    char *port;

    ip_address = safe_malloc(sizeof(char) * NI_MAXHOST, err);
    port       = safe_malloc(sizeof(char) * NI_MAXSERV, err);

    if (get_sockaddr_info(addr, &ip_address, &port, err) != 0)
    {
        return -1;
    }

    printf("binding to: %s:%s\n", ip_address, port);

    if (bind(sockfd, (struct sockaddr *)addr, size_of_address(addr)) == -1)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    printf("Bound to socket: %s:%s\n", ip_address, port);

    free(ip_address);
    free(port);

    return 0;
}

socklen_t size_of_address(struct sockaddr_storage *addr)
{
    return addr->ss_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
}

int get_sockaddr_info(struct sockaddr_storage *addr, char **ip_address, char **port, struct fsm_error *err)
{
    char      temp_ip[NI_MAXHOST];
    char      temp_port[NI_MAXSERV];
    socklen_t ip_size;
    int       result;

    ip_size = sizeof(*addr);
    result  = getnameinfo((struct sockaddr *)addr,
                          ip_size, temp_ip, sizeof(temp_ip), temp_port, sizeof(temp_port),
                          NI_NUMERICHOST | NI_NUMERICSERV);
    if (result != 0)
    {
        SET_ERROR(err, strerror(errno));
        return -1;
    }

    strcpy(*ip_address, temp_ip);
    strcpy(*port, temp_port);

    return 0;
}
