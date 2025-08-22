#include <stdio.h>
#include <string.h>

#include "scan.h"

enum {
    /* Scan buffer size, in bytes */
    scan_buf_sz = 256
};

static pres scan_int(const char *format, void *int_ptr)
{
    char buf[scan_buf_sz];
    char *s;
    int i;

    s = fgets(buf, sizeof(buf), stdin);

    /* EOF or error */
    if(s == NULL) {
        return pres_fail;
    }

    i = sscanf(s, format, int_ptr);
    /* Error */
    if(i != 1) {
        return pres_fail;
    }

    return pres_ok;

}

int scan_char(void)
{
    char buf[scan_buf_sz];
    char *s;

    s = fgets(buf, sizeof(buf), stdin);

    /* EOF or error */
    if(s == NULL) {
        return -1;
    }

    /* Error */
    if(strlen(s) != 2 || s[1] != '\n') {
        return '\0';
    }

    return s[0];
}

pu32 scan_pu32(void)
{
    pres res;
    pu32 i;

    res = scan_int("%lu", &i);
    if(!res) {
        return 0;
    }

    return i;
}

pu64 scan_pu64(void)
{
    pres res;
    pu64 i;

    res = scan_int("%llu", &i);
    if(!res) {
        return 0;
    }

    return i;
}

