#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void *site_debug_malloc_(size_t s, char const *f, int l)
{
    void *p = malloc(s);
    printf("\033[31m[MALLOC]\033[0m: f: %s l: %i s: %lu p: %p\n", f, l, s, p);
    return p;
}

void site_debug_free_(void *p, char const *f, int l)
{
    printf("\033[31m[FREE]\033[0m: f: %s l: %i p: %p\n", f, l, p);
    free(p);
}

#if defined(NDEBUG)
#define SITE_MALLOC(s) malloc(s)
#define SITE_FREE(p) free(p)
#else
#define SITE_MALLOC(s) site_debug_malloc_(s, __FILE__, __LINE__)
#define SITE_FREE(p) site_debug_free_(p, __FILE__, __LINE__)
#endif

struct site_post {
    int content_size;
    char const *content_type;
    char const *content; /* NOTE(samuel): not NULL terminated */
};

#include "posts.inl"

#define SITE_HTTP_VERSION "1.1"
#define SITE_DEFAULT_BACKLOG 1000
#define SITE_DEFAULT_PORT 8080
#define SITE_DEFAULT_MAX_FORK_COUNT 5
#define SITE_DEFAULT_TIMEOUT 100000

#define SITE_CONTENT_TYPE_TEXT_PLAIN "text/plain"
#define SITE_CONTENT_TYPE_TEXT_HTML "text/html"

enum site_code {
    SITE_SUCCESS = 0,
    SITE_ERROR_SOCKET_CREATE,
    SITE_ERROR_SOCKET_BIND,
    SITE_ERROR_SOCKET_LISTEN,
    SITE_ERROR_SOCKET_ACCEPT,
    SITE_ERROR_UNKNOWN,
    SITE_REQUEST_CONNECTION_CLOSE,
    SITE_ERROR_REQUEST_BAD_READ,
    SITE_ERROR_REQUEST_OVERFLOW,
    SITE_ERROR_REPLY_BAD_ENCODING,
    SITE_ERROR_REPLY_CONTENT_OVERFLOW
};

static char const *site_code_messages[] = {
    "\033[34m[LOG]\033[0m: success",                               /**/
    "\033[31m[ERROR]\033[0m: failure to create a socket",          /**/
    "\033[31m[ERROR]\033[0m: failure to bind the server socket",   /**/
    "\033[31m[ERROR]\033[0m: failure to listen the server socket", /**/
    "\033[31m[ERROR]\033[0m: failure to accept the server socket", /**/
    "\033[31m[ERROR]\033[0m: unknown failure",                     /**/
    "\033[34m[LOG]\033[0m: requested to close the connection",     /**/
    "\033[31m[ERROR]\033[0m: couldn't red the request",            /**/
    "\033[31m[ERROR]\033[0m: request overflowed",                  /**/
    "\033[31m[ERROR]\033[0m: couldn't encode the reply",           /**/
    "\033[31m[ERROR]\033[0m: reply overflowed"                     /**/
};

enum site_http_code {
    SITE_HTTP_OK = 200,
    SITE_HTTP_BAD_REQUEST = 400,
    SITE_HTTP_NOT_FOUND = 404,
    SITE_HTTP_SERVER_ERROR = 500,
    SITE_HTTP_NOT_IMPLEMENTED = 501
};

struct site_reply_desc {
    char const *http_version;
    enum site_http_code http_code;
    char const *content_type;
    char const *content;
    int content_size;
};

#define SITE_MAX_REPLY_SIZE (1 << 20)
struct site_reply {
    int buffer_size;
    char buffer[SITE_MAX_REPLY_SIZE];
};

#define SITE_MAX_REQUEST_SIZE (1 << 20)
struct site_request {
    int buffer_size;
    char buffer[SITE_MAX_REQUEST_SIZE];
};

#define SITE_REPLY_FORMAT                                                     \
    "HTTP/%s %d \nContent-Type: %s\nContent-Length: %i\n\n%s\n"

static enum site_code site_reply_init(struct site_reply_desc const *desc,
                                      struct site_reply *reply)
{
    int result;
    enum site_code code = SITE_SUCCESS;

    result = snprintf(reply->buffer, SITE_MAX_REPLY_SIZE, SITE_REPLY_FORMAT,
                      desc->http_version, desc->http_code, desc->content_type,
                      desc->content_size, desc->content);

    if (SITE_MAX_REPLY_SIZE < result) {
        code = SITE_ERROR_REPLY_CONTENT_OVERFLOW;
        goto out;
    }

    reply->buffer_size = result;

out:
    return code;
}

static enum site_code site_reply_send(struct site_reply const *reply,
                                      int client)
{
    int sent;
    int left = reply->buffer_size;
    enum site_code code = SITE_SUCCESS;

    while (left) {
        sent = send(client, reply->buffer, left, 0);

        if (-1 == sent) {
            code = SITE_ERROR_UNKNOWN;
            goto out;
        }

        left -= sent;
    }

out:
    return code;
}

static enum site_code site_request_receive(struct site_request *request,
                                           int client)
{
    int received = 0;
    int left = SITE_MAX_REQUEST_SIZE;
    enum site_code code = SITE_SUCCESS;

    request->buffer_size = 0;

    while (left) {
        received =
            recv(client, &request->buffer[request->buffer_size], left, 0);

        if (0 == received) {
            code = SITE_REQUEST_CONNECTION_CLOSE;
            goto out;
        }

        if (-1 == received) {
            code = SITE_ERROR_REQUEST_BAD_READ;
            goto out;
        }

        request->buffer_size += received;
        left -= received;

        if (SITE_MAX_REQUEST_SIZE < request->buffer_size) {
            code = SITE_ERROR_REQUEST_OVERFLOW;
            goto out;
        }

        if (2 > request->buffer_size)
            continue;

        if ('\r' != request->buffer[request->buffer_size - 2])
            continue;

        if ('\n' != request->buffer[request->buffer_size - 1])
            continue;

        goto out;
    }

out:
    return code;
}

static int site_request_is_get(struct site_request const *request)
{
    return 0 == strncmp(request->buffer, "GET /", 4);
}

#define SITE_HTTP_OK_MESSAGE "OK"
#define SITE_HTTP_BAD_REQUEST_MESSAGE "BAD REQUEST"
#define SITE_HTTP_SERVER_ERROR_MESSAGE "INTERNAL SERVER ERROR"
#define SITE_HTTP_NOT_FOUND_MESSAGE "CONTENT NOT FOUND"
#define SITE_HTTP_NOT_IMPLEMENTED_MESSAGE "NOT IMPLEMENTED"

struct site_server {
    int socket;
    int max_fork_count;
    int current_fork_count;
};

static enum site_code site_process_requests(int client)
{
    struct site_reply *reply;
    struct site_request *request;
    enum site_code code = SITE_SUCCESS;

    if (!(reply = SITE_MALLOC(sizeof(*reply)))) {
        code = SITE_ERROR_UNKNOWN;
        goto out;
    }

    if (!(request = SITE_MALLOC(sizeof(*request)))) {
        code = SITE_ERROR_UNKNOWN;
        goto out_free_reply;
    }

    code = site_request_receive(request, client);

    if (SITE_REQUEST_CONNECTION_CLOSE == code) {
        goto out_free_request;
    }

    if (SITE_ERROR_REQUEST_BAD_READ == code) {
        struct site_reply_desc reply_desc;

        reply_desc.http_version = SITE_HTTP_VERSION;
        reply_desc.http_code = SITE_HTTP_BAD_REQUEST;
        reply_desc.content_type = SITE_CONTENT_TYPE_TEXT_PLAIN;
        reply_desc.content = SITE_HTTP_BAD_REQUEST_MESSAGE;
        reply_desc.content_size = sizeof(SITE_HTTP_BAD_REQUEST_MESSAGE);

        code = site_reply_init(&reply_desc, reply);

        goto out_reply_send;
    }

    if (SITE_ERROR_REQUEST_OVERFLOW == code) {
        struct site_reply_desc reply_desc;

        reply_desc.http_version = SITE_HTTP_VERSION;
        reply_desc.http_code = SITE_HTTP_BAD_REQUEST;
        reply_desc.content_type = SITE_CONTENT_TYPE_TEXT_PLAIN;
        reply_desc.content = SITE_HTTP_BAD_REQUEST_MESSAGE;
        reply_desc.content_size = sizeof(SITE_HTTP_BAD_REQUEST_MESSAGE);

        code = site_reply_init(&reply_desc, reply);

        goto out_reply_send;
    }

    if (site_request_is_get(request)) {
        char *content;
        struct site_reply_desc reply_desc;
        struct site_post const *post = &posts[0];

        if (SITE_MAX_REPLY_SIZE < post->content_size + 1) {
            code = SITE_ERROR_UNKNOWN;
            goto out_free_request;
        }

        if (!(content = SITE_MALLOC((size_t)post->content_size + 1))) {
            code = SITE_ERROR_UNKNOWN;
            goto out_free_request;
        }

        if ('0' == request->buffer[5]) {
            printf("hi\n");
        }

        /* add a null terminator to the content */
        memcpy(content, post->content, post->content_size);
        content[post->content_size] = '\0';

        reply_desc.http_version = SITE_HTTP_VERSION;
        reply_desc.http_code = SITE_HTTP_OK;
        reply_desc.content_type = post->content_type;
        reply_desc.content = content;
        reply_desc.content_size = post->content_size + 1;

        /* FIXME(samuel): should I null terminate this? */
        code = site_reply_init(&reply_desc, reply);

        SITE_FREE(content);

        goto out_reply_send;
    } else {
        struct site_reply_desc reply_desc;

        reply_desc.http_version = SITE_HTTP_VERSION;
        reply_desc.http_code = SITE_HTTP_NOT_FOUND;
        reply_desc.content_type = SITE_CONTENT_TYPE_TEXT_PLAIN;
        reply_desc.content = SITE_HTTP_NOT_FOUND_MESSAGE;
        reply_desc.content_size = sizeof(SITE_HTTP_NOT_FOUND_MESSAGE);

        code = site_reply_init(&reply_desc, reply);

        goto out_reply_send;
    }

out_reply_send:
    code = site_reply_send(reply, client);

out_free_request:
    SITE_FREE(request);

out_free_reply:
    SITE_FREE(reply);

out:
    return code;
}

static enum site_code site_server_init(struct site_server *server)
{
    enum site_code code = SITE_SUCCESS;

    server->current_fork_count = 0;
    server->max_fork_count = SITE_DEFAULT_MAX_FORK_COUNT;
    server->socket = socket(AF_INET, SOCK_STREAM, 0);

    if (!server->socket) {
        code = SITE_ERROR_SOCKET_CREATE;
        goto out;
    }

out:
    return code;
}

static enum site_code site_server_run(struct site_server *server)
{
    int client;
    int sa_in_addr_len;
    struct sockaddr *sa;
    struct sockaddr_in sa_in;

    int result = 0;
    enum site_code code = SITE_SUCCESS;

    memset(&sa_in, 0, sizeof(sa_in));
    sa_in.sin_family = AF_INET;
    sa_in.sin_port = htons(SITE_DEFAULT_PORT);
    sa_in.sin_addr.s_addr = INADDR_ANY;

    sa = (struct sockaddr *)&sa_in;

    if (-1 == (result = bind(server->socket, sa, sizeof(*sa)))) {
        code = SITE_ERROR_SOCKET_BIND;
        goto out;
    }

    if (0 > (result = listen(server->socket, SITE_DEFAULT_BACKLOG))) {
        code = SITE_ERROR_SOCKET_LISTEN;
        goto out;
    }

    sa_in_addr_len = sizeof(sa_in);

    for (;;) {
        pid_t process;

        client = accept(server->socket, sa, (socklen_t *)&sa_in_addr_len);

        if (-1 == client) {
            code = SITE_ERROR_SOCKET_ACCEPT;
            goto out;
        }

        if (0 == (process = fork())) {
            code = site_process_requests(client);
            close(client);
            exit(EXIT_SUCCESS);
        } else if (0 > process) {
            close(client);
        } else {
            ++server->current_fork_count;
            printf("current_fork_count: %i\n", server->current_fork_count);
            close(client);
        }

        if (server->current_fork_count > server->max_fork_count) {
            while (0 < waitpid(-1, NULL, WNOHANG)) {
                --server->current_fork_count;
                printf("current_fork_count: %i\n", server->current_fork_count);
            }
        }
    }

out:
    return code;
}

static void site_server_deinit(struct site_server *server)
{
    close(server->socket);
}

int main(void)
{
    struct site_server server;
    enum site_code code = SITE_SUCCESS;

    if (SITE_SUCCESS != (code = site_server_init(&server)))
        goto out;

    printf("%s\n", site_code_messages[code]);

    if (SITE_SUCCESS != (code = site_server_run(&server)))
        goto out_server_deinit;

    printf("%s\n", site_code_messages[code]);

out_server_deinit:
    site_server_deinit(&server);

out:
    return 0;
}
