#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include "../postprocess/slice.h"
#include "../postprocess/input_set.h"

//#define DETAILS 1

using namespace std;

#define DEBUG_IP 0x848e1e7

void cmpSlice(S::Slice &baseline, S::Slice &testing) 
{
    S::Slice oneLessTwo = baseline.diff(testing); 
    cerr << "there are " << oneLessTwo._nodes.size() 
	 << " nodes in baseline but not testing\n";

    for (auto &l : oneLessTwo._nodes)
	cerr << l << endl;
    
    S::Slice twoLessOne = testing.diff(baseline); 
    cerr << "there are " << twoLessOne._nodes.size() 
	 << " nodes in testing but not baseline \n";

//    for (auto &l : twoLessOne._nodes)
//	cerr << l << endl;
}

void mergeSlice (S::Slice &slice, int index)
{
    S::Slice n;
//    IS::InputSet is;
    S::Slice is; 
    char fname[256];

    sprintf(fname,"/tmp/%d/slice",index);
    ifstream ifs(fname);
    n.deserialize(ifs);
    ifs.close();

    slice.merge(n);    

//    sprintf(fname,"/tmp/%d/dift_addrs",index);
//    ifs.open(fname);
//    is.deserialize_addresses(ifs);
//    is.deserialize(ifs);
//    ifs.close();


//    slice.merge(is);    
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    char *baseline = argv[1];
    int numEpochs = atoi(argv[2]); 
    S::Slice baseSlice; 
    ifstream stream(baseline); 
    
    baseSlice.deserialize(stream);
    
    S::Slice slice; 
    for (int i = 0; i < numEpochs; ++i) 
    {	              
	mergeSlice(slice, i);
    }    
    
    cmpSlice(baseSlice, slice); 

    return -1;
}

