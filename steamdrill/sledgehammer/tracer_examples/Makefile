CXXFLAGS= -Wall -Werror -Wno-unused-variable
LDFLAGS := -Wall -Werror -g

CLANG = clang
PTRACE_LIB=../tracer_library/
PASS_DIR=../isolation_pass/isolate

MYPASS=libIsolationPass
MY_PASS_COMMAND=-load ${PASS_DIR}/${MYPASS}.so -isolate

LIBUTIL=../../test/libutil.a
LIBRT=../../eglibc-2.15/prefix/lib/librt.a

CLANG_FLAGS=-Xclang -load -Xclang ${PASS_DIR}/${MYPASS}.so

TARGETS=MallocFreeFork.so MallocFreeLLVM.so Dispatcher.so

all: $(TARGETS)

MallocFreeFork.o: MallocFree.c
	$(CXX) $(CXXFLAGS) -c -o $@ $^

MallocFreeFork.so: MallocFreeFork.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -shared


Dispatcher.bc: Dispatcher.c
	$(CLANG) $(CFLAGS) -I $(PTRACE_LIB) -c -emit-llvm -g -o $@ $<

Dispatcher.linked.bc: Dispatcher.bc $(PTRACE_LIB)tracer_library.bc
	llvm-link -o $@ $^

Dispatcher.noopt.bc: Dispatcher.linked.bc
	opt $(MY_PASS_COMMAND)  -o=$@ $^

Dispatcher.so: Dispatcher.noopt.bc $(LIBUTIL) $(LIBRT)
	$(CLANG) $(MY_CFLAGS) -g -o $@ $^ -shared


MallocFreeLLVM.bc: MallocFreeLLVM.c
	$(CLANG) $(CFLAGS) -I $(PTRACE_LIB) -c -emit-llvm -g -o $@ $<

MallocFreeLLVM.linked.bc: MallocFreeLLVM.bc $(PTRACE_LIB)tracer_library.bc
	llvm-link -o $@ $^

MallocFreeLLVM.noopt.bc: MallocFreeLLVM.linked.bc
	opt $(MY_PASS_COMMAND)  -o=$@ $^ 

MallocFreeLLVM.so: MallocFreeLLVM.noopt.bc $(LIBUTIL) $(LIBRT)
	$(CLANG) $(MY_CFLAGS) -g -o $@ $^ -shared #-lrt


.PHONY: clean
clean:
	rm -r *.o *.bc $(TARGETS)
