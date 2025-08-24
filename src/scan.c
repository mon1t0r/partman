#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "log.h"

enum {
    /* Scan buffer size, in bytes */
    scan_buf_sz = 256
};

static enum scan_res scan_int(const char *format, void *int_ptr)
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

enum scan_res
prompt_range_pu32(const char *prompt, pu32 *int_ptr, pu32 start, pu32 end,
                  pu32 def)
{
    enum scan_res res;

    pprint("%s (%lu-%lu, default %lu): ", prompt, start, end, def);

    res = scan_int("%lu", int_ptr);

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

    if(*int_ptr < start || *int_ptr > end) {
        pprint("Value out of range");
        return scan_fail;
    }

    return scan_ok;
}

enum scan_res
prompt_range_pu64(const char *prompt, pu64 *int_ptr, pu64 start, pu64 end,
                  pu64 def)
{
    enum scan_res res;

    pprint("%s (%llu-%llu, default %llu): ", prompt, start, end, def);

    res = scan_int("%lu", int_ptr);

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

    if(*int_ptr < start || *int_ptr > end) {
        pprint("Value out of range");
        return scan_fail;
    }

    return scan_ok;
}

