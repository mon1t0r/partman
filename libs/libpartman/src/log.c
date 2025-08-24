#include <stdio.h>
#include <stdarg.h>

#include "log.h"

/* Global variable, that stores current log level */
static enum log_level cur_log_level;

void plog_set_level(enum log_level level) {
    cur_log_level = level;
}

void plog(const char *level, const char *format, va_list args)
{
    fprintf(stderr, "[%s] ", level);

    vfprintf(stderr, format, args);

    fputc('\n', stdout);
}

void pprint(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

void plog_dbg(const char *format, ...)
{
    va_list args;

    if(cur_log_level >= log_debug) {
        va_start(args, format);
        plog(LOG_STR_DBG, format, args);
        va_end(args);
    }
}

void plog_info(const char *format, ...)
{
    va_list args;

    if(cur_log_level >= log_info) {
        va_start(args, format);
        plog(LOG_STR_INFO, format, args);
        va_end(args);
    }
}

void plog_warn(const char *format, ...)
{
    va_list args;

    if(cur_log_level >= log_warn) {
        va_start(args, format);
        plog(LOG_STR_WARN, format, args);
        va_end(args);
    }
}

void plog_err(const char *format, ...)
{
    va_list args;

    if(cur_log_level >= log_error) {
        va_start(args, format);
        plog(LOG_STR_ERR, format, args);
        va_end(args);
    }
}

