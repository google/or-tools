# ---------- CSharp support using SWIG ----------

# csharp

csharp: csharpcp csharplp csharpalgorithms csharpgraph

CSHARPEXE = \
	cslinearprogramming.exe \
	csintegerprogramming.exe \
	csrabbitspheasants.exe \
	csflow.exe \
	csknapsack.exe

csharpexe: $(CSHARPEXE)

# clearcsharp

cleancsharp:
	$(DEL) Google.OrTools.*.$(SHAREDLIBEXT)
	$(DEL) Google.OrTools.*.dll
	$(DEL) Google.OrTools.*.lib
	$(DEL) Google.OrTools.*.pdb
	$(DEL) Google.OrTools.*.exp
	$(DEL) Google.OrTools.*.netmodule
	$(DEL) gen$Slinear_solver$S*csharp_wrap*
	$(DEL) gen$Sconstraint_solver$S*csharp_wrap*
	$(DEL) gen$Salgorithms$S*csharp_wrap*
	$(DEL) gen$Sgraph$S*csharp_wrap*
	$(DEL) gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	$(DEL) gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	$(DEL) gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	$(DEL) gen$Scom$Sgoogle$Sortools$Sflow$S*.cs
	$(DEL) objs$S*csharp_wrap.$O
	$(DEL) cs*.exe

# csharplp

csharplp: Google.OrTools.LinearSolver.$(SHAREDLIBEXT)

gen/linear_solver/linear_solver_csharp_wrap.cc: linear_solver/linear_solver.swig base/base.swig util/data.swig linear_solver/linear_solver.h gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport Google.OrTools.LinearSolver.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Slinearsolver linear_solver$Slinear_solver.swig

objs/linear_solver_csharp_wrap.$O: gen/linear_solver/linear_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/linear_solver/linear_solver_csharp_wrap.cc $(OBJOUT)objs/linear_solver_csharp_wrap.$O

Google.OrTools.LinearSolver.$(SHAREDLIBEXT): objs/linear_solver_csharp_wrap.$O $(LP_DEPS)
	$(CSC) /target:module /out:Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.LinearSolver.$(SHAREDLIBEXT) Google.OrTools.LinearSolver.netmodule objs/linear_solver_csharp_wrap.$O $(LP_LNK) $(LDFLAGS)

# csharp linearsolver examples

cslinearprogramming.exe: csharplp csharp/cslinearprogramming.cs
	$(CSC) /target:exe /out:cslinearprogramming.exe /platform:$(NETPLATFORM) /r:Google.OrTools.LinearSolver.$(SHAREDLIBEXT) csharp$Scslinearprogramming.cs

csintegerprogramming.exe: csharplp csharp/csintegerprogramming.cs
	$(CSC) /target:exe /out:csintegerprogramming.exe /platform:$(NETPLATFORM) /r:Google.OrTools.LinearSolver.$(SHAREDLIBEXT) csharp$Scsintegerprogramming.cs


# csharpcp

csharpcp: Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT)

gen/constraint_solver/constraint_solver_csharp_wrap.cc: constraint_solver/routing.swig constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Sconstraintsolver constraint_solver$Srouting.swig
	$(SED) -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/Solver.cs
	$(SED) -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/RoutingModel.cs

objs/constraint_solver_csharp_wrap.$O: gen/constraint_solver/constraint_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/constraint_solver/constraint_solver_csharp_wrap.cc $(OBJOUT)objs/constraint_solver_csharp_wrap.$O

Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT): objs/constraint_solver_csharp_wrap.$O $(ROUTING_DEPS)
	$(CSC) /target:module /out:Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs com$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) Google.OrTools.ConstraintSolver.netmodule objs/constraint_solver_csharp_wrap.$O $(ROUTING_LNK) $(LDFLAGS)

# csharp cp examples

csrabbitspheasants.exe: csharpcp csharp/csrabbitspheasants.cs
	$(CSC) /target:exe /out:csrabbitspheasants.exe /platform:$(NETPLATFORM) /r:Google.OrTools.ConstraintSolver.$(SHAREDLIBEXT) csharp$Scsrabbitspheasants.cs


# csharpalgorithms

csharpalgorithms: Google.OrTools.Algorithms.$(SHAREDLIBEXT)

gen/algorithms/knapsack_solver_csharp_wrap.cc: algorithms/knapsack_solver.swig algorithms/knapsack_solver.swig base/base.swig util/data.swig algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport Google.OrTools.Algorithms.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Sknapsacksolver algorithms$Sknapsack_solver.swig

objs/knapsack_solver_csharp_wrap.$O: gen/algorithms/knapsack_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/algorithms/knapsack_solver_csharp_wrap.cc $(OBJOUT)objs/knapsack_solver_csharp_wrap.$O

Google.OrTools.Algorithms.$(SHAREDLIBEXT): objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_DEPS)
	$(CSC) /target:module /out:Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.Algorithms.$(SHAREDLIBEXT) Google.OrTools.Algorithms.netmodule objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)

# csharp algorithm examples

csknapsack.exe: csharpalgorithms csharp/csknapsack.cs
	$(CSC) /target:exe /out:csknapsack.exe /platform:$(NETPLATFORM) /r:Google.OrTools.Algorithms.$(SHAREDLIBEXT) csharp$Scsknapsack.cs

# csharpgraph

csharpgraph: Google.OrTools.Graph.$(SHAREDLIBEXT)

gen/graph/graph_csharp_wrap.cc: graph/graph.swig base/base.swig util/data.swig graph/max_flow.h graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Sgraph$Sgraph_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport Google.OrTools.Graph.$(SHAREDLIBEXT) -outdir gen$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

objs/graph_csharp_wrap.$O: gen/graph/graph_csharp_wrap.cc
	$(CCC) -DWIN32 -D_USR$(SHAREDLIBEXT) -DD_WIN$(SHAREDLIBEXT) $(CFLAGS) -c gen$Sgraph$Sgraph_csharp_wrap.cc $(OBJOUT)objs$Sgraph_csharp_wrap.$O

Google.OrTools.Graph.$(SHAREDLIBEXT): objs/graph_csharp_wrap.$O $(GRAPH_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /unsafe /out:Google.OrTools.Graph.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	$(LD) $(LDOUT)Google.OrTools.Graph.$(SHAREDLIBEXT) Google.OrTools.Graph.netmodule objs$Sgraph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
else
	$(CSC) /target:library /unsafe /out:Google.OrTools.Graph.dll /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	$(LD) $(LDOUT)Google.OrTools.Graph.$(SHAREDLIBEXT) objs$Sgraph_csharp_wrap.$O $(GRAPH_LNK) $(LDFLAGS)
endif

# csharp graph examples

csflow.exe: csharpgraph csharp/csflow.cs
	$(CSC) /target:exe /out:csflow.exe /platform:$(NETPLATFORM) /r:Google.OrTools.Graph.dll csharp$Scsflow.cs

# Build archive.

dotnet_archive: csharp
	-$(DELREC) temp
	tools\mkdir temp
	tools\mkdir temp\or-tools.$(PLATFORM)
	tools\mkdir temp\or-tools.$(PLATFORM)\csharp
	tools\mkdir temp\or-tools.$(PLATFORM)\csharp\solution
	tools\mkdir temp\or-tools.$(PLATFORM)\csharp\solution\Properties
	copy Google.OrTools.*.$(SHAREDLIBEXT) temp\or-tools.$(PLATFORM)
ifneq ($(SYSTEM),win)
	copy Google.OrTools.*.dll temp\or-tools.$(PLATFORM)
endif
	copy csharp\*.cs temp\or-tools.$(PLATFORM)\csharp
	copy csharp\*.sln temp\or-tools.$(PLATFORM)\csharp
	copy csharp\solution\*.csproj temp\or-tools.$(PLATFORM)\csharp\solution
	copy csharp\solution\Properties\*.cs temp\or-tools.$(PLATFORM)\csharp\solution\Properties
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.NET.$(PLATFORM).$(SVNVERSION).zip or-tools.$(PLATFORM)
	-$(DELREC) temp