#ifndef PARTMAN_SCAN_H
#define PARTMAN_SCAN_H

#include "partman_types.h"

enum scan_res {
    scan_ok, scan_empty, scan_eof, scan_fail
};

enum scan_res scan_char(char *c);

enum scan_res
prompt_range_pu32(const char *prompt, pu32 *int_ptr, pu32 start, pu32 end,
                  pu32 def);

enum scan_res
prompt_range_pu64(const char *prompt, pu64 *int_ptr, pu64 start, pu64 end,
                  pu64 def);

#endif
