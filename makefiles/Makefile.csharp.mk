# ---------- CSharp support using SWIG ----------
CSHARPEXE = \
	$(BIN_DIR)$Scslinearprogramming.exe \
	$(BIN_DIR)$Scsintegerprogramming.exe \
	$(BIN_DIR)$Scsrabbitspheasants.exe \
	$(BIN_DIR)$Scsflow.exe \
	$(BIN_DIR)$Scsknapsack.exe \
	$(BIN_DIR)$Sfurniture_moving_intervals.exe \
	$(BIN_DIR)$Sorganize_day_intervals.exe \
	$(BIN_DIR)$Scstsp.exe

csharpexe: $(CSHARPEXE)

# Main target.
csharp: csharpcp csharplp csharpalgorithms csharpgraph csharpexe

# Clean target.
clean_csharp:
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.*.$(SHAREDLIBEXT)
	-$(DEL) $(BIN_DIR)$SGoogle.OrTools.*.dll
	-$(DEL) $(BIN_DIR)$SGoogle.OrTools.*.mdb
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.*.lib
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.*.exp
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.*.netmodule
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Salgorithms$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Sgraph$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	-$(DEL) $(OBJ_DIR)$S*csharp_wrap.$O
	-$(DEL) $(BIN_DIR)$S*.exe

# csharplp

ifeq ($(SYSTEM),win)
IMPORTPREFIX=$(OR_ROOT_FULL)\\bin\\
else
IMPORTPREFIX=$(OR_ROOT_FULL)/lib/lib
endif

csharplp: $(BIN_DIR)/Google.OrTools.LinearSolver.dll

$(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc: $(SRC_DIR)linear_solver/linear_solver.swig $(SRC_DIR)base/base.swig $(SRC_DIR)util/data.swig $(SRC_DIR)linear_solver/linear_solver.h $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport $(IMPORTPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)linear_solver$Slinear_solver.swig

$(OBJ_DIR)/linear_solver_csharp_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc $(OBJ_OUT)linear_solver_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.LinearSolver.dll: $(OBJ_DIR)/linear_solver_csharp_wrap.$O $(LP_DEPS) $(SRC_DIR)com/google/ortools/linearsolver/LinearExpr.cs $(SRC_DIR)com/google/ortools/linearsolver/LinearConstraint.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\linearsolver\\*.cs $(SRC_DIR)com\\google\\ortools\\linearsolver\\*.cs
	$(LD) $(LDOUT)$(BIN_DIR)\\Google.OrTools.LinearSolver.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule $(OBJ_DIR)\\linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.LinearSolver.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/linearsolver/*.cs $(SRC_DIR)com/google/ortools/linearsolver/*.cs
	$(LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) $(OBJ_DIR)/linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
endif

# csharp linearsolver examples

$(BIN_DIR)/cslinearprogramming.exe: $(BIN_DIR)/Google.OrTools.LinearSolver.dll $(EX_DIR)csharp/cslinearprogramming.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Scslinearprogramming.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.LinearSolver.dll $(EX_DIR)csharp$Scslinearprogramming.cs

$(BIN_DIR)/csintegerprogramming.exe: $(BIN_DIR)/Google.OrTools.LinearSolver.dll $(EX_DIR)csharp/csintegerprogramming.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Scsintegerprogramming.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.LinearSolver.dll $(EX_DIR)csharp$Scsintegerprogramming.cs

# csharp linearsolver tests

$(BIN_DIR)/testlp.exe: $(BIN_DIR)/Google.OrTools.LinearSolver.dll $(EX_DIR)tests/testlp.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Stestlp.exe /platform:$(NETPLATFORM) /r:$(BIN_DIR)$SGoogle.OrTools.LinearSolver.dll $(EX_DIR)tests$Stestlp.cs

testlp: $(BIN_DIR)/testlp.exe
	$(MONO) $(BIN_DIR)$Stestlp.exe

# csharpcp

csharpcp: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll

$(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc: $(SRC_DIR)constraint_solver/routing.swig $(SRC_DIR)constraint_solver/constraint_solver.swig $(SRC_DIR)base/base.swig $(SRC_DIR)util/data.swig $(SRC_DIR)constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport $(IMPORTPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)constraint_solver$Srouting.swig

$(OBJ_DIR)/constraint_solver_csharp_wrap.$O: $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJ_OUT)constraint_solver_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.ConstraintSolver.dll: $(OBJ_DIR)/constraint_solver_csharp_wrap.$O $(ROUTING_DEPS) $(SRC_DIR)com/google/ortools/constraintsolver/IntVarArrayHelper.cs $(SRC_DIR)com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs $(SRC_DIR)com/google/ortools/constraintsolver/IntArrayHelper.cs $(SRC_DIR)com/google/ortools/constraintsolver/ValCstPair.cs $(SRC_DIR)com/google/ortools/constraintsolver/NetDecisionBuilder.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs com\\google\\ortools\\constraintsolver\\*.cs
	$(LD) $(LDOUT)$(BIN_DIR)\\Google.OrTools.ConstraintSolver.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule $(OBJ_DIR)$Sconstraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.ConstraintSolver.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)com/google/ortools/constraintsolver/*.cs
	$(LD)  $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) $(OBJ_DIR)/constraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
endif

# csharp cp examples

$(BIN_DIR)/csrabbitspheasants.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp/csrabbitspheasants.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Scsrabbitspheasants.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp$Scsrabbitspheasants.cs

$(BIN_DIR)/send_more_money.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp/send_more_money.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Ssend_more_money.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp$Ssend_more_money.cs

$(BIN_DIR)/furniture_moving_intervals.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp/furniture_moving_intervals.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Sfurniture_moving_intervals.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp$Sfurniture_moving_intervals.cs

$(BIN_DIR)/organize_day_intervals.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp/organize_day_intervals.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Sorganize_day_intervals.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp$Sorganize_day_intervals.cs

$(BIN_DIR)/cstsp.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp/cstsp.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Scstsp.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp$Scstsp.cs

# csharp constraint solver tests

$(BIN_DIR)/testcp.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)tests/testcp.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Stestcp.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)tests$Stestcp.cs

testcp: $(BIN_DIR)/testcp.exe
	$(MONO) $(BIN_DIR)$Stestcp.exe

# csharpalgorithms

csharpalgorithms: $(BIN_DIR)/Google.OrTools.Algorithms.dll

$(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc: $(SRC_DIR)algorithms/knapsack_solver.swig $(SRC_DIR)algorithms/knapsack_solver.swig $(SRC_DIR)base/base.swig $(SRC_DIR)util/data.swig $(SRC_DIR)algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport $(IMPORTPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver $(SRC_DIR)algorithms$Sknapsack_solver.swig

$(OBJ_DIR)/knapsack_solver_csharp_wrap.$O: $(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc $(OBJ_OUT)knapsack_solver_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.Algorithms.dll: $(OBJ_DIR)/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\knapsacksolver\\*.cs
	$(LD) $(LDOUT)$(BIN_DIR)\\Google.OrTools.Algorithms.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Algorithms.netmodule $(OBJ_DIR)\\knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.Algorithms.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/knapsacksolver/*.cs
	$(LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) $(OBJ_DIR)/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
endif

# csharp algorithm examples

$(BIN_DIR)/csknapsack.exe: $(BIN_DIR)/Google.OrTools.Algorithms.dll $(EX_DIR)csharp/csknapsack.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Scsknapsack.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.Algorithms.dll $(EX_DIR)csharp$Scsknapsack.cs

# csharpgraph

csharpgraph: $(BIN_DIR)/Google.OrTools.Graph.dll

$(GEN_DIR)/graph/graph_csharp_wrap.cc: $(SRC_DIR)graph/graph.swig $(SRC_DIR)base/base.swig $(SRC_DIR)util/data.swig $(SRC_DIR)graph/max_flow.h $(SRC_DIR)graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport $(IMPORTPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

$(OBJ_DIR)/graph_csharp_wrap.$O: $(GEN_DIR)/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc $(OBJ_OUT)graph_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.Graph.dll: $(OBJ_DIR)/graph_csharp_wrap.$O $(GRAPH_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /unsafe /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Graph.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\graph\\*.cs
	$(LD) $(LDOUT)$(BIN_DIR)\\Google.OrTools.Graph.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Graph.netmodule $(OBJ_DIR)\\graph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /unsafe /out:$(BIN_DIR)/Google.OrTools.Graph.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/graph/*.cs
	$(LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) $(OBJ_DIR)/graph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
endif

# csharp graph examples

$(BIN_DIR)/csflow.exe: $(BIN_DIR)/Google.OrTools.Graph.dll $(EX_DIR)csharp/csflow.cs
	$(CSC) /target:exe /out:$(BIN_DIR)$Scsflow.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.Graph.dll $(EX_DIR)csharp$Scsflow.cs

# Build and compile custome CP examples

csc: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp/$(EX).cs
	$(CSC) /target:exe /out:$(BIN_DIR)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)csharp$S$(EX).cs

rcs: csc
	$(MONO) $(BIN_DIR)$S$(EX).exe $(ARGS)


# Build archive.

dotnet_archive: csharp
	-$(DELREC) temp
	tools\mkdir temp
	tools\mkdir temp\or-tools.$(PLATFORM)
	tools\mkdir temp\or-tools.$(PLATFORM)\lib
	tools\mkdir temp\or-tools.$(PLATFORM)\bin
	tools\mkdir temp\or-tools.$(PLATFORM)\csharp
	tools\mkdir temp\or-tools.$(PLATFORM)\csharp\solution
	tools\mkdir temp\or-tools.$(PLATFORM)\csharp\solution\Properties
	tools\mkdir temp\or-tools.$(PLATFORM)\data
	tools\mkdir temp\or-tools.$(PLATFORM)\data\discrete_tomography
	tools\mkdir temp\or-tools.$(PLATFORM)\data\fill_a_pix
	tools\mkdir temp\or-tools.$(PLATFORM)\data\minesweeper
	tools\mkdir temp\or-tools.$(PLATFORM)\data\rogo
	tools\mkdir temp\or-tools.$(PLATFORM)\data\survo_puzzle
	tools\mkdir temp\or-tools.$(PLATFORM)\data\quasigroup_completion
ifneq ($(SYSTEM),win)
	copy bin\Google.OrTools.*.dll temp\or-tools.$(PLATFORM)\bin
endif
	copy csharp\*.cs temp\or-tools.$(PLATFORM)\csharp
	copy csharp\*.sln temp\or-tools.$(PLATFORM)\csharp
	copy csharp\solution\*.csproj temp\or-tools.$(PLATFORM)\csharp\solution
	copy csharp\solution\Properties\*.cs temp\or-tools.$(PLATFORM)\csharp\solution\Properties
	copy data\discrete_tomography\* temp\or-tools.$(PLATFORM)\data\discrete_tomography
	copy data\fill_a_pix\* temp\or-tools.$(PLATFORM)\data\fill_a_pix
	copy data\minesweeper\* temp\or-tools.$(PLATFORM)\data\minesweeper
	copy data\rogo\* temp\or-tools.$(PLATFORM)\data\rogo
	copy data\survo_puzzle\* temp\or-tools.$(PLATFORM)\data\survo_puzzle
	copy data\quasigroup_completion\* temp\or-tools.$(PLATFORM)\data\quasigroup_completion
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.NET.$(PLATFORM).$(SVNVERSION).zip or-tools.$(PLATFORM)
	-$(DELREC) temp
