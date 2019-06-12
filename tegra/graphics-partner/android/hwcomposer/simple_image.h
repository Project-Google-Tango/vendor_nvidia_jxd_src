#ifndef __SIMPLE_IMAGE
#define __SIMPLE_IMAGE

#include <stdint.h>

struct simple_image {
    int xsize;
    int ysize;
    int bpp;
    uint32_t len;
    uint8_t buf[];
};

#endif
