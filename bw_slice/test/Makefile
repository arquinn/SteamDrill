
CFLAGS= -g

all: test_slice microbenchmark


test_slice: test_slice.c
	gcc test_slice.c -o test_slice $(CFLAGS)

microbenchmark: microbenchmark.c
	$(CC) $(CFLAGS) -o $@ $< 

clean: 
	rm -f test_slice
