#!/usr/bin/python

import sys
import os

def print_usage():
    print "./split_test epoch_file dest_prefix per_test"

            

def main():
    print "hello world"

    if len(sys.argv) < 3:
        print_usage()
        return -1

    epoch_file = sys.argv[1]
    dest_prefix = sys.argv[2]
    per_test = int(sys.argv[3])

    base_epoch = os.path.basename(epoch_file)

    try:
        os.mkdir(dest_prefix)
    except OSError:
        print dest_prefix, "already existed...moving on"


    with open(epoch_file, "r") as starting:
        test_count = 0
        test_offset = 0

        folder = "{}/{}.{}".format(dest_prefix, base_epoch, test_offset)

        try:
            os.mkdir(folder)
        except OSError:
            print dest_prefix, "already existed...moving on"

        curr_test_file = open("{}/{}.{}/{}.{}".format(dest_prefix, base_epoch, test_offset,\
                                                      base_epoch, test_offset), "w+")
        print curr_test_file

        for line in starting:
            if test_count == per_test:
                test_count = 0
                test_offset += 1

                folder = "{}/{}.{}".format(dest_prefix, base_epoch, test_offset)
                try:
                    os.mkdir(folder)
                except OSError:
                    print dest_prefix, "already existed...moving on"

                curr_test_file = open("{}/{}.{}".format(folder, base_epoch, test_offset), "w+")

            curr_test_file.write(line)
            test_count += 1

            



main()
