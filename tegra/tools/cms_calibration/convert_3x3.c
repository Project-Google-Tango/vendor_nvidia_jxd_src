/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// quick way to convert float to S1.8 fixed for range [-2.0, 2.0)
// won't overflow, but may cause minimal rounding errors
unsigned short floatS18(float c) {
	unsigned short result;

	if (c >= 1.0f) {
		result = (unsigned short)roundf((c - 1.0f)*255.0f);
		result |= 0x100;
	} else if (c >= 0.0f) {
		result = (unsigned short)roundf(c * 255.0f);
	} else if (c > -1.0f) {
		result = (unsigned short)roundf((c + 1.0f)*255.0f);
		result |= 0x300;
	} else {
		result = (unsigned short)roundf((c + 2.0f)*255.0f);
		result |= 0x200;
	}

	return result;
}

int main(int argc, char **argv)
{
    float a00 = strtod(argv[1], NULL); float a01 = strtod(argv[2], NULL); float a02 = strtod(argv[3], NULL);
    float a10 = strtod(argv[4], NULL); float a11 = strtod(argv[5], NULL); float a12 = strtod(argv[6], NULL);
    float a20 = strtod(argv[7], NULL); float a21 = strtod(argv[8], NULL); float a22 = strtod(argv[9], NULL);

	printf("0x%.3X, 0x%.3X, 0x%.3X,\n0x%.3X, 0x%.3X, 0x%.3X,\n0x%.3X, 0x%.3X, 0x%.3X,\n",
			floatS18(a00), floatS18(a01), floatS18(a02),
			floatS18(a10), floatS18(a11), floatS18(a12),
			floatS18(a20), floatS18(a21), floatS18(a22)
			);

	return 0;
}

