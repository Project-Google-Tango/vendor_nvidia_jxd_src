#!/usr/bin/python
#
# Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
#

from scipy.interpolate import interp1d
import scipy.interpolate as scint

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.image as mpimg

import sys
import argparse

THRESHOLD = 512
PRECISION_DIFF = 12 - 9
OVERLAP = THRESHOLD >> PRECISION_DIFF

lut1_input_precision = 8
lut1_output_precision = 12
lut2_output_precision = 8

lut1_size = (1 << lut1_input_precision)
lut2_size = (1 << 10) - OVERLAP
lut1_output_size = 1 << lut1_output_precision
lut2_output_size = 1 << lut2_output_precision

lut1_input_max = lut1_size - 1
lut1_output_max = (lut1_size - 1) << (lut1_output_precision - lut1_input_precision)
lut1_output_max_full = ((lut1_output_size - 1) / float(1 << (lut1_output_precision - lut1_input_precision)))
lut2_output_max = lut1_input_max

class Args_generic:
    lut2_fill = None

class Args_sRGB:
    interpolation = None
    samples = None
    spline_smoothing = None
    oversampling = None

class Args_device:
    interpolation = None
    spline_smoothing = None
    oversampling = None

class Args_plot:
    grays_duplicate = None
    grays_missing = None
    grays_curve = None
    grays_aspect_ratio = None

def phi(y=2.4, a=0.055):
    return (((1 + a) ** y) * (y - 1) ** (y - 1)) / ((a ** (y - 1)) * (y**y))

def f_gamma_sRGB_decode(color):
    a = 0.055
    if(color <= 0.04045):
        return color / 12.92
    else:
        return ((color + a) / (1 + a)) ** 2.4

def f_gamma_sRGB_encode(color):
    a = 0.055
    if(color <= 0.0031308):
        return color * 12.92
    else:
        return (1 + a) * (color ** (1 / 2.4)) - a

def f_gamma_2p2_decode(color):
    return color ** 2.2

def f_gamma_2p2_encode(color):
    return color ** (1 / 2.2)

def f_gamma_custom_decode(color, gamma=2.0):
    return color ** gamma

def f_gamma_custom_encode(color, gamma=2.0):
    return color ** (1 / float(gamma))

def f_gamma_custom_encode_lins(color, gamma=2.0):
    a = 0.035
    if(color <= 0.0031308):
        return color * phi(gamma, a)
    else:
        return (1 + a) * (color ** (1 / gamma)) - a

def linear_interpolate(x, xp, yp):
    for ii in range(len(xp)):
        if x == xp[ii]:
            return yp[ii]

        if x < xp[ii]:
            if ii == 0:
                return yp[0]

            return yp[ii - 1] + (x - xp[ii - 1]) * (yp[ii] - yp[ii - 1]) / float(xp[ii] - xp[ii - 1])

        if ii == len(xp) - 1:
            return yp[-1]

def spline_interpolate(fin, fout, frange=None, num_samples=lut1_output_size, splrep_s=None):
    weights = map(lambda x: 1, range(len(fin)))

    if frange == None:
        frange = (fin[0], fin[-1])

    if splrep_s == None:
        splrep_s = ((fin[-1] - fin[0]) / float(num_samples)) ** 2

    rep = scint.splrep(fin, fout, w=weights, s=splrep_s)
    fin_new = np.linspace(frange[0], frange[1], num_samples)
    fout_new = scint.splev(fin_new, rep, der=0)

    return (fin_new, fout_new)

def zone_select(cc):
    if(cc >= THRESHOLD):
        return (cc >> PRECISION_DIFF) + THRESHOLD - OVERLAP
    else:
        return cc

def inverse_zone_select_first(cc):
    if(cc >= THRESHOLD):
        return (cc - THRESHOLD + OVERLAP) << PRECISION_DIFF
    else:
        return cc

def inverse_zone_select_mid(cc):
    if(cc >= THRESHOLD):
        return ((cc - THRESHOLD + OVERLAP) << PRECISION_DIFF) + ((1 << PRECISION_DIFF ) / 2)
    else:
        return cc

inverse_zone_select = inverse_zone_select_mid

def convert_float_to_fp(ff, mm, nn):
    return int(ff * (2 ** nn)) & ((1 << (mm + nn)) - 1)

def convert_fp_to_float(ff, mm, nn):
    return float(ff) * (2 ** -nn)

def gen_lut1_from_func(func):
    lut1 = [None] * lut1_size

    for ii in range(lut1_size):
        lut1[ii] = int(round((func(ii / float(lut1_input_max)) * lut1_output_max)))

    lut1 = np.clip(lut1, 0, lut1_output_max)

    return lut1

def gen_lut1_from_func_hd(func):
    lut1_acc = [None] * lut1_size
    lut1 = [None] * lut1_size

    shift = 4

    for ii in range(lut1_size << shift):
        if lut1_acc[ii >> shift] == None:
            lut1_acc[ii >> shift] = [0, 0]

        lut1_acc[ii >> shift][0] += func(ii / float(lut1_input_max << shift)) * lut1_output_max
        lut1_acc[ii >> shift][1] += 1

    for ii in range(lut1_size):
        lut1[ii] = int(round(lut1_acc[ii][0] / float(lut1_acc[ii][1])))

    lut1 = np.clip(lut1, 0, lut1_output_max)

    return lut1

def gen_lut2_from_func_izs(func):
    lut2 = [None] * lut2_size

    for ii in range(lut2_size):
        lut2[ii] = int(round(func(inverse_zone_select(ii) / float(lut1_output_max)) * lut2_output_max))

    lut2 = np.clip(lut2, 0, lut2_output_max)

    return lut2

def gen_lut2_from_func_avg(func):
    lut2_acc = [None] * lut2_size
    lut2 = [None] * lut2_size

    for ii in range(lut1_output_max + 1):
        if lut2_acc[zone_select(ii)] == None:
            lut2_acc[zone_select(ii)] = [0, 0]

        lut2_acc[zone_select(ii)][0] += func(ii / float(lut1_output_max)) * lut2_output_max
        lut2_acc[zone_select(ii)][1] += 1

    for ii in range(lut1_output_size):
        if lut2_acc[zone_select(ii)] != None:
            lut2[zone_select(ii)] = int(round(lut2_acc[zone_select(ii)][0] / float(lut2_acc[zone_select(ii)][1])))
        else:
            lut2[zone_select(ii)] = lut2_output_max

    lut2 = np.clip(lut2, 0, lut2_output_max)

    return lut2

def gen_lut2_from_samples_izs(gamma_in_scaled, gamma_out, shift, interp, splrep_s=None):
    lut2 = [lut2_output_max] * lut2_size

    gamma_out_scaled_full = gamma_out / float(lut1_output_max_full)
    gamma_in_scaled_full = gamma_in_scaled * lut1_input_max  / float(lut1_output_max_full)

    if interp == "spline":
        gamma_in_spline, gamma_out_i = spline_interpolate(gamma_in_scaled_full, gamma_out_scaled_full, [0.0, 1.0], lut1_output_size, splrep_s)
    elif interp == "linear":
        gamma_out_i = map(lambda x: linear_interpolate(x, gamma_in_scaled_full, gamma_out_scaled_full), np.linspace(0.0, 1.0, lut1_output_size))
    else:
        raise Exception("Invalid interpolation method")

    for ii in range(lut2_size):
        lut2[ii] = int(round(gamma_out_i[inverse_zone_select(ii)] * lut1_output_max_full))

    lut2 = np.clip(lut2, 0, lut2_output_max)

    return lut2

def gen_lut2_from_samples_avg(gamma_in_scaled, gamma_out, shift, interp, splrep_s=None):
    lut2_acc = [None] * lut2_size
    lut2 = [None] * lut2_size

    gamma_out_scaled_full = gamma_out / float(lut1_output_max_full)
    gamma_in_scaled_full = gamma_in_scaled * lut1_input_max  / float(lut1_output_max_full)

    if interp == "spline":
        gamma_in_spline, gamma_out_i = spline_interpolate(gamma_in_scaled_full, gamma_out_scaled_full, [0.0, 1.0], lut1_output_size, splrep_s)
    elif interp == "linear":
        gamma_out_i = map(lambda x: linear_interpolate(x, gamma_in_scaled_full, gamma_out_scaled_full), np.linspace(0.0, 1.0, lut1_output_size))
    else:
        raise Exception("Invalid interpolation method")

    for ii in range(lut1_output_size):
        if lut2_acc[zone_select(ii)] == None:
            lut2_acc[zone_select(ii)] = [0, 0]

        lut2_acc[zone_select(ii)][0] += gamma_out_i[ii] * lut1_output_max_full
        lut2_acc[zone_select(ii)][1] += 1

    for ii in range(lut1_output_size):
        if lut2_acc[zone_select(ii)] != None:
            lut2[zone_select(ii)] = int(round(lut2_acc[zone_select(ii)][0] / float(lut2_acc[zone_select(ii)][1])))
        else:
            lut2[zone_select(ii)] = lut2_output_max

    lut2 = np.clip(lut2, 0, lut2_output_max)

    return lut2

def gen_lut2_from_samples_avg_hd(gamma_in_scaled, gamma_out, shift, interp, splrep_s=None):
    lut2_acc = [None] * lut2_size
    lut2 = [None] * lut2_size

    gamma_out_scaled_full = gamma_out / float(lut1_output_max_full)
    gamma_in_scaled_full = gamma_in_scaled * lut1_input_max  / float(lut1_output_max_full)

    if interp == "spline":
        gamma_in_spline, gamma_out_i = spline_interpolate(gamma_in_scaled_full, gamma_out_scaled_full, [0.0, 1.0], lut1_output_size << shift, splrep_s)
    elif interp == "linear":
        gamma_out_i = map(lambda x: linear_interpolate(x, gamma_in_scaled_full, gamma_out_scaled_full), np.linspace(0.0, 1.0, lut1_output_size << shift))
    else:
        raise Exception("Invalid interpolation method")

    for ii in range(lut1_output_size << shift):
        if lut2_acc[zone_select(ii >> shift)] == None:
            lut2_acc[zone_select(ii >> shift)] = [0, 0]

        lut2_acc[zone_select(ii >> shift)][0] += gamma_out_i[ii] * lut1_output_max_full
        lut2_acc[zone_select(ii >> shift)][1] += 1

    for ii in range(lut1_output_size):
        if lut2_acc[zone_select(ii)] != None:
            lut2[zone_select(ii)] = int(round(lut2_acc[zone_select(ii)][0] / float(lut2_acc[zone_select(ii)][1])))
        else:
            lut2[zone_select(ii)] = lut2_output_max

    lut2 = np.clip(lut2, 0, lut2_output_max)

    return lut2

def new_plot_with_title(title, window=True, axes=True):
    fig = plt.figure()

    if window:
        fig.canvas.set_window_title(title)

    if axes:
        plt.title(title)

def plot_grays(lut1, lut2, plot, title):
    grays_out = [None] * lut2_output_size
    grays_img = [[1.0, 1.0, 0.0]] * lut2_output_size
    grays_plt = [None] * lut2_output_size

    maxv = 1.0
    offset = 0.5

    if title:
        new_plot_with_title(title)

    for ii in range(len(grays_out)):
        grays_out[ii] = lut2[zone_select(lut1[ii])]

    plt.xlim(xmin=0, xmax=lut2_output_max)
    plt.ylim(ymin=0, ymax=lut2_output_max)

    prev_val = None
    for ii in range(lut2_output_size):
        if plot.grays_missing:
            grays_img[grays_out[ii]] = [grays_out[ii] / float(lut2_output_max)] * 3

            if plot.grays_duplicate and grays_out[ii] == prev_val:
                grays_img[grays_out[ii]] = [1.0, 0.0, 0.0]

        else:
            grays_img[ii] = [grays_out[ii] / float(lut2_output_max)] * 3

            if plot.grays_duplicate and grays_out[ii] == prev_val:
                grays_img[ii] = [1.0, 0.0, 0.0]

        grays_plt[ii] = grays_out[ii]
        prev_val = grays_out[ii]

    grays_img_t = np.tile(np.array(grays_img), (lut2_output_size, 1, 1))

    img_plot = plt.imshow(grays_img_t, origin='lower', aspect=plot.grays_aspect_ratio)
    img_plot.set_interpolation('nearest')

    for ii in range(lut2_output_size):
        if grays_plt[ii] == None:
            for jj in reversed(range(ii)):
                if grays_plt[jj] != None:
                    grays_plt[ii] = grays_plt[jj]
                    break
                if jj == 0:
                    grays_plt[ii] = 0

    marker_size = 4.0
    if plot.grays_curve:
        range_x = range(len(grays_out))
        plt.plot(range_x, grays_plt, ".-", label="lut2 output", color="b", markeredgewidth=0.0, markersize=marker_size)

    if plot.grays_missing:
        plt.plot(0, 0, "-", label="missing gray value", color="y")

    if plot.grays_duplicate:
        plt.plot(0, 0, "-", label="duplicate gray value", color="r")

    if plot.grays_duplicate or plot.grays_missing:
        plt.legend(loc=4)

def set_plot_zoom(zoom):
    margin = 16

    if zoom == 0:
        plt.xlim(xmin=-margin, xmax=(lut1_output_size - 1) + margin)
        plt.ylim(ymin=-margin, ymax=(lut1_output_size - 1) + margin)
    elif zoom == 1:
        plt.xlim(xmin=-margin, xmax=(lut2_size - 1) + margin)
        plt.ylim(ymin=-margin, ymax=(lut2_size - 1) + margin)
    elif zoom == 2:
        plt.xlim(xmin=-margin, xmax=(lut1_size - 1) + margin)
        plt.ylim(ymin=-margin, ymax=(lut1_size - 1) + margin)
    elif zoom == 3:
        plt.xlim(xmin=-margin, xmax=(36) + margin)
        plt.ylim(ymin=-margin, ymax=(36) + margin)
    else:
        raise Exception("Invalid zoom level")

def plot_gamma_reduced(lut1, lut2, title, zoom):
    if zoom == None:
        zoom = 2

    map_lut1 = map(lambda x: convert_fp_to_float(lut1[x], 8, 4), range(lut1_size))
    map_lut1_input = range(0, len(map_lut1))

    map_lut2 = map(lambda x: lut2[zone_select(x)], range(lut1_output_size))
    map_lut2_input = map(lambda x: convert_fp_to_float(x, 8, 4), range(0, lut1_output_size))

    map_gamma = map(lambda x: lut2[zone_select(lut1[x])], range(lut1_size))

    map_2p2_decode = map(lambda x: f_gamma_2p2_decode(x / float(lut1_size - 1)) * (lut1_size - 1), range(lut1_size))
    map_2p2_encode = map(lambda x: f_gamma_2p2_encode(x / float(lut1_size - 1)) * (lut1_size - 1), range(lut1_size))

    if title:
        new_plot_with_title(title)

    marker_size = 4.0
    line_color = "0.78"
    plt.axhline(color=line_color)
    plt.axvline(color=line_color)
    plt.axhline(y=(lut1_size - 1), color=line_color)
    plt.axvline(x=(lut1_size - 1), color=line_color)
    plt.plot([0, (lut1_size - 1)], [0, (lut1_size - 1)], color=line_color)

    plt.plot(range(0, len(map_2p2_decode)), map_2p2_decode, label="gamma 2.2", color="#ff9999")
    plt.plot(range(0, len(map_2p2_encode)), map_2p2_encode, label="gamma 1/2.2", color="#99ff99")

    plt.plot(map_lut1_input, map_lut1, ".-", label="[0 - %d] -> lut1 (8.4 linear)" % (lut1_input_max), color="r", markeredgewidth=0.0, markersize=marker_size)
    plt.plot(map_lut2_input, map_lut2, "-", label="(8.4 linear) -> lut2 [0 - %d]" % (lut2_output_max), color="g", markeredgewidth=0.0, markersize=marker_size)
    plt.plot(range(0, len(map_gamma)), map_gamma, ".-", label="[0 - %d] lut1 -> lut2 [0 - %d]" % (lut1_input_max, lut2_output_max), color="b", markeredgewidth=0.0, markersize=marker_size)

    plt.legend(loc=4)

    set_plot_zoom(zoom)

def plot_gamma_full(lut1, lut2, title, zoom):
    if zoom == None:
        zoom = 3

    map_lut1 = map(lambda x: lut1[x], range(lut1_size))
    map_lut1_input = range(0, len(map_lut1))

    map_lut2 = map(lambda x: lut2[zone_select(x)], range(0, lut1_output_size))
    map_lut2_input = map(lambda x: x, range(0, lut1_output_size))

    map_sRGB_decode = map(lambda x: f_gamma_sRGB_decode(x / float(lut1_size - 1)) * (lut1_output_size - 1), range(0, lut1_size))
    map_sRGB_encode = map(lambda x: f_gamma_sRGB_encode(x / float(lut1_output_size - 1)) * (lut1_size - 1), range(0, lut1_output_size))

    map_gamma = map(lambda x: lut2[zone_select(lut1[x])], range(0, lut1_size))

    new_plot_with_title(title)

    marker_size = 5.0
    line_color = "0.78"
    plt.axhline(color=line_color)
    plt.axvline(color=line_color)
    plt.plot([0, (lut1_size - 1)], [0, (lut1_size - 1)], color=line_color)

    plt.plot(map_lut1_input, map_lut1, ".-", label="[0 - %d] lut1 -> [0 - %d]" % (lut1_size - 1, lut1_output_size - 1), color="r", markeredgewidth=0.0, markersize=marker_size)
    plt.plot(range(0, lut1_size), map_sRGB_decode, "-", label="sRGB decode", color="#ff9999")

    plt.plot(map_lut2_input, map_lut2, ".-", label="[0 - %d] lut2 -> [0 - %d]" % (lut1_output_size - 1, lut1_size - 1), color="g", markeredgewidth=0.0, markersize=marker_size)
    plt.plot(range(0, lut1_output_size), map_sRGB_encode, "-", label="sRGB encode", color="#99ff99")

    plt.plot(range(0, len(map_gamma)), map_gamma, ".-", label="[0 - %d] lut1 -> lut2 [0 - %d]" % (lut1_size - 1, lut1_size - 1), color="b", markeredgewidth=0.0, markersize=marker_size)

    plt.legend(loc=4)

    plt.xlim(xmax=len(map_lut2))
    plt.ylim(ymax=map_lut2[-1])

    set_plot_zoom(zoom)

def plot_measurements(device_scaled, grays, title=None):
    marker_size = 5.0
    line_color = "0.78"

    if title == None:
        title = "gamma 2.2 vs gamma device"

    new_plot_with_title(title)

    plt.axhline(color=line_color)
    plt.axvline(color=line_color)

    plt.plot([0.0, 1.0], [0.0, 1.0], color=line_color)
    plt.plot(grays / float(lut1_size), device_scaled, ".-", label="gamma device", markeredgewidth=0.0, markersize=marker_size)
    plt.plot(np.linspace(0.0, 1.0, lut1_size), map(lambda x: f_gamma_2p2_decode(x / float(lut1_size - 1)), range(lut1_size)), ".-", label="gamma 2.2", markeredgewidth=0.0, markersize=marker_size)

    plt.legend(loc=4)

def write_array(arr, title=None):
    if title:
        print "*** %s ***" % (title)

    for ii in range(len(arr)):
        if ii == 0:
            sys.stdout.write('\t\t'),
        elif ii % 8 == 0:
            sys.stdout.write('\n\t\t'),
        print "%d, " % arr[ii],

    print "\n"

def gen_luts(generic, device, srgb, plot, zoom):
    # Compile the device measurement file in case it has
    # been updated and then import the relevant contents.
    import py_compile
    py_compile.compile("device_measurements.py")

    from device_measurements import gamma_device_scaled
    gamma_device_scaled = np.array(gamma_device_scaled)

    from device_measurements import gray_abs
    gray_abs = np.array(gray_abs)

    try:
        from device_measurements import csc_matrix_device
        csc_matrix_sRGB_f = np.array([[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]])
        csc_matrix_device_f = np.array_split(map(lambda x: convert_fp_to_float(x, 1, 8), csc_matrix_device), 3)
    except ImportError:
        pass

    lut1 = gen_lut1_from_func(f_gamma_sRGB_decode)

    if generic.lut2_fill == "izs":
        gen_lut2_from_func =  gen_lut2_from_func_izs
    else:
        gen_lut2_from_func =  gen_lut2_from_func_avg

    if generic.lut2_fill == "izs":
        gen_lut2_from_samples = gen_lut2_from_samples_izs
    else:
        gen_lut2_from_samples = gen_lut2_from_samples_avg_hd

    write_array(lut1, "sRGB lut1")

    if srgb:
        gray_abs_sRGB = np.array(map(lambda x: x, np.linspace(0, lut1_size - 1, srgb.samples)))
        gamma_sRGB_scaled = np.array(map(lambda x: f_gamma_sRGB_decode(x / float(lut1_size - 1)), gray_abs_sRGB))

        if srgb.interpolation == "func":
            lut2_sRGB_encode = gen_lut2_from_func(f_gamma_sRGB_encode)
        else:
            lut2_sRGB_encode = gen_lut2_from_samples(gamma_sRGB_scaled, gray_abs_sRGB, srgb.oversampling, srgb.interpolation, srgb.spline_smoothing)

        if plot:
            plot_grays(lut1, lut2_sRGB_encode, plot, "Quantization in sRGB lut1 -> sRGB lut2")
            plot_gamma_reduced(lut1, lut2_sRGB_encode, "[0 - %d] sRGB lut1 -> sRGB lut2 [0 - %d]" % (lut1_size - 1, lut1_size - 1), zoom)
            plot_gamma_full(lut1, lut2_sRGB_encode, "sRGB lut1 -> sRGB lut2", zoom)

        write_array(lut2_sRGB_encode, "sRGB lut2")

    if device:
        gamma_device_scaled_norm = ((gamma_device_scaled - gamma_device_scaled[0])
                                    / (gamma_device_scaled[len(gamma_device_scaled) - 1]
                                       - gamma_device_scaled[0]))

        lut2_dev_encode = gen_lut2_from_samples(gamma_device_scaled_norm, gray_abs, device.oversampling, device.interpolation, device.spline_smoothing)

        if plot:
            plot_grays(lut1, lut2_dev_encode, plot, "Quantization in sRGB lut1 -> device lut2")
            plot_measurements(gamma_device_scaled, gray_abs)
            plot_gamma_reduced(lut1, lut2_dev_encode, "[0 - %d] sRGB lut1 -> device lut2 [0 - %d]" % (lut1_size - 1, lut1_size - 1), zoom)
            plot_gamma_full(lut1, lut2_dev_encode, "sRGB lut1 -> device lut2", zoom)

        write_array(lut2_dev_encode, "device lut2")

    plt.show()

def main(argv=None):
    parser = argparse.ArgumentParser(description="Calculate CMU lut2 from the device measurements",
                                     epilog="Change device_measurements.py to match your measurements. Use the --plot argument for helpful visualizations.",
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("-l2f", "--lut2-fill", help="The filling method for the lut2 indices", default="avg", choices=["avg", "izs"])
    parser.add_argument("-os", "--oversampling", help="Oversampling factor when interpolating and averaging", default=0, type=int)
    parser.add_argument("-di", "--device-interpolation", help="Interpolation type for device lut2", choices=["linear", "spline"], default="linear")
    parser.add_argument("-dss", "--device-spline-smoothing", help="Smoothing condition for device spline interpolation", type=float)
    parser.add_argument("-s", "--srgb", help="Generate sRGB lut2 instead of the device one", action="store_true")
    parser.add_argument("-si", "--srgb-interpolation", help="Interpolation type for sRGB lut2.", choices=["linear", "spline", "func"], default="linear")
    parser.add_argument("-ss", "--srgb-samples", help="The number of interpolation sample points for sRGB lut2 generation. Linear interpolation from 64 samples is on par with 'func' interpolation", default=64, type=int)
    parser.add_argument("-sss", "--srgb-spline-smoothing", help="Smoothing condition for sRGB spline interpolation", type=float)
    parser.add_argument("-p", "--plot", help="Draw all plots", action="store_true")
    parser.add_argument("-z", "--zoom", help="Gamma plot zoom level [0 - 3]", type=int)
    parser.add_argument("-ngd", "--no-grays-duplicate", help="Don't show missing gray values", action="store_true")
    parser.add_argument("-ngm", "--no-grays-missing", help="Don't show duplicate gray values", action="store_true")
    parser.add_argument("-ngc", "--no-grays-curve", help="Don't overlay the gradient plot with a curve", action="store_true")
    parser.add_argument("-gar", "--grays-aspect-ratio", help="The aspect ratio of the gradient plot", choices=["auto", "equal"], default="auto")
    args = parser.parse_args()

    device = None
    srgb = None
    plot = None
    gen = None

    if args.srgb:
        srgb = Args_sRGB()
        srgb.interpolation = args.srgb_interpolation
        srgb.samples = args.srgb_samples
        srgb.oversampling = args.oversampling
    else:
        device = Args_device()
        device.interpolation = args.device_interpolation
        device.spline_smoothing = args.device_spline_smoothing
        device.oversampling = args.oversampling

    if args.plot:
        plot = Args_plot()
        plot.grays_duplicate = not args.no_grays_duplicate
        plot.grays_missing = not args.no_grays_missing
        plot.grays_curve = not args.no_grays_curve
        plot.grays_aspect_ratio = args.grays_aspect_ratio

    gen = Args_generic()
    gen.lut2_fill = args.lut2_fill

    gen_luts(gen, device, srgb, plot, args.zoom)

if __name__ == "__main__":
    sys.exit(main())
