#!/usr/bin/python

#  Copyright (C) 2006, 2007 William McCune
#
#  NOTICE ADDED IN THIS TREE: the LADR-2009-11A release shipped this
#  file without a licence notice.  What follows is the notice every
#  .c and .h file of that release carries, reproduced so that this
#  file is self-describing.  The terms are the release's own; this
#  is not a new grant, and McCune did not write these lines here.
#
#  This file is part of the LADR Deduction Library.
#
#  The LADR Deduction Library is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License,
#  version 2.
#
#  The LADR Deduction Library is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the LADR Deduction Library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

#  CHANGE NOTICE (GPL-2, section 2a): this file was modified by
#  Chris Arena on 2026-10-16, and is not as William McCune released it
#  in LADR-2009-11A.  The only change is the licence notice above,
#  which the release omitted from this file.

import re, sys

print '    dependencies = ['

for line in sys.stdin.readlines():

    line = line.replace('automatic', 'auto')
    line = line.replace('TRUE', 'True')
    line = line.replace('FALSE', 'False')
    line = line.replace('INT_MAX', 'sys.maxint')
    line = line.replace('"', "'")
    
    (_,type,opt,a,b,c,_) = re.split('_dependency\(\w+->|,\s*\w+->|,\s*|\);\n|\s*', line)

    if (opt in ['default_parts', 'default_output'] or
        b in ['lrs_ticks', 'echo_input', 'quiet']):
        com = '#'
    else:
        com = ''
    
    if type in ['flag_flag', 'flag_parm', 'flag_stringparm']:
        print "%s        (('%s', %s), ('%s', %s))," % (com, opt, a, b, c)
    elif type == 'parm_parm':
        if c == 'True':
            v2 = "('multiply', %s)" % b
        else:
            v2 = b
        print "%s        (('%s', 'any'), ('%s', %s))," % (com, opt, a, v2)
    else:
        print '# not handled: line'
    
print '        ]'
