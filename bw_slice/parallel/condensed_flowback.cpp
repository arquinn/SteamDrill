#include <set>
#include <vector>
#include <cassert>

#include "condensed_flowback.h"


void CF::condensed_flowback::add_node(node_t *n, nodeid id) {

    //prolly don't need this in here, for convenience of for loop @ 30 
    node_wrapper node(n, id);

    //if this instcount hasn't been seen, create a new node_wrapper in graph
    auto nodeid_index = instcount_index.find(n->instcount);
    if (nodeid_index == instcount_index.end()) { 

//	std::cerr << std::hex << "cf: new node " << id << std::endl;
	std::set<nodeid> new_children; 

	for (auto c : node._children) 
	{
	    auto cnode = graph.find(c); 
	    if (cnode != graph.end()) 
	    {
		//should erase old one, but don't want to skrew up the iterator
//		std::cerr << std::hex << "\t" << c << " is " << cnode->second._id<< std::endl;  

		new_children.insert(cnode->second._id);
		graph[c].add_parent(id); 
	    }
	    else {
		new_children.insert(c); 
	    }
	}

	node._children.clear();
	for (auto c : new_children)
	{
	    node._children.insert(c);
	}

	instcount_index[node._dynid] = id; 
	graph[id] = node; 
    }

    //o/w merge with the existing one

    else { 
	node_wrapper &nw = graph[nodeid_index->second]; 
	for (auto c : node._children){
	    auto cnode = graph.find(c); 
	    auto cinput = inputs.find(c);
	    if (cnode != graph.end()) 
	    {
		nw._children.insert(cnode->second._id);
		graph[c].add_parent(nw._id); 
	    }

	    else if (cinput != inputs.end())
	    {
		nw._children.insert(c);
	    }
	}
	graph[id] = nw; 
    }

}

void CF::condensed_flowback::add_input_node(input_info *ii, nodeid id, bool isStart) {

    input_node_wrapper node(ii, id, isStart);
    inputs[id] = node; 
}


void CF::condensed_flowback::condense() { 
    std::vector<nodeid> work_list;
    std::vector<nodeid> remove_list; 

    //find root nodes... 
    for (auto &node : graph) { 
	if (node.second._parents.size() == 0) {
	    bool found = false;
	    for (auto pid : work_list) {
		node_wrapper &p = graph[pid];
		if (p._ip == node.second._ip) { 
		    found = true;
		    p._count++;

		    /*
		     * fixup graph: 
		     *  1. remove the pruned node
		     *  2. fixup the children of the pruned node
		     */

		    remove_list.push_back(node.first);
		    //tell our children!
		    //should cache these 1 away nodes
		    for (auto &c : node.second._children) { 
			node_wrapper &cnode = graph[c];
			cnode._parents.erase(node.first);
			cnode._parents.insert(p._id);
			
			//this node gets these children! 
			p._children.insert(c); 
//			std::cerr << "\tinherited child " << c << std::endl;
		    }
		}
	    }
	    if (!found) { 
		//we don't match any roots, do nothing
		std::cerr << "found a new root " << node.second ; 
		work_list.push_back(node.first); 
	    }
	}
    }
    for (auto &n: remove_list) 
    {
	graph.erase(n);
    }

    //now iterate through work_list (nodes that have matched in the last round 
    
    while( work_list.size() > 0) {
	nodeid nid = work_list.back();
	work_list.pop_back();
	node_wrapper &n = graph[nid];

	//iterate through all children looking for a match
	//afaik this should work with the inner loop doing removes (c never is removed)
	for (std::set<nodeid>::iterator c = n._children.begin();
	     c != n._children.end(); ++c) { 
	    
	    node_wrapper &cnode = graph[*c];
	    std::set<nodeid>::iterator c2 = std::next(c);
	    while (c2 != n._children.end()) { 
		
		node_wrapper &c2node = graph[*c2];
		
		if (cnode._ip == c2node._ip) {
		    work_list.push_back(*c);
		    cnode._count++;
		    for (auto &gc : c2node._children) {
			node_wrapper &gcnode = graph[gc];
			gcnode._parents.erase(*c2);
			gcnode._parents.insert(*c);			
			cnode._children.insert(gc);
		    }
		    graph.erase(*c2);
		    c2 = n._children.erase(c2); 
		}
		else { 
//		    std::cerr << "\tdiff " << c2node;
		    ++c2; 
		}
	    }
	}
    }
}    

std::ostream& CF::operator<<(std::ostream &os, const CF::node_wrapper nw) 
{
    char base[1024]; 
    sprintf(base, "%llx,%x,%x",nw._id, nw._dynid,nw._ip);
    for (auto c : nw._children) { 
	os << std::hex << base << "," << c << ",0,"<< 
	    nw._location << "," << nw._value << std::endl;
    }   
    return os;
}

std::ostream& CF::operator<<(std::ostream &os, const CF::input_node_wrapper nw) 
{
    if (nw._isStart)
    {
    	os << std::hex << nw._id << "," << nw._dynid << ",starting state,0,0,0,0" << std::endl;
    }
    else 
    {
	switch (nw._type)
	{
	case input_info_type::READ:	
	case input_info_type::PREAD:	
	    os << std::hex << nw._id << "," << nw._id << "," << " read from " 
	       << nw._fileno << "@" << nw._syscall_cnt 
	       << "," << 0 << "," << 0
	       << "," << nw._id << "," << 0 << std::endl;    
	    break;
	case input_info_type::MMAP:       
	    os << std::hex << nw._id << "," << nw._id << "," << " mmap from " 
	       << nw._fileno << "@" << nw._syscall_cnt 
	       << "," << 0 << "," << 0
	       << "," << nw._id << "," << 0 << std::endl;    
	    break;
	case input_info_type::IMM:
	    os << std::hex << nw._id << "," << nw._dynid << "," << nw._ip 
	       << ",0,0," << nw._location << ",0" << std::endl;
	    break;
	    
	case input_info_type::WRITE:
	    std::cerr << "huh? can't really print this input_info" << std::endl;
	}
    }
    return os;
}


std::ostream& CF::operator<<(std::ostream &os, const CF::condensed_flowback cf) 
{

    for (auto n : cf.graph) {
	if (n.first == n.second._id)
	    os << n.second; 
    }

    for (auto n : cf.inputs) {
	os << n.second; 
    }

    return os;
}
