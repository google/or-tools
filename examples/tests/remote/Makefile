OR_TOOLS_TOP=$(shell pwd)/../../..
include $(OR_TOOLS_TOP)/Makefile

tsp.o: tsp.cc $(OR_TOOLS_TOP)/src/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c tsp.cc $(OBJ_OUT)tsp.$O

tsp$E: $(DYNAMIC_ROUTING_DEPS) tsp.$O
	$(CCC) $(CFLAGS) tsp.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)tsp$E

local_clean:
	rm tsp.o tsp

