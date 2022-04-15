
Targets= HelloWorld.so customLoaderTest

CC=clang

CFLAGS= -nostdinc -I$(MUSL)/include

all: $(Targets)

HelloWorld.so: HelloWorld.c
	$(CC) $(CFLAGS) -L$(MUSL)/lib -static  $^ -shared -o $@
clean:
	$(RM) *.o $(Targets)

.phony: clean all
