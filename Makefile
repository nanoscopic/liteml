CCSRCS = main.c xmlbare/red_black_tree.c xmlbare/string_tree.c xmlbare/parser.c xmlbare/stack.c xmlbare/misc.c

HDRS = xmlbare/parser.h xmlbare/string_tree.h

CC = gcc -Wno-write-strings
CCC = gcc

NO_UNUSED = -ffunction-sections -fdata-sections -Wl,--gc-sections
LIBFLAGS = -lffi -ldl

CFLAGS = -g -Wall -pedantic  -O3 $(LIBFLAGS) $(NO_UNUSED)

PROGRAM = go

.PHONY:	mem_check clean

all: $(PROGRAM)

$(PROGRAM): 	$(CCSRCS) $(SRCS) $(COS)
	$(CCC) -o $(PROGRAM) $(CCSRCS) $(COS) $(CFLAGS)

clean:			
	rm -f *.o *.lo *.la *.slo *~ go






