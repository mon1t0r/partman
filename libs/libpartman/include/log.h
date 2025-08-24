#ifndef LIBPARTMAN_LOG_H
#define LIBPARTMAN_LOG_H

#define LOG_STR_DBG "DEBUG"
#define LOG_STR_INFO "INFO"
#define LOG_STR_WARN "WARN"
#define LOG_STR_ERR "ERROR"

enum log_level {
    log_debug = 4,
    log_info  = 3,
    log_warn  = 2,
    log_error = 1,
    log_none  = 0
};

void plog_set_level(enum log_level level);

void pprint(const char *format, ...);

void plog_dbg(const char *format, ...);

void plog_info(const char *format, ...);

void plog_warn(const char *format, ...);

void plog_err(const char *format, ...);

#endif

