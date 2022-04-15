CLANG = clang
CLANGFLAGS = -nostdinc -Wall -Werror  -I$(SDEV) -I$(MUSL)/include -I$(SDUTILINC) -I$(CONFIGINC)

TARGETS=tracer_library.bc shared_state.bc

HEADER_FILES = $(wildcard *.h)

all: $(TARGETS)

##seems like this is bad. But, hard to get right otherwise :/
shared_state.bc: $(SDUTILINC)/shared_state.c $(SDUTILINC)/shared_state.h
	$(CLANG) $(CLANGFLAGS) $(DEBUG_FLAGS) -c -emit-llvm -o $@ $<

%.bc: %.c $(HEADER_FILES)
	$(CLANG) $(CLANGFLAGS) $(DEBUG_FLAGS) -c -emit-llvm -o $@ $<

%.o: %.c $(HEADER_FILES)
	$(CLANG) $(CLANGFLAGS) $(DEBUG_FLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -r $(TARGETS)
