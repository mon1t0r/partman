#include <stdio.h>
#include <string.h>

#include "scan.h"

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
        return scan_eof;
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

enum scan_res scan_char(const char *prompt, char *c)
{
    char buf[scan_buf_sz];
    char *s;

    printf("%s: ", prompt);

    s = fgets(buf, sizeof(buf), stdin);

    /* EOF or error */
    if(s == NULL) {
        return scan_eof;
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

enum scan_res scan_pu32(const char *prompt, pu32 *int_ptr)
{
    printf("%s: ", prompt);
    return scan_int("%lu", int_ptr);
}

enum scan_res scan_pu64(const char *prompt, pu64 *int_ptr)
{
    printf("%s: ", prompt);
    return scan_int("%llu", int_ptr);
}

enum scan_res scan_range_pu32(const char *prompt, pu32 *int_ptr, pu32 start,
                              pu32 end, pu32 def)
{
    enum scan_res res;

    printf("%s (%ld-%ld, default %ld): ", prompt, start, end, def);

    res = scan_int("%lu", int_ptr);

    if(res == scan_empty) {
        *int_ptr = def;
        return scan_ok;
    }

    if(res != scan_ok) {
        return res;
    }

    if(*int_ptr < start || *int_ptr > end) {
        fprintf(stderr, "Value is out of range\n");
        return scan_fail;
    }

    return scan_ok;
}

enum scan_res scan_range_pu64(const char *prompt, pu64 *int_ptr, pu64 start,
                              pu64 end, pu64 def)
{
    enum scan_res res;

    printf("%s (%lld-%lld, default %lld): ", prompt, start, end, def);

    res = scan_int("%lu", int_ptr);

    if(res == scan_empty) {
        *int_ptr = def;
        return scan_ok;
    }

    if(res != scan_ok) {
        return res;
    }

    if(*int_ptr < start || *int_ptr > end) {
        fprintf(stderr, "Value is out of range\n");
        return scan_fail;
    }

    return scan_ok;
}

