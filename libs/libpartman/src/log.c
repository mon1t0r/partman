#include <stdio.h>
#include <stdarg.h>

#include "log.h"

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

    va_start(args, format);
    plog("DEBUG", format, args);
    va_end(args);
}

void plog_info(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    plog("INFO", format, args);
    va_end(args);
}

void plog_warn(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    plog("WARN", format, args);
    va_end(args);
}

void plog_err(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    plog("ERROR", format, args);
    va_end(args);
}

