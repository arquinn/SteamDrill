TARGETS = analyzer
all: $(TARGETS)

analyzer: analyzer.cpp $(LIBUTIL) $(LIBNW)
	$(CXX) $(CXXFLAGS) -o $@ $^ -ldl -lpthread

clean:
	rm -r *.a *.o $(TARGETS)
