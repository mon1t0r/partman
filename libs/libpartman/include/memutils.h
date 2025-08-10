#ifndef LIBPARTMAN_MEMUTILS_H
#define LIBPARTMAN_MEMUTILS_H

#include "partman_types.h"

void write_pu8(pu8 *buf, pu8 i);

void write_pu16(pu8 *buf, pu16 i);

void write_pu24(pu8 *buf, pu32 i);

void write_pu32(pu8 *buf, pu32 i);

void write_pu64(pu8 *buf, pu64 i);

pu8 read_pu8(const pu8 *buf);

pu16 read_pu16(const pu8 *buf);

pu32 read_pu24(const pu8 *buf);

pu32 read_pu32(const pu8 *buf);

pu64 read_pu64(const pu8 *buf);

#endif
