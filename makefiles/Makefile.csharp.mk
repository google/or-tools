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
	-$(DEL) $(OR_ROOT)gen$Slinear_solver$S*csharp_wrap*
	-$(DEL) $(OR_ROOT)gen$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) $(OR_ROOT)gen$Salgorithms$S*csharp_wrap*
	-$(DEL) $(OR_ROOT)gen$Sgraph$S*csharp_wrap*
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	-$(DEL) $(OR_ROOT)objs$S*csharp_wrap.$O
	-$(DEL) $(BINPREFIX)$S*.exe

# csharplp

ifeq ($(SYSTEM),win)
IMPORTPREFIX=$(OR_ROOT_FULL)\\bin\\
else
IMPORTPREFIX=$(OR_ROOT_FULL)/lib/lib
endif

csharplp: $(BINPREFIX)/Google.OrTools.LinearSolver.dll

$(OR_ROOT)gen/linear_solver/linear_solver_csharp_wrap.cc: $(OR_ROOT)linear_solver/linear_solver.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)linear_solver/linear_solver.h $(OR_ROOT)gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(OR_ROOT_INC) -c++ -csharp -o $(OR_ROOT)gen$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport $(IMPORTPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Slinearsolver $(OR_ROOT)linear_solver$Slinear_solver.swig

$(OR_ROOT)objs/linear_solver_csharp_wrap.$O: $(OR_ROOT)gen/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/linear_solver/linear_solver_csharp_wrap.cc $(OBJOUT)objs/linear_solver_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.LinearSolver.dll: $(OR_ROOT)objs/linear_solver_csharp_wrap.$O $(LP_DEPS) $(OR_ROOT)com/google/ortools/linearsolver/LinearExpr.cs $(OR_ROOT)com/google/ortools/linearsolver/LinearConstraint.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug $(OR_ROOT)gen\\com\\google\\ortools\\linearsolver\\*.cs $(OR_ROOT)com\\google\\ortools\\linearsolver\\*.cs
	$(LD) $(LDOUT)$(OR_ROOT)bin\\Google.OrTools.LinearSolver.dll $(LIBPREFIX)Google.OrTools.LinearSolver.netmodule $(OR_ROOT)objs\\linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BINPREFIX)/Google.OrTools.LinearSolver.dll /warn:0 /nologo /debug $(OR_ROOT)gen/com/google/ortools/linearsolver/*.cs $(OR_ROOT)com/google/ortools/linearsolver/*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) $(OR_ROOT)objs/linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)
endif

# csharp linearsolver examples

$(BINPREFIX)/cslinearprogramming.exe: $(BINPREFIX)/Google.OrTools.LinearSolver.dll $(OR_ROOT)csharp/cslinearprogramming.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scslinearprogramming.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.LinearSolver.dll $(OR_ROOT)csharp$Scslinearprogramming.cs

$(BINPREFIX)/csintegerprogramming.exe: $(BINPREFIX)/Google.OrTools.LinearSolver.dll $(OR_ROOT)csharp/csintegerprogramming.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsintegerprogramming.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.LinearSolver.dll $(OR_ROOT)csharp$Scsintegerprogramming.cs

# csharp linearsolver tests

$(BINPREFIX)/testlp.exe: $(BINPREFIX)/Google.OrTools.LinearSolver.dll $(OR_ROOT)tests/testlp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Stestlp.exe /platform:$(NETPLATFORM) /r:$(OR_ROOT)bin$SGoogle.OrTools.LinearSolver.dll $(OR_ROOT)tests$Stestlp.cs

testlp: $(BINPREFIX)/testlp.exe
	$(MONO) bin$Stestlp.exe

# csharpcp

csharpcp: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll

$(OR_ROOT)gen/constraint_solver/constraint_solver_csharp_wrap.cc: $(OR_ROOT)constraint_solver/routing.swig $(OR_ROOT)constraint_solver/constraint_solver.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(OR_ROOT_INC) -c++ -csharp -o $(OR_ROOT)gen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport $(IMPORTPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sconstraintsolver $(OR_ROOT)constraint_solver$Srouting.swig

$(OR_ROOT)objs/constraint_solver_csharp_wrap.$O: $(OR_ROOT)gen/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJOUT)objs$Sconstraint_solver_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.ConstraintSolver.dll: $(OR_ROOT)objs/constraint_solver_csharp_wrap.$O $(ROUTING_DEPS) $(OR_ROOT)com/google/ortools/constraintsolver/IntVarArrayHelper.cs $(OR_ROOT)com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs $(OR_ROOT)com/google/ortools/constraintsolver/IntArrayHelper.cs $(OR_ROOT)com/google/ortools/constraintsolver/ValCstPair.cs $(OR_ROOT)com/google/ortools/constraintsolver/NetDecisionBuilder.cs
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug $(OR_ROOT)gen\\com\\google\\ortools\\constraintsolver\\*.cs com\\google\\ortools\\constraintsolver\\*.cs
	$(LD) $(LDOUT)$(OR_ROOT)bin\\Google.OrTools.ConstraintSolver.dll $(LIBPREFIX)Google.OrTools.ConstraintSolver.netmodule $(OR_ROOT)objs$Sconstraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BINPREFIX)/Google.OrTools.ConstraintSolver.dll /warn:0 /nologo /debug $(OR_ROOT)gen/com/google/ortools/constraintsolver/*.cs $(OR_ROOT)com/google/ortools/constraintsolver/*.cs
	$(LD)  $(LDOUT)$(LIBPREFIX)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) $(OR_ROOT)objs/constraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)
endif

# csharp cp examples

$(BINPREFIX)/csrabbitspheasants.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp/csrabbitspheasants.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsrabbitspheasants.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp$Scsrabbitspheasants.cs

$(BINPREFIX)/send_more_money.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp/send_more_money.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Ssend_more_money.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp$Ssend_more_money.cs

$(BINPREFIX)/furniture_moving_intervals.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp/furniture_moving_intervals.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Sfurniture_moving_intervals.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp$Sfurniture_moving_intervals.cs

$(BINPREFIX)/organize_day_intervals.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp/organize_day_intervals.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Sorganize_day_intervals.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp$Sorganize_day_intervals.cs

$(BINPREFIX)/cstsp.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp/cstsp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scstsp.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp$Scstsp.cs

# csharp constraint solver tests

$(BINPREFIX)/testcp.exe: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)tests/testcp.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Stestcp.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)tests$Stestcp.cs

testcp: $(BINPREFIX)/testcp.exe
	$(MONO) $(BINPREFIX)$Stestcp.exe

# csharpalgorithms

csharpalgorithms: $(BINPREFIX)/Google.OrTools.Algorithms.dll

$(OR_ROOT)gen/algorithms/knapsack_solver_csharp_wrap.cc: $(OR_ROOT)algorithms/knapsack_solver.swig $(OR_ROOT)algorithms/knapsack_solver.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(OR_ROOT_INC) -c++ -csharp -o $(OR_ROOT)gen$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport $(IMPORTPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sknapsacksolver $(OR_ROOT)algorithms$Sknapsack_solver.swig

$(OR_ROOT)objs/knapsack_solver_csharp_wrap.$O: $(OR_ROOT)gen/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/algorithms/knapsack_solver_csharp_wrap.cc $(OBJOUT)objs$Sknapsack_solver_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.Algorithms.dll: $(OR_ROOT)objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIBPREFIX)Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug $(OR_ROOT)gen\\com\\google\\ortools\\knapsacksolver\\*.cs
	$(LD) $(LDOUT)$(OR_ROOT)bin\\Google.OrTools.Algorithms.dll $(LIBPREFIX)Google.OrTools.Algorithms.netmodule $(OR_ROOT)objs\\knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /out:$(BINPREFIX)/Google.OrTools.Algorithms.dll /warn:0 /nologo /debug $(OR_ROOT)gen/com/google/ortools/knapsacksolver/*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.Algorithms.$(SHAREDLIBEXT) $(OR_ROOT)objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)
endif

# csharp algorithm examples

$(BINPREFIX)/csknapsack.exe: $(BINPREFIX)/Google.OrTools.Algorithms.dll $(OR_ROOT)csharp/csknapsack.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsknapsack.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.Algorithms.dll $(OR_ROOT)csharp$Scsknapsack.cs

# csharpgraph

csharpgraph: $(BINPREFIX)/Google.OrTools.Graph.dll

$(OR_ROOT)gen/graph/graph_csharp_wrap.cc: $(OR_ROOT)graph/graph.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)graph/max_flow.h $(OR_ROOT)graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(OR_ROOT_INC) -c++ -csharp -o $(OR_ROOT)gen$Sgraph$Sgraph_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport $(IMPORTPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

$(OR_ROOT)objs/graph_csharp_wrap.$O: $(OR_ROOT)gen/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen$Sgraph$Sgraph_csharp_wrap.cc $(OBJOUT)objs$Sgraph_csharp_wrap.$O

$(BINPREFIX)/Google.OrTools.Graph.dll: $(OR_ROOT)objs/graph_csharp_wrap.$O $(GRAPH_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /unsafe /out:$(LIBPREFIX)Google.OrTools.Graph.netmodule /warn:0 /nologo /debug $(OR_ROOT)gen\\com\\google\\ortools\\graph\\*.cs
	$(LD) $(LDOUT)$(OR_ROOT)bin\\Google.OrTools.Graph.dll $(LIBPREFIX)Google.OrTools.Graph.netmodule $(OR_ROOT)objs\\graph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /unsafe /out:$(BINPREFIX)/Google.OrTools.Graph.dll /warn:0 /nologo /debug $(OR_ROOT)gen/com/google/ortools/graph/*.cs
	$(LD) $(LDOUT)$(LIBPREFIX)Google.OrTools.Graph.$(SHAREDLIBEXT) $(OR_ROOT)objs/graph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
endif

# csharp graph examples

$(BINPREFIX)/csflow.exe: $(BINPREFIX)/Google.OrTools.Graph.dll $(OR_ROOT)csharp/csflow.cs
	$(CSC) /target:exe /out:$(BINPREFIX)$Scsflow.exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.Graph.dll $(OR_ROOT)csharp$Scsflow.cs

# Build and compile custome CP examples

csc: $(BINPREFIX)/Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp/$(EX).cs
	$(CSC) /target:exe /out:$(BINPREFIX)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(OR_ROOT)bin /r:Google.OrTools.ConstraintSolver.dll $(OR_ROOT)csharp$S$(EX).cs

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
