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
	-$(DEL) $(LIB_DIR)$S_pywrap*.$(SHAREDLIBEXT)
	-$(DEL) $(OBJ_DIR)$S*python_wrap.$O

# pywrapknapsack_solver
pyalgorithms: $(LIB_DIR)/_pywrapknapsack_solver.$(SHAREDLIBEXT) $(GEN_DIR)/algorithms/pywrapknapsack_solver.py

$(GEN_DIR)/algorithms/pywrapknapsack_solver.py: $(SRC_DIR)/algorithms/knapsack_solver.swig $(SRC_DIR)/algorithms/knapsack_solver.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(SRC_DIR)/algorithms$Sknapsack_solver.swig

$(GEN_DIR)/algorithms/knapsack_solver_python_wrap.cc: $(GEN_DIR)/algorithms/pywrapknapsack_solver.py

$(OBJ_DIR)/knapsack_solver_python_wrap.$O: $(GEN_DIR)/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJ_OUT)knapsack_solver_python_wrap.$O

$(LIB_DIR)/_pywrapknapsack_solver.$(SHAREDLIBEXT): $(OBJ_DIR)/knapsack_solver_python_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(LIB_DIR)$S_pywrapknapsack_solver.$(SHAREDLIBEXT) $(OBJ_DIR)$Sknapsack_solver_python_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapknapsack_solver.dll $(GEN_DIR)\\algorithms\\_pywrapknapsack_solver.pyd
endif

# pywrapgraph
pygraph: $(LIB_DIR)/_pywrapgraph.$(SHAREDLIBEXT) $(GEN_DIR)/graph/pywrapgraph.py

$(GEN_DIR)/graph/pywrapgraph.py: $(SRC_DIR)/graph/graph.swig $(SRC_DIR)/graph/min_cost_flow.h $(SRC_DIR)/graph/max_flow.h $(SRC_DIR)/graph/ebert_graph.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Sgraph$Spywrapgraph_python_wrap.cc -module pywrapgraph $(SRC_DIR)/graph$Sgraph.swig

$(GEN_DIR)/graph/pywrapgraph_python_wrap.cc: $(GEN_DIR)/graph/pywrapgraph.py

$(OBJ_DIR)/pywrapgraph_python_wrap.$O: $(GEN_DIR)/graph/pywrapgraph_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)/graph/pywrapgraph_python_wrap.cc $(OBJ_OUT)pywrapgraph_python_wrap.$O

$(LIB_DIR)/_pywrapgraph.$(SHAREDLIBEXT): $(OBJ_DIR)/pywrapgraph_python_wrap.$O $(GRAPH_DEPS)
	$(LD) $(LDOUT)$(LIB_DIR)$S_pywrapgraph.$(SHAREDLIBEXT) $(OBJ_DIR)$Spywrapgraph_python_wrap.$O $(GRAPH_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapgraph.dll $(GEN_DIR)\\graph\\_pywrapgraph.pyd
endif

# pywrapcp

pycp: $(LIB_DIR)/_pywrapcp.$(SHAREDLIBEXT) $(GEN_DIR)/constraint_solver/pywrapcp.py $(LIB_DIR)/_pywraprouting.$(SHAREDLIBEXT) $(GEN_DIR)/constraint_solver/pywraprouting.py

$(GEN_DIR)/constraint_solver/pywrapcp.py: $(SRC_DIR)/constraint_solver/constraint_solver.swig $(SRC_DIR)/constraint_solver/constraint_solver.h $(SRC_DIR)/constraint_solver/constraint_solveri.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(SRC_DIR)/constraint_solver$Sconstraint_solver.swig

$(GEN_DIR)/constraint_solver/constraint_solver_python_wrap.cc: $(GEN_DIR)/constraint_solver/pywrapcp.py

$(OBJ_DIR)/constraint_solver_python_wrap.$O: $(GEN_DIR)/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJ_OUT)constraint_solver_python_wrap.$O

$(LIB_DIR)/_pywrapcp.$(SHAREDLIBEXT): $(OBJ_DIR)/constraint_solver_python_wrap.$O $(CP_DEPS)
	$(LD) $(LDOUT)$(LIB_DIR)$S_pywrapcp.$(SHAREDLIBEXT) $(OBJ_DIR)$Sconstraint_solver_python_wrap.$O $(CP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapcp.dll $(GEN_DIR)\\constraint_solver\\_pywrapcp.pyd
endif

# pywraprouting

$(GEN_DIR)/constraint_solver/pywraprouting.py: $(SRC_DIR)/constraint_solver/routing.swig $(SRC_DIR)/constraint_solver/constraint_solver.h $(SRC_DIR)/constraint_solver/constraint_solveri.h $(SRC_DIR)/constraint_solver/routing.h $(SRC_DIR)/base/base.swig
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Sconstraint_solver$Srouting_python_wrap.cc -module pywraprouting $(SRC_DIR)/constraint_solver$Srouting.swig

$(GEN_DIR)/constraint_solver/routing_python_wrap.cc: $(GEN_DIR)/constraint_solver/pywraprouting.py

$(OBJ_DIR)/routing_python_wrap.$O: $(GEN_DIR)/constraint_solver/routing_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)/constraint_solver/routing_python_wrap.cc $(OBJ_OUT)routing_python_wrap.$O

$(LIB_DIR)/_pywraprouting.$(SHAREDLIBEXT): $(OBJ_DIR)/routing_python_wrap.$O $(ROUTING_DEPS)
	$(LD) $(LDOUT)$(LIB_DIR)$S_pywraprouting.$(SHAREDLIBEXT) $(OBJ_DIR)$Srouting_python_wrap.$O $(ROUTING_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraprouting.dll $(GEN_DIR)\\constraint_solver\\_pywraprouting.pyd
endif

# pywraplp

pylp: $(LIB_DIR)/_pywraplp.$(SHAREDLIBEXT) $(GEN_DIR)/linear_solver/pywraplp.py

$(GEN_DIR)/linear_solver/pywraplp.py: $(SRC_DIR)/linear_solver/linear_solver.swig $(SRC_DIR)/linear_solver/linear_solver.h $(SRC_DIR)/base/base.swig $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY)  $(SWIG_INC) -I$(INC_DIR) -c++ -python -o $(GEN_DIR)$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(SRC_DIR)/linear_solver$Slinear_solver.swig

$(GEN_DIR)/linear_solver/linear_solver_python_wrap.cc: $(GEN_DIR)/linear_solver/pywraplp.py

$(OBJ_DIR)/linear_solver_python_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJ_OUT)linear_solver_python_wrap.$O

$(LIB_DIR)/_pywraplp.$(SHAREDLIBEXT): $(OBJ_DIR)/linear_solver_python_wrap.$O $(LP_DEPS)
	$(LD) $(LDOUT)$(LIB_DIR)$S_pywraplp.$(SHAREDLIBEXT) $(OBJ_DIR)$Slinear_solver_python_wrap.$O $(LP_LNK) $(LDFLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraplp.dll $(GEN_DIR)\\linear_solver\\_pywraplp.pyd
endif

# Run a single example

rpy: $(LIB_DIR)/_pywraplp.$(SHAREDLIBEXT) $(LIB_DIR)/_pywrapcp.$(SHAREDLIBEXT) $(LIB_DIR)/_pywrapgraph.$(SHAREDLIBEXT) $(LIB_DIR)/_pywrapknapsack_solver.$(SHAREDLIBEXT) $(LIB_DIR)/_pywraprouting.$(SHAREDLIBEXT) $(EX_DIR)/python/$(EX).py
ifeq ($(SYSTEM),win)
	@echo Running python$S$(EX).py
	@set PYTHONPATH=$(OR_ROOT_FULL)\\src && $(WINDOWS_PYTHON_PATH)$Spython $(EX_DIR)/python$S$(EX).py
else
	@echo Running python$S$(EX).py
	@PYTHONPATH=$(OR_ROOT_FULL)/src python$(PYTHONVERSION) $(EX_DIR)/python$S$(EX).py
endif


# Build stand-alone archive file for redistribution.

python_archive: python
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sor-tools.$(PLATFORM)
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Sexamples
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Sexamples$Spython
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sconstraint_solver
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Slinear_solver
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sgraph
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Salgorithms
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sconstraint_solver
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Slinear_solver
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sgraph
	$(MKDIR) temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Salgorithms
	$(COPY) examples$Spython$S*.py temp$Sor-tools.$(PLATFORM)$Sexamples$Spython
	$(COPY) src$Sgen$Sconstraint_solver$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sconstraint_solver
	$(COPY) src$Sgen$Slinear_solver$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Slinear_solver
	$(COPY) src$Sgen$Sgraph$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sgraph
	$(COPY) src$Sgen$Salgorithms$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Salgorithms
	$(COPY) src$Sconstraint_solver$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Sconstraint_solver
	$(COPY) src$Slinear_solver$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Slinear_solver
	$(COPY) src$Sgraph$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Sgraph
	$(COPY) src$Salgorithms$S*.py temp$Sor-tools.$(PLATFORM)$Ssrc$Salgorithms
ifeq ("$(SYSTEM)","win")
	cd temp$Sor-tools.$(PLATFORM) && ..\..\tools\tar.exe -C ..$S.. -c -v --exclude *svn* data | ..\..\tools\tar.exe xvm
	$(COPY) src$Sgen$Sconstraint_solver$S_pywrapcp.pyd temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sconstraint_solver
	$(COPY) src$Sgen$Sconstraint_solver$S_pywraprouting.pyd temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sconstraint_solver
	$(COPY) src$Sgen$Slinear_solver$S_pywraplp.pyd temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Slinear_solver
	$(COPY) src$Sgen$Sgraph$S_pywrapgraph.pyd temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Sgraph
	$(COPY) src$Sgen$Salgorithms$S_pywrapknapsack_solver.pyd temp$Sor-tools.$(PLATFORM)$Ssrc$Sgen$Salgorithms
	cd temp && ..$Stools$Szip.exe -r ..$SGoogle.OrTools.python.$(PLATFORM).$(SVNVERSION).zip or-tools.$(PLATFORM)
	$(WINDOWS_PYTHON_PATH)$Spython dependencies\sources\googlecode-support\scripts\googlecode_upload.py -s "Google OR-Tools, Python archive, Windows $(PLATFORM) platform, svn release $(SVNVERSION)" -p or-tools -l Type-Achive,OpSys-Windows,Featured Google.OrTools.python.$(PLATFORM).$(SVNVERSION).zip -u laurent.perron@gmail.com -w $(PW)
else
	cd temp$Sor-tools.$(PLATFORM) && tar -C ..$S.. -c -v --exclude \*svn\* data | tar xvm -
	$(COPY) _pywrap*.$(SHAREDLIBEXT) temp$Sor-tools.$(PLATFORM)
	cd temp && tar cvzf ..$SGoogle.OrTools.python.$(PLATFORM).$(SVNVERSION).tar.gz or-tools.$(PLATFORM)
endif
	-$(DELREC) temp
