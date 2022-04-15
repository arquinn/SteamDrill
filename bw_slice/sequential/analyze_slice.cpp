#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cassert>

#include "slice.h"
#include "input_set.h"

using namespace std;
void read_slice(char *slice, S::Slice &s)
{
    ifstream ifs(slice);
    s.deserialize(ifs);

    cout << "slice for" << slice << endl;
    cout << s << endl;
    cout << "finished with slice\n"; 
}


void read_dift(char *slice, IS::InputSet &is)
{
    ifstream ifs(slice);
    is.deserialize(ifs);

    cout << "input set for" << slice << endl;
    cout << is << endl;
}

void check(IS::InputSet &is, S::Slice &s) 
{
    for (auto l : is._locations) 
    {
	S::Location l2(l._ip);
	assert(s._nodes.count(l2));
    }   
}

void print_format()
{
    cerr << "format: cmp_slice <slice_one> <slice_two>\n";
}

int main(int argc, char** argv)
{
    char *slice_one = NULL, *dift_set = NULL;

    S::Slice s;
    IS::InputSet is;
    ofstream os;

    if (argc < 3) 
    {
	print_format();
	return -1;
    }
    slice_one = argv[1];
    dift_set = argv[2];
    
    cerr << "calculating overlap of dift_set\n"; 


    read_slice(slice_one, s);
    read_dift(dift_set, is);

    return -1;
}


