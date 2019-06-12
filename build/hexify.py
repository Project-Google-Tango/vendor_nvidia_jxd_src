#!/usr/bin/env python

# Takes binary file and creates C-compatible character array out of it.
import sys

chars_per_line = 16
first_line = True

fi = open(sys.argv[1], "rb")
fo = open(sys.argv[2], "wb")
while True:
    data = fi.read(chars_per_line)
    if len(data) == 0:
        break
    if first_line:
        first_line = False
    else:
        fo.write(',\n')
    for i in range(0,len(data)):
        if i > 0:
            fo.write(',')
        fo.write('0x%02x' % ord(data[i]))
fo.write('\n')
fi.close()
fo.close()
