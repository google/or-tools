OR_TOOLS_TOP=XXX
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

SOURCES= report_jobshopdata.cc jobshop.cc dummy_ls.cc jobshop_ls1.cc jobshop_ls2.cc \
         jobshop_ls3.cc dummy_ls_filtering.cc

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=$E)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all local_clean

all: $(EXE)

report_jobshopdata.$O: report_jobshopdata.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h jobshop.h
	$(CCC) $(CFLAGS) -c report_jobshopdata.cc -o report_jobshopdata.$O

report_jobshopdata$E: $(CP_DEPS) report_jobshopdata.$O
	$(CCC) $(CFLAGS) report_jobshopdata.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o report_jobshopdata$E

jobshop.$O: jobshop.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h jobshop.h
	$(CCC) $(CFLAGS) -c jobshop.cc -o jobshop.$O

jobshop$E: $(CP_DEPS) jobshop.$O
	$(CCC) $(CFLAGS) jobshop.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop$E

dummy_ls.$O: dummy_ls.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solveri.h 
	$(CCC) $(CFLAGS) -c dummy_ls.cc -o dummy_ls.$O

dummy_ls$E: $(CP_DEPS) dummy_ls.$O
	$(CCC) $(CFLAGS) dummy_ls.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o dummy_ls$E

jobshop_ls1.$O: jobshop_ls1.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h jobshop.h jobshop_ls.h
	$(CCC) $(CFLAGS) -c jobshop_ls1.cc -o jobshop_ls1.$O

jobshop_ls1$E: $(CP_DEPS) jobshop_ls1.$O 
	$(CCC) $(CFLAGS) jobshop_ls1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ls1$E

jobshop_ls2.$O: jobshop_ls2.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h jobshop.h jobshop_ls.h
	$(CCC) $(CFLAGS) -c jobshop_ls2.cc -o jobshop_ls2.$O

jobshop_ls2$E: $(CP_DEPS) jobshop_ls2.$O 
	$(CCC) $(CFLAGS) jobshop_ls2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ls2$E

jobshop_ls3.$O: jobshop_ls3.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h jobshop.h jobshop_ls.h
	$(CCC) $(CFLAGS) -c jobshop_ls3.cc -o jobshop_ls3.$O

jobshop_ls3$E: $(CP_DEPS) jobshop_ls3.$O 
	$(CCC) $(CFLAGS) jobshop_ls3.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o jobshop_ls3$E

dummy_ls_filtering.$O: dummy_ls_filtering.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solveri.h
	$(CCC) $(CFLAGS) -c dummy_ls_filtering.cc -o dummy_ls_filtering.$O

dummy_ls_filtering$E: $(CP_DEPS) dummy_ls_filtering.$O
	$(CCC) $(CFLAGS) dummy_ls_filtering.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o dummy_ls_filtering$E


local_clean:
	rm $(OBJECTS) $(EXE)

