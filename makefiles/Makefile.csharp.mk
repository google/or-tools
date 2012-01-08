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

csharplp: Google.OrTools.LinearSolver.dll

gen/linear_solver/linear_solver_csharp_wrap.cc: linear_solver/linear_solver.swig base/base.swig util/data.swig linear_solver/linear_solver.h gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport Google.OrTools.LinearSolver.dll -outdir gen$Scom$Sgoogle$Sortools$Slinearsolver linear_solver$Slinear_solver.swig

objs/linear_solver_csharp_wrap.$O: gen/linear_solver/linear_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/linear_solver/linear_solver_csharp_wrap.cc $(OBJOUT)objs/linear_solver_csharp_wrap.$O

Google.OrTools.LinearSolver.dll: objs/linear_solver_csharp_wrap.$O $(LP_LIBS) $(BASE_LIBS)
	$(CSC) /target:module /out:Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.LinearSolver.dll Google.OrTools.LinearSolver.netmodule objs/linear_solver_csharp_wrap.$O $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# csharp linearsolver examples

cslinearprogramming.exe: csharplp csharp\cslinearprogramming.cs
	$(CSC) /target:exe /out:cslinearprogramming.exe /platform:$(NETPLATFORM) /r:Google.OrTools.LinearSolver.dll csharp\cslinearprogramming.cs

csintegerprogramming.exe: csharplp csharp\csintegerprogramming.cs
	$(CSC) /target:exe /out:csintegerprogramming.exe /platform:$(NETPLATFORM) /r:Google.OrTools.LinearSolver.dll csharp\csintegerprogramming.cs


# csharpcp

csharpcp: Google.OrTools.ConstraintSolver.dll

gen/constraint_solver/constraint_solver_csharp_wrap.cc: constraint_solver/routing.swig constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport Google.OrTools.ConstraintSolver.dll -outdir gen$Scom$Sgoogle$Sortools$Sconstraintsolver constraint_solver$Srouting.swig
	tools$Ssed.exe -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/Solver.cs
	tools$Ssed.exe -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/RoutingModel.cs

objs/constraint_solver_csharp_wrap.$O: gen/constraint_solver/constraint_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/constraint_solver/constraint_solver_csharp_wrap.cc $(OBJOUT)objs/constraint_solver_csharp_wrap.$O

Google.OrTools.ConstraintSolver.dll: objs/constraint_solver_csharp_wrap.$O $(ROUTING_LIBS) $(BASE_LIBS) $(LP_LIBS)
	$(CSC) /target:module /out:Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs com$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.ConstraintSolver.dll Google.OrTools.ConstraintSolver.netmodule objs/constraint_solver_csharp_wrap.$O $(ROUTING_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# csharp cp examples

csrabbitspheasants.exe: csharpcp csharp\csrabbitspheasants.cs
	$(CSC) /target:exe /out:csrabbitspheasants.exe /platform:$(NETPLATFORM) /r:Google.OrTools.ConstraintSolver.dll csharp\csrabbitspheasants.cs


# csharpalgorithms

csharpalgorithms: Google.OrTools.Algorithms.dll

gen/algorithms/knapsack_solver_csharp_wrap.cc: algorithms/knapsack_solver.swig algorithms/knapsack_solver.swig base/base.swig util/data.swig algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport Google.OrTools.Algorithms.dll -outdir gen$Scom$Sgoogle$Sortools$Sknapsacksolver algorithms$Sknapsack_solver.swig

objs/knapsack_solver_csharp_wrap.$O: gen/algorithms/knapsack_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/algorithms/knapsack_solver_csharp_wrap.cc $(OBJOUT)objs/knapsack_solver_csharp_wrap.$O

Google.OrTools.Algorithms.dll: objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(CSC) /target:module /out:Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.Algorithms.dll Google.OrTools.Algorithms.netmodule objs/knapsack_solver_csharp_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# csharp algorithm examples

csknapsack.exe: csharpalgorithms csharp\csknapsack.cs
	$(CSC) /target:exe /out:csknapsack.exe /platform:$(NETPLATFORM) /r:Google.OrTools.Algorithms.dll csharp\csknapsack.cs

# csharpgraph

csharpgraph: Google.OrTools.Graph.dll

gen\graph\flow_csharp_wrap.cc: graph\flow.swig base\base.swig util\data.swig graph\max_flow.h graph\min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen$Sgraph$Sflow_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport Google.OrTools.Graph.dll -outdir gen$Scom$Sgoogle$Sortools$Sflow graph$Sflow.swig

objs\flow_csharp_wrap.$O: gen\graph\flow_csharp_wrap.cc
	cl /clr /nologo /EHa /MD /DWIN32 /D_USRDLL /D_WINDLL $(CFLAGS) /c gen$Sgraph$Sflow_csharp_wrap.cc /Foobjs$S

Google.OrTools.Graph.dll: objs\flow_csharp_wrap.$O $(GRAPH_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(CSC) /target:module /unsafe /out:Google.OrTools.Graph.netmodule /warn:0 /nologo /debug gen$Scom$Sgoogle$Sortools$Sflow$S*.cs
	link /DLL /LTCG /OUT:Google.OrTools.Graph.dll Google.OrTools.Graph.netmodule objs$Sflow_csharp_wrap.$O $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS)

# csharp graph examples

csflow.exe: csharpgraph csharp\csflow.cs
	$(CSC) /target:exe /out:csflow.exe /platform:$(NETPLATFORM) /r:Google.OrTools.Graph.dll csharp\csflow.cs


dotnet_archive: csharp
	tools\zip.exe -r Google.OrTools.NET.$(PLATFORM).$(SVNVERSION).zip Google.OrTools.*.dll csharp/*.cs csharp/solution/*sln csharp/solution/*csproj