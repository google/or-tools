OR_TOOLS_TOP=XXX
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

SOURCES= cp_is_fun1.cc cp_is_fun2.cc cp_is_fun3.cc cp_is_fun4.cc

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=$E)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all local_clean

all: $(EXE)

cp_is_fun1.$O: cp_is_fun1.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c cp_is_fun1.cc -o cp_is_fun1.$O

cp_is_fun1$E: $(CP_DEPS) cp_is_fun1.$O
	$(CCC) $(CFLAGS) cp_is_fun1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o cp_is_fun1$E
	
cp_is_fun2.$O: cp_is_fun2.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c cp_is_fun2.cc -o cp_is_fun2.$O

cp_is_fun2$E: $(CP_DEPS) cp_is_fun2.$O
	$(CCC) $(CFLAGS) cp_is_fun2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o cp_is_fun2$E

cp_is_fun3.$O: cp_is_fun3.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c cp_is_fun3.cc -o cp_is_fun3.$O

cp_is_fun3$E: $(CP_DEPS) cp_is_fun3.$O
	$(CCC) $(CFLAGS) cp_is_fun3.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o cp_is_fun3$E

cp_is_fun4.$O: cp_is_fun4.cc $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c cp_is_fun4.cc -o cp_is_fun4.$O

cp_is_fun4$E: $(CP_DEPS) cp_is_fun4.$O
	$(CCC) $(CFLAGS) cp_is_fun4.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_BASE_LNK) $(DYNAMIC_LD_FLAGS) -o cp_is_fun4$E

local_clean:
	rm $(OBJECTS) $(EXE)
