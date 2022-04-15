TARGETS=test
all: $(TARGETS)

CXX=g++
CXXFLAGS=-g -std=c++0x
INCLUDE=../../lib/json/single_include
SH_CONFIG=../../config_structs

JSON="../../lib/json/single_include"
GTEST="../../lib/gtest/googletest/include"
INCLUDES=-I$(JSON) -I$(GTEST)
GTESTLIB="../../lib/gtest/prefix/lib/libgtest.a"

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -I$(INCLUDE) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I$(INCLUDE) -c -o $@ $<

test: relational_tree.o expression.o test.o
	$(CXX) -o $@ $^

clean:
	rm $(TARGETS) *.o
