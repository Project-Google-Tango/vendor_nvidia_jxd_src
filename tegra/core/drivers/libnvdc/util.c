#define _ISOC99_SOURCE
#include <math.h>

#include <nvdc.h>
#include <nvassert.h>

ufixed_20_12_t pack_ufixed_20_12_f(double x)
{
    double integral, fractional = modf(x, &integral);
    return pack_ufixed_20_12((unsigned int)integral,
                             (unsigned int)(fractional * ((1 << 12) - 1)));
}
