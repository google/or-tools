# Python support using SWIG

# Main target
python: pycp pyalgorithms pygraph pylp

# Clean target
clean_python:
	-$(DEL) $(OR_ROOT)gen$Salgorithms$S*python_wrap*
	-$(DEL) $(OR_ROOT)gen$Sconstraint_solver$S*python_wrap*
	-$(DEL) $(OR_ROOT)gen$Sgraph$S*python_wrap*
	-$(DEL) $(OR_ROOT)gen$Slinear_solver$S*python_wrap*
	-$(DEL) $(OR_ROOT)gen$Salgorithms$S*.py
	-$(DEL) $(OR_ROOT)gen$Sconstraint_solver$S*.py
	-$(DEL) $(OR_ROOT)gen$Sgraph$S*.py
	-$(DEL) $(OR_ROOT)gen$Slinear_solver$S*.py
	-$(DEL) $(OR_ROOT)gen$Salgorithms$S*.pyc
	-$(DEL) $(OR_ROOT)gen$Sconstraint_solver$S*.pyc
	-$(DEL) $(OR_ROOT)gen$Sgraph$S*.pyc
	-$(DEL) $(OR_ROOT)gen$Slinear_solver$S*.pyc
	-$(DEL) $(OR_ROOT)lib$S_pywrap*.$(SHAREDLIBEXT)
	-$(DEL) $(OR_ROOT)objs$S*python_wrap.$O

# pywrapknapsack_solver
pyalgorithms: $(OR_ROOT)lib/_pywrapknapsack_solver.$(SHAREDLIBEXT) $(OR_ROOT)gen/algorithms/pywrapknapsack_solver.py

$(OR_ROOT)gen/algorithms/pywrapknapsack_solver.py: $(OR_ROOT)algorithms/knapsack_solver.swig $(OR_ROOT)algorithms/knapsack_solver.h $(OR_ROOT)base/base.swig
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -python -o $(OR_ROOT)gen$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(OR_ROOT)algorithms$Sknapsack_solver.swig

$(OR_ROOT)gen/algorithms/knapsack_solver_python_wrap.cc: $(OR_ROOT)gen/algorithms/pywrapknapsack_solver.py

$(OR_ROOT)objs/knapsack_solver_python_wrap.$O: $(OR_ROOT)gen/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(OR_ROOT)gen$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJOUT)objs$Sknapsack_solver_python_wrap.$O

$(OR_ROOT)lib/_pywrapknapsack_solver.$(SHAREDLIBEXT): $(OR_ROOT)objs/knapsack_solver_python_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(OR_ROOT)lib$S_pywrapknapsack_solver.$(SHAREDLIBEXT) $(OR_ROOT)objs$Sknapsack_solver_python_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(OR_ROOT)lib\\_pywrapknapsack_solver.dll $(OR_ROOT)gen\\algorithms\\_pywrapknapsack_solver.pyd
endif

# pywrapgraph
pygraph: $(OR_ROOT)lib/_pywrapgraph.$(SHAREDLIBEXT) $(OR_ROOT)gen/graph/pywrapgraph.py

$(OR_ROOT)gen/graph/pywrapgraph.py: $(OR_ROOT)graph/graph.swig $(OR_ROOT)graph/min_cost_flow.h $(OR_ROOT)graph/max_flow.h $(OR_ROOT)graph/ebert_graph.h $(OR_ROOT)base/base.swig
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -python -o $(OR_ROOT)gen$Sgraph$Spywrapgraph_python_wrap.cc -module pywrapgraph $(OR_ROOT)graph$Sgraph.swig

$(OR_ROOT)gen/graph/pywrapgraph_python_wrap.cc: $(OR_ROOT)gen/graph/pywrapgraph.py

$(OR_ROOT)objs/pywrapgraph_python_wrap.$O: $(OR_ROOT)gen/graph/pywrapgraph_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(OR_ROOT)gen/graph/pywrapgraph_python_wrap.cc $(OBJOUT)objs$Spywrapgraph_python_wrap.$O

$(OR_ROOT)lib/_pywrapgraph.$(SHAREDLIBEXT): $(OR_ROOT)objs/pywrapgraph_python_wrap.$O $(GRAPH_DEPS)
	$(LD) $(LDOUT)$(OR_ROOT)lib$S_pywrapgraph.$(SHAREDLIBEXT) $(OR_ROOT)objs$Spywrapgraph_python_wrap.$O $(GRAPH_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(OR_ROOT)lib\\_pywrapgraph.dll $(OR_ROOT)gen\\graph\\_pywrapgraph.pyd
endif

# pywrapcp

pycp: $(OR_ROOT)lib/_pywrapcp.$(SHAREDLIBEXT) $(OR_ROOT)gen/constraint_solver/pywrapcp.py $(OR_ROOT)lib/_pywraprouting.$(SHAREDLIBEXT) $(OR_ROOT)gen/constraint_solver/pywraprouting.py

$(OR_ROOT)gen/constraint_solver/pywrapcp.py: $(OR_ROOT)constraint_solver/constraint_solver.swig $(OR_ROOT)constraint_solver/constraint_solver.h $(OR_ROOT)constraint_solver/constraint_solveri.h $(OR_ROOT)base/base.swig
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -python -o $(OR_ROOT)gen$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(OR_ROOT)constraint_solver$Sconstraint_solver.swig

$(OR_ROOT)gen/constraint_solver/constraint_solver_python_wrap.cc: $(OR_ROOT)gen/constraint_solver/pywrapcp.py

$(OR_ROOT)objs/constraint_solver_python_wrap.$O: $(OR_ROOT)gen/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(OR_ROOT)gen$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJOUT)objs/constraint_solver_python_wrap.$O

$(OR_ROOT)lib/_pywrapcp.$(SHAREDLIBEXT): $(OR_ROOT)objs/constraint_solver_python_wrap.$O $(CP_DEPS)
	$(LD) $(LDOUT)$(OR_ROOT)lib$S_pywrapcp.$(SHAREDLIBEXT) $(OR_ROOT)objs$Sconstraint_solver_python_wrap.$O $(CP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(OR_ROOT)lib\\_pywrapcp.dll $(OR_ROOT)gen\\constraint_solver\\_pywrapcp.pyd
endif

# pywraprouting

$(OR_ROOT)gen/constraint_solver/pywraprouting.py: $(OR_ROOT)constraint_solver/routing.swig $(OR_ROOT)constraint_solver/constraint_solver.h $(OR_ROOT)constraint_solver/constraint_solveri.h $(OR_ROOT)constraint_solver/routing.h $(OR_ROOT)base/base.swig
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -python -o $(OR_ROOT)gen$Sconstraint_solver$Srouting_python_wrap.cc -module pywraprouting $(OR_ROOT)constraint_solver$Srouting.swig

$(OR_ROOT)gen/constraint_solver/routing_python_wrap.cc: $(OR_ROOT)gen/constraint_solver/pywraprouting.py

$(OR_ROOT)objs/routing_python_wrap.$O: $(OR_ROOT)gen/constraint_solver/routing_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(OR_ROOT)gen/constraint_solver/routing_python_wrap.cc $(OBJOUT)objs/routing_python_wrap.$O

$(OR_ROOT)lib/_pywraprouting.$(SHAREDLIBEXT): $(OR_ROOT)objs/routing_python_wrap.$O $(ROUTING_DEPS)
	$(LD) $(LDOUT)$(OR_ROOT)lib$S_pywraprouting.$(SHAREDLIBEXT) $(OR_ROOT)objs$Srouting_python_wrap.$O $(ROUTING_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(OR_ROOT)lib\\_pywraprouting.dll $(OR_ROOT)gen\\constraint_solver\\_pywraprouting.pyd
endif

# pywraplp

pylp: $(OR_ROOT)lib/_pywraplp.$(SHAREDLIBEXT) $(OR_ROOT)gen/linear_solver/pywraplp.py

$(OR_ROOT)gen/linear_solver/pywraplp.py: $(OR_ROOT)linear_solver/linear_solver.swig $(OR_ROOT)linear_solver/linear_solver.h $(OR_ROOT)base/base.swig $(OR_ROOT)gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY)  $(SWIG_INC) -I$(OR_ROOT_INC) -c++ -python -o $(OR_ROOT)gen$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(OR_ROOT)linear_solver$Slinear_solver.swig

$(OR_ROOT)gen/linear_solver/linear_solver_python_wrap.cc: $(OR_ROOT)gen/linear_solver/pywraplp.py

$(OR_ROOT)objs/linear_solver_python_wrap.$O: $(OR_ROOT)gen/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(OR_ROOT)gen$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJOUT)objs/linear_solver_python_wrap.$O

$(OR_ROOT)lib/_pywraplp.$(SHAREDLIBEXT): $(OR_ROOT)objs/linear_solver_python_wrap.$O $(LP_DEPS)
	$(LD) $(LDOUT)$(OR_ROOT)lib$S_pywraplp.$(SHAREDLIBEXT) $(OR_ROOT)objs$Slinear_solver_python_wrap.$O $(LP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(OR_ROOT)lib\\_pywraplp.dll $(OR_ROOT)gen\\linear_solver\\_pywraplp.pyd
endif

# Run a single example

rpy: $(OR_ROOT)lib/_pywraplp.$(SHAREDLIBEXT) $(OR_ROOT)lib/_pywrapcp.$(SHAREDLIBEXT) $(OR_ROOT)lib/_pywrapgraph.$(SHAREDLIBEXT) $(OR_ROOT)lib/_pywrapknapsack_solver.$(SHAREDLIBEXT) $(OR_ROOT)lib/_pywraprouting.$(SHAREDLIBEXT) $(OR_ROOT)python/$(EX).py
ifeq ($(SYSTEM),win)
	@echo Running python$S$(EX).py
	@set PYTHONPATH=$(TOP) && $(WINDOWS_PYTHON_PATH)$Spython $(OR_ROOT)python$S$(EX).py
else
	@echo Running python$S$(EX).py
	@PYTHONPATH=$(TOP) python$(PYTHONVERSION) $(OR_ROOT)python$S$(EX).py
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

