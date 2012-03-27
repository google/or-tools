# ---------- CSharp support using SWIG ----------
CSHARPEXE = \
	$(BINPREFIX)$Scslinearprogramming.exe \
	$(BINPREFIX)$Scsintegerprogramming.exe \
	$(BINPREFIX)$Scsrabbitspheasants.exe \
	$(BINPREFIX)$Scsflow.exe \
	$(BINPREFIX)$Scsknapsack.exe \
	$(BINPREFIX)$Sfurniture_moving_intervals.exe \
	$(BINPREFIX)$Sorganize_day_intervals.exe \
	$(BINPREFIX)$Scstsp.exe

csharpexe: $(CSHARPEXE)

# Main target.
csharp: csharpcp csharplp csharpalgorithms csharpgraph csharpexe

# Clean target.
clean_csharp:
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.$(SHAREDLIBEXT)
	-$(DEL) $(BINPREFIX)$SGoogle.OrTools.*.dll
	-$(DEL) $(BINPREFIX)$SGoogle.OrTools.*.mdb
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.lib
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.pdb
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.exp
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.netmodule
	-$(DEL) $(TOP)$Sgen$Slinear_solver$S*csharp_wrap*
	-$(DEL) $(TOP)$Sgen$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) $(TOP)$Sgen$Salgorithms$S*csharp_wrap*
	-$(DEL) $(TOP)$Sgen$Sgraph$S*csharp_wrap*
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	-$(DEL) $(TOP)$Sobjs$S*csharp_wrap.$O
	-$(DEL) $(BINPREFIX)$S*.exe

# csharplp

ifeq ($(SYSTEM),win)
IMPORTPREFIX=$(TOP)\\bin\\
else
IMPORTPREFIX=$(TOP)/lib/lib
endif

csharplp: $(BINPREFIX)/Google.OrTools.LinearSolver.dll

$(TOP)/gen/linear_solver/linear_solver_csharp_wrap.cc: $(TOP)/linear_solver/linear_solver.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/linear_solver/linear_solver.h $(TOP)/gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(TOP) -c++ -csharp -o $(TOP)$Sgen$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport $(IMPORTPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Slinearsolver $(TOP)$Slinear_solver$Slinear_solver.swig

$(TOP)/objs/linear_solver_csharp_wrap.$O: $(TOP)/gen/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/linear_solver/linear_solver_csharp_wrap.cc $(OBJOUT)objs/linear_solver_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.LinearSolver.dll: $(TOP)/objs/linear_solver_csharp_wrap.$O $(LP_DEPS) $(TOP)/com/google/ortools/linearsolver/LinearExpr.cs $(TOP)/com/google/ortools/linearsolver/LinearConstraint.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug $(TOP)\\gen\\com\\google\\ortools\\linearsolver\\*.cs $(TOP)\\com\\google\\ortools\\linearsolver\\*.cs
	$(LD) $(LDOUT)$(TOP)\\bin\\Google.OrTools.LinearSolver.dll $(LIBPREFIX)Google.OrTools.LinearSolver.netmodule $(TOP)\\objs\\linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BINPREFIX)/Google.OrTools.LinearSolver.dll /warn:0 /nologo /debug $(TOP)/gen/com/google/ortools/linearsolver/*.cs $(TOP)/com/google/ortools/linearsolver/*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) $(TOP)/objs/linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
endif

# csharp linearsolver examples

$(BINPREFIX)/cslinearprogramming.exe: $(BINPREFIX)/Google.OrTools.LinearSolver.dll $(TOP)/csharp/cslinearprogramming.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scslinearprogramming.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.LinearSolver.dll $(TOP)$Scsharp$Scslinearprogramming.cs

$(BINPREFIX)/csintegerprogramming.exe: $(BINPREFIX)/Google.OrTools.LinearSolver.dll $(TOP)/csharp/csintegerprogramming.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsintegerprogramming.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.LinearSolver.dll $(TOP)$Scsharp$Scsintegerprogramming.cs

# csharp linearsolver tests

$(BINPREFIX)/testlp.exe: $(BINPREFIX)/Google.OrTools.LinearSolver.dll $(TOP)/tests/testlp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Stestlp.exe /platform:$(NETPLATFORM) /r:$(TOP)$Sbin$SGoogle.OrTools.LinearSolver.dll $(TOP)$Stests$Stestlp.cs

testlp: $(BINPREFIX)/testlp.exe
	$(MONO) bin$Stestlp.exe

# csharpcp

csharpcp: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll

$(TOP)/gen/constraint_solver/constraint_solver_csharp_wrap.cc: $(TOP)/constraint_solver/routing.swig $(TOP)/constraint_solver/constraint_solver.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(TOP) -c++ -csharp -o $(TOP)$Sgen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport $(IMPORTPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver $(TOP)$Sconstraint_solver$Srouting.swig

$(TOP)/objs/constraint_solver_csharp_wrap.$O: $(TOP)/gen/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(TOP)$Sgen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJOUT)objs$Sconstraint_solver_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.ConstraintSolver.dll: $(TOP)/objs/constraint_solver_csharp_wrap.$O $(ROUTING_DEPS) $(TOP)/com/google/ortools/constraintsolver/IntVarArrayHelper.cs $(TOP)/com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs $(TOP)/com/google/ortools/constraintsolver/IntArrayHelper.cs $(TOP)/com/google/ortools/constraintsolver/ValCstPair.cs $(TOP)/com/google/ortools/constraintsolver/NetDecisionBuilder.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug $(TOP)\\gen\\com\\google\\ortools\\constraintsolver\\*.cs com\\google\\ortools\\constraintsolver\\*.cs
	$(LD) $(LDOUT)$(TOP)\\bin\\Google.OrTools.ConstraintSolver.dll $(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule $(TOP)$Sobjs$Sconstraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BINPREFIX)/Google.OrTools.ConstraintSolver.dll /warn:0 /nologo /debug $(TOP)/gen/com/google/ortools/constraintsolver/*.cs $(TOP)/com/google/ortools/constraintsolver/*.cs
	$(LD)  $(LDOUT)$(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) $(TOP)/objs/constraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
endif

# csharp cp examples

$(BINPREFIX)/csrabbitspheasants.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/csharp/csrabbitspheasants.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsrabbitspheasants.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Scsharp$Scsrabbitspheasants.cs

$(BINPREFIX)/send_more_money.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/csharp/send_more_money.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Ssend_more_money.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Scsharp$Ssend_more_money.cs

$(BINPREFIX)/furniture_moving_intervals.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/csharp/furniture_moving_intervals.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Sfurniture_moving_intervals.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Scsharp$Sfurniture_moving_intervals.cs

$(BINPREFIX)/organize_day_intervals.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/csharp/organize_day_intervals.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Sorganize_day_intervals.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Scsharp$Sorganize_day_intervals.cs

$(BINPREFIX)/cstsp.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/csharp/cstsp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scstsp.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Scsharp$Scstsp.cs

# csharp constraint solver tests

$(BINPREFIX)/testcp.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/tests/testcp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Stestcp.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Stests$Stestcp.cs

testcp: $(BINPREFIX)/testcp.exe
	$(MONO) $(BINPREFIX)$Stestcp.exe

# csharpalgorithms

csharpalgorithms: $(BINPREFIX)/Google.OrTools.Algorithms.dll

$(TOP)/gen/algorithms/knapsack_solver_csharp_wrap.cc: $(TOP)/algorithms/knapsack_solver.swig $(TOP)/algorithms/knapsack_solver.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(TOP) -c++ -csharp -o $(TOP)$Sgen$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport $(IMPORTPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sknapsacksolver $(TOP)$Salgorithms$Sknapsack_solver.swig

$(TOP)/objs/knapsack_solver_csharp_wrap.$O: $(TOP)/gen/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/algorithms/knapsack_solver_csharp_wrap.cc $(OBJOUT)objs$Sknapsack_solver_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.Algorithms.dll: $(TOP)/objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug $(TOP)\\gen\\com\\google\\ortools\\knapsacksolver\\*.cs
	$(LD) $(LDOUT)$(TOP)\\bin\\Google.OrTools.Algorithms.dll $(LIBPREFIX)Google.OrTools.Algorithms.netmodule $(TOP)\\objs\\knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BINPREFIX)/Google.OrTools.Algorithms.dll /warn:0 /nologo /debug $(TOP)/gen/com/google/ortools/knapsacksolver/*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) $(TOP)/objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
endif

# csharp algorithm examples

$(BINPREFIX)/csknapsack.exe: $(BINPREFIX)/Google.OrTools.Algorithms.dll $(TOP)/csharp/csknapsack.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsknapsack.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.Algorithms.dll $(TOP)$Scsharp$Scsknapsack.cs

# csharpgraph

csharpgraph: $(BINPREFIX)/Google.OrTools.Graph.dll

$(TOP)/gen/graph/graph_csharp_wrap.cc: $(TOP)/graph/graph.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/graph/max_flow.h $(TOP)/graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(TOP) -c++ -csharp -o $(TOP)$Sgen$Sgraph$Sgraph_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport $(IMPORTPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

$(TOP)/objs/graph_csharp_wrap.$O: $(TOP)/gen/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(TOP)$Sgen$Sgraph$Sgraph_csharp_wrap.cc $(OBJOUT)objs$Sgraph_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.Graph.dll: $(TOP)/objs/graph_csharp_wrap.$O $(GRAPH_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /unsafe /out:$(LIBPREFIX)Google.OrTools.Graph.netmodule /warn:0 /nologo /debug $(TOP)\\gen\\com\\google\\ortools\\graph\\*.cs
	$(LD) $(LDOUT)$(TOP)\\bin\\Google.OrTools.Graph.dll $(LIBPREFIX)Google.OrTools.Graph.netmodule $(TOP)\\objs\\graph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /unsafe /out:$(BINPREFIX)/Google.OrTools.Graph.dll /warn:0 /nologo /debug $(TOP)/gen/com/google/ortools/graph/*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) $(TOP)/objs/graph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
endif

# csharp graph examples

$(BINPREFIX)/csflow.exe: $(BINPREFIX)/Google.OrTools.Graph.dll $(TOP)/csharp/csflow.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsflow.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.Graph.dll $(TOP)$Scsharp$Scsflow.cs

# Build and compile custome CP examples

csc: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(TOP)/csharp/$(EX).cs
	$(CSC) /target:exe /out:$(BINPREFIX)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll $(TOP)$Scsharp$S$(EX).cs

rcs: csc
	$(MONO) $(BINPREFIX)$S$(EX).exe $(ARGS)


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
