#include "memutils.h"

void write_int8(unsigned char *buf, unsigned char i)
{
    buf[0] = i & 0xFF;
}

void write_int16(unsigned char *buf, unsigned short i)
{
    buf[0] = (i >> 0) & 0xFF;
    buf[1] = (i >> 8) & 0xFF;
}

void write_int32(unsigned char *buf, unsigned long i)
{
    buf[0] = (i >> 0 ) & 0xFF;
    buf[1] = (i >> 8 ) & 0xFF;
    buf[2] = (i >> 16) & 0xFF;
    buf[3] = (i >> 24) & 0xFF;
}

void write_int64(unsigned char *buf, unsigned long long i)
{
    buf[0] = (i >> 0 ) & 0xFF;
    buf[1] = (i >> 8 ) & 0xFF;
    buf[2] = (i >> 16) & 0xFF;
    buf[3] = (i >> 24) & 0xFF;
    buf[4] = (i >> 32) & 0xFF;
    buf[5] = (i >> 40) & 0xFF;
    buf[6] = (i >> 48) & 0xFF;
    buf[7] = (i >> 56) & 0xFF;
}
