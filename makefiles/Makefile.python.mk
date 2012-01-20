# Python support using SWIG

# Main target
python: pycp pyalgorithms pygraph pylp

# Clean target
clean_python:
	-$(DEL) gen$Salgorithms$S*python_wrap*
	-$(DEL) gen$Sconstraint_solver$S*python_wrap*
	-$(DEL) gen$Sgraph$S*python_wrap*
	-$(DEL) gen$Slinear_solver$S*python_wrap*
	-$(DEL) gen$Salgorithms$S*.py
	-$(DEL) gen$Sconstraint_solver$S*.py
	-$(DEL) gen$Sgraph$S*.py
	-$(DEL) gen$Slinear_solver$S*.py
	-$(DEL) _pywrap*.$(SHAREDLIBEXT)
	-$(DEL) objs$S*python_wrap.$O

# pywrapknapsack_solver
pyalgorithms: _pywrapknapsack_solver.$(SHAREDLIBEXT) gen/algorithms/pywrapknapsack_solver.py

gen/algorithms/pywrapknapsack_solver.py: algorithms/knapsack_solver.swig algorithms/knapsack_solver.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/algorithms/knapsack_solver_python_wrap.cc -module pywrapknapsack_solver algorithms/knapsack_solver.swig

gen/algorithms/knapsack_solver_python_wrap.cc: gen/algorithms/pywrapknapsack_solver.py

objs/knapsack_solver_python_wrap.$O: gen/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/algorithms/knapsack_solver_python_wrap.cc $(OBJOUT)objs/knapsack_solver_python_wrap.$O

_pywrapknapsack_solver.$(SHAREDLIBEXT): objs/knapsack_solver_python_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)_pywrapknapsack_solver.$(SHAREDLIBEXT) objs/knapsack_solver_python_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywrapknapsack_solver.dll gen\\algorithms\\_pywrapknapsack_solver.pyd
endif

# pywrapgraph
pygraph: _pywrapgraph.$(SHAREDLIBEXT) gen/graph/pywrapgraph.py

gen/graph/pywrapgraph.py: graph/graph.swig graph/min_cost_flow.h graph/max_flow.h graph/ebert_graph.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/graph/pywrapgraph_python_wrap.cc -module pywrapgraph graph/graph.swig

gen/graph/pywrapgraph_python_wrap.cc: gen/graph/pywrapgraph.py

objs/pywrapgraph_python_wrap.$O: gen/graph/pywrapgraph_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/graph/pywrapgraph_python_wrap.cc $(OBJOUT)objs/pywrapgraph_python_wrap.$O

_pywrapgraph.$(SHAREDLIBEXT): objs/pywrapgraph_python_wrap.$O $(GRAPH_DEPS)
	$(LD) $(LDOUT)_pywrapgraph.$(SHAREDLIBEXT) objs/pywrapgraph_python_wrap.$O $(GRAPH_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywrapgraph.dll gen\\graph\\_pywrapgraph.pyd
endif

# pywrapcp

pycp: _pywrapcp.$(SHAREDLIBEXT) gen/constraint_solver/pywrapcp.py _pywraprouting.$(SHAREDLIBEXT) gen/constraint_solver/pywraprouting.py

gen/constraint_solver/pywrapcp.py: constraint_solver/constraint_solver.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/constraint_solver/constraint_solver_python_wrap.cc -module pywrapcp constraint_solver/constraint_solver.swig

gen/constraint_solver/constraint_solver_python_wrap.cc: gen/constraint_solver/pywrapcp.py

objs/constraint_solver_python_wrap.$O: gen/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/constraint_solver/constraint_solver_python_wrap.cc $(OBJOUT)objs/constraint_solver_python_wrap.$O

_pywrapcp.$(SHAREDLIBEXT): objs/constraint_solver_python_wrap.$O $(CP_DEPS)
	$(LD) $(LDOUT)_pywrapcp.$(SHAREDLIBEXT) objs/constraint_solver_python_wrap.$O $(CP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywrapcp.dll gen\\constraint_solver\\_pywrapcp.pyd
endif


# pywraprouting

gen/constraint_solver/pywraprouting.py: constraint_solver/routing.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h constraint_solver/routing.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/constraint_solver/routing_python_wrap.cc -module pywraprouting constraint_solver/routing.swig

gen/constraint_solver/routing_python_wrap.cc: gen/constraint_solver/pywraprouting.py

objs/routing_python_wrap.$O: gen/constraint_solver/routing_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/constraint_solver/routing_python_wrap.cc $(OBJOUT)objs/routing_python_wrap.$O

_pywraprouting.$(SHAREDLIBEXT): objs/routing_python_wrap.$O $(ROUTING_DEPS)
	$(LD) $(LDOUT)_pywraprouting.$(SHAREDLIBEXT) objs/routing_python_wrap.$O $(ROUTING_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywraprouting.dll gen\\constraint_solver\\_pywraprouting.pyd
endif

# pywraplp

pylp: _pywraplp.$(SHAREDLIBEXT) gen/linear_solver/pywraplp.py

gen/linear_solver/pywraplp.py: linear_solver/linear_solver.swig linear_solver/linear_solver.h base/base.swig gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY)  $(SWIG_INC) -c++ -python -o gen/linear_solver/linear_solver_python_wrap.cc -module pywraplp linear_solver/linear_solver.swig

gen/linear_solver/linear_solver_python_wrap.cc: gen/linear_solver/pywraplp.py

objs/linear_solver_python_wrap.$O: gen/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/linear_solver/linear_solver_python_wrap.cc $(OBJOUT)objs/linear_solver_python_wrap.$O

_pywraplp.$(SHAREDLIBEXT): objs/linear_solver_python_wrap.$O $(LP_DEPS)
	$(LD) $(LDOUT)_pywraplp.$(SHAREDLIBEXT) objs/linear_solver_python_wrap.$O $(LP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy _pywraplp.dll gen\\linear_solver\\_pywraplp.pyd
endif

# Build stand-alone archive file for redistribution.

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
	$(COPY) gen$Sgraph$S_pywrapgraph.pyd temp$Sor-tools.$(PLATFORM)$Sgraph
	$(COPY) gen$Salgorithms$S_pywrapknapsack_solver.pyd temp$Sor-tools.$(PLATFORM)$Salgorithms
	cd temp && ..$Stools$Szip.exe -r ..$SGoogle.OrTools.python.$(PLATFORM).$(SVNVERSION).zip or-tools.$(PLATFORM)
else
	cd temp$Sor-tools.$(PLATFORM) && tar -C ..$S.. -c -v --exclude \*svn\* data | tar xvm -
	$(COPY) _pywrap*.$(SHAREDLIBEXT) temp$Sor-tools.$(PLATFORM)
	cd temp && tar cvzf ..$SGoogle.OrTools.python.$(PLATFORM).$(SVNVERSION).tar.gz or-tools.$(PLATFORM)
endif
	-$(DELREC) temp

