#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static LogLevel g_min_level = LOG_INFO;
static FILE *g_log_file = NULL;
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_init(LogLevel min_level)
{
    g_min_level = min_level;
}

void log_set_file(FILE *fp)
{
    g_log_file = fp;
}

void log_set_level(LogLevel level)
{
    g_min_level = level;
}

static void log_write(LogLevel level, const char *file, int line, const char *fmt, va_list ap)
{
    if (level < g_min_level) return;

    const char *level_str;
    switch (level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO:  level_str = "INFO "; break;
        case LOG_WARN:  level_str = "WARN "; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        default:        level_str = "?????"; break;
    }

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    const char *filename = file;
    for (const char *p = file; *p; p++) {
        if (*p == '/') filename = p + 1;
    }

    pthread_mutex_lock(&g_log_mutex);
    
    FILE *out = g_log_file ? g_log_file : stderr;
    fprintf(out, "[%s] [%s] (%s:%d) ", time_str, level_str, filename, line);
    vfprintf(out, fmt, ap);
    fprintf(out, "\n");
    fflush(out);
    
    pthread_mutex_unlock(&g_log_mutex);
}

void log_debug(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_DEBUG, file, line, fmt, ap);
    va_end(ap);
}

void log_info(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_INFO, file, line, fmt, ap);
    va_end(ap);
}

void log_warn(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_WARN, file, line, fmt, ap);
    va_end(ap);
}

void log_error(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write(LOG_ERROR, file, line, fmt, ap);
    va_end(ap);
}