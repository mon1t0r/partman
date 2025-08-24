#ifndef LIBPARTMAN_LOG_H
#define LIBPARTMAN_LOG_H

void pprint(const char *format, ...);

void plog_dbg(const char *format, ...);

void plog_info(const char *format, ...);

void plog_warn(const char *format, ...);

void plog_err(const char *format, ...);

#endif
