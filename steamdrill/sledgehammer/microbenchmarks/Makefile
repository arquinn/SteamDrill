targets=test runner

all: $(targets)

test: test.c

runner: runner.cpp ../../../test/libutil.a
	$(CXX) -std=c++0x -I../../lib -I../../../test $^ -o $@ -lpthread
