# Python support using SWIG

# pywrapknapsack_solver

pyalgorithms: _pywrapknapsack_solver.$(SHAREDLIBEXT) gen/algorithms/pywrapknapsack_solver.py $(ALGORITHMS_LIBS) $(BASE_LIBS)

gen/algorithms/pywrapknapsack_solver.py: algorithms/knapsack_solver.swig algorithms/knapsack_solver.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/algorithms/knapsack_solver_python_wrap.cc -module pywrapknapsack_solver algorithms/knapsack_solver.swig

gen/algorithms/knapsack_solver_python_wrap.cc: gen/algorithms/pywrapknapsack_solver.py

objs/knapsack_solver_python_wrap.$O: gen/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/algorithms/knapsack_solver_python_wrap.cc $(OBJOUT)objs/knapsack_solver_python_wrap.$O

_pywrapknapsack_solver.$(SHAREDLIBEXT): objs/knapsack_solver_python_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)_pywrapknapsack_solver.$(SHAREDLIBEXT) objs/knapsack_solver_python_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywrapknapsack_solver.dll gen\\algorithms\\_pywrapknapsack_solver.pyd
endif
# pywrapflow

pygraph: _pywrapflow.$(SHAREDLIBEXT) gen/graph/pywrapflow.py $(GRAPH_LIBS) $(BASE_LIBS)

gen/graph/pywrapflow.py: graph/flow.swig graph/min_cost_flow.h graph/max_flow.h graph/ebert_graph.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/graph/pywrapflow_python_wrap.cc -module pywrapflow graph/flow.swig

gen/graph/pywrapflow_python_wrap.cc: gen/graph/pywrapflow.py

objs/pywrapflow_python_wrap.$O: gen/graph/pywrapflow_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/graph/pywrapflow_python_wrap.cc $(OBJOUT)objs/pywrapflow_python_wrap.$O

_pywrapflow.$(SHAREDLIBEXT): objs/pywrapflow_python_wrap.$O $(GRAPH_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)_pywrapflow.$(SHAREDLIBEXT) objs/pywrapflow_python_wrap.$O $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywrapflow.dll gen\\graph\\_pywrapflow.pyd
endif

# pywrapcp

pycp: _pywrapcp.$(SHAREDLIBEXT) gen/constraint_solver/pywrapcp.py _pywraprouting.$(SHAREDLIBEXT) gen/constraint_solver/pywraprouting.py $(CP_LIBS) $(BASE_LIBS)

gen/constraint_solver/pywrapcp.py: constraint_solver/constraint_solver.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/constraint_solver/constraint_solver_python_wrap.cc -module pywrapcp constraint_solver/constraint_solver.swig

gen/constraint_solver/constraint_solver_python_wrap.cc: gen/constraint_solver/pywrapcp.py

objs/constraint_solver_python_wrap.$O: gen/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/constraint_solver/constraint_solver_python_wrap.cc $(OBJOUT)objs/constraint_solver_python_wrap.$O

_pywrapcp.$(SHAREDLIBEXT): objs/constraint_solver_python_wrap.$O $(CP_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)_pywrapcp.$(SHAREDLIBEXT) objs/constraint_solver_python_wrap.$O $(CP_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDFLAGS) $(LDLPDEPS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywrapcp.dll gen\\constraint_solver\\_pywrapcp.pyd
endif


# pywraprouting

gen/constraint_solver/pywraprouting.py: constraint_solver/routing.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h constraint_solver/routing.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/constraint_solver/routing_python_wrap.cc -module pywraprouting constraint_solver/routing.swig

gen/constraint_solver/routing_python_wrap.cc: gen/constraint_solver/pywraprouting.py

objs/routing_python_wrap.$O: gen/constraint_solver/routing_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/constraint_solver/routing_python_wrap.cc $(OBJOUT)objs/routing_python_wrap.$O

_pywraprouting.$(SHAREDLIBEXT): objs/routing_python_wrap.$O $(ROUTING_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)_pywraprouting.$(SHAREDLIBEXT) objs/routing_python_wrap.$O $(ROUTING_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywraprouting.dll gen\\constraint_solver\\_pywraprouting.pyd
endif

# pywraplp

pylp: _pywraplp.$(SHAREDLIBEXT) gen/linear_solver/pywraplp.py $(LP_LIBS) $(BASE_LIBS)

gen/linear_solver/pywraplp.py: linear_solver/linear_solver.swig linear_solver/linear_solver.h base/base.swig gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY)  $(SWIG_INC) -c++ -python -o gen/linear_solver/linear_solver_python_wrap.cc -module pywraplp linear_solver/linear_solver.swig

gen/linear_solver/linear_solver_python_wrap.cc: gen/linear_solver/pywraplp.py

objs/linear_solver_python_wrap.$O: gen/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/linear_solver/linear_solver_python_wrap.cc $(OBJOUT)objs/linear_solver_python_wrap.$O

_pywraplp.$(SHAREDLIBEXT): objs/linear_solver_python_wrap.$O $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)_pywraplp.$(SHAREDLIBEXT) objs/linear_solver_python_wrap.$O $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywraplp.dll gen\\linear_solver\\_pywraplp.pyd
endif

# Build archive.

python_archive: python
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sor-tools.$(PLATFORM)
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Spython
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Sconstraint_solver
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Slinear_solver
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Sgraph
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Salgorithms
	$(COPY) python$S*.py temp$Sor-tools.$(PLATFORM)$Spython
	$(COPY) gen$Sconstraint_solver$S*.py temp$Sor-tools.$(PLATFORM)$Sconstraint_solver
	$(COPY) gen$Slinear_solver$S*.py temp$Sor-tools.$(PLATFORM)$Slinear_solver
	$(COPY) gen$Sgraph$S*.py temp$Sor-tools.$(PLATFORM)$Sgraph
	$(COPY) gen$Salgorithms$S*.py temp$Sor-tools.$(PLATFORM)$Salgorithms
	$(TOUCH) temp$Sor-tools.$(PLATFORM)$Sconstraint_solver$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PLATFORM)$Slinear_solver$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PLATFORM)$Sgraph$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PLATFORM)$Salgorithms$S__init__.py
ifeq ("$(SYSTEM)","win")
	cd temp$Sor-tools.$(PLATFORM) && ..\..\tools\tar.exe -C ..$S.. -c -v --exclude *svn* data | ..\..\tools\tar.exe xvm
	$(COPY) gen$Sconstraint_solver$S_pywrapcp.pyd temp$Sor-tools.$(PLATFORM)$Sconstraint_solver
	$(COPY) gen$Slinear_solver$S_pywraplp.pyd temp$Sor-tools.$(PLATFORM)$Slinear_solver
	$(COPY) gen$Sgraph$S_pywrapflow.pyd temp$Sor-tools.$(PLATFORM)$Sgraph
	$(COPY) gen$Salgorithms$S_pywrapknapsack_solver.pyd temp$Sor-tools.$(PLATFORM)$Salgorithms
	cd temp && ..$Stools$Szip.exe -r ..$SGoogle.OrTools.python.$(PLATFORM).$(SVNVERSION).zip or-tools.$(PLATFORM)
else
	cd temp$Sor-tools.$(PLATFORM) && tar -C ..$S.. -c -v --exclude \*svn\* data | tar xvm -
	$(COPY) _pywrap*.$(SHAREDLIBEXT) temp$Sor-tools.$(PLATFORM)
	cd temp && tar cvzf ..$SGoogle.OrTools.python.$(PLATFORM).$(SVNVERSION).tar.gz or-tools.$(PLATFORM)
endif
	-$(DELREC) temp

