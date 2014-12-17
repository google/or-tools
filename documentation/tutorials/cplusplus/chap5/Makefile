OR_TOOLS_TOP=
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

SOURCES= nqueens1.cc nqueens2.cc nqueens3.cc nqueens4.cc phases1.cc nqueens5.cc nqueens6.cc nqueens7.cc nqueens8.cc

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all tutorials local_clean

tutorials: $(EXE)

nqueens1.$O: nqueens1.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens1.cc -o nqueens1.$O

nqueens1: $(CP_DEPS) nqueens1.$O
	$(CCC) $(CFLAGS) nqueens1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens1

nqueens2.$O: nqueens2.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens2.cc -o nqueens2.$O

nqueens2: $(CP_DEPS) nqueens2.$O
	$(CCC) $(CFLAGS) nqueens2.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens2
	
nqueens3.$O: nqueens3.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens3.cc -o nqueens3.$O

nqueens3: $(CP_DEPS) nqueens3.$O
	$(CCC) $(CFLAGS) nqueens3.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens3

nqueens4.$O: nqueens4.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens4.cc -o nqueens4.$O

nqueens4: $(CP_DEPS) nqueens4.$O
	$(CCC) $(CFLAGS) nqueens4.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens4

phases1.$O: phases1.cc nqueens_utilities.h solver_benchmark.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c phases1.cc -o phases1.$O

phases1$E: $(CP_DEPS) phases1.$O
	$(CCC) $(CFLAGS) phases1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o phases1$E

nqueens5.$O: nqueens5.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens5.cc -o nqueens5.$O

nqueens5: $(CP_DEPS) nqueens5.$O
	$(CCC) $(CFLAGS) nqueens5.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens5

nqueens6.$O: nqueens6.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens6.cc -o nqueens6.$O

nqueens6: $(CP_DEPS) nqueens6.$O
	$(CCC) $(CFLAGS) nqueens6.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens6
	
nqueens7.$O: nqueens7.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens7.cc -o nqueens7.$O

nqueens7: $(CP_DEPS) nqueens7.$O
	$(CCC) $(CFLAGS) nqueens7.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens7
	
nqueens8.$O: nqueens8.cc nqueens_utilities.h $(OR_TOOLS_SOURCES)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c nqueens8.cc -o nqueens8.$O

nqueens8: $(CP_DEPS) nqueens8.$O
	$(CCC) $(CFLAGS) nqueens8.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) -o nqueens8
	
local_clean:
	rm $(OBJECTS) $(EXE)

