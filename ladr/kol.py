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

import sys

# print "Hello, world!"

MIN = 2

def kolmogorov(string):
   print "string=%s" % string
   total = 0
   length = len(string) - 2
   index = MIN
   print "Will check from index %d to %d." % (index,length)
   print "Length of string is %d." % length
   while index <= length:  # index of where the search begins
       maxlook = (length - index) + 2
       # don't check for occurences longer than head of string
       if maxlook > index:
           maxlook = index
       print "Checking at index: %d." % index
       print "Will check strings of length %d through %d." % (MIN,maxlook)
       flen = 0 # length of found redundancy
       # index of length of lookahead to check
       for window in range(MIN, maxlook+1):  # note: range(2,5) is [2,3,4]
           tocheck = string[ index : index+window ]  # note: "abcde"[1:3] is "bc"
           print "Want to know if %s occurs in pos 0 until %d." % (tocheck,index)
           ind = string.find(tocheck)
           if ind < index:
               flen = window

       if flen == 0:
           index += 1
       else:
           index += flen
           total += flen
       print "total=%d, index=%d, length=%d, MIN=%d" % (total,index,length,MIN)
   print "DONE.";
   print "TOTAL REDUNDANCY: %d" % total
   return float(total) / (length-1)

while 1:
   string = sys.stdin.readline()
   if not string:
       break;
   print str(kolmogorov(string))

