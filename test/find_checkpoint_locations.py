#!/usr/bin/python

import sys

#        if (2.5 * float(words[3])) >= curr_time + 500:

def main(): 

    if len(sys.argv) < 2:
        print "need a time arg!"

    step_time = float(sys.argv[1])
    curr_time = 0.00

    for line in sys.stdin:
        words = line.strip().split(",")
        if float(words[4]) > curr_time + step_time:
            print words[2].strip()
            curr_time += step_time

main()
