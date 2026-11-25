#define _POSIX_C_SOURCE 200809L
#include "../include/logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

int logger_write(const char *event, const char *format, ...)
{
    char timestamp[32];
    time_t now = time(NULL);
    struct tm local_time;
    if (now == (time_t)-1 || localtime_r(&now, &local_time) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                 &local_time) == 0) {
        fprintf(stderr, "watchdog.log: Could not format the timestamp.\n");
        return -1;
    }

    FILE *log_file = fopen("watchdog.log", "a");
    if (log_file == NULL) {
        perror("watchdog.log");
        return -1;
    }

    va_list args;
    va_start(args, format);
    int failed = fprintf(log_file, "[%s] [%s] ", timestamp, event) < 0 ||
                 vfprintf(log_file, format, args) < 0 ||
                 fputc('\n', log_file) == EOF;
    va_end(args);

    if (fclose(log_file) == EOF || failed) {
        fprintf(stderr, "watchdog.log: Could not write the log record.\n");
        return -1;
    }
    return 0;
}
