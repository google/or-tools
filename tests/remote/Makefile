OR_TOOLS_TOP=$(shell pwd)/../..
include $(OR_TOOLS_TOP)/Makefile

tsp.o: tsp.cc $(OR_TOOLS_TOP)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c tsp.cc -o tsp.o

tsp: $(ROUTING_DEPS) tsp.o
	$(CCC) $(CFLAGS) tsp.o $(ROUTING_LNK) $(LDFLAGS) -o tsp

local_clean:
	rm tsp.o tsp