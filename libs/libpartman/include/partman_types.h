#ifndef LIBPARTMAN_COMMON_TYPES_H
#define LIBPARTMAN_COMMON_TYPES_H

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Partman minimum size unsigned types. Each type 'puN' must have AT LEAST
 * N bit width (except pu8, this type is required to be exactly 1 byte) */

typedef unsigned char pu8;

typedef unsigned short pu16;

typedef unsigned long pu32;

typedef unsigned long long pu64;

/* Partman minimum size signed types. Each type 'puN' must have AT LEAST
 * N bit width (except p8, this type is required to be exactly 1 byte) */

typedef signed char p8;

typedef signed short p16;

typedef signed long p32;

typedef signed long long p64;

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

/* Partman LBA type for function results (can be negative) */
typedef p64 plba_res;

/* Partman CHS type */
typedef pu32 pchs;

/* Partman UCS-2 character */
typedef pu32 pchar_ucs;

#endif
