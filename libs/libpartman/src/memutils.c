#include "memutils.h"

void write_pu8(pu8 *buf, pu8 i)
{
    buf[0] = i & 0xFF;
}

void write_pu16(pu8 *buf, pu16 i)
{
    buf[0] = (i >> 0) & 0xFF;
    buf[1] = (i >> 8) & 0xFF;
}

void write_pu24(pu8 *buf, pu32 i)
{
    buf[0] = (i >> 0 ) & 0xFF;
    buf[1] = (i >> 8 ) & 0xFF;
    buf[2] = (i >> 16) & 0xFF;
}

void write_pu32(pu8 *buf, pu32 i)
{
    buf[0] = (i >> 0 ) & 0xFF;
    buf[1] = (i >> 8 ) & 0xFF;
    buf[2] = (i >> 16) & 0xFF;
    buf[3] = (i >> 24) & 0xFF;
}

void write_pu64(pu8 *buf, pu64 i)
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

pu8 read_pu8(const pu8 *buf)
{
    return buf[0];
}

pu16 read_pu16(const pu8 *buf)
{
    return ((pu16) buf[0] << 0) |
           ((pu16) buf[1] << 8);
}

pu32 read_pu24(const pu8 *buf)
{
    return ((pu32) buf[0] << 0)  |
           ((pu32) buf[1] << 8)  |
           ((pu32) buf[2] << 16);
}

pu32 read_pu32(const pu8 *buf)
{
    return ((pu32) buf[0] << 0)  |
           ((pu32) buf[1] << 8)  |
           ((pu32) buf[2] << 16) |
           ((pu32) buf[3] << 24);
}

pu64 read_pu64(const pu8 *buf)
{
    return ((pu64) buf[0] << 0)  |
           ((pu64) buf[1] << 8)  |
           ((pu64) buf[2] << 16) |
           ((pu64) buf[3] << 24) |
           ((pu64) buf[4] << 32) |
           ((pu64) buf[5] << 40) |
           ((pu64) buf[6] << 48) |
           ((pu64) buf[7] << 56);
}

