#include <unistd.h>
#include <iostream>
#include <fstream>

#include "slice.h"

using namespace std;
void read_slice(char *slice, S::Slice &s)
{
    ifstream ifs(slice);
    s.deserialize(ifs);

    cout << "slice for" << slice << endl;
//    cout << s << endl;
}


void difference(S::Slice &first, S::Slice &second, ostream &os)
{
    S::Slice diff = first.diff(second);
    os << diff;
}

void print_format()
{
    cerr << "format: cmp_slice <slice_one> <slice_two>\n";
}

int main(int argc, char** argv)
{
    char *slice_one = NULL, *slice_two = NULL, 
	*outfile = NULL;        
    S::Slice s1, s2; 
    ofstream os;

    if (argc < 4) 
    {
	print_format();
	return -1;
    }
    slice_one = argv[1];
    slice_two = argv[2];
    outfile = argv[3];
    
    cerr << "calculating " << slice_one 
	 << " - " << slice_two << endl;


    read_slice(slice_one, s1);
    read_slice(slice_two, s2);

    os.open(outfile);

    difference(s1, s2, os);

    return -1;
}


