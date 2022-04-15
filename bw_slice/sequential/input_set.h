#ifndef INPUT_SET_H_
#define INPUT_SET_H_

#include <iostream>
#include <unordered_set> 

namespace IS { 
    class Location { 
    public:
	int16_t _type;
	int16_t _record_pid;
	uint32_t _token_num;
	uint32_t _ip;
	uint32_t _fileno; 
	

	Location(int16_t type, int16_t record_pid, u_long token_num,
			 u_long ip, u_long fileno);
	Location(u_long token_num);

	bool operator==(const Location &) const;
	friend std::ostream& operator<< (std::ostream &os, 
					 const Location &l);
    };

    class SliceCriteria { 
    public:
	u_long _ip; 
	u_long _sliceLoc; 

	SliceCriteria(u_long ip, u_long sliceLoc);

	bool operator==(const SliceCriteria &) const;
	friend std::ostream& operator<< (std::ostream &os, 
					 const SliceCriteria &l);
    };


    struct LocationHasher { 
	size_t operator() (const Location &l ) const { 
	    return std::hash<u_long>() (l._token_num);
	}
    };

    struct SliceCriteriaHasher { 
	size_t operator() (const SliceCriteria &sc ) const { 
	    return std::hash<u_long>() (sc._ip) 
		^ std::hash<u_long>() (sc._sliceLoc) << 1; 
	}
    };


    class InputSet { 
    public:    
	std::unordered_set<Location, LocationHasher> _locations;
	std::unordered_set<SliceCriteria, SliceCriteriaHasher> _scNodes;

	void addLocation(Location &l);
	void addSliceCriteria(SliceCriteria &node);
	void deserialize(std::istream &is);
	void merge(IS::InputSet &other);

	void deserialize_addresses(std::istream &is);
	void dump_addresses(std::ostream &os);

	IS::InputSet diff(IS::InputSet &other);

	
	friend std::ostream& operator<<(std::ostream &os, const InputSet &s);
    };
}

#endif /* SLICE_H_ */
