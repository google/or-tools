OR_TOOLS_TOP=
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

SOURCES= report_jobshopdata.cc jobshop.cc dummy_ls.cc jobshop_ls1.cc jobshop_ls2.cc jobshop_ls3.cc dummy_ls_filtering.cc

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all tutorials local_clean

tutorials: $(EXE)

report_jobshopdata.$O: report_jobshopdata.cc jobshop.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c report_jobshopdata.cc -o report_jobshopdata.$O

report_jobshopdata: $(CP_DEPS) report_jobshopdata.$O
	$(CCC) $(CFLAGS) report_jobshopdata.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o report_jobshopdata

jobshop.$O: jobshop.cc jobshop.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop.cc -o jobshop.$O

jobshop: $(CP_DEPS) jobshop.$O
	$(CCC) $(CFLAGS) jobshop.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop

dummy_ls.$O: dummy_ls.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c dummy_ls.cc -o dummy_ls.$O

dummy_ls: $(CP_DEPS) dummy_ls.$O
	$(CCC) $(CFLAGS) dummy_ls.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o dummy_ls

jobshop_ls1.$O: jobshop_ls1.cc jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_ls1.cc -o jobshop_ls1.$O

jobshop_ls1: $(CP_DEPS) jobshop_ls1.$O
	$(CCC) $(CFLAGS) jobshop_ls1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ls1

jobshop_ls2.$O: jobshop_ls2.cc jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_ls2.cc -o jobshop_ls2.$O

jobshop_ls2: $(CP_DEPS) jobshop_ls2.$O
	$(CCC) $(CFLAGS) jobshop_ls2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ls2
	
jobshop_ls3.$O: jobshop_ls3.cc jobshop_ls.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c jobshop_ls3.cc -o jobshop_ls3.$O

jobshop_ls3: $(CP_DEPS) jobshop_ls3.$O
	$(CCC) $(CFLAGS) jobshop_ls3.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ls3

dummy_ls_filtering.$O: dummy_ls_filtering.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c dummy_ls_filtering.cc -o dummy_ls_filtering.$O

dummy_ls_filtering: $(CP_DEPS) dummy_ls_filtering.$O
	$(CCC) $(CFLAGS) dummy_ls_filtering.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o dummy_ls_filtering


local_clean:
	rm $(OBJECTS) $(EXE)

