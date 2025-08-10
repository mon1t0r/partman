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

void write_int24(unsigned char *buf, unsigned long i)
{
    buf[0] = (i >> 0 ) & 0xFF;
    buf[1] = (i >> 8 ) & 0xFF;
    buf[2] = (i >> 16) & 0xFF;
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

unsigned char read_int8(const unsigned char *buf)
{
    return buf[0];
}

unsigned short read_int16(const unsigned char *buf)
{
    return ((unsigned short) buf[0] << 0) |
           ((unsigned short) buf[1] << 8);
}

unsigned long read_int24(const unsigned char *buf)
{
    return ((unsigned long) buf[0] << 0)  |
           ((unsigned long) buf[1] << 8)  |
           ((unsigned long) buf[2] << 16);
}

unsigned long read_int32(const unsigned char *buf)
{
    return ((unsigned long) buf[0] << 0)  |
           ((unsigned long) buf[1] << 8)  |
           ((unsigned long) buf[2] << 16) |
           ((unsigned long) buf[3] << 24);
}

unsigned long long read_int64(const unsigned char *buf)
{
    return ((unsigned long long) buf[0] << 0)  |
           ((unsigned long long) buf[1] << 8)  |
           ((unsigned long long) buf[2] << 16) |
           ((unsigned long long) buf[3] << 24) |
           ((unsigned long long) buf[4] << 32) |
           ((unsigned long long) buf[5] << 40) |
           ((unsigned long long) buf[6] << 48) |
           ((unsigned long long) buf[7] << 56);
}
