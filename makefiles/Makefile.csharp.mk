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
	-$(DEL) bin$SGoogle.OrTools.*.dll
	-$(DEL) bin$SGoogle.OrTools.*.mdb
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.lib
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.pdb
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.exp
	-$(DEL) $(LIBPREFIX)Google.OrTools.*.netmodule
	-$(DEL) gen$Slinear_solver$S*csharp_wrap*
	-$(DEL) gen$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) gen$Salgorithms$S*csharp_wrap*
	-$(DEL) gen$Sgraph$S*csharp_wrap*
	-$(DEL) gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	-$(DEL) gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	-$(DEL) gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	-$(DEL) gen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	-$(DEL) objs$S*csharp_wrap.$O
	-$(DEL) bin$S*.exe

# csharplp

csharplp: $(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT)

gen/linear_solver/linear_solver_csharp_wrap.cc: linear_solver/linear_solver.swig base/base.swig util/data.swig linear_solver/linear_solver.h gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport $(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Slinearsolver linear_solver$Slinear_solver.swig

objs/linear_solver_csharp_wrap.$O: gen/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c gen/linear_solver/linear_solver_csharp_wrap.cc $(OBJOUT)objs/linear_solver_csharp_wrap.$O

$(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT): objs/linear_solver_csharp_wrap.$O $(LP_DEPS) com/google/ortools/linearsolver/LinearExpr.cs com/google/ortools/linearsolver/LinearConstraint.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs com$Sgoogle$Sortools$Slinearsolver$S*.cs
	$(LD) $(LDOUT)$(TOP)$Sbin$SGoogle.OrTools.LinearSolver.$(SHAREDLIBEXT) $(LIBPREFIX)Google.OrTools.LinearSolver.netmodule objs/linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(TOP)/bin$SGoogle.OrTools.LinearSolver.dll /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs com$Sgoogle$Sortools$Slinearsolver$S*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) objs/linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
endif

# csharp linearsolver examples

$(BINPREFIX)/cslinearprogramming.exe: $(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) csharp/cslinearprogramming.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scslinearprogramming.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.LinearSolver.dll csharp$Scslinearprogramming.cs

$(BINPREFIX)/csintegerprogramming.exe: $(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) csharp/csintegerprogramming.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsintegerprogramming.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.LinearSolver.dll csharp$Scsintegerprogramming.cs

# csharp linearsolver tests

$(BINPREFIX)/testlp.exe: $(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) tests/testlp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Stestlp.exe /platform:$(NETPLATFORM) /r:$(TOP)$Sbin$SGoogle.OrTools.LinearSolver.dll tests$Stestlp.cs

testlp: $(BINPREFIX)/testlp.exe
	$(MONO) bin$Stestlp.exe

# csharpcp

csharpcp: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT)

gen/constraint_solver/constraint_solver_csharp_wrap.cc: constraint_solver/routing.swig constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Sconstraintsolver constraint_solver$Srouting.swig

objs/constraint_solver_csharp_wrap.$O: gen/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/constraint_solver_csharp_wrap.cc $(OBJOUT)objs/constraint_solver_csharp_wrap.$O

$(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT): objs/constraint_solver_csharp_wrap.$O $(ROUTING_DEPS) com/google/ortools/constraintsolver/IntVarArrayHelper.cs com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs com/google/ortools/constraintsolver/IntArrayHelper.cs com/google/ortools/constraintsolver/ValCstPair.cs com/google/ortools/constraintsolver/NetDecisionBuilder.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs com$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	$(LD) $(LDOUT)$(TOP)$Sbin$SGoogle.OrTools.ConstraintSolver.$(SHAREDLIBEXT) $(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule objs/constraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:bin$SGoogle.OrTools.ConstraintSolver.dll /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs com$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	$(LD)  $(LDOUT)$(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) objs/constraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
endif

# csharp cp examples

$(BINPREFIX)/csrabbitspheasants.exe: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp/csrabbitspheasants.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsrabbitspheasants.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll csharp$Scsrabbitspheasants.cs

$(BINPREFIX)/send_more_money.exe: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp/send_more_money.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Ssend_more_money.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll csharp$Ssend_more_money.cs

$(BINPREFIX)/furniture_moving_intervals.exe: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp/furniture_moving_intervals.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Sfurniture_moving_intervals.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll csharp$Sfurniture_moving_intervals.cs

$(BINPREFIX)/organize_day_intervals.exe: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp/organize_day_intervals.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Sorganize_day_intervals.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll csharp$Sorganize_day_intervals.cs

$(BINPREFIX)/cstsp.exe: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp/cstsp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scstsp.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll csharp$Scstsp.cs

# csharp constraint solver tests

$(BINPREFIX)/testcp.exe: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) tests/testcp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Stestcp.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll tests$Stestcp.cs

testcp: testcp.exe
	$(MONO) testcp.exe

# csharpalgorithms

csharpalgorithms: $(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT)

gen/algorithms/knapsack_solver_csharp_wrap.cc: algorithms/knapsack_solver.swig algorithms/knapsack_solver.swig base/base.swig util/data.swig algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport $(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Sknapsacksolver algorithms$Sknapsack_solver.swig

objs/knapsack_solver_csharp_wrap.$O: gen/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c gen/algorithms/knapsack_solver_csharp_wrap.cc $(OBJOUT)objs/knapsack_solver_csharp_wrap.$O

$(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT): objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	$(LD) $(LDOUT)$(TOP)$sbin$SGoogle.OrTools.Algorithms.$(SHAREDLIBEXT) $(LIBPREFIX)Google.OrTools.Algorithms.netmodule objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(TOP)$Sbin$SGoogle.OrTools.Algorithms.dll /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
endif

# csharp algorithm examples

$(BINPREFIX)/csknapsack.exe: $(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) csharp/csknapsack.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsknapsack.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.Algorithms.dll csharp$Scsknapsack.cs

# csharpgraph

csharpgraph: $(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT)

gen/graph/graph_csharp_wrap.cc: graph/graph.swig base/base.swig util/data.swig graph/max_flow.h graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Sgraph$Sgraph_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport $(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

objs/graph_csharp_wrap.$O: gen/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c gen$Sgraph$Sgraph_csharp_wrap.cc $(OBJOUT)objs$Sgraph_csharp_wrap.$O

$(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT): objs/graph_csharp_wrap.$O $(GRAPH_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /unsafe /out:$(LIBPREFIX)Google.OrTools.Graph.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	$(LD) $(LDOUT)$(TOP)$Sbin$SGoogle.OrTools.Graph.$(SHAREDLIBEXT) $(LIBPREFIX)Google.OrTools.Graph.netmodule objs$Sgraph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /unsafe /out:$(TOP)$Sbin$SGoogle.OrTools.Graph.dll /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) objs$Sgraph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
endif

# csharp graph examples

$(BINPREFIX)/csflow.exe: $(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) csharp/csflow.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsflow.exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.Graph.dll csharp$Scsflow.cs

# Build and compile custome CP examples

csc: $(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp/$(EX).cs
	$(CSC) /target:exe /out:$(BINPREFIX)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(TOP)$Sbin /r:Google.OrTools.ConstraintSolver.dll csharp$S$(EX).cs

rcs: csc
	$(MONO) $(EX).exe $(ARGS)


# Build archive.

dotnet_archive: csharp
	-$(DELREC) temp
	tools\mkdir temp
	tools\mkdir temp\or-tools.$(PLATFORM)
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
	copy $(LIBPREFIX)Google.OrTools.*.$(SHAREDLIBEXT) temp\or-tools.$(PLATFORM)
ifneq ($(SYSTEM),win)
	copy Google.OrTools.*.dll temp\or-tools.$(PLATFORM)
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
