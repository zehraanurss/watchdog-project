#ifndef LOGGER_H
#define LOGGER_H

int logger_write(const char *event, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif
