OR_TOOLS_TOP=
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

SOURCES= golomb1.cc golomb2.cc golomb3.cc  golomb4.cc  golomb5.cc golomb6.cc golomb7.cc

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all tutorials local_clean

tutorials: $(EXE)

golomb1.$O: golomb1.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb1.cc -o golomb1.$O

golomb1: $(ROUTING_DEPS) golomb1.$O
	$(CCC) $(CFLAGS) golomb1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb1

golomb2.$O: golomb2.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb2.cc -o golomb2.$O

golomb2: $(ROUTING_DEPS) golomb2.$O
	$(CCC) $(CFLAGS) golomb2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb2

golomb3.$O: golomb3.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb3.cc -o golomb3.$O

golomb3: $(ROUTING_DEPS) golomb3.$O
	$(CCC) $(CFLAGS) golomb3.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb3

golomb4.$O: golomb4.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb4.cc -o golomb4.$O

golomb4: $(ROUTING_DEPS) golomb4.$O
	$(CCC) $(CFLAGS) golomb4.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb4

golomb5.$O: golomb5.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb5.cc -o golomb5.$O

golomb5: $(ROUTING_DEPS) golomb5.$O
	$(CCC) $(CFLAGS) golomb5.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb5
	
golomb6.$O: golomb6.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb6.cc -o golomb6.$O

golomb6: $(ROUTING_DEPS) golomb6.$O
	$(CCC) $(CFLAGS) golomb6.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb6

golomb7.$O: golomb7.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c golomb7.cc -o golomb7.$O

golomb7: $(ROUTING_DEPS) golomb7.$O
	$(CCC) $(CFLAGS) golomb7.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb7
	
local_clean:
	rm $(OBJECTS) $(EXE)

