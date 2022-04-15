#include "../graph_interface/node.h"
#include "../graph_interface/input_output.h"

#include <set>
#include <map>
#include <unordered_map>
#include <iostream>

#ifndef _NODE_WRAPPER_
#define _NODE_WRAPPER_ 
namespace CF { 

    class node_wrapper{    
    public:
	nodeid _id; 
	uint32_t _dynid;
	uint32_t _ip; 
	uint32_t _count; 
	u_long   _value;
	long     _location;

	std::set<nodeid> _parents; 
	std::set<nodeid> _children;
	
    node_wrapper(): _id(0), _ip(0), _count(0), _value(0), _location(0){}
	
	node_wrapper(node_t *n, nodeid id) {
	    _id = id;
	    _ip = n->ip; 
	    _dynid = n->instcount;
	    _count = 1; 

	    _value = n->value;
	    _location = n->location;


	    if (n->p1) { 
		_children.insert(n->p1);
	    }
	    if (n->p2) { 
		_children.insert(n->p2);
	    }       	
	}

	inline void add_parent(nodeid p) { 
	    _parents.insert(p); 
	}

	friend std::ostream& operator<<(std::ostream &os, const CF::node_wrapper nw);

    };

    class input_node_wrapper {
    public:
	nodeid _id; 
	bool _isStart;
	input_info_type _type;
	uint32_t _ip; 
	uint32_t _dynid; 
	long     _location;
	u_long   _fileno;
	u_long   _syscall_cnt;

	input_node_wrapper() {}
	input_node_wrapper(input_info *ii, nodeid id, bool isStart = false) {
	    _id = id;
	    _isStart = isStart;
	    if (!_isStart)
	    {

		_type = ii->type;
		switch (ii->type)
		{
		case input_info_type::IMM:
		    _ip = ii->imm_info.ip;
		    _dynid = ii->imm_info.instcount;
		    _location = ii->location;
		    break;
		default:
		    _syscall_cnt = ii->syscall_cnt;
		    _fileno = ii->syscall_info.fileno;
		    break;
		}
	    }
	}


	friend std::ostream& operator<<(std::ostream &os, const CF::input_node_wrapper nw);
    };


    class condensed_flowback {
    public:
	std::map<nodeid, node_wrapper> graph;
	std::map<nodeid, input_node_wrapper> inputs;
	std::unordered_map<uint32_t, nodeid> instcount_index;

	void add_node(node_t *n, nodeid id);
	void add_input_node(input_info *ii, nodeid id, bool isStart = false);
	void condense();
	friend std::ostream& operator<<(std::ostream &os, const CF::condensed_flowback cf); 
    };
}



#endif /*_NODE_WRAPPER_*/
