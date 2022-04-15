#ifndef SLICE_H_
#define SLICE_H_

#include <iostream>
#include <unordered_set> 
#include "input_set.h"

namespace S { 
    class Location {    
    public:
	u_long _ip;
	Location(u_long ip);

	bool operator==(const Location &) const;
	friend std::ostream& operator<< (std::ostream &os, const Location &l);
    };

    class SliceCriteria { 
    public:
	u_long _ip; 
	u_long _sliceLoc; 
	u_long _lastModifiedIp; // might be reasonable to differentiate this

	SliceCriteria(u_long ip, u_long sliceLoc, u_long lastModifiedIp); 

	bool operator==(const SliceCriteria &) const;
	friend std::ostream& operator<< (std::ostream &os, 
					 const SliceCriteria &l);
    };


    struct LocationHasher { 
	size_t operator() (const Location &l ) const { 
	    return std::hash<u_long>() (l._ip);
	}
    };

    struct SliceCriteriaHasher { 
	size_t operator() (const SliceCriteria &sc ) const { 
	    return std::hash<u_long>() (sc._ip) 
		^ std::hash<u_long>() (sc._lastModifiedIp) << 1; 
	}
    };


    class Slice { 
    public:    
	std::unordered_set<Location, LocationHasher> _nodes;
	std::unordered_set<SliceCriteria, SliceCriteriaHasher> _scNodes;

	Location addLocation(Location &node);
	SliceCriteria addSliceCriteria(SliceCriteria &sc); 

	void merge(Slice &other);
	void mergeIS(IS::InputSet &other);
	void deserialize(std::istream &is);
	Slice diff(Slice &other); 

	friend std::ostream& operator<<(std::ostream &os, const Slice &s);
    };
}


#endif /* SLICE_H_ */
