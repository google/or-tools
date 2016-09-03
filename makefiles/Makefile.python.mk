# Python support using SWIG

# Detect python3

ifeq ("$(PYTHON_VERSION)","3.2")
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3 -DPY3
endif
ifeq ("$(PYTHON_VERSION)","3.3")
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3 -DPY3
endif
ifeq ("$(PYTHON_VERSION)","3.4")
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3 -DPY3
endif
ifeq ("$(PYTHON_VERSION)","3.5")
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3 -DPY3
endif
ifeq ("$(PYTHON_VERSION)","35")
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3 -DPY3
endif

# Main target
python: pyinit pycp pyalgorithms pygraph pylp

# Clean target
clean_python:
	-$(DELREC) $(GEN_DIR)$Sortools$Salgorithms$S*
	-$(DELREC) $(GEN_DIR)$Sortools$Sconstraint_solver$S*
	-$(DELREC) $(GEN_DIR)$Sortools$Sgraph$S*
	-$(DELREC) $(GEN_DIR)$Sortools$Slinear_solver$S*
	-$(DEL) $(GEN_DIR)$Sortools$S__init__.py
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
	-$(DEL) $(LIB_DIR)$S_pywrap*.$(SWIG_LIB_SUFFIX)
	-$(DEL) $(OBJ_DIR)$Sswig$S*python_wrap.$O

pyinit: $(GEN_DIR)$Sortools$S__init__.py

$(GEN_DIR)$Sortools$S__init__.py:
	$(COPY) $(SRC_DIR)$Sortools$S__init__.py $(GEN_DIR)$Sortools$S__init__.py

# pywrapknapsack_solver
pyalgorithms: $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/python/vector.swig \
		$(SRC_DIR)/algorithms/python/knapsack_solver.swig \
		$(SRC_DIR)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(SRC_DIR)/algorithms$Spython$Sknapsack_solver.swig

$(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc: $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(OBJ_DIR)/swig/knapsack_solver_python_wrap.$O: $(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc $(ALGORITHMS_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_python_wrap.$O

$(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX): $(OBJ_DIR)/swig/knapsack_solver_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Sknapsack_solver_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapknapsack_solver.dll $(GEN_DIR)\\ortools\\algorithms\\_pywrapknapsack_solver.pyd
else
	cp $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/algorithms
endif

# pywrapgraph
pygraph: $(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/graph/pywrapgraph.py

$(GEN_DIR)/ortools/graph/pywrapgraph.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/python/vector.swig \
		$(SRC_DIR)/graph/python/graph.swig \
		$(SRC_DIR)/graph/min_cost_flow.h \
		$(SRC_DIR)/graph/max_flow.h \
		$(SRC_DIR)/graph/ebert_graph.h \
		$(SRC_DIR)/graph/shortestpaths.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sgraph$Sgraph_python_wrap.cc -module pywrapgraph $(SRC_DIR)/graph$Spython$Sgraph.swig

$(GEN_DIR)/ortools/graph/graph_python_wrap.cc: $(GEN_DIR)/ortools/graph/pywrapgraph.py

$(OBJ_DIR)/swig/graph_python_wrap.$O: $(GEN_DIR)/ortools/graph/graph_python_wrap.cc $(GRAPH_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)/ortools/graph/graph_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_python_wrap.$O

$(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX): $(OBJ_DIR)/swig/graph_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapgraph.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Sgraph_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapgraph.dll $(GEN_DIR)\\ortools\\graph\\_pywrapgraph.pyd
else
	cp $(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/graph
endif

# pywrapcp

pycp: $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py $(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX)

$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py: $(SRC_DIR)/constraint_solver/search_limit.proto
	$(COPY) $(SRC_DIR)$Sconstraint_solver$Ssearch_limit.proto  $(GEN_DIR)$Sortools$Sconstraint_solver
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(GEN_DIR) --python_out=$(GEN_DIR) $(GEN_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/ortools/constraint_solver/model_pb2.py: $(SRC_DIR)/constraint_solver/model.proto $(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py
	$(COPY) $(SRC_DIR)$Sconstraint_solver$Smodel.proto  $(GEN_DIR)$Sortools$Sconstraint_solver
	$(SED) -i -e "s/constraint_solver/ortools\/constraint_solver/g" $(GEN_DIR)$Sortools$Sconstraint_solver$Smodel.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(GEN_DIR) --python_out=$(GEN_DIR) $(GEN_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py: $(SRC_DIR)/constraint_solver/assignment.proto
	$(COPY) $(SRC_DIR)$Sconstraint_solver$Sassignment.proto  $(GEN_DIR)$Sortools$Sconstraint_solver
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(GEN_DIR) --python_out=$(GEN_DIR) $(GEN_DIR)$Sortools$Sconstraint_solver$Sassignment.proto

$(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py: $(SRC_DIR)/constraint_solver/solver_parameters.proto
	$(COPY) $(SRC_DIR)$Sconstraint_solver$Ssolver_parameters.proto  $(GEN_DIR)$Sortools$Sconstraint_solver
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(GEN_DIR) --python_out=$(GEN_DIR) $(GEN_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py: $(SRC_DIR)/constraint_solver/routing_enums.proto
	$(COPY) $(SRC_DIR)$Sconstraint_solver$Srouting_enums.proto  $(GEN_DIR)$Sortools$Sconstraint_solver
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(GEN_DIR) --python_out=$(GEN_DIR) $(GEN_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py: $(SRC_DIR)/constraint_solver/routing_parameters.proto $(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py $(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py
	$(COPY) $(SRC_DIR)$Sconstraint_solver$Srouting_parameters.proto  $(GEN_DIR)$Sortools$Sconstraint_solver
	$(SED) -i -e "s/constraint_solver/ortools\/constraint_solver/g" $(GEN_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(GEN_DIR) --python_out=$(GEN_DIR) $(GEN_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/pywrapcp.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/python/vector.swig \
		$(SRC_DIR)/constraint_solver/python/constraint_solver.swig \
		$(SRC_DIR)/constraint_solver/python/routing.swig \
		$(SRC_DIR)/constraint_solver/constraint_solver.h \
		$(SRC_DIR)/constraint_solver/constraint_solveri.h \
		$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/model_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py \
		$(GEN_DIR)/constraint_solver/assignment.pb.h \
		$(GEN_DIR)/constraint_solver/model.pb.h \
		$(GEN_DIR)/constraint_solver/search_limit.pb.h \
		$(CP_LIB_OBJS)
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(SRC_DIR)/constraint_solver$Spython$Srouting.swig

# TODO(user): Support pywraprouting as well.

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc: $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py

$(OBJ_DIR)/swig/constraint_solver_python_wrap.$O: $(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_python_wrap.$O

$(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX): \
		$(OBJ_DIR)/swig/constraint_solver_python_wrap.$O \
			$(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapcp.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Sconstraint_solver_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapcp.dll $(GEN_DIR)\\ortools\\constraint_solver\\_pywrapcp.pyd
else
	cp $(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/constraint_solver
endif

# pywraplp

pylp: $(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/linear_solver/pywraplp.py

$(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py: $(SRC_DIR)/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --python_out=$(GEN_DIR)$Sortools $(SRC_DIR)/linear_solver/linear_solver.proto

$(GEN_DIR)/ortools/linear_solver/pywraplp.py: \
		$(SRC_DIR)/base/base.swig \
		$(SRC_DIR)/util/python/vector.swig \
		$(SRC_DIR)/linear_solver/python/linear_solver.swig \
		$(SRC_DIR)/linear_solver/linear_solver.h \
		$(GEN_DIR)/linear_solver/linear_solver.pb.h \
		$(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(SRC_DIR)/linear_solver$Spython$Slinear_solver.swig

$(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc: $(GEN_DIR)/ortools/linear_solver/pywraplp.py

$(OBJ_DIR)/swig/linear_solver_python_wrap.$O: $(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_python_wrap.$O

$(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX): \
		$(OBJ_DIR)/swig/linear_solver_python_wrap.$O \
			$(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywraplp.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Slinear_solver_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraplp.dll $(GEN_DIR)\\ortools\\linear_solver\\_pywraplp.pyd
else
	cp $(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/linear_solver
endif

# Run a single example

rpy: $(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(EX_DIR)/python/$(EX).py
ifeq ($(SYSTEM),win)
	@echo Running python$S$(EX).py
	@set PYTHONPATH=$(OR_ROOT_FULL)\\src && $(WINDOWS_PYTHON_PATH)$Spython $(EX_DIR)/python$S$(EX).py $(ARGS)
else
	@echo Running python$S$(EX).py
	@PYTHONPATH=$(OR_ROOT_FULL)/src python$(PYTHON_VERSION) $(EX_DIR)/python$S$(EX).py $(ARGS)
endif


# Build stand-alone archive file for redistribution.

python_examples_archive:
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sortools_examples
	$(MKDIR) temp$Sortools_examples$Sexamples
	$(MKDIR) temp$Sortools_examples$Sexamples$Spython
	$(MKDIR) temp$Sortools_examples$Sexamples$Sdata
	$(COPY) examples$Spython$S*.py temp$Sortools_examples$Sexamples$Spython
	$(COPY) tools$SREADME.examples.python temp$Sortools_examples$SREADME.txt
	$(COPY) LICENSE-2.0.txt temp$Sortools_examples
	$(COPY) tools$Ssetup_data.py temp$Sortools_examples$Ssetup.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools_examples$Ssetup.py
	$(COPY) tools$Scheck_python_deps.py temp$Sortools_examples
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools_examples$Scheck_python_deps.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' temp$Sortools_examples$Scheck_python_deps.py
	-$(DEL) temp$Sortools_examples$Ssetup.py-e
ifeq ($(SYSTEM),win)
	cd temp\ortools_examples && ..\..\tools\tar.exe -C ..\.. -c -v --exclude *svn* --exclude *roadef* examples\data | ..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\or-tools.python.examples_$(OR_TOOLS_VERSION).zip ortools_examples
else
	cd temp/ortools_examples && tar -C ../.. -c -v --exclude *svn* --exclude *roadef* examples/data | tar xvm
	cd temp && tar -c -v -z --no-same-owner -f ../or-tools.python.examples_$(OR_TOOLS_VERSION).tar.gz ortools_examples
endif

pypi2_archive: python $(PATCHELF)
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sortools
	$(MKDIR) temp$Sortools$Sortools
	$(MKDIR) temp$Sortools$Sortools$Sconstraint_solver
	$(MKDIR) temp$Sortools$Sortools$Slinear_solver
	$(MKDIR) temp$Sortools$Sortools$Sgraph
	$(MKDIR) temp$Sortools$Sortools$Salgorithms
	$(MKDIR) temp$Sortools$Sdummy
	$(COPY) src$Sgen$Sortools$Sconstraint_solver$S*.py temp$Sortools$Sortools$Sconstraint_solver
	$(COPY) src$Sortools$Slinear_solver$S*.py temp$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Slinear_solver$S*.py temp$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Sgraph$Spywrapgraph.py temp$Sortools$Sortools$Sgraph
	$(COPY) src$Sgen$Sortools$Salgorithms$Spywrapknapsack_solver.py temp$Sortools$Sortools$Salgorithms
	$(COPY) $(GEN_DIR)$Sortools$S__init__.py temp$Sortools$Sortools$S__init__.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools$Sortools$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Sconstraint_solver$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Slinear_solver$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Sgraph$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Salgorithms$S__init__.py
	$(COPY) tools$Sdummy_ortools_dependency.cc temp$Sortools$Sdummy
	$(COPY) tools$SREADME.pypi temp$Sortools$SREADME.txt
	$(COPY) LICENSE-2.0.txt temp$Sortools
	$(COPY) tools$Ssetup.py temp$Sortools
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools$Ssetup.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' temp$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	copy src\gen\ortools\constraint_solver\_pywrapcp.pyd temp$Sortools$Sortools$Sconstraint_solver
	copy src\gen\ortools\linear_solver\_pywraplp.pyd temp$Sortools$Sortools$Slinear_solver
	copy src\gen\ortools\graph\_pywrapgraph.pyd temp$Sortools$Sortools$Sgraph
	copy src\gen\ortools\algorithms\_pywrapknapsack_solver.pyd temp$Sortools$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.pyd/' temp/ortools/setup.py
	$(SED) -i -e '/DELETEWIN/d' temp/ortools/setup.py
	$(SED) -i -e 's/DELETEUNIX/          /g' temp/ortools/setup.py
	-del temp\ortools\setup.py-e
else
	cp lib/_pywrapcp.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/constraint_solver
	cp lib/_pywraplp.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/linear_solver
	cp lib/_pywrapgraph.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/graph
	cp lib/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/algorithms
	cp lib/libortools.$(LIB_SUFFIX) temp/ortools/ortools
	$(SED) -i -e 's/\.dll/\.so/' temp/ortools/setup.py
	$(SED) -i -e 's/DELETEWIN //g' temp/ortools/setup.py
	$(SED) -i -e '/DELETEUNIX/d' temp/ortools/setup.py
	$(SED) -i -e 's/DLL/$(LIB_SUFFIX)/g' temp/ortools/setup.py
	-rm temp/ortools/setup.py-e
ifeq ($(PLATFORM),MACOSX)
	tools/fix_python_libraries_on_mac.sh
endif
ifeq ($(PLATFORM),LINUX)
	tools/fix_python_libraries_on_linux.sh
endif
endif

pypi_upload: pypi_archive
	@echo Uploading Pypi module for python$(PYTHON_VERSION).
ifeq ($(SYSTEM),win)
	@echo Do not forget to run: set VS90COMNTOOLS="$(VS$(VS_COMTOOLS)COMNTOOLS)
	cd temp\ortools && $(WINDOWS_PYTHON_PATH)\python setup.py register bdist_egg bdist_wheel bdist_wininst upload"
else
  ifeq ($(PLATFORM),MACOSX)
	cd temp/ortools && python$(PYTHON_VERSION) setup.py register bdist_egg bdist_wheel upload
  else
	cd temp/ortools && python$(PYTHON_VERSION) setup.py register bdist_egg upload
  endif
endif

pypi3_archive: python $(PATCHELF)
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sortools
	$(MKDIR) temp$Sortools$Sortools
	$(MKDIR) temp$Sortools$Sortools$Sconstraint_solver
	$(MKDIR) temp$Sortools$Sortools$Slinear_solver
	$(MKDIR) temp$Sortools$Sortools$Sgraph
	$(MKDIR) temp$Sortools$Sortools$Salgorithms
	$(MKDIR) temp$Sortools$Sdummy
	$(COPY) src$Sgen$Sortools$Sconstraint_solver$S*.py temp$Sortools$Sortools$Sconstraint_solver
	$(COPY) src$Sortools$Slinear_solver$S*.py temp$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Slinear_solver$S*.py temp$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Sgraph$Spywrapgraph.py temp$Sortools$Sortools$Sgraph
	$(COPY) src$Sgen$Sortools$Salgorithms$Spywrapknapsack_solver.py temp$Sortools$Sortools$Salgorithms
	$(COPY) $(GEN_DIR)$Sortools$S__init__.py temp$Sortools$Sortools$S__init__.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools$Sortools$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Sconstraint_solver$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Slinear_solver$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Sgraph$S__init__.py
	$(TOUCH) temp$Sortools$Sortools$Salgorithms$S__init__.py
	$(COPY) tools$Sdummy_ortools_dependency.cc temp$Sortools$Sdummy
	$(COPY) tools$SREADME.pypi temp$Sortools$SREADME.txt
	$(COPY) LICENSE-2.0.txt temp$Sortools
	$(COPY) tools$Ssetup_py3.py temp$Sortools$Ssetup.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools$Ssetup.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' temp$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	copy src\gen\ortools\constraint_solver\_pywrapcp.pyd temp$Sortools$Sortools$Sconstraint_solver
	copy src\gen\ortools\linear_solver\_pywraplp.pyd temp$Sortools$Sortools$Slinear_solver
	copy src\gen\ortools\graph\_pywrapgraph.pyd temp$Sortools$Sortools$Sgraph
	copy src\gen\ortools\algorithms\_pywrapknapsack_solver.pyd temp$Sortools$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.pyd/' temp/ortools/setup.py
	$(SED) -i -e '/DELETEWIN/d' temp/ortools/setup.py
	$(SED) -i -e 's/DELETEUNIX/          /g' temp/ortools/setup.py
	-del temp\ortools\setup.py-e
else
	cp lib/_pywrapcp.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/constraint_solver
	cp lib/_pywraplp.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/linear_solver
	cp lib/_pywrapgraph.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/graph
	cp lib/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) temp/ortools/ortools/algorithms
	cp lib/libortools.$(LIB_SUFFIX) temp/ortools/ortools
	$(SED) -i -e 's/\.dll/\.so/' temp/ortools/setup.py
	$(SED) -i -e 's/DELETEWIN //g' temp/ortools/setup.py
	$(SED) -i -e '/DELETEUNIX/d' temp/ortools/setup.py
	$(SED) -i -e 's/DLL/$(LIB_SUFFIX)/g' temp/ortools/setup.py
	-rm temp/ortools/setup.py-e
ifeq ($(PLATFORM),MACOSX)
	tools/fix_python_libraries_on_mac.sh
endif
ifeq ($(PLATFORM),LINUX)
	tools/fix_python_libraries_on_linux.sh
endif
endif

ifeq ($(PYTHON3),true)
PYPI_ARCHIVE=pypi3_archive
else
PYPI_ARCHIVE=pypi2_archive
endif

pypi_archive: $(PYPI_ARCHIVE)
pypi_upload: $(PYPI_UPLOAD)

detect_python:
	@echo PYTHON3 = $(PYTHON3)
	@echo SWIG_PYTHON3_FLAG = $(SWIG_PYTHON3_FLAG)
