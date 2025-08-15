#ifndef LIBPARTMAN_COMMON_TYPES_H
#define LIBPARTMAN_COMMON_TYPES_H

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Partman minimum size unsigned types. Each type 'puN' must have AT LEAST
 * N bit width */

typedef unsigned char pu8;

typedef unsigned short pu16;

typedef unsigned long pu32;

typedef unsigned long long pu64;

/* Partman result type. Should be used as return value of subroutines,
 * which can execute successfully or fail */
typedef enum {
    pres_fail = 0,
    pres_ok
} pres;

/* Partman flag type. Should be used when representation of a logical value
 * is required */
typedef int pflag;

/* Partman LBA type */
typedef pu64 plba;

/* Partman LBA type for MBR */
typedef pu32 plba_mbr;

/* Partman CHS type */
typedef pu32 pchs;

/* Partman UCS-2 character */
typedef pu32 pchar_ucs;

#endif
