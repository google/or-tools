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
	-$(DEL) gen$Salgorithms$S*.pyc
	-$(DEL) gen$Sconstraint_solver$S*.pyc
	-$(DEL) gen$Sgraph$S*.pyc
	-$(DEL) gen$Slinear_solver$S*.pyc
	-$(DEL) lib$S_pywrap*.$(SHAREDLIBEXT)
	-$(DEL) objs$S*python_wrap.$O

# pywrapknapsack_solver
pyalgorithms: $(TOP)/lib/_pywrapknapsack_solver.$(SHAREDLIBEXT) $(TOP)/gen/algorithms/pywrapknapsack_solver.py

$(TOP)/gen/algorithms/pywrapknapsack_solver.py: $(TOP)/algorithms/knapsack_solver.swig $(TOP)/algorithms/knapsack_solver.h $(TOP)/base/base.swig
	$(SWIG_BINARY) -I$(TOP) -c++ -python -o $(TOP)$Sgen$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(TOP)$Salgorithms$Sknapsack_solver.swig

$(TOP)/gen/algorithms/knapsack_solver_python_wrap.cc: $(TOP)/gen/algorithms/pywrapknapsack_solver.py

$(TOP)/objs/knapsack_solver_python_wrap.$O: $(TOP)/gen/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(TOP)$Sgen$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJOUT)objs$Sknapsack_solver_python_wrap.$O

$(TOP)/lib/_pywrapknapsack_solver.$(SHAREDLIBEXT): $(TOP)/objs/knapsack_solver_python_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(TOP)$Slib$S_pywrapknapsack_solver.$(SHAREDLIBEXT) $(TOP)$Sobjs$Sknapsack_solver_python_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(TOP)\\lib\\_pywrapknapsack_solver.dll $(TOP)\\gen\\algorithms\\_pywrapknapsack_solver.pyd
endif

# pywrapgraph
pygraph: $(TOP)/lib/_pywrapgraph.$(SHAREDLIBEXT) $(TOP)/gen/graph/pywrapgraph.py

$(TOP)/gen/graph/pywrapgraph.py: $(TOP)/graph/graph.swig $(TOP)/graph/min_cost_flow.h $(TOP)/graph/max_flow.h $(TOP)/graph/ebert_graph.h $(TOP)/base/base.swig
	$(SWIG_BINARY) -I$(TOP) -c++ -python -o $(TOP)$Sgen$Sgraph$Spywrapgraph_python_wrap.cc -module pywrapgraph $(TOP)$Sgraph$Sgraph.swig

$(TOP)/gen/graph/pywrapgraph_python_wrap.cc: $(TOP)/gen/graph/pywrapgraph.py

$(TOP)/objs/pywrapgraph_python_wrap.$O: $(TOP)/gen/graph/pywrapgraph_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(TOP)/gen/graph/pywrapgraph_python_wrap.cc $(OBJOUT)objs$Spywrapgraph_python_wrap.$O

$(TOP)/lib/_pywrapgraph.$(SHAREDLIBEXT): $(TOP)/objs/pywrapgraph_python_wrap.$O $(GRAPH_DEPS)
	$(LD) $(LDOUT)$(TOP)$Slib$S_pywrapgraph.$(SHAREDLIBEXT) $(TOP)$Sobjs$Spywrapgraph_python_wrap.$O $(GRAPH_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(TOP)\\lib\\_pywrapgraph.dll $(TOP)\\gen\\graph\\_pywrapgraph.pyd
endif

# pywrapcp

pycp: $(TOP)/lib/_pywrapcp.$(SHAREDLIBEXT) $(TOP)/gen/constraint_solver/pywrapcp.py $(TOP)/lib/_pywraprouting.$(SHAREDLIBEXT) $(TOP)/gen/constraint_solver/pywraprouting.py

$(TOP)/gen/constraint_solver/pywrapcp.py: $(TOP)/constraint_solver/constraint_solver.swig $(TOP)/constraint_solver/constraint_solver.h $(TOP)/constraint_solver/constraint_solveri.h $(TOP)/base/base.swig
	$(SWIG_BINARY) -I$(TOP) -c++ -python -o $(TOP)$Sgen$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(TOP)$Sconstraint_solver$Sconstraint_solver.swig

$(TOP)/gen/constraint_solver/constraint_solver_python_wrap.cc: $(TOP)/gen/constraint_solver/pywrapcp.py

$(TOP)/objs/constraint_solver_python_wrap.$O: $(TOP)/gen/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(TOP)$Sgen$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJOUT)objs/constraint_solver_python_wrap.$O

$(TOP)/lib/_pywrapcp.$(SHAREDLIBEXT): $(TOP)/objs/constraint_solver_python_wrap.$O $(CP_DEPS)
	$(LD) $(LDOUT)$(TOP)$Slib$S_pywrapcp.$(SHAREDLIBEXT) $(TOP)$Sobjs$Sconstraint_solver_python_wrap.$O $(CP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(TOP)\\lib\\_pywrapcp.dll $(TOP)\\gen\\constraint_solver\\_pywrapcp.pyd
endif

# pywraprouting

$(TOP)/gen/constraint_solver/pywraprouting.py: $(TOP)/constraint_solver/routing.swig $(TOP)/constraint_solver/constraint_solver.h $(TOP)/constraint_solver/constraint_solveri.h $(TOP)/constraint_solver/routing.h $(TOP)/base/base.swig
	$(SWIG_BINARY) -I$(TOP) -c++ -python -o $(TOP)$Sgen$Sconstraint_solver$Srouting_python_wrap.cc -module pywraprouting $(TOP)$Sconstraint_solver$Srouting.swig

$(TOP)/gen/constraint_solver/routing_python_wrap.cc: $(TOP)/gen/constraint_solver/pywraprouting.py

$(TOP)/objs/routing_python_wrap.$O: $(TOP)/gen/constraint_solver/routing_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(TOP)/gen/constraint_solver/routing_python_wrap.cc $(OBJOUT)objs/routing_python_wrap.$O

$(TOP)/lib/_pywraprouting.$(SHAREDLIBEXT): $(TOP)/objs/routing_python_wrap.$O $(ROUTING_DEPS)
	$(LD) $(LDOUT)$(TOP)$Slib$S_pywraprouting.$(SHAREDLIBEXT) $(TOP)$Sobjs$Srouting_python_wrap.$O $(ROUTING_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(TOP)\\lib\\_pywraprouting.dll $(TOP)\\gen\\constraint_solver\\_pywraprouting.pyd
endif

# pywraplp

pylp: $(TOP)/lib/_pywraplp.$(SHAREDLIBEXT) $(TOP)/gen/linear_solver/pywraplp.py

$(TOP)/gen/linear_solver/pywraplp.py: $(TOP)/linear_solver/linear_solver.swig $(TOP)/linear_solver/linear_solver.h $(TOP)/base/base.swig $(TOP)/gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY)  $(SWIG_INC) -I$(TOP) -c++ -python -o $(TOP)$Sgen$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(TOP)$Slinear_solver$Slinear_solver.swig

$(TOP)/gen/linear_solver/linear_solver_python_wrap.cc: $(TOP)/gen/linear_solver/pywraplp.py

$(TOP)/objs/linear_solver_python_wrap.$O: $(TOP)/gen/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(TOP)$Sgen$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJOUT)objs/linear_solver_python_wrap.$O

$(TOP)/lib/_pywraplp.$(SHAREDLIBEXT): $(TOP)/objs/linear_solver_python_wrap.$O $(LP_DEPS)
	$(LD) $(LDOUT)$(TOP)$Slib$S_pywraplp.$(SHAREDLIBEXT) $(TOP)$Sobjs$Slinear_solver_python_wrap.$O $(LP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(TOP)\\lib\\_pywraplp.dll $(TOP)\\gen\\linear_solver\\_pywraplp.pyd
endif

# Run a single example

rpy: $(TOP)/lib/_pywraplp.$(SHAREDLIBEXT) $(TOP)/lib/_pywrapcp.$(SHAREDLIBEXT) $(TOP)/lib/_pywrapgraph.$(SHAREDLIBEXT) $(TOP)/lib/_pywrapknapsack_solver.$(SHAREDLIBEXT) $(TOP)/lib/_pywraprouting.$(SHAREDLIBEXT) $(TOP)/python/$(EX).py
ifeq ($(SYSTEM),win)
	@echo Running python$S$(EX).py
	@set PYTHONPATH=$(TOP) && $(WINDOWS_PYTHON_PATH)$Spython $(TOP)$Spython$S$(EX).py
else
	@echo Running python$S$(EX).py
	@PYTHONPATH=$(TOP) python$(PYTHONVERSION) $(TOP)$Spython$S$(EX).py
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

