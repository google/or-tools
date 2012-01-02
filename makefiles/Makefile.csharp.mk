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
	$(DEL) gen\\linear_solver\\*csharp_wrap*
	$(DEL) gen\\constraint_solver\\*csharp_wrap*
	$(DEL) gen\\algorithms\\*csharp_wrap*
	$(DEL) gen\\graph\\*csharp_wrap*
	$(DEL) gen\\com\\google\\ortools\\linearsolver\\*.cs
	$(DEL) gen\\com\\google\\ortools\\constraintsolver\\*.cs
	$(DEL) gen\\com\\google\\ortools\\knapsacksolver\\*.cs
	$(DEL) gen\\com\\google\\ortools\\flow\\*.cs
	$(DEL) objs\\*csharp_wrap.obj
	$(DEL) cs*.exe

# csharplp

csharplp: Google.OrTools.LinearSolver.dll

gen/linear_solver/linear_solver_csharp_wrap.cc: linear_solver/linear_solver.swig base/base.swig util/data.swig linear_solver/linear_solver.h gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen\\linear_solver\\linear_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.LinearSolver -dllimport Google.OrTools.LinearSolver.dll -outdir gen\\com\\google\\ortools\\linearsolver linear_solver\\linear_solver.swig

objs/linear_solver_csharp_wrap.obj: gen/linear_solver/linear_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/linear_solver/linear_solver_csharp_wrap.cc $(OBJOUT)objs/linear_solver_csharp_wrap.obj

Google.OrTools.LinearSolver.dll: objs/linear_solver_csharp_wrap.obj $(LP_LIBS) $(BASE_LIBS)
	csc /target:module /out:Google.OrTools.LinearSolver.netmodule /warn:0 /nologo /debug gen\\com\\google\\ortools\\linearsolver\\*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.LinearSolver.dll Google.OrTools.LinearSolver.netmodule objs/linear_solver_csharp_wrap.obj $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# csharp linearsolver examples

cslinearprogramming.exe: csharplp csharp\cslinearprogramming.cs
	csc /target:exe /out:cslinearprogramming.exe /platform:$(NETPLATFORM) /r:Google.OrTools.LinearSolver.dll csharp\cslinearprogramming.cs

csintegerprogramming.exe: csharplp csharp\csintegerprogramming.cs
	csc /target:exe /out:csintegerprogramming.exe /platform:$(NETPLATFORM) /r:Google.OrTools.LinearSolver.dll csharp\csintegerprogramming.cs


# csharpcp

csharpcp: Google.OrTools.ConstraintSolver.dll

gen/constraint_solver/constraint_solver_csharp_wrap.cc: constraint_solver/routing.swig constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen\\constraint_solver\\constraint_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.ConstraintSolver -dllimport Google.OrTools.ConstraintSolver.dll -outdir gen\\com\\google\\ortools\\constraintsolver constraint_solver\\routing.swig
	tools\\sed.exe -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/Solver.cs
	tools\\sed.exe -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/RoutingModel.cs

objs/constraint_solver_csharp_wrap.obj: gen/constraint_solver/constraint_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/constraint_solver/constraint_solver_csharp_wrap.cc $(OBJOUT)objs/constraint_solver_csharp_wrap.obj

Google.OrTools.ConstraintSolver.dll: objs/constraint_solver_csharp_wrap.obj $(ROUTING_LIBS) $(BASE_LIBS) $(LP_LIBS)
	csc /target:module /out:Google.OrTools.ConstraintSolver.netmodule /warn:0 /nologo /debug gen\\com\\google\\ortools\\constraintsolver\\*.cs com\\google\\ortools\\constraintsolver\\*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.ConstraintSolver.dll Google.OrTools.ConstraintSolver.netmodule objs/constraint_solver_csharp_wrap.obj $(ROUTING_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

#	$(LD) /nodefaultlib:libcmt.lib /LTCG $(LDOUT)Google.OrTools.ConstraintSolver.dll Google.OrTools.ConstraintSolver.netmodule objs/constraint_solver_csharp_wrap.obj $(ROUTING_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# csharp cp examples

csrabbitspheasants.exe: csharpcp csharp\csrabbitspheasants.cs
	csc /target:exe /out:csrabbitspheasants.exe /platform:$(NETPLATFORM) /r:Google.OrTools.ConstraintSolver.dll csharp\csrabbitspheasants.cs


# csharpalgorithms

csharpalgorithms: Google.OrTools.Algorithms.dll

gen/algorithms/knapsack_solver_csharp_wrap.cc: algorithms/knapsack_solver.swig algorithms/knapsack_solver.swig base/base.swig util/data.swig algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen\\algorithms\\knapsack_solver_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Algorithms -dllimport Google.OrTools.Algorithms.dll -outdir gen\\com\\google\\ortools\\knapsacksolver algorithms\\knapsack_solver.swig

objs/knapsack_solver_csharp_wrap.obj: gen/algorithms/knapsack_solver_csharp_wrap.cc
	cl /clr /EHa /MD $(CFLAGS) -c gen/algorithms/knapsack_solver_csharp_wrap.cc $(OBJOUT)objs/knapsack_solver_csharp_wrap.obj

Google.OrTools.Algorithms.dll: objs/knapsack_solver_csharp_wrap.obj $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	csc /target:module /out:Google.OrTools.Algorithms.netmodule /warn:0 /nologo /debug gen\\com\\google\\ortools\\knapsacksolver\\*.cs
	$(LD) /LTCG $(LDOUT)Google.OrTools.Algorithms.dll Google.OrTools.Algorithms.netmodule objs/knapsack_solver_csharp_wrap.obj $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# csharp algorithm examples

csknapsack.exe: csharpalgorithms csharp\csknapsack.cs
	csc /target:exe /out:csknapsack.exe /platform:$(NETPLATFORM) /r:Google.OrTools.Algorithms.dll csharp\csknapsack.cs

# csharpgraph

csharpgraph: Google.OrTools.Graph.dll

gen\graph\flow_csharp_wrap.cc: graph\flow.swig base\base.swig util\data.swig graph\max_flow.h graph\min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -csharp -o gen\\graph\\flow_csharp_wrap.cc -module operations_research -namespace Google.OrTools.Graph -dllimport Google.OrTools.Graph.dll -outdir gen\\com\\google\\ortools\\flow graph\\flow.swig

objs\flow_csharp_wrap.obj: gen\graph\flow_csharp_wrap.cc
	cl /clr /nologo /EHa /MD /DWIN32 /D_USRDLL /D_WINDLL $(CFLAGS) /c gen\\graph\\flow_csharp_wrap.cc /Foobjs\\

Google.OrTools.Graph.dll: objs\flow_csharp_wrap.obj $(GRAPH_LIBS) $(LP_LIBS) $(BASE_LIBS)
	csc /target:module /unsafe /out:Google.OrTools.Graph.netmodule /warn:0 /nologo /debug gen\\com\\google\\ortools\\flow\\*.cs
	link /DLL /LTCG /OUT:Google.OrTools.Graph.dll Google.OrTools.Graph.netmodule objs\\flow_csharp_wrap.obj $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS)

# csharp graph examples

csflow.exe: csharpgraph csharp\csflow.cs
	csc /target:exe /out:csflow.exe /platform:$(NETPLATFORM) /r:Google.OrTools.Graph.dll csharp\csflow.cs

