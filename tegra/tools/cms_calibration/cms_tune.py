#
# Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
#

from scipy.interpolate import interp1d
import numpy as np
import matplotlib.pyplot as plt
import decimal as dec
import sys
import subprocess

gained_matrix=[1.176, -0.254, 0.078, -0.037, 1.086, -0.098, -0.003, -0.027, 0.859];
normalized_matrix=[1.176, -0.254, 0.078, -0.039, 1.142, -0.103, -0.004, -0.032, 1.036];

use_gained_matrix = int(sys.argv[1])
cms_blend = float(sys.argv[2])
cms_matrix_used=[];
cms_final_matrix=[];

if use_gained_matrix == 0:
	use_gained_matrix = 0
	cms_matrix_used = normalized_matrix
else:
	use_gained_matrix = 1
	cms_matrix_used = gained_matrix

if cms_blend >= 1.0:
	cms_blend = 1.0
elif cms_blend <= 0.0:
	cms_blend = 0.0

for i in range(9):
	temp = (1-cms_blend) + cms_blend*cms_matrix_used[i];
	cms_final_matrix.insert(i, temp);

compile_args = ("gcc", "-lm", "convert_3x3.c", "-o", "convert_3x3");
popen = subprocess.Popen(compile_args, stdout=subprocess.PIPE)
popen.wait()
output = popen.stdout.read()
print output

args = ("./convert_3x3", str(cms_final_matrix[0]), str(cms_final_matrix[1]), str(cms_final_matrix[2]),
					     str(cms_final_matrix[3]), str(cms_final_matrix[4]), str(cms_final_matrix[5]),
					 	 str(cms_final_matrix[6]), str(cms_final_matrix[7]), str(cms_final_matrix[8]))

popen = subprocess.Popen(args, stdout=subprocess.PIPE)
popen.wait()
output = popen.stdout.read()
print output

args = ("rm", "-rf", "convert_3x3");
popen = subprocess.Popen(args, stdout=subprocess.PIPE)
popen.wait()
output = popen.stdout.read()
print output
