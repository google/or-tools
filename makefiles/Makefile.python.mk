# Python support using SWIG

# Main target
python: pycp pyalgorithms pygraph pylp

# Clean target
clean_python:
	-$(DEL) $(GEN_DIR)$Salgorithms$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sgraph$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Salgorithms$S*.py
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*.py
	-$(DEL) $(GEN_DIR)$Sgraph$S*.py
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*.py
	-$(DEL) $(GEN_DIR)$Salgorithms$S*.pyc
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*.pyc
	-$(DEL) $(GEN_DIR)$Sgraph$S*.pyc
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*.pyc
	-$(DEL) $(LIB_DIR)$S_pywrap*.$(DYNAMIC_SWIG_LIB_SUFFIX)
	-$(DEL) $(OBJ_DIR)$S*python_wrap.$O

# pywrapknapsack_solver
pyalgorithms: $(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/algorithms/pywrapknapsack_solver.py

$(GEN_DIR)/algorithms/pywrapknapsack_solver.py: $(SRC_DIR)/algorithms/knapsack_solver.swig $(SRC_DIR)/algorithms/knapsack_solver.h $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(SRC_DIR)/algorithms$Sknapsack_solver.swig

$(GEN_DIR)/algorithms/knapsack_solver_python_wrap.cc: $(GEN_DIR)/algorithms/pywrapknapsack_solver.py

$(OBJ_DIR)/knapsack_solver_python_wrap.$O: $(GEN_DIR)/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sknapsack_solver_python_wrap.$O

$(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/knapsack_solver_python_wrap.$O $(STATIC_ALGORITHMS_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sknapsack_solver_python_wrap.$O $(STATIC_ALGORITHMS_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapknapsack_solver.dll $(GEN_DIR)\\algorithms\\_pywrapknapsack_solver.pyd
endif

# pywrapgraph
pygraph: $(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/graph/pywrapgraph.py

$(GEN_DIR)/graph/pywrapgraph.py: $(SRC_DIR)/graph/graph.swig $(SRC_DIR)/graph/min_cost_flow.h $(SRC_DIR)/graph/max_flow.h $(SRC_DIR)/graph/ebert_graph.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Sgraph$Spywrapgraph_python_wrap.cc -module pywrapgraph $(SRC_DIR)/graph$Sgraph.swig

$(GEN_DIR)/graph/pywrapgraph_python_wrap.cc: $(GEN_DIR)/graph/pywrapgraph.py

$(OBJ_DIR)/pywrapgraph_python_wrap.$O: $(GEN_DIR)/graph/pywrapgraph_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)/graph/pywrapgraph_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Spywrapgraph_python_wrap.$O

$(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/pywrapgraph_python_wrap.$O $(STATIC_GRAPH_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Spywrapgraph_python_wrap.$O $(STATIC_GRAPH_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapgraph.dll $(GEN_DIR)\\graph\\_pywrapgraph.pyd
endif

# pywrapcp

pycp: $(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/constraint_solver/pywrapcp.py $(LIB_DIR)/_pywraprouting.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/constraint_solver/pywraprouting.py

$(GEN_DIR)/constraint_solver/pywrapcp.py: $(SRC_DIR)/constraint_solver/constraint_solver.swig $(SRC_DIR)/constraint_solver/constraint_solver.h $(SRC_DIR)/constraint_solver/constraint_solveri.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(SRC_DIR)/constraint_solver$Sconstraint_solver.swig

$(GEN_DIR)/constraint_solver/constraint_solver_python_wrap.cc: $(GEN_DIR)/constraint_solver/pywrapcp.py

$(OBJ_DIR)/constraint_solver_python_wrap.$O: $(GEN_DIR)/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver_python_wrap.$O

$(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/constraint_solver_python_wrap.$O $(STATIC_CP_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sconstraint_solver_python_wrap.$O $(STATIC_CP_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapcp.dll $(GEN_DIR)\\constraint_solver\\_pywrapcp.pyd
endif

# pywraprouting

$(GEN_DIR)/constraint_solver/pywraprouting.py: $(SRC_DIR)/constraint_solver/routing.swig $(SRC_DIR)/constraint_solver/constraint_solver.h $(SRC_DIR)/constraint_solver/constraint_solveri.h $(SRC_DIR)/constraint_solver/routing.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Sconstraint_solver$Srouting_python_wrap.cc -module pywraprouting $(SRC_DIR)/constraint_solver$Srouting.swig

$(GEN_DIR)/constraint_solver/routing_python_wrap.cc: $(GEN_DIR)/constraint_solver/pywraprouting.py

$(OBJ_DIR)/routing_python_wrap.$O: $(GEN_DIR)/constraint_solver/routing_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)/constraint_solver/routing_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Srouting_python_wrap.$O

$(LIB_DIR)/_pywraprouting.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/routing_python_wrap.$O $(STATIC_ROUTING_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywraprouting.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Srouting_python_wrap.$O $(STATIC_ROUTING_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraprouting.dll $(GEN_DIR)\\constraint_solver\\_pywraprouting.pyd
endif

# pywraplp

pylp: $(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/linear_solver/pywraplp.py

$(GEN_DIR)/linear_solver/pywraplp.py: $(SRC_DIR)/linear_solver/linear_solver.swig $(SRC_DIR)/linear_solver/linear_solver.h $(SRC_DIR)/base/base.swig $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY)  $(SWIG_INC) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(SRC_DIR)/linear_solver$Slinear_solver.swig

$(GEN_DIR)/linear_solver/linear_solver_python_wrap.cc: $(GEN_DIR)/linear_solver/pywraplp.py

$(OBJ_DIR)/linear_solver_python_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_python_wrap.$O

$(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/linear_solver_python_wrap.$O $(STATIC_LP_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Slinear_solver_python_wrap.$O $(STATIC_LP_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraplp.dll $(GEN_DIR)\\linear_solver\\_pywraplp.pyd
endif

# Run a single example

rpy: $(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywraprouting.$(DYNAMIC_SWIG_LIB_SUFFIX) $(EX_DIR)/python/$(EX).py
ifeq ($(SYSTEM),win)
	@echo Running python$S$(EX).py
	@set PYTHONPATH=$(OR_ROOT_FULL)\\src && $(WINDOWS_PYTHON_PATH)$Spython $(EX_DIR)/python$S$(EX).py
else
	@echo Running python$S$(EX).py
	@PYTHONPATH=$(OR_ROOT_FULL)/src python$(PYTHON_VERSION) $(EX_DIR)/python$S$(EX).py
endif


# Build stand-alone archive file for redistribution.

python_archive: python
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sor-tools.$(PORT)
	$(MKDIR) temp$Sor-tools.$(PORT)$Sexamples
	$(MKDIR) temp$Sor-tools.$(PORT)$Sdata
	$(MKDIR) temp$Sor-tools.$(PORT)$Sconstraint_solver
	$(MKDIR) temp$Sor-tools.$(PORT)$Slinear_solver
	$(MKDIR) temp$Sor-tools.$(PORT)$Sgraph
	$(MKDIR) temp$Sor-tools.$(PORT)$Salgorithms
	$(COPY) src$Sgen$Sconstraint_solver$Spywrapcp.py temp$Sor-tools.$(PORT)$Sconstraint_solver
	$(COPY) src$Sgen$Sconstraint_solver$Spywraprouting.py temp$Sor-tools.$(PORT)$Sconstraint_solver
	$(COPY) src$Sgen$Slinear_solver$Spywraplp.py temp$Sor-tools.$(PORT)$Slinear_solver
	$(COPY) src$Sgen$Sgraph$Spywrapgraph.py temp$Sor-tools.$(PORT)$Sgraph
	$(COPY) src$Sgen$Salgorithms$Spywrapknapsack_solver.py temp$Sor-tools.$(PORT)$Salgorithms
	$(COPY) examples$Spython$S*.py temp$Sor-tools.$(PORT)$Sexamples
	$(TOUCH) temp$Sor-tools.$(PORT)$Sconstraint_solver$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Slinear_solver$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Sgraph$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Salgorithms$S__init__.py
	$(COPY) tools$SREADME.python temp$Sor-tools.$(PORT)$SREADME
	$(COPY) tools$Ssetup.py temp$Sor-tools.$(PORT)
	$(SED) -i -e 's/VVVV/$(shell svnversion)/' temp$Sor-tools.$(PORT)$Ssetup.py
ifeq ($(SYSTEM),win)
	copy src\gen\constraint_solver\_pywrapcp.pyd temp$Sor-tools.$(PORT)$Sconstraint_solver
	copy src\gen\constraint_solver\_pywraprouting.pyd temp$Sor-tools.$(PORT)$Sconstraint_solver
	copy src\gen\linear_solver\_pywraplp.pyd temp$Sor-tools.$(PORT)$Slinear_solver
	copy src\gen\graph\_pywrapgraph.pyd temp$Sor-tools.$(PORT)$Sgraph
	copy src\gen\algorithms\_pywrapknapsack_solver.pyd temp$Sor-tools.$(PORT)$Salgorithms
	$(SED) -i -e 's/\.dll/\.pyd/' temp/or-tools.$(PORT)/setup.py
	-del temp\or-tools.$(PORT)\setup.py-e
	cd temp\or-tools.$(PORT) && ..\..\tools\tar.exe -C ..\.. -c -v --exclude *svn* --exclude *roadef* data | ..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.python.$(PORT).$(SVNVERSION).zip or-tools.$(PORT)
else
	cp lib$S_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sconstraint_solver
	cp lib$S_pywraprouting.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sconstraint_solver
	cp lib$S_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Slinear_solver
	cp lib$S_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sgraph
	cp lib$S_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Salgorithms
	$(SED) -i -e 's/\.dll/\.so/' temp/or-tools.$(PORT)/setup.py
	-rm temp/or-tools.$(PORT)/setup.py-e
	cd temp/or-tools.$(PORT) && tar -C ../.. -c -v --exclude *svn* --exclude *roadef* data | tar xvm
	cd temp && tar cvzf ../Google.OrTools.python.$(PORT).$(SVNVERSION).tar.gz or-tools.$(PORT)
endif
