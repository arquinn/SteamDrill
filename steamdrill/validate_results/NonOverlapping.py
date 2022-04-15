#!/usr/bin/env python3
'''
Script for nextJoin operator (for testing SD implementation)
'''

from argparse import ArgumentParser
from RelationalData import RelationalData, get_data

def find_no_overlap(data, val_lambda, lower, upper):
    '''Find non overlapping data based on the lambdas '''
    for item in data.get_items():

        found = False
        value = val_lambda(item)
        for contain in data.get_items():
            if lower(contain) < value < upper(contain):
                print("found that", item, "\n\tduring", contain)
                found = True
                break

        if not found:
            print(item)

def main():
    '''Main function'''
    parser = ArgumentParser(description="nextJoin impl for test")
    parser.add_argument("tfile", action="store")
    args = parser.parse_args()

    with open(args.tfile, "r") as tfile:
        data = get_data(tfile)
        for item in data.get_items():
            print(item)
        find_no_overlap(data,
                        lambda it: int(it[data.get_index("entry_order")]),
                        lambda it: int(it[data.get_index("entry_order")]),
                        lambda it: int(it[data.get_index("return_order")]))


if __name__ == "__main__":
    main()
