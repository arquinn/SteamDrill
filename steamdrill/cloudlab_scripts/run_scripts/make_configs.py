#!/usr/bin/python

import sys
import os

def print_usage():
    print "./make_configs base_config_file tests_dir output_dir"

def main():
    print "hello world"

    if len(sys.argv) < 3:
        print_usage()
        return -1

    base_config_file = sys.argv[1]
    tests_dir = sys.argv[2]
    output_dir = sys.argv[3]


    try:
        os.mkdir(output_dir)
    except OSError:
        print output_dir, "already existed...moving on"

    tests_base_name = os.path.basename(tests_dir)
    print tests_base_name

    for d in os.listdir(tests_dir):
        test_dir = "tests_dir:{}".format(os.path.join(tests_base_name, d))

        new_config_file = os.path.join(output_dir, d)
        print d, test_dir, new_config_file
        
        with open(new_config_file, "w+") as config_file:                 
            with open(base_config_file, "r") as starting_file:

                for line in starting_file:                    
                    config_file.write(line)
                    if "bm_dir" in line:
                        config_file.write("{}\n".format(test_dir))
            




main()
