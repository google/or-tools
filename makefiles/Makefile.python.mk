# Python support using SWIG

# Detect python3

ifeq ("$(PYTHON_VERSION)","3.3")
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3
endif

# Main target
python: pycp pyalgorithms pygraph pylp

# Clean target
clean_python:
	-$(DELREC) $(GEN_DIR)$Sortools$Salgorithms$S*
	-$(DELREC) $(GEN_DIR)$Sortools$Sconstraint_solver$S*
	-$(DELREC) $(GEN_DIR)$Sortools$Sgraph$S*
	-$(DELREC) $(GEN_DIR)$Sortools$Slinear_solver$S*
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
pyalgorithms: $(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/data.swig \
		$(SRC_DIR)/algorithms/knapsack_solver.swig \
		$(SRC_DIR)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(SRC_DIR)/algorithms$Sknapsack_solver.swig

$(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc: $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(OBJ_DIR)/knapsack_solver_python_wrap.$O: $(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sknapsack_solver_python_wrap.$O

$(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/knapsack_solver_python_wrap.$O $(STATIC_ALGORITHMS_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sknapsack_solver_python_wrap.$O $(STATIC_ALGORITHMS_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapknapsack_solver.dll $(GEN_DIR)\\ortools\\algorithms\\_pywrapknapsack_solver.pyd
else
	cp $(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/algorithms
endif

# pywrapgraph
pygraph: $(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/graph/pywrapgraph.py

$(GEN_DIR)/ortools/graph/pywrapgraph.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/data.swig \
		$(SRC_DIR)/graph/graph.swig \
		$(SRC_DIR)/graph/min_cost_flow.h \
		$(SRC_DIR)/graph/max_flow.h \
		$(SRC_DIR)/graph/ebert_graph.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sgraph$Spywrapgraph_python_wrap.cc -module pywrapgraph $(SRC_DIR)/graph$Sgraph.swig

$(GEN_DIR)/ortools/graph/pywrapgraph_python_wrap.cc: $(GEN_DIR)/ortools/graph/pywrapgraph.py

$(OBJ_DIR)/pywrapgraph_python_wrap.$O: $(GEN_DIR)/ortools/graph/pywrapgraph_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)/ortools/graph/pywrapgraph_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Spywrapgraph_python_wrap.$O

$(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/pywrapgraph_python_wrap.$O $(STATIC_GRAPH_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Spywrapgraph_python_wrap.$O $(STATIC_GRAPH_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapgraph.dll $(GEN_DIR)\\ortools\\graph\\_pywrapgraph.pyd
else
	cp $(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/graph
endif

# pywrapcp

pycp: $(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py

$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py: $(SRC_DIR)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --python_out=$(GEN_DIR)$Sortools $(SRC_DIR)/constraint_solver/search_limit.proto

$(GEN_DIR)/ortools/constraint_solver/model_pb2.py: $(SRC_DIR)/constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --python_out=$(GEN_DIR)$Sortools $(SRC_DIR)/constraint_solver/model.proto

$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py: $(SRC_DIR)/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --python_out=$(GEN_DIR)$Sortools $(SRC_DIR)/constraint_solver/assignment.proto

$(GEN_DIR)/ortools/constraint_solver/pywrapcp.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/data.swig \
		$(SRC_DIR)/constraint_solver/constraint_solver.swig \
		$(SRC_DIR)/constraint_solver/routing.swig \
		$(SRC_DIR)/constraint_solver/constraint_solver.h \
		$(SRC_DIR)/constraint_solver/constraint_solveri.h \
		$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/model_pb2.py \
		$(GEN_DIR)/constraint_solver/assignment.pb.h \
		$(GEN_DIR)/constraint_solver/model.pb.h \
		$(GEN_DIR)/constraint_solver/search_limit.pb.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(SRC_DIR)/constraint_solver$Srouting.swig

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc: $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py

$(OBJ_DIR)/constraint_solver_python_wrap.$O: $(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver_python_wrap.$O

$(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/constraint_solver_python_wrap.$O $(STATIC_ROUTING_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sconstraint_solver_python_wrap.$O $(STATIC_ROUTING_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapcp.dll $(GEN_DIR)\\ortools\\constraint_solver\\_pywrapcp.pyd
else
	cp $(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/constraint_solver
endif

# pywraplp

pylp: $(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/linear_solver/pywraplp.py

$(GEN_DIR)/ortools/linear_solver/pywraplp.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/data.swig \
		$(SRC_DIR)/linear_solver/linear_solver.swig \
		$(SRC_DIR)/linear_solver/linear_solver.h \
		$(GEN_DIR)/linear_solver/linear_solver2.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(SRC_DIR)/linear_solver$Slinear_solver.swig

$(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc: $(GEN_DIR)/ortools/linear_solver/pywraplp.py

$(OBJ_DIR)/linear_solver_python_wrap.$O: $(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_python_wrap.$O

$(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX): $(OBJ_DIR)/linear_solver_python_wrap.$O $(STATIC_LP_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)$Slinear_solver_python_wrap.$O $(STATIC_LP_LNK) $(STATIC_LD_FLAGS) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraplp.dll $(GEN_DIR)\\ortools\\linear_solver\\_pywraplp.pyd
else
	cp $(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/linear_solver
endif

# Run a single example

rpy: $(LIB_DIR)/_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) $(EX_DIR)/python/$(EX).py
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
	$(MKDIR) temp$Sor-tools.$(PORT)$Sortools
	$(MKDIR) temp$Sor-tools.$(PORT)$Sortools$Sconstraint_solver
	$(MKDIR) temp$Sor-tools.$(PORT)$Sortools$Slinear_solver
	$(MKDIR) temp$Sor-tools.$(PORT)$Sortools$Sgraph
	$(MKDIR) temp$Sor-tools.$(PORT)$Sortools$Salgorithms
	$(COPY) src$Sgen$Sortools$Sconstraint_solver$Spywrapcp.py temp$Sor-tools.$(PORT)$Sortools$Sconstraint_solver
	$(COPY) src$Sgen$Sortools$Slinear_solver$Spywraplp.py temp$Sor-tools.$(PORT)$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Sgraph$Spywrapgraph.py temp$Sor-tools.$(PORT)$Sortools$Sgraph
	$(COPY) src$Sgen$Sortools$Salgorithms$Spywrapknapsack_solver.py temp$Sor-tools.$(PORT)$Sortools$Salgorithms
	$(COPY) examples$Spython$S*.py temp$Sor-tools.$(PORT)$Sexamples
	$(TOUCH) temp$Sor-tools.$(PORT)$Sortools$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Sortools$Sconstraint_solver$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Sortools$Slinear_solver$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Sortools$Sgraph$S__init__.py
	$(TOUCH) temp$Sor-tools.$(PORT)$Sortools$Salgorithms$S__init__.py
	$(COPY) tools$SREADME.python temp$Sor-tools.$(PORT)$SREADME
	$(COPY) LICENSE-2.0.txt temp$Sor-tools.$(PORT)
	$(COPY) tools$Ssetup.py temp$Sor-tools.$(PORT)
	$(SED) -i -e 's/VVVV/$(shell svnversion)/' temp$Sor-tools.$(PORT)$Ssetup.py
ifeq ($(SYSTEM),win)
	copy src\gen\ortools\constraint_solver\_pywrapcp.pyd temp$Sor-tools.$(PORT)$Sortools$Sconstraint_solver
	copy src\gen\ortools\linear_solver\_pywraplp.pyd temp$Sor-tools.$(PORT)$Sortools$Slinear_solver
	copy src\gen\ortools\graph\_pywrapgraph.pyd temp$Sor-tools.$(PORT)$Sortools$Sgraph
	copy src\gen\ortools\algorithms\_pywrapknapsack_solver.pyd temp$Sor-tools.$(PORT)$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.pyd/' temp/or-tools.$(PORT)/setup.py
	-del temp\or-tools.$(PORT)\setup.py-e
	cd temp\or-tools.$(PORT) && ..\..\tools\tar.exe -C ..\.. -c -v --exclude *svn* --exclude *roadef* data | ..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.python.$(PORT).$(SVNVERSION).zip or-tools.$(PORT)
else
	cp lib$S_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sortools$Sconstraint_solver
	cp lib$S_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sortools$Slinear_solver
	cp lib$S_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sortools$Sgraph
	cp lib$S_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sor-tools.$(PORT)$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.so/' temp/or-tools.$(PORT)/setup.py
	-rm temp/or-tools.$(PORT)/setup.py-e
	cd temp/or-tools.$(PORT) && tar -C ../.. -c -v --exclude *svn* --exclude *roadef* data | tar xvm
	cd temp && tar cvzf ../Google.OrTools.python.$(PORT).$(SVNVERSION).tar.gz or-tools.$(PORT)
endif

pypi_archive: python
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sortools
	$(MKDIR) temp$Sortools$Sortools
	$(MKDIR) temp$Sortools$Sortools$Sconstraint_solver
	$(MKDIR) temp$Sortools$Sortools$Slinear_solver
	$(MKDIR) temp$Sortools$Sortools$Sgraph
	$(MKDIR) temp$Sortools$Sortools$Salgorithms
	$(MKDIR) temp$Sortools$Sdummy
	$(COPY) src$Sgen$Sortools$Sconstraint_solver$Spywrapcp.py temp$Sortools$Sortools$Sconstraint_solver
	$(COPY) src$Sgen$Sortools$Slinear_solver$Spywraplp.py temp$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Sgraph$Spywrapgraph.py temp$Sortools$Sortools$Sgraph
	$(COPY) src$Sgen$Sortools$Salgorithms$Spywrapknapsack_solver.py temp$Sortools$Sortools$Salgorithms
	$(TOUCH) temp$Sortools$Sortools$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Sconstraint_solver$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Slinear_solver$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Sgraph$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Salgorithms$S__init__.py
	$(COPY) tools\dummy_ortools_dependency.cc temp$Sortools$Sdummy
	$(COPY) tools$SREADME.pypi temp$Sortools$SREADME.txt
	$(COPY) LICENSE-2.0.txt temp$Sortools
	$(COPY) tools$Ssetup.py temp$Sortools
	$(SED) -i -e 's/VVVV/$(shell svnversion)/' temp$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	copy src\gen\ortools\constraint_solver\_pywrapcp.pyd temp$Sortools$Sortools$Sconstraint_solver
	copy src\gen\ortools\linear_solver\_pywraplp.pyd temp$Sortools$Sortools$Slinear_solver
	copy src\gen\ortools\graph\_pywrapgraph.pyd temp$Sortools$Sortools$Sgraph
	copy src\gen\ortools\algorithms\_pywrapknapsack_solver.pyd temp$Sortools$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.pyd/' temp/ortools/setup.py
	-del temp\ortools\setup.py-e
else
	cp lib$S_pywrapcp.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sortools$Sortools$Sconstraint_solver
	cp lib$S_pywraplp.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sortools$Sortools$Slinear_solver
	cp lib$S_pywrapgraph.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sortools$Sortools$Sgraph
	cp lib$S_pywrapknapsack_solver.$(DYNAMIC_SWIG_LIB_SUFFIX) temp$Sortools$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.so/' temp/ortools/setup.py
	-rm temp/ortools/setup.py-e
endif

pypi_upload: pypi_archive
	@echo Uploading Pypi module.
ifeq ($(SYSTEM),win)
	@echo Do not forget to run: set VS90COMNTOOLS="$(VS$(VS_COMTOOLS)COMNTOOLS)
	cd temp\ortools && $(WINDOWS_PYTHON_PATH)\python setup.py bdist_egg upload"
else
	cd temp/ortools && python$(PYTHON_VERSION) setup.py bdist_egg upload
endif
