/**
 * @file main.c
 * @brief Mock provider server entry point
 *
 * A standalone HTTP server that returns pre-scripted responses
 * in OpenAI Chat Completions SSE format. Used for deterministic
 * manual testing of ikigai.
 */

#include "apps/mock-provider/http_server.h"
#include "apps/mock-provider/mock_queue.h"
#include "apps/mock-provider/openai_serializer.h"

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <talloc.h>
#include <unistd.h>

#define DEFAULT_PORT 9100
#define MAX_REQUEST_SIZE 65536

static volatile sig_atomic_t g_shutdown = 0;

static void signal_handler(int sig)
{
    (void)sig;
    g_shutdown = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);
}

static int parse_port(int argc, char **argv)
{
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            long port = strtol(argv[i + 1], NULL, 10);
            if (port > 0 && port <= 65535) {
                return (int)port;
            }
        }
    }
    return DEFAULT_PORT;
}

static int create_listen_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 8) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * Read a full HTTP request from a client socket.
 * Returns bytes read, or -1 on error.
 */
static ssize_t read_request(int client_fd, char *buf, size_t buf_size)
{
    size_t total = 0;

    while (total < buf_size - 1) {
        ssize_t n = read(client_fd, buf + total, buf_size - 1 - total);
        if (n <= 0) {
            if (n == 0 && total > 0) break;
            return n;
        }
        total += (size_t)n;

        /* Check if we've received the full request.
         * Look for \r\n\r\n (end of headers), then check
         * Content-Length to see if body is complete. */
        char *header_end = strstr(buf, "\r\n\r\n");
        if (header_end == NULL) {
            /* Also handle \n\n */
            header_end = strstr(buf, "\n\n");
            if (header_end != NULL) {
                header_end += 2;
            }
        } else {
            header_end += 4;
        }

        if (header_end != NULL) {
            /* Find Content-Length */
            size_t content_length = 0;
            buf[total] = '\0';
            char *cl = strcasestr(buf, "Content-Length:");
            if (cl != NULL) {
                content_length = (size_t)strtoul(
                    cl + strlen("Content-Length:"), NULL, 10);
            }

            size_t header_size = (size_t)(header_end - buf);
            size_t body_received = total - header_size;
            if (body_received >= content_length) {
                break;
            }
        } else if (total > 4) {
            /* No headers end found yet, keep reading */
            continue;
        }
    }

    buf[total] = '\0';
    return (ssize_t)total;
}

/**
 * Handle a single HTTP request.
 */
static void handle_request(TALLOC_CTX *ctx, int client_fd,
                           mock_queue_t *queue, const char *raw,
                           size_t raw_len)
{
    TALLOC_CTX *tmp = talloc_new(ctx);

    http_request_t *req = http_request_parse(tmp, raw, raw_len);
    if (req == NULL) {
        http_respond_error(client_fd, 400, "Malformed HTTP request");
        talloc_free(tmp);
        return;
    }

    /* Route: /_mock/expect */
    if (strcmp(req->path, "/_mock/expect") == 0) {
        if (req->body == NULL || req->body_len == 0) {
            http_respond_error(client_fd, 400, "Missing request body");
        } else {
            int rc = mock_queue_load(queue, req->body, req->body_len);
            if (rc != 0) {
                http_respond_error(client_fd, 400,
                                   "Invalid JSON in request body");
            } else {
                http_respond_json(client_fd, 200, "{\"status\":\"ok\"}");
            }
        }
        talloc_free(tmp);
        return;
    }

    /* Route: /_mock/reset */
    if (strcmp(req->path, "/_mock/reset") == 0) {
        mock_queue_reset(queue);
        http_respond_json(client_fd, 200, "{\"status\":\"ok\"}");
        talloc_free(tmp);
        return;
    }

    /* Route: /v1/chat/completions */
    if (strcmp(req->path, "/v1/chat/completions") == 0) {
        mock_response_t *resp = mock_queue_pop(queue);
        if (resp == NULL) {
            http_respond_error(client_fd, 503,
                "Response queue empty - load responses with "
                "/_mock/expect first");
            talloc_free(tmp);
            return;
        }

        http_respond_sse_start(client_fd);

        if (resp->type == MOCK_TEXT) {
            openai_serialize_text(tmp, resp->content, client_fd);
        } else if (resp->type == MOCK_TOOL_CALLS) {
            openai_serialize_tool_calls(tmp, resp->tool_calls,
                                        resp->tool_call_count,
                                        client_fd);
        }

        talloc_free(resp);
        talloc_free(tmp);
        return;
    }

    /* Route: /v1/responses (OpenAI Responses API) */
    if (strcmp(req->path, "/v1/responses") == 0) {
        mock_response_t *resp = mock_queue_pop(queue);
        if (resp == NULL) {
            http_respond_error(client_fd, 503,
                "Response queue empty - load responses with "
                "/_mock/expect first");
            talloc_free(tmp);
            return;
        }

        http_respond_sse_start(client_fd);

        if (resp->type == MOCK_TEXT) {
            openai_responses_serialize_text(tmp, resp->content,
                                            client_fd);
        } else if (resp->type == MOCK_TOOL_CALLS) {
            openai_responses_serialize_tool_calls(tmp, resp->tool_calls,
                                                  resp->tool_call_count,
                                                  client_fd);
        }

        talloc_free(resp);
        talloc_free(tmp);
        return;
    }

    /* Unknown route */
    http_respond_error(client_fd, 404, "Not found");
    talloc_free(tmp);
}

int main(int argc, char **argv)
{
    install_signal_handlers();

    int port = parse_port(argc, argv);

    TALLOC_CTX *ctx = talloc_new(NULL);
    assert(ctx != NULL);

    mock_queue_t *queue = mock_queue_create(ctx);

    int listen_fd = create_listen_socket(port);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to create listen socket on port %d\n",
                port);
        talloc_free(ctx);
        return 1;
    }

    fprintf(stderr, "mock-provider listening on port %d\n", port);

    char buf[MAX_REQUEST_SIZE];

    while (!g_shutdown) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ready = select(listen_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        if (ready == 0) continue;

        if (FD_ISSET(listen_fd, &read_fds)) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                continue;
            }

            ssize_t n = read_request(client_fd, buf, sizeof(buf));
            if (n > 0) {
                handle_request(ctx, client_fd, queue, buf, (size_t)n);
            }

            close(client_fd);
        }
    }

    close(listen_fd);
    talloc_free(ctx);

    fprintf(stderr, "mock-provider shut down\n");
    return 0;
}
