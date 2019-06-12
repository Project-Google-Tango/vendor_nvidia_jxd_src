#! /usr/bin/env python

# Copyright (c) 2009-2013, NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.

""" An extension script to be used in conjunction with systrace.py.
    Adds support for automatic atrace augmentation from tegrastats
    as well as the ability to pass the --open command in order to
    auto-open the trace on completion.

    To install copy this script to the same directory as systrace.py
    and run megatrace.py with all the same options as you would run
    systrace.py.
"""

import os, sys, systrace, re, optparse

# We don't care about these, but parse them so that we can recognize the categories
def add_systrace_options(parser):
  parser.add_option('-o', dest='output_file', help='write HTML to FILE',
                    default='trace.html', metavar='FILE')
  parser.add_option('-b', '--buf-size', dest='trace_buf_size', type='int',
                    help='use a trace buffer size of N KB', metavar='N')
  parser.add_option('-k', '--ktrace', dest='kfuncs', action='store',
                    help='specify a comma-separated list of kernel functions to trace')
  parser.add_option('-l', '--list-categories', dest='list_categories', default=False,
                    action='store_true', help='list the available categories and exit')
  parser.add_option('-a', '--app', dest='app_name', default=None, type='string',
                    action='store', help='enable application-level tracing for comma-separated ' +
                    'list of app cmdlines')

  parser.add_option('--link-assets', dest='link_assets', default=False,
                    action='store_true', help='link to original CSS or JS resources '
                    'instead of embedding them')
  parser.add_option('--from-file', dest='from_file', action='store',
                    help='read the trace from a file (compressed) rather than running a live trace')
  parser.add_option('--asset-dir', dest='asset_dir', default='trace-viewer',
                    type='string', help='')
  parser.add_option('-e', '--serial', dest='device_serial', type='string',
                    help='adb device serial number')

def main():

  parser = systrace.OptionParserIgnoreErrors(add_help_option=False)
  add_systrace_options(parser)
  # Catch --help
  parser.add_option('-h', '--help', action='store_true', default=False, dest='help')
  # We also want to capture the trace time
  parser.add_option('-t', '--time', dest='trace_time_seconds', type='int',
                    help='trace for N seconds', metavar='N', default=5)
  parser.add_option('--open', action='store_true', default=False, dest='auto_open')

  options, args = parser.parse_args()

  # Call systrace help, then append our bit
  if options.help:
    ret = systrace.main()
    print '--open'
    print '                         automatically open the trace file after completion'
    return ret

  # TODO more unified handling of added categories.
  # Fix this if you add some more
  # Call systrace categories, then append our bit
  if options.list_categories:
    ret = systrace.main()
    print '        temp - Temperature'
    print '      limits - Frequency Limits'
    return ret

  # filter non-systrace options
  if options.auto_open:
    sys.argv = [arg for arg in sys.argv if arg != '--open']

  temp = 'temp' in args
  limits = 'limits' in args

  added_categories = ['temp', 'limits']

  # Strip the added categories
  if not set(args).isdisjoint(set(added_categories)):
    # assumes categories are at end of args
    lead_argv = sys.argv[:(len(sys.argv)-len(args))]
    filter_categories = [c for c in args if c not in added_categories]
    sys.argv = lead_argv + filter_categories

  os.system("adb shell tegrastats --stop")
  os.system("adb shell tegrastats --systrace" + \
          (" --systrace-temp" if temp else "") + \
          (" --systrace-limits" if limits else "") + \
          " &")

  systrace.main()
  os.system("adb shell tegrastats --stop")

  if options.auto_open:
    os.system("gnome-open " + options.output_file)

if __name__ == '__main__':
  main()
