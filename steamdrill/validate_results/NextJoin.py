#!/usr/bin/env python3
'''
Script for nextJoin operator (for testing SD implementation)
'''

from argparse import ArgumentParser
from sys import stderr

from RelationalData import RelationalData, get_data

class Stack:
    '''Wrapper around stack w/ a digest function'''
    def __init__(self):
        '''Constructor'''
        self._data = []

    def __str__(self):
        '''string version of digest'''
        for item in self._data:
            print(item)

    def __len__(self):
        return len(self._data)

    def push(self, item):
        '''Add item to stack'''
        self._data.append(item)

    def pop(self):
        '''Pop item from stack'''
        item = self._data.pop()
        return item

    def last(self):
        '''Get pointer to last element in stack'''
        return self._data[-1]


class HashIndex:

    '''Class for a hash index over some data'''
    def __init__(self, data, cols):
        indexes = []
        for col_name in cols:
            indexes.append(data.get_index(col_name))

        self._data = {}
        for item in data.get_items():
            item_hash = []
            for index in indexes:
                item_hash.append(item[index].strip())

            key = ":".join(item_hash)
            if key not in self._data:
                self._data[key] = []
            self._data[key].append(item)

    def get_items_by_key(self, key):
        '''Get all matching elements by key '''
        try:
            for data in self._data[key]:
                yield data
        except KeyError:
            return

    def get_keys(self):
        '''Iterate all key-value pairs'''
        for data in self._data:
            yield data

def get_digest(item, digest_idx_list):
    '''get digest (hash string) based on list and item'''
    return ":".join([item[index]  for index in digest_idx_list])

def next_join(data, equals, order):
    '''Stack-based NextJoin function'''
    lorder = order[0]
    rorder = order[1]

    # should sort data[0] and data[1]...
    lhash = HashIndex(data[0], equals[0])
    rhash = HashIndex(data[1], equals[1])

    result = RelationalData()
    result.add_schema_as_list(data[0].get_schema() +\
                              data[1].get_schema())

    debug_fxns = ["__libc_sigaction", "event_init"]
    for key in lhash.get_keys():
        debug = False
        for fxn in debug_fxns:
            if fxn in key:
                debug = True

        try:
            riter = rhash.get_items_by_key(key)
            # rrow = next(riter)
            if debug:
                print("making %s" %(key), file=stderr)

            for lrow in lhash.get_items_by_key(key):
                if debug:
                    print("lrow %s" %(lrow), file=stderr)

                rrow = next(riter)
                if debug:
                    print("rrow %s" %(rrow), file=stderr)

                while rorder(rrow) < lorder(lrow):
                    if debug:
                        print("skip row %s" %(rrow))

                    rrow = next(riter)

                result.add_item_as_list(lrow + rrow)

        except StopIteration:
            print("stop iteration on %s" %(key), file=stderr)

    return result

def main():
    '''Main function'''
    parser = ArgumentParser(description="nextJoin impl for test")

    parser.add_argument("left", action="store")
    parser.add_argument("right", action="store")

    args = parser.parse_args()

    with open(args.left, "r") as left_file,\
         open(args.right, "r") as right_file:
        left = get_data(left_file)
        right = get_data(right_file)
        output = next_join((left, right),\
                           (["sname", "sdepth", "stid"], ["ename", "edepth", "etid"]),\
                           (lambda it: int(it[left.get_index("sOrder")], 16),\
                            lambda it: int(it[right.get_index("eOrder")], 16)))

        #lorder = data[0].get_index(order[0])
        #rorder = data[1].get_index(order[1])


        output.sort(lambda it: int(it[output.get_index("sOrder")],16))
        print(output.get_formatted_schema())
        for item in output.get_formatted_items():
            print(item)


if __name__ == "__main__":
    main()
