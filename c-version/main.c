#include "http_server.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>

static volatile int g_running = 1;
static FILE *log_file = NULL;
static HttpServer *g_server = NULL;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
    if (g_server) {
        http_server_stop(g_server);
    }
    sleep(1);
    _exit(0);
}

int main(int argc, char *argv[])
{
    const char *port = "8080";

    if (argc > 1) {
        port = argv[1];
    }

    log_file = fopen("log.txt", "a");
    
    log_init(LOG_INFO);
    log_set_file(stderr);
    LOGI("Starting toxhttpd port %s", port);
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    HttpServer server;
    g_server = &server;
    if (http_server_init(&server, port) != 0) {
        LOGE("Failed to initialize HTTP server");
        return 1;
    }

    while (g_running) {
        http_server_poll(&server, 100);
    }

    LOGI("Shutting down...");
    http_server_destroy(&server);
    
    if (log_file) {
        fclose(log_file);
    }

    return 0;
}