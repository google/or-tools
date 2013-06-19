OR_TOOLS_TOP=$(shell pwd)/../../..
include $(OR_TOOLS_TOP)/Makefile

tsp.$O: tsp.cc $(OR_TOOLS_TOP)/src/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c tsp.cc $(OBJ_OUT)tsp.$O

tsp$E: $(DYNAMIC_ROUTING_DEPS) tsp.$O
	$(CCC) $(CFLAGS) tsp.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)tsp$E

linear_programming.$O: linear_programming.cc $(OR_TOOLS_TOP)/src/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c linear_programming.cc $(OBJ_OUT)linear_programming.$O

linear_programming$E: $(DYNAMIC_LP_DEPS) linear_programming.$O
	$(CCC) $(CFLAGS) linear_programming.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)linear_programming$E

local_clean:
	rm tsp.$O tsp$E linear_programming.$O linear_programming$E

