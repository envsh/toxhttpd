#include "http_server.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static volatile int g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    const char *port = DEFAULT_HTTP_PORT;

    if (argc > 1) {
        port = argv[1];
    }

    printf("Starting toxhttpd on port %s\n", port);
    printf("REST API:   http://localhost:%s/api/*\n", port);
    printf("SSE:        http://localhost:%s/events/sse\n", port);
    printf("WebSocket:  ws://localhost:%s/events/ws\n", port);
    printf("Long Poll:  http://localhost:%s/events/poll\n", port);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    HttpServer server;
    if (http_server_init(&server, port) != 0) {
        fprintf(stderr, "Failed to initialize HTTP server\n");
        return 1;
    }

    while (g_running) {
        http_server_poll(&server, 100);
    }

    printf("\nShutting down...\n");
    http_server_destroy(&server);

    return 0;
}