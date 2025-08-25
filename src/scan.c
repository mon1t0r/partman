#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "log.h"

enum {
    /* Scan buffer size, in bytes */
    scan_buf_sz = 256
};

enum scan_res scan_str(char *buf, int buf_sz)
{
    buf = fgets(buf, buf_sz, stdin);

    /* EOF of error */
    if(buf == NULL) {
        if(feof(stdin)) {
            /* Reset EOF */
            clearerr(stdin);
            return scan_eof;
        }
        return scan_fail;
    }

    /* Remove newline at the end, if present */
    buf[strcspn(buf, "\n\r")] = '\0';

    /* Empty string */
    if(buf[0] == '\0') {
        return scan_empty;
    }

    return scan_ok;
}

enum scan_res scan_char(char *c)
{
    char buf[scan_buf_sz];
    enum scan_res res;
    int i;

    res = scan_str(buf, sizeof(buf));
    if(res != scan_ok) {
        return res;
    }

    i = sscanf(buf, " %c", c);

    /* Error */
    if(i != 1) {
        return scan_fail;
    }

    return scan_ok;
}

enum scan_res scan_guid(struct guid *guid)
{
    char buf[scan_buf_sz];
    enum scan_res scan_res;
    pres proc_res;

    scan_res = scan_str(buf, sizeof(buf));
    if(scan_res != scan_ok) {
        return scan_res;
    }

    proc_res = str_to_guid(buf, guid);

    return proc_res ? scan_ok : scan_fail;
}

enum scan_res scan_int(const char *format, void *int_ptr)
{
    char buf[scan_buf_sz];
    enum scan_res res;
    int i;

    res = scan_str(buf, sizeof(buf));
    if(res != scan_ok) {
        return res;
    }

    i = sscanf(buf, format, int_ptr);

    /* Error */
    if(i != 1) {
        return scan_fail;
    }

    return scan_ok;
}

enum scan_res
prompt_sector_ext(const char *prompt, plba *int_ptr, plba start, plba end,
                  plba def)
{
    char buf[scan_buf_sz];
    enum scan_res res;
    int i;
    char sign_c;
    char mod_c;

    pprint("%s, +/-sectors or +/-size{K,M,G,T,P} (%llu-%llu, default %llu): ",
           prompt, start, end, def);

    res = scan_str(buf, sizeof(buf));

    if(res == scan_empty) {
        *int_ptr = def;
        return scan_ok;
    }

    if(res != scan_ok) {
        if(res != scan_eof) {
            pprint("Invalid value");
        }
        return res;
    }

    /* TODO: Implement correct parsing */

    i = sscanf(buf, " %c%llu%c", &sign_c, int_ptr, &mod_c);

    /* Error */
    if(i != 3) {
        pprint("Invalid value");
        return scan_fail;
    }

    if(*int_ptr < start || *int_ptr > end) {
        pprint("Value out of range");
        return scan_fail;
    }

    return scan_ok;
}

#define FUNC_DEFINE_PROMPT_RANGE(TYPE, CONV_SPEC)                          \
enum scan_res                                                              \
prompt_range_##TYPE(const char *prompt, TYPE *int_ptr, TYPE start,         \
                    TYPE end, TYPE def)                                    \
{                                                                          \
    enum scan_res res;                                                     \
                                                                           \
    pprint("%s (" CONV_SPEC "-" CONV_SPEC ", default " CONV_SPEC "): ",    \
           prompt, start, end, def);                                       \
                                                                           \
    res = scan_int(" " CONV_SPEC, int_ptr);                                \
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

FUNC_DEFINE_PROMPT_RANGE(pu32, "%lu")

FUNC_DEFINE_PROMPT_RANGE(pu64, "%llu")

#undef FUNC_DEFINE_PROMPT_RANGE

