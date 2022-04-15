#!/usr/bin/python

import sys

def print_usage():
    ''' dumps the usage string '''
    print "./mergeTest.py <combo> <one> <two>"

def next_line(line):
    

def main():
    ''' main fxn'''
    if len(sys.argv) < 4:
        print_usage()
        return -1

    combo = open(sys.argv[1]).readlines()
    one = open(sys.argv[2]).readlines()
    two = open(sys.argv[3]).readlines()

    print combo[0]

main()
