#ifndef READ_NETPBM_H
#define READ_NETPBM_H

#define READ_BYTE(FD, B) \
    if(!read(FD, &B, 1)) \
        return -1;

// Reads a file from fb and writes numbytes of it into dmem.
// Assumes destination will be Gfx8 formatted
void readPBMIntoGfx8(int fd, void* dmem);

// Reads a file from fb and writes numbytes of it into dmem.
// Assumes destination will be Gfx9 formatted
void readPGMIntoGfx9(int fd, void* tmem, void* dmem);

#endif // READ_NETPBM_H