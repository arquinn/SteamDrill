
Targets= HelloWorld TestReturns UseAfterFree ThreadTest DlopenTest CFETest signalTest tracerLibTest RecursionTest CrashTest perfWatchdogTest RunawayLoopTest ForkTest test_suffix

CXX=clang++
CC=clang
#CC=gcc

CFLAGS+= -g -fverbose-asm -fcf-protection=full
CXXFLAGS+=-g -pthread -fPIC -std=c++14 -fverbose-asm -fcf-protection
LDLIBS=-ldl -fPIE -pthread

all: $(Targets)

DlopenTest: DlopenTest.c
	g++ -o DlopenTest DlopenTest.c 

clean:
	$(RM) *.o $(Targets)

.phony: clean all
