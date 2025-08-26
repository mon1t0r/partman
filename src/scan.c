#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "scan.h"
#include "log.h"

enum {
    /* Scan buffer size, in bytes */
    scan_buf_sz = 256
};

static char *str_trim(char *buf)
{
    char *s;
    int len;

    s = buf;

    /* Remove leading whitespace chars */
    while(isspace(*s)) {
        s++;
    }

    len = strlen(s);

    /* Remove trailing whitespace chars */
    while(isspace(s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }

    return s;
}

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

    /* Error scanning character */
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
prompt_sector_ext(const char *prompt, plba start, plba end, plba def,
                  pu64 sec_sz, plba *int_ptr)
{
    static const char modifiers[] = { 'K', 'M', 'G', 'T', 'P' };

    char buf[scan_buf_sz];
    enum scan_res res;
    char *s;
    int i;
    char mod_c;
    plba mod_val;
    char sign_c;

    pprint("%s, +/-sectors or +/-size{K,M,G,T,P} (%llu-%llu, default %llu): ",
           prompt, start, end, def);

    res = scan_str(buf, sizeof(buf));

    if(res == scan_empty) {
        *int_ptr = def;

        res = scan_ok;
        goto exit;
    }

    if(res != scan_ok) {
        goto exit;
    }

    s = str_trim(buf);
    i = strlen(s);

    if(i < 1) {
        res = scan_fail;
        goto exit;
    }

    /* Extract the modifier character, if present */
    if(!isdigit(s[i - 1])) {
        mod_c = s[i - 1];
        s[i - 1] = '\0';
    } else {
        mod_c = '\0';
    }

    /* Extract the sign character, if present */
    if(!isdigit(s[0])) {
        sign_c = s[0];
        s++;
    } else {
        sign_c = '\0';
    }

    /* Modifier character must only be with sign character */
    if(mod_c && !sign_c) {
        res = scan_fail;
        goto exit;
    }

    i = sscanf(s, "%llu", int_ptr);

    /* Error scanning integer */
    if(i != 1) {
        res = scan_fail;
        goto exit;
    }

    /* Process modifier character */
    if(mod_c) {
        mod_val = 0;

        for(i = 0; i < ARRAY_SIZE(modifiers); i++) {
            if(toupper(mod_c) != modifiers[i]) {
                continue;
            }

            /* Modifier value is 2^(10 * (i + 1)) */
            mod_val = 1 << (10 * (i + 1));
        }

        if(mod_val == 0) {
            res = scan_fail;
            goto exit;
        }

        /* Multiply integer with modifier value - result is value in bytes */
        (*int_ptr) *= mod_val;

        /* Divide integer with sector size - result is value in sectors */
        (*int_ptr) /= sec_sz;
    }

    /* Process sign character */
    if(sign_c) {
        switch(sign_c) {
            case '+':
                (*int_ptr) += start - 1;
                break;

            case '-':
                *int_ptr = end - *int_ptr;
                break;

            default:
                res = scan_fail;
                goto exit;
        }
    }

    if(*int_ptr < start || *int_ptr > end) {
        pprint("Value out of range");
        return scan_fail;
    }

    res = scan_ok;

exit:
    if(res != scan_ok && res != scan_eof) {
        pprint("Invalid value");
    }
    return res;
}

#define FUNC_DEFINE_PROMPT_RANGE(TYPE, CONV_SPEC)                          \
enum scan_res                                                              \
prompt_range_##TYPE(const char *prompt, TYPE start, TYPE end, TYPE def,    \
                    TYPE *int_ptr)                                         \
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

