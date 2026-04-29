#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
} LogLevel;

void log_init(LogLevel min_level);
void log_set_file(FILE *fp);
void log_set_level(LogLevel level);

void log_debug(const char *file, int line, const char *fmt, ...);
void log_info(const char *file, int line, const char *fmt, ...);
void log_warn(const char *file, int line, const char *fmt, ...);
void log_error(const char *file, int line, const char *fmt, ...);

#define LOGD(fmt, ...) log_debug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) log_info(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) log_warn(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) log_error(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif