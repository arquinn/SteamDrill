#!/usr/bin/python
''' Program to parse a libc configuration out of a replay'''

from __future__ import print_function

import argparse
import json
import subprocess

PARSER = "../binary_parser/functions_ELF"
CFILES = "../../test/parse_cfiles"

#Symbol Names
DLOPEN = "__libc_dlopen_mode"
DLSYM = "__libc_dlsym"
SYSCALL = "syscall"
WAITPID = "__waitpid"
MMAP = "mmap"
OPEN = "open"
MPROTECT = "mprotect"

SPECIAL_CHARS = {DLOPEN:0,\
                 DLSYM:0,\
                 SYSCALL:0,\
                 WAITPID:0,\
                 MMAP:0,\
                 OPEN:0,\
                 MPROTECT:0}

def get_libc (replay):
    libraries = subprocess.check_output([CFILES, replay])
    for lib in libraries.split("\n"):
        sections = lib.split(":")

        if "libc" in sections[0]:
            return sections[1].strip()
    return ""
def get_functions(libc):
    '''run the functions binary to pull out all functions'''
    funcs = subprocess.check_output([PARSER, libc])
    j = json.loads(funcs)
    return j


def produce_configuration(obj, libcName):
    for i in obj:
        if i['name'] in SPECIAL_CHARS:
            SPECIAL_CHARS[i['name']] = hex(i['location'])

    print(libcName, "e", SPECIAL_CHARS[DLOPEN], sep=", ")
    print(libcName, "e", SPECIAL_CHARS[DLSYM], sep=", ")
    print(libcName, "e", SPECIAL_CHARS[SYSCALL], sep=", ")
    print(libcName, "e", SPECIAL_CHARS[WAITPID], sep=", ")
    print(libcName, "e", SPECIAL_CHARS[MMAP], sep=", ")
    print(libcName, "e", SPECIAL_CHARS[OPEN], sep=", ")
    print(libcName, "e", SPECIAL_CHARS[MPROTECT], sep=", ")

def main():
    '''get the libc of a replay configuration'''
    parser = argparse.ArgumentParser()
    parser.add_argument("replay_name")

    cmd_args = parser.parse_args()
    libc = get_libc(cmd_args.replay_name)
    if len(libc) == 0:
        print("Cannot find libc, problem in cfiles?")
        return 1

    j = get_functions(libc)
    produce_configuration(j, libc)
    return 0


main()
