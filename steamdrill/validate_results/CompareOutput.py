#!/usr/bin/env python3
'''
Script used to compare two relational data files
'''

from argparse import ArgumentParser
from sys import stderr
from RelationalData import get_data

def compare(first, second):
    '''Compare two RelationalData objects '''

    schema_shift = []
    for elem in first.get_schema():
        idx = second.get_index(elem)
        schema_shift.append(idx)

        if idx == -1:
            print ("WARNING: cannot find", elem, file=stderr)

    count = 0
    for (iter1, iter2) in zip(first.get_items(), second.get_items()):
        count += 1
        for idx, sc in enumerate(schema_shift):
            if iter1[idx].strip().upper() != iter2[sc].strip().upper():
                print("%s not equal to %s" %(iter1, iter2), file=stderr)
                print("field %s vs. %s" %(iter1[idx].strip().upper(),\
                                          iter2[sc].strip().upper()),\
                      file=stderr)
                assert False

    print("Checked %d: I'm good with this" %(count))

def main():
    '''Main function'''
    parser = ArgumentParser(description="compare spark output")

    parser.add_argument("first", action="store")
    parser.add_argument("second", action="store")

    args = parser.parse_args()

    with open(args.first, "r") as first_file,\
         open(args.second, "r") as second_file:
        first_data = get_data(first_file)
        fdata_order = first_data.get_index("sOrder")
        fdata_name = first_data.get_index("sname")
        #yet more ways to  ties. **sighs**
        fdata_seip = first_data.get_index("seip")
        fdata_stid = first_data.get_index("stid")

        first_data.sort(lambda it: (int(it[fdata_order], 16),
                                    it[fdata_name],
                                    it[fdata_seip],
                                    it[fdata_stid]))

        second_data = get_data(second_file)
        sdata_order = second_data.get_index("sOrder")
        sdata_name = second_data.get_index("sname")
        sdata_seip = second_data.get_index("seip")
        sdata_stid = second_data.get_index("stid")

        second_data.sort(lambda it: (int(it[sdata_order], 16),
                                     it[sdata_name],
                                     it[sdata_seip],
                                     it[sdata_stid]))
        compare(first_data, second_data)


if __name__ == "__main__":
    main()
