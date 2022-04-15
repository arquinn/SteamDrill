#include "node.h" 
#include <iostream>

#ifndef DIFT
std::ostream& operator<<(std::ostream &os, const node_t nt) 
{
    os << "[" << nt.ip << "," << nt.instcount << "] ->" 
       << "(" << nt.p1 << "," << nt.p2 << ")"; 
    
    return os; 
};
#else 
std::ostream& operator<<(std::ostream &os, const node_t nt) 
{
    os << "(" << nt.p1 << "," << nt.p2 << ")"; 
    
    return os; 
};
#endif

