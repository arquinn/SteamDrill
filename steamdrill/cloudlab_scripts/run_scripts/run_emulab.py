#!/usr/bin/python

import argparse

import server
import benchmark
import os


def main(): 

    parser = argparse.ArgumentParser()
    parser.add_argument("-t", "--test", dest="test", required=True)
    parser.add_argument("-s", "--server_name",dest="server_name", required=True)
    parser.add_argument("-f", "--test_filter", dest="test_filter")

    cmd_args = parser.parse_args()


    print cmd_args.test_filter
    if cmd_args.test_filter:
        b = benchmark.Benchmark.make_benchmark(cmd_args.test, cmd_args.stream_dir, cmd_args.test_filter)
    else:
        b = benchmark.Benchmark.make_benchmark(cmd_args.test, cmd_args.stream_dir, None)
    b.prep_tests(hosts)
    b.run_tests(hosts, host_file)

main()
