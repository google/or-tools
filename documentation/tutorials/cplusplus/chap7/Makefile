OR_TOOLS_TOP=
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

SOURCES= jobshop_ts1.cc jobshop_ts2.cc jobshop_sa1.cc jobshop_sa2.cc dummy_lns.cc jobshop_lns.cc jobshop_heuristic.cc  golomb_default_search1.cc golomb_default_search2.cc    

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all tutorials local_clean

tutorials: $(EXE)

jobshop_ts1.$O: jobshop_ts1.cc limits.h jobshop.h jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_ts1.cc -o jobshop_ts1.$O

jobshop_ts1: $(CP_DEPS) jobshop_ts1.$O
	$(CCC) $(CFLAGS) jobshop_ts1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ts1

jobshop_ts2.$O: jobshop_ts2.cc limits.h jobshop.h jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_ts2.cc -o jobshop_ts2.$O

jobshop_ts2: $(CP_DEPS) jobshop_ts2.$O
	$(CCC) $(CFLAGS) jobshop_ts2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ts2

jobshop_sa1.$O: jobshop_sa1.cc limits.h jobshop.h jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_sa1.cc -o jobshop_sa1.$O

jobshop_sa1: $(CP_DEPS) jobshop_sa1.$O
	$(CCC) $(CFLAGS) jobshop_sa1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_sa1

jobshop_sa2.$O: jobshop_sa2.cc limits.h jobshop.h jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_sa2.cc -o jobshop_sa2.$O

jobshop_sa2: $(CP_DEPS) jobshop_sa2.$O
	$(CCC) $(CFLAGS) jobshop_sa2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_sa2

dummy_lns.$O: dummy_lns.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solveri.h
	$(CCC) $(CFLAGS) -c dummy_lns.cc -o dummy_lns.$O

dummy_lns: $(CP_DEPS) dummy_lns.$O
	$(CCC) $(CFLAGS) dummy_lns.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o dummy_lns

jobshop_lns.$O: jobshop_lns.cc limits.h jobshop.h jobshop_ls.h jobshop_lns.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_lns.cc -o jobshop_lns.$O

jobshop_lns: $(CP_DEPS) jobshop_lns.$O
	$(CCC) $(CFLAGS) jobshop_lns.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_lns
	
jobshop_heuristic.$O: jobshop_heuristic.cc limits.h jobshop.h jobshop_ls.h jobshop_lns.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_heuristic.cc -o jobshop_heuristic.$O

jobshop_heuristic: $(CP_DEPS) jobshop_heuristic.$O
	$(CCC) $(CFLAGS) jobshop_heuristic.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_heuristic

golomb_default_search1.$O: golomb_default_search1.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c golomb_default_search1.cc -o golomb_default_search1.$O

golomb_default_search1: $(CP_DEPS) golomb_default_search1.$O
	$(CCC) $(CFLAGS) golomb_default_search1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb_default_search1
	
golomb_default_search2.$O: golomb_default_search2.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c golomb_default_search2.cc -o golomb_default_search2.$O

golomb_default_search2: $(CP_DEPS) golomb_default_search2.$O
	$(CCC) $(CFLAGS) golomb_default_search2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o golomb_default_search2

local_clean:
	rm $(OBJECTS) $(EXE)

