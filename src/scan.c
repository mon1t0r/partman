#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "log.h"

enum {
    /* Scan buffer size, in bytes */
    scan_buf_sz = 256
};

enum scan_res scan_int(const char *format, void *int_ptr)
{
    char buf[scan_buf_sz];
    char *s;
    int i;

    s = fgets(buf, sizeof(buf), stdin);

    /* EOF or error */
    if(s == NULL) {
        /* Reset EOF */
        if(feof(stdin)) {
            clearerr(stdin);
            return scan_eof;
        }
        return scan_fail;
    }

    i = sscanf(s, format, int_ptr);

    /* Empty string */
    if(i == EOF) {
        return scan_empty;
    }

    /* Error */
    if(i != 1) {
        return scan_fail;
    }

    return scan_ok;
}

enum scan_res scan_char(char *c)
{
    char buf[scan_buf_sz];
    char *s;

    s = fgets(buf, sizeof(buf), stdin);

    /* EOF or error */
    if(s == NULL) {
        /* Reset EOF */
        if(feof(stdin)) {
            clearerr(stdin);
            return scan_eof;
        }
        return scan_fail;
    }

    /* Empty string */
    if(s[0] == '\0') {
        return scan_empty;
    }

    /* Error */
    if(strlen(s) != 2 || s[1] != '\n') {
        return scan_fail;
    }

    *c = s[0];

    return scan_ok;
}

#define FUNC_DEFINE_PROMPT_RANGE(TYPE, CONV_SPEC)                          \
enum scan_res                                                              \
prompt_range_##TYPE(const char *prompt, TYPE *int_ptr, TYPE start,         \
                    TYPE end, TYPE def)                                    \
{                                                                          \
    enum scan_res res;                                                     \
                                                                           \
    pprint("%s (" #CONV_SPEC "-" #CONV_SPEC ", default " #CONV_SPEC "): ", \
           prompt, start, end, def);                                       \
                                                                           \
    res = scan_int(#CONV_SPEC, int_ptr);                                   \
                                                                           \
    if(res == scan_empty) {                                                \
        *int_ptr = def;                                                    \
        return scan_ok;                                                    \
    }                                                                      \
                                                                           \
    if(res != scan_ok) {                                                   \
        if(res != scan_eof) {                                              \
            pprint("Invalid value");                                       \
        }                                                                  \
        return res;                                                        \
    }                                                                      \
                                                                           \
    if(*int_ptr < start || *int_ptr > end) {                               \
        pprint("Value out of range");                                      \
        return scan_fail;                                                  \
    }                                                                      \
                                                                           \
    return scan_ok;                                                        \
}

FUNC_DEFINE_PROMPT_RANGE(pu32, %lu)

FUNC_DEFINE_PROMPT_RANGE(pu64, %llu)

#undef FUNC_DEFINE_PROMPT_RANGE

