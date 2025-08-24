#include <stdlib.h>

#include "rand.h"

pu8 rand_8(void)
{
    return rand() / (RAND_MAX / 0x100 + 1);
}

pu16 rand_16(void)
{
    return (((pu16) rand_8()) << 0 ) |
           (((pu16) rand_8()) << 8 );
}

pu32 rand_32(void)
{
    return (((pu32) rand_8()) << 0 ) |
           (((pu32) rand_8()) << 8 ) |
           (((pu32) rand_8()) << 16) |
           (((pu32) rand_8()) << 24);
}

