CXX = c++ -g  -std=c++0x
LEX = flex
YACC = bison
SHARED_STRUCTS= ../shared_structures

all: sql_parser

sql_parser: sql_parser.tab.o sql_parser.o \
	$(SHARED_STRUCTS)/query_types.o $(SHARED_STRUCTS)/Extraction.o
	${CXX}  -o $@ $^ -lboost_serialization

sql_parser.tab.c sql_parser.tab.h: sql_parser.y
	${YACC} -vd -t sql_parser.y

sql_parser.c: sql_parser.l
	${LEX}  -o $*.c $<

sql_parser.tab.o: sql_parser.tab.c sql_parser.tab.h
	${CXX} -c -o $@ $< -I$(SHARED_STRUCTS)

sql_parser.o: sql_parser.c sql_parser.tab.h
	${CXX} -c -o $@ $< -I$(SHARED_STRUCTS)

test: all

.SUFFIXES: .pgm .l .y .c

clean:
	rm sql_parser.tab.* sql_parser sql_parser.c sql_parser.o
