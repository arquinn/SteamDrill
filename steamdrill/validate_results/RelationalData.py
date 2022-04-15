'''
Class for parsing spark-style data output
'''

class RelationalData():
    '''Data class - stores data together with a schema'''

    def __init__(self):
        self._schema = []
        self._items = []

    def __len__(self):
        return len(self._items)

    def __bool__(self):
        return len(self._items) > 0

    __nonzero__ = __bool__

    def add_schema(self, line):
        '''add a schema to the data'''
        self._schema = [l.strip() for l in line.split("|")[1:-1]]

    def add_schema_as_list(self, schema):
        '''add a schema to the data'''
        self._schema = schema

    def add_item(self, line):
        '''add an item to the data'''
        self._items.append(line.split("|")[1:-1])

    def add_item_as_list(self, item):
        '''add an item to the data'''
        self._items.append(item)

    def get_schema(self):
        '''Return the schema'''
        return self._schema

    def get_formatted_schema(self):
        '''Get printable schema'''
        line = "|".join(self._schema)
        surrounds = "-" * len(line)

        return str.format("+{}+\n|{}|\n+{}+",\
                          surrounds, line, surrounds)

    def get_items(self):
        '''Generator to iterate all rows in data'''
        for item in self._items:
            yield item

    def get_index(self, col_name):
        '''Get the index of the column in the schema '''
        try:
            return self._schema.index(col_name)
        except ValueError:
            return -1

    def get_formatted_items(self):
        '''Helper to print row '''
        for item in self._items:
            yield str.format("|{}|", "|".join(item))

    def sort(self, lambda_function):
        '''Sort the items by schmea row'''
        # idx = self.get_index(schema_row)
        self._items.sort(key=lambda_function)


def get_data(in_file):
    '''Parse data from a file and return a data object'''
    parsed_data = RelationalData()

    # file looks like:
    #
    # +----+
    # schema line
    # +----+
    # data
    # data
    # ...
    # +----+

    in_file.readline()
    parsed_data.add_schema(in_file.readline())
    in_file.readline()
    for line in in_file:
        if not line[0] == "+":
            parsed_data.add_item(line)

    return parsed_data
