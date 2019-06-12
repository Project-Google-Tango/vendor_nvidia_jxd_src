
from numpy import *

verbose = False

_bl_pwm_meas = [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 255]
_bl_luma_meas = [0, 0.743, 2.23, 6.569, 14.42, 26.6, 42.15, 58.55, 77.31, 98.45, 120, 142.8, 167.2, 193.8, 224, 257.7, 292.3, 326.8, 360.5, 393.3, 427.5, 459.5, 491.9, 520.9, 549.4, 575.1, 584.9]


if (verbose):
    print "_bl_luma_meas = ", _bl_luma_meas
    print "_bl_pwm_meas = ", _bl_pwm_meas

# verify that both _in and _out are monotonically increasing; otherwise, no
# unique reverse mapping exists

if ( all(diff(_bl_pwm_meas) > 0) == False ):
    print "$0: input pwm measurements not monotonically increasing"
    sys.exit(1)
if ( all(diff(_bl_luma_meas) > 0) == False ):
    print "$0: input luma measurements not monotonically increasing"
    sys.exit(1)

# desired luminance values

bl_luma_range = max(_bl_luma_meas) -  min(_bl_luma_meas)
bl_luma_target = min(_bl_luma_meas) + r_[0.:256.] * (bl_luma_range/255.)

print "bl_luma_target = ", bl_luma_target

# interplate pwm settings for each desired luma value
bl_pwm_target = interp(bl_luma_target, _bl_luma_meas, _bl_pwm_meas)

# round pwm settings to nearest integer; compute corresponding luma value
bl_pwm = bl_pwm_target.round(decimals=0, out=None)
bl_luma = interp(bl_pwm, _bl_pwm_meas, _bl_luma_meas)

print "bl_pwm = ", bl_pwm
print "bl_luma = ", bl_luma

if (verbose):
    print "delta bl_luma = ", bl_luma_target - bl_luma

# optional, debugging output
if (verbose):
    print "diff(bl_luma) = ", diff(bl_luma)
    print "diff(bl_pwm) = ", diff(bl_pwm)
    print "diff(_bl_luma_meas) = ", diff(_bl_luma_meas)
    print "diff(_bl_pwm_meas) = ", diff(_bl_pwm_meas)

import matplotlib.pyplot as plt
plt.plot(_bl_pwm_meas, _bl_luma_meas,'go')
plt.plot(bl_pwm, bl_luma, 'r-')
plt.plot(r_[0.:256.], bl_luma, 'b-')
plt.plot(r_[0.:256.], bl_luma_target, 'g-')
plt.xlabel('pwm')
plt.ylabel('luma')
plt.show()

