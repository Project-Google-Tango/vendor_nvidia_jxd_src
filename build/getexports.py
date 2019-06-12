#!/usr/bin/env python
from __future__ import with_statement
import sys

if len(sys.argv) < 6:
    sys.stderr.write('usage: getexports.py [script type] [build_flavor] [target_os] [target_cpu] (--forward [impl_module]) [input files]\n')
    sys.exit(1)

script_type = sys.argv[1]
build_flavor = sys.argv[2]
target_os = sys.argv[3]
target_cpu = sys.argv[4]
impl_module = None
process_ordinals = True

if sys.argv[5] == '--forward':
    impl_module = sys.argv[6]
    input_filenames = sys.argv[7:]
else:
    input_filenames = sys.argv[5:]

if script_type == '-imp-def':
    process_ordinals = False
    script_type = '-def'

if script_type != '-def':
    impl_module = None
    
funcs = []
ordinals = {}
forward = []

for input_filename in input_filenames:
    with open(input_filename, 'rt') as f:
        forward_toggle = False        
        for line in f:
            # Trim leading and trailing whitespace; ignore comments; skip blank lines
            line = line.strip()
            pos = line.find('#')
            if pos != -1:
                line = line[0:pos]
            if len(line) == 0:
                continue

            # Disallow semicolons and //'s, which are an incorrect way to
            # specify comments.  Note that this comes after # filtering, so ; or
            # // inside a comment is OK
            if ';' in line:
                sys.stderr.write('Semicolon detected -- use #, not ;, for comments in .export files\n')
                sys.exit(1)
            if '//' in line:
                sys.stderr.write('// detected -- use #, not //, for comments in .export files\n')
                sys.exit(1)

            # Handle call forward toggle
            if line == 'call_forward = on':
                forward_toggle = True
                continue
            if line == 'call_forward = off':
                forward_toggle = False
                continue

            # Handle qualifiers at beginning of line
            line = line.split(' ')
            filtered = False
            while len(line) > 1:
                if line[0] == 'debug_only':
                    sys.stderr.write('debug_only deprecated -- remove keyword from .export file\n')
                    sys.exit(1)
                elif line[0] == 'cpu_only':
                    # Filter function from AVP builds
                    if target_cpu == 'armv4':
                        filtered = True
                else:
                    break
                line = line[1:]

            # Try to find an explicit ordinal
            for i in range(len(line)):
                if line[i].count('@') == 1:                    
                    parts = line[i].split('@')
                    line[i] = parts[0]
                    if process_ordinals:
                        ordinals[parts[0]] = parts[1]
                    break

            line = ' '.join(line)

            if not filtered:
                funcs.append(line)
                forward.append(forward_toggle)

if script_type == '-def':
    always_private_funcs = set()
    always_private_funcs.add('DllMain')
    always_private_funcs.add('DllRegisterServer')
    always_private_funcs.add('DllRegisterServerEx')
    always_private_funcs.add('DllUnregisterServer')
    always_private_funcs.add('DllGetClassObject')
    always_private_funcs.add('DllCanUnloadNow')
    print 'EXPORTS'
    for i, f in enumerate(funcs):
        func = f
        if impl_module and forward[i]:
            func = func + " = " + impl_module + "." + func
        if ordinals.has_key(f):
            func = func + " @" + ordinals[f]
        if f in always_private_funcs:
            func = func + " PRIVATE"
        print '    %s' % func
elif script_type == '-script':
    print '{'
    print '    global:'
    for func in funcs:
        print '        %s;' % func
    print ''
    print '    local:'
    print '        *;'
    print '};'
elif script_type == '-steer':
    print 'HIDE *'
    for func in funcs:
        print 'SHOW %s' % func
elif script_type == '-c':
    for func in funcs:
        print 'void %s(void); void %s(void) {}' % (func, func)
elif script_type == '-apicheck':
    for func in funcs:
        print 'void %s(void);' % (func)
    print 'int main(void) {'
    for func in funcs:
        if not func.endswith('_Dispatch'):
            print '    %s();' % (func)
    print 'return 0; }'
else:
    sys.stderr.write("Unknown script type '%s'\n" % script_type)
    sys.exit(1)
