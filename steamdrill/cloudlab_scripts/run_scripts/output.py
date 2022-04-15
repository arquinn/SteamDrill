import os
import re

class Epoch_Stats:
    def __init__(self, epoch_path):
        with open(epoch_path) as ifile: 
            for l in ifile:
                if len(l) > 0:
                    self._parse_stats(l)


    def _parse_stats(self, l):
        
        s = l.split(":")
        key = s[0]
        value = s[1].strip()        
        m = re.search("(\d+) us", value)        
        ms = -1
        if m is not None:
            ms = int(m.group(1))
        
        
        if (key.startswith("total time")):
            self._total_time = ms
        elif (key.startswith("total gadget time")):
            self._gadget_time = ms
        elif (key.startswith("total fork time")):
            self._fork_time = ms
        elif (key.startswith("total map time")):
            self._map_time = ms
        elif (key.startswith("total call time")):
            self._call_time = ms
        elif (key.startswith("total wait time")):
            self._wait_time = ms
        elif (key.startswith("total finish time")):
            self._finish_time = ms
        elif (key.startswith("total other time")):
            self._other_time = ms
        elif (key.startswith("number of gadgets")):
            self._num_gadgets = int(value)

    def __str__(self):
        return "{:6d}  {:6d} {:6d} {:6d} {:6d} {:6d}  {:6d} {:6d}    {:6d}".format(self._total_time, self._gadget_time, self._fork_time, self._map_time, self._call_time,self._wait_time, self._finish_time, self._other_time, self._num_gadgets)

    @staticmethod
    def get_header():
        return " Total  Gadget   Fork    Map   Call   Wait  Finish  Other  #Gadgets"
            
class Test:
    def __init__(self, directory):
        self._epochs = []

        for epoch in os.listdir(directory):
            es = Epoch_Stats(os.path.join(directory, epoch))
            self._epochs.append(es)

        self._num_epochs = len(self._epochs)

    def __str__(self):

        r = Epoch_Stats.get_header()
        for e in self._epochs:
            r += "\n" + str(e)

        return r
            
class Output:
    def __init__(self):
        self._tests = {}



    def create_stats(self):
        for t in sorted(self._tests):
            
            print self._tests[t],"\n"
        
    @staticmethod
    def make_output(bm_dir):
        #each test directory MUST contain the following folder:
        #  output

        o = Output()
        full_bm_path = os.path.abspath(bm_dir)


        tests_dir = os.path.join(full_bm_path, "output")
        for test in os.listdir(tests_dir):
            t = Test(os.path.join(tests_dir,test))
            o._tests[t._num_epochs] = t


        return o
