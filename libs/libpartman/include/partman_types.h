#ifndef LIBPARTMAN_COMMON_TYPES_H
#define LIBPARTMAN_COMMON_TYPES_H

/* Partman minimum size unsigned types. Each type 'puN' must have AT LEAST
 * N bit width. */

typedef unsigned char pu8;

typedef unsigned short pu16;

typedef unsigned long pu32;

typedef unsigned long long pu64;

/* Partman result type. Should be used as return value of subroutines,
 * which can execute successfully or fail */
typedef enum {
    pres_fail = 0,
    pres_succ
} pres;

/* Partman flag type. Should be used when representation of a logical value
 * is required */
typedef int pflag;

#endif
