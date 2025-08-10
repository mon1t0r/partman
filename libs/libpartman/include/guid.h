#ifndef LIBPARTMAN_GUID_H
#define LIBPARTMAN_GUID_H

/* Globally Unique Identifier (GUID) structure */
struct guid {

    unsigned long time_lo;
    unsigned short time_mid;
    unsigned short time_hi_ver;
    unsigned char cl_seq_hi_res;
    unsigned char cl_seq_lo;
    unsigned char node[6];
};

#endif
