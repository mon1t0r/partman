## Overview
This program is a tool for editing partitioning schemes in raw image files or
directly on disk devices.

Currently supported partitioning schemes are:
 - MBR (no EBR support);
 - GPT.

The program uses 1 MiB alignment by default for aligning partition boundaries.
This ensures the optimal alignment for the most present hardware.

### Structure
The program consists of two parts: `partman` and `libpartman`. `partman` is a
dialog-driven command-line frontend for the library, heavily inspired by
`fdisk`. Most of the command keys and text dialogs were copied from `fdisk`, so
users, familiar with `fdisk` should have no problem in using `partman`.
`libpartman` is a statically linked library, which contains tools for working
with mentioned partitioning schemes.

The program is written in plain `ANSI C`, however `long long` type is required
in order to compile. The program should also be `endian-independent`, and `no
compiler extensions` are used.

Required basic type sizes are:
- `char`      - 1 byte;
- `short`     - at least 2 bytes;
- `int`       - at least 2 bytes;
- `long`      - at least 4 bytes;
- `long long` - at least 8 bytes;

### Note on C/H/S support
All addressing in the program is done using LBA, however when writing to the
MBR, correct C/H/S addresses are generated to correspond to the LBA values.
If LBA address is too large to be represented with C/H/S, then tuple
`(1023, 254, 63)` is used, except for the case, when writing an MBR partition
with type `0xEE` (GPT Protective). In this particular case, tuple
`(1023, 255, 63)` is used instead.

## Build and run
### Requirements
```
gcc
make
```

### Build
```
git clone https://github.com/mon1t0r/partman
cd partman
make
```

### Run
```
release/partman [OPTION]... [IMG_FILE]
```

### Options
```
IMG_FILE - image or device file

-L --log-level              log level (DEBUG, WARN, INFO, ERORR)
-b --sector-size            logical sector size, in bytes (512, 1024, 2048,
                            4096)
-m --min-img-size           minimal image size, in bytes. If set, than upon
                            program start, if actual image size is less, than
                            this parameter's value, image file will get
                            extended by writing a zero byte to the calculated
                            location. Do not use this parameter on device files
```

### Example usage
Create and open an image file `test.bin` with the size of 1 GiB:
```
release/partman -m1073741824 test.img
```

Open device `/dev/sda`:
```
sudo release/partman /dev/sda
```

Open existing image file `hdd.img`, which uses sector size of 4096 bytes:
```
release/partman -b4096 hdd.img
```

## TODO
 - perform testing on different images;
 - perform testing on different platforms.

