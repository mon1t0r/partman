#ifndef LIBPARTMAN_MEMUTILS_H
#define LIBPARTMAN_MEMUTILS_H

void write_int8(unsigned char *buf, unsigned char i);

void write_int16(unsigned char *buf, unsigned short i);

void write_int24(unsigned char *buf, unsigned long i);

void write_int32(unsigned char *buf, unsigned long i);

void write_int64(unsigned char *buf, unsigned long long i);

unsigned char read_int8(const unsigned char *buf);

unsigned short read_int16(const unsigned char *buf);

unsigned long read_int24(const unsigned char *buf);

unsigned long read_int32(const unsigned char *buf);

unsigned long long read_int64(const unsigned char *buf);

#endif
