# ---------- CSharp support using SWIG ----------

# Assembly Signing
# ----------------
# All C# assemblies can optionally be signed. This includes the
# DLLs and the test case EXEs. Signing is currently supported only
# on Windows with the .NET framework.
#
# To get signed assemblies, use for example:
# make CLR_KEYFILE="c:\full\path\to\keyfile.snk" csharp
#
# For delay signing, use for example:
# make CLR_KEYFILE="c:\full\path\to\keyfile_pub.snk" CLR_DELAYSIGN=1

ifeq ($(SYSTEM),win)
ifdef CLR_KEYFILE
ifdef CLR_DELAYSIGN
SIGNING_FLAGS = /keyfile:"$(CLR_KEYFILE)" /delaysign
else
SIGNING_FLAGS = /keyfile:"$(CLR_KEYFILE)"
endif
endif
endif

CSHARPEXE = \
	$(BIN_DIR)/cslinearprogramming.exe \
	$(BIN_DIR)/csintegerprogramming.exe \
	$(BIN_DIR)/csrabbitspheasants.exe \
	$(BIN_DIR)/csflow.exe \
	$(BIN_DIR)/csknapsack.exe \
	$(BIN_DIR)/furniture_moving_intervals.exe \
	$(BIN_DIR)/organize_day_intervals.exe \
	$(BIN_DIR)/csls_api.exe \
	$(BIN_DIR)/cscvrptw.exe \
	$(BIN_DIR)/cstsp.exe

csharpexe: $(CSHARPEXE)

# Main target.
csharp: csharpcp csharplp csharpalgorithms csharpgraph csharpexe

# Clean target.
clean_csharp:
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.*.$(DYNAMIC_SWIG_LIB_SUFFIX)
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

csharplp: $(BIN_DIR)/Google.OrTools.LinearSolver.dll

$(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc: $(SRC_DIR)/linear_solver/linear_solver.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/linear_solver/linear_solver.h $(GEN_DIR)/linear_solver/linear_solver2.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport "Google.OrTools.LinearSolver.$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)/linear_solver$Slinear_solver.swig

$(OBJ_DIR)/linear_solver_csharp_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.LinearSolver.dll: $(OBJ_DIR)/linear_solver_csharp_wrap.$O $(STATIC_LP_DEPS) $(SRC_DIR)/com/google/ortools/linearsolver/LinearExpr.cs $(SRC_DIR)/com/google/ortools/linearsolver/LinearConstraint.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\linearsolver\\*.cs $(SRC_DIR)\\com\\google\\ortools\\linearsolver\\*.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)\\Google.OrTools.LinearSolver.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule $(OBJ_DIR)\\linear_solver_csharp_wrap.$O $(STATIC_LP_LNK) $(STATIC_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.LinearSolver.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/linearsolver/*.cs $(SRC_DIR)/com/google/ortools/linearsolver/*.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.LinearSolver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/linear_solver_csharp_wrap.$O $(STATIC_LP_LNK) $(STATIC_LD_FLAGS)
endif

# csharp linearsolver examples

$(BIN_DIR)/cslinearprogramming.exe: $(BIN_DIR)/Google.OrTools.LinearSolver.dll $(EX_DIR)/csharp/cslinearprogramming.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scslinearprogramming.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.LinearSolver.dll $(EX_DIR)$Scsharp$Scslinearprogramming.cs

$(BIN_DIR)/csintegerprogramming.exe: $(BIN_DIR)/Google.OrTools.LinearSolver.dll $(EX_DIR)/csharp/csintegerprogramming.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsintegerprogramming.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.LinearSolver.dll $(EX_DIR)$Scsharp$Scsintegerprogramming.cs

# csharp linearsolver tests

$(BIN_DIR)/testlp.exe: $(BIN_DIR)/Google.OrTools.LinearSolver.dll $(EX_DIR)/tests/testlp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestlp.exe /platform:$(NETPLATFORM) /r:$(BIN_DIR)$SGoogle.OrTools.LinearSolver.dll $(EX_DIR)$Stests$Stestlp.cs

testlp: $(BIN_DIR)/testlp.exe
	$(MONO) $(BIN_DIR)$Stestlp.exe

# csharpcp

csharpcp: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll

$(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc: $(SRC_DIR)/constraint_solver/routing.swig $(SRC_DIR)/constraint_solver/constraint_solver.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport "Google.OrTools.ConstraintSolver.$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)/constraint_solver$Srouting.swig

$(OBJ_DIR)/constraint_solver_csharp_wrap.$O: $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.ConstraintSolver.dll: $(OBJ_DIR)/constraint_solver_csharp_wrap.$O $(STATIC_ROUTING_DEPS) $(SRC_DIR)/com/google/ortools/constraintsolver/IntVarArrayHelper.cs $(SRC_DIR)/com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs $(SRC_DIR)/com/google/ortools/constraintsolver/IntArrayHelper.cs $(SRC_DIR)/com/google/ortools/constraintsolver/ValCstPair.cs $(SRC_DIR)/com/google/ortools/constraintsolver/NetDecisionBuilder.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs $(SRC_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)\\Google.OrTools.ConstraintSolver.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule $(OBJ_DIR)$Sconstraint_solver_csharp_wrap.$O $(STATIC_ROUTING_LNK) $(STATIC_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.ConstraintSolver.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/com/google/ortools/constraintsolver/*.cs
	$(DYNAMIC_LD)  $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.ConstraintSolver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/constraint_solver_csharp_wrap.$O $(STATIC_ROUTING_LNK) $(STATIC_LD_FLAGS)
endif

# csharp cp examples

$(BIN_DIR)/csrabbitspheasants.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/csrabbitspheasants.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsrabbitspheasants.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Scsrabbitspheasants.cs

$(BIN_DIR)/send_more_money.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/send_more_money.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Ssend_more_money.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Ssend_more_money.cs

$(BIN_DIR)/furniture_moving_intervals.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/furniture_moving_intervals.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sfurniture_moving_intervals.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Sfurniture_moving_intervals.cs

$(BIN_DIR)/organize_day_intervals.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/organize_day_intervals.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sorganize_day_intervals.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Sorganize_day_intervals.cs

$(BIN_DIR)/cstsp.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/cstsp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scstsp.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Scstsp.cs

$(BIN_DIR)/cscvrptw.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/cscvrptw.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scscvrptw.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Scscvrptw.cs

$(BIN_DIR)/csls_api.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/csls_api.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsls_api.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Scsls_api.cs

# csharp constraint solver tests

$(BIN_DIR)/testcp.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/tests/testcp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestcp.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Stests$Stestcp.cs

testcp: $(BIN_DIR)/testcp.exe
	$(MONO) $(BIN_DIR)$Stestcp.exe

$(BIN_DIR)/issue18.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/tests/issue18.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue18.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Stests$Sissue18.cs

issue18: $(BIN_DIR)/issue18.exe
	$(MONO) $(BIN_DIR)$Sissue18.exe

$(BIN_DIR)/issue22.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/tests/issue22.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue22.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Stests$Sissue22.cs

issue22: $(BIN_DIR)/issue22.exe
	$(MONO) $(BIN_DIR)$Sissue22.exe

$(BIN_DIR)/issue33.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/tests/issue33.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue33.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Stests$Sissue33.cs

issue33: $(BIN_DIR)/issue33.exe
	$(MONO) $(BIN_DIR)$Sissue33.exe

$(BIN_DIR)/jobshop_bug.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/tests/jobshop_bug.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sjobshop_bug.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Stests$Sjobshop_bug.cs

jobshop_bug: $(BIN_DIR)/jobshop_bug.exe
	$(MONO) $(BIN_DIR)$Sjobshop_bug.exe

$(BIN_DIR)/slow_scheduling.exe: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/slow_scheduling.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sslow_scheduling.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$Sslow_scheduling.cs

slow_scheduling: $(BIN_DIR)/slow_scheduling.exe
	$(MONO) $(BIN_DIR)$Sslow_scheduling.exe

# csharpalgorithms

csharpalgorithms: $(BIN_DIR)/Google.OrTools.Algorithms.dll

$(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc: $(SRC_DIR)/algorithms/knapsack_solver.swig $(SRC_DIR)/algorithms/knapsack_solver.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport "Google.OrTools.Algorithms.$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver $(SRC_DIR)/algorithms$Sknapsack_solver.swig

$(OBJ_DIR)/knapsack_solver_csharp_wrap.$O: $(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sknapsack_solver_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.Algorithms.dll: $(OBJ_DIR)/knapsack_solver_csharp_wrap.$O $(STATIC_ALGORITHMS_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\knapsacksolver\\*.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)\\Google.OrTools.Algorithms.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Algorithms.netmodule $(OBJ_DIR)\\knapsack_solver_csharp_wrap.$O $(STATIC_ALGORITHMS_LNK) $(STATIC_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.Algorithms.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/knapsacksolver/*.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Algorithms.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/knapsack_solver_csharp_wrap.$O $(STATIC_ALGORITHMS_LNK) $(STATIC_LD_FLAGS)
endif

# csharp algorithm examples

$(BIN_DIR)/csknapsack.exe: $(BIN_DIR)/Google.OrTools.Algorithms.dll $(EX_DIR)/csharp/csknapsack.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsknapsack.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.Algorithms.dll $(EX_DIR)$Scsharp$Scsknapsack.cs

# csharpgraph

csharpgraph: $(BIN_DIR)/Google.OrTools.Graph.dll

$(GEN_DIR)/graph/graph_csharp_wrap.cc: $(SRC_DIR)/graph/graph.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/graph/max_flow.h $(SRC_DIR)/graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport "Google.OrTools.Graph.$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

$(OBJ_DIR)/graph_csharp_wrap.$O: $(GEN_DIR)/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.Graph.dll: $(OBJ_DIR)/graph_csharp_wrap.$O $(STATIC_GRAPH_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /unsafe /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Graph.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\graph\\*.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)\\Google.OrTools.Graph.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Graph.netmodule $(OBJ_DIR)\\graph_csharp_wrap.$O $(STATIC_GRAPH_LNK) $(STATIC_LD_FLAGS)
else
	$(CSC) /target:library /unsafe /out:$(BIN_DIR)/Google.OrTools.Graph.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/graph/*.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Graph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/graph_csharp_wrap.$O $(STATIC_GRAPH_LNK) $(STATIC_LD_FLAGS)
endif

# csharp graph examples

$(BIN_DIR)/csflow.exe: $(BIN_DIR)/Google.OrTools.Graph.dll $(EX_DIR)/csharp/csflow.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsflow.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.Graph.dll $(EX_DIR)$Scsharp$Scsflow.cs

# Build and compile custome CP examples

csc: $(BIN_DIR)/Google.OrTools.ConstraintSolver.dll $(EX_DIR)/csharp/$(EX).cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.ConstraintSolver.dll $(EX_DIR)$Scsharp$S$(EX).cs

rcs: csc
	$(MONO) $(BIN_DIR)$S$(EX).exe $(ARGS)


# Build archive.

dotnet_archive: csharp
	-$(DELREC) temp
ifeq ("$(SYSTEM)","win")
	tools\mkdir temp
	tools\mkdir temp\or-tools.$(PORT)
	tools\mkdir temp\or-tools.$(PORT)\bin
	tools\mkdir temp\or-tools.$(PORT)\examples
	tools\mkdir temp\or-tools.$(PORT)\examples\solution
	tools\mkdir temp\or-tools.$(PORT)\examples\solution\Properties
	tools\mkdir temp\or-tools.$(PORT)\data
	tools\mkdir temp\or-tools.$(PORT)\data\discrete_tomography
	tools\mkdir temp\or-tools.$(PORT)\data\fill_a_pix
	tools\mkdir temp\or-tools.$(PORT)\data\minesweeper
	tools\mkdir temp\or-tools.$(PORT)\data\rogo
	tools\mkdir temp\or-tools.$(PORT)\data\survo_puzzle
	tools\mkdir temp\or-tools.$(PORT)\data\quasigroup_completion
	copy LICENSE-2.0.txt temp$Sor-tools.$(PORT)
	copy tools\README.dotnet temp\or-tools.$(PORT)\README
	copy bin\Google.OrTools.*.dll temp\or-tools.$(PORT)\bin
	copy examples\csharp\*.cs temp\or-tools.$(PORT)\examples
	copy examples\csharp\*.sln temp\or-tools.$(PORT)\examples
	copy examples\csharp\solution\*.csproj temp\or-tools.$(PORT)\examples\solution
	copy examples\csharp\solution\Properties\*.cs temp\or-tools.$(PORT)\examples\solution\Properties
	copy data\discrete_tomography\* temp\or-tools.$(PORT)\data\discrete_tomography
	copy data\fill_a_pix\* temp\or-tools.$(PORT)\data\fill_a_pix
	copy data\minesweeper\* temp\or-tools.$(PORT)\data\minesweeper
	copy data\rogo\* temp\or-tools.$(PORT)\data\rogo
	copy data\survo_puzzle\* temp\or-tools.$(PORT)\data\survo_puzzle
	copy data\quasigroup_completion\* temp\or-tools.$(PORT)\data\quasigroup_completion
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.NET.$(PORT).$(SVNVERSION).zip or-tools.$(PORT)
else
	mkdir temp
	mkdir temp/or-tools.$(PORT)
	mkdir temp/or-tools.$(PORT)/bin
	mkdir temp/or-tools.$(PORT)/examples
	mkdir temp/or-tools.$(PORT)/data
	mkdir temp/or-tools.$(PORT)/data/discrete_tomography
	mkdir temp/or-tools.$(PORT)/data/fill_a_pix
	mkdir temp/or-tools.$(PORT)/data/minesweeper
	mkdir temp/or-tools.$(PORT)/data/rogo
	mkdir temp/or-tools.$(PORT)/data/survo_puzzle
	mkdir temp/or-tools.$(PORT)/data/quasigroup_completion
	cp LICENSE-2.0.txt temp/or-tools.$(PORT)
	cp tools/README.dotnet temp/or-tools.$(PORT)/README
	cp bin/Google.OrTools.*.dll temp/or-tools.$(PORT)/bin
	cp lib/libGoogle.OrTools.*.so temp/or-tools.$(PORT)/bin
	cp examples/csharp/*.cs temp/or-tools.$(PORT)/examples
	cp data/discrete_tomography/* temp/or-tools.$(PORT)/data/discrete_tomography
	cp data/fill_a_pix/* temp/or-tools.$(PORT)/data/fill_a_pix
	cp data/minesweeper/* temp/or-tools.$(PORT)/data/minesweeper
	cp data/rogo/* temp/or-tools.$(PORT)/data/rogo
	cp data/survo_puzzle/* temp/or-tools.$(PORT)/data/survo_puzzle
	cp data/quasigroup_completion/* temp/or-tools.$(PORT)/data/quasigroup_completion
	cd temp && tar cvzf ../Google.OrTools.NET.$(PORT).$(SVNVERSION).tar.gz or-tools.$(PORT)
endif
	-$(DELREC) temp
