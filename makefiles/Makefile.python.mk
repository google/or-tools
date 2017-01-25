.PHONY : python install_python_modules pypi_archive pyinit pycp pyalgorithms pygraph pylp

# Python support using SWIG

# Detect python3

OR_TOOLS_PYTHONPATH = $(OR_ROOT_FULL)$Ssrc$(CPSEP)$(OR_ROOT_FULL)$Sdependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython

ifeq ($(SYSTEM),win)
  PYTHON_EXECUTABLE = $(WINDOWS_PATH_TO_PYTHON)$Spython.exe
  SET_PYTHONPATH = @set PYTHONPATH=$(OR_TOOLS_PYTHONPATH) &&
else #UNIX
  PYTHON_EXECUTABLE = $(shell which python$(UNIX_PYTHON_VER))
  SET_PYTHONPATH = @PYTHONPATH=$(OR_TOOLS_PYTHONPATH)
endif

ifeq ($(shell $(PYTHON_EXECUTABLE) -c "from sys import version_info as v; print (str(v[0]))"),3)
  PYTHON3 = true
  SWIG_PYTHON3_FLAG=-py3 -DPY3
endif

# Main target
CANONIC_PYTHON_EXECUTABLE = $(subst $(SPACE),$(BACKSLASH_SPACE),$(subst \,/,$(subst \\,/,$(PYTHON_EXECUTABLE))))
ifeq ($(wildcard  $(CANONIC_PYTHON_EXECUTABLE)),)
python:
	@echo "The python executable was not set properly. Check Makefile.local for more information."
test_python: python

else
python: install_python_modules pyinit pycp pyalgorithms pygraph pylp
test_python: test_python_examples
BUILT_LANGUAGES +=, python
endif

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

install_python_modules: dependencies/sources/protobuf-3.0.0/python/google/protobuf/descriptor_pb2.py

dependencies/sources/protobuf-3.0.0/python/google/protobuf/descriptor_pb2.py: \
dependencies/sources/protobuf-$(PROTOBUF_TAG)/python/setup.py
ifeq ("$(SYSTEM)", "win")
	copy dependencies$Sinstall$Sbin$Sprotoc.exe dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Ssrc
endif
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython && $(PYTHON_EXECUTABLE) setup.py build

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

rpy: $(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(EX)
	@echo Running $(EX)
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX) $(ARGS)

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
	$(COPY) tools$SMakefile.python temp$Sortools_examples$SMakefile
	$(COPY) LICENSE-2.0.txt temp$Sortools_examples
	$(COPY) tools$Ssetup_data.py temp$Sortools_examples$Ssetup.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools_examples$Ssetup.py
	$(COPY) tools$Scheck_python_deps.py temp$Sortools_examples
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' temp$Sortools_examples$Scheck_python_deps.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' temp$Sortools_examples$Scheck_python_deps.py
	-$(DEL) temp$Sortools_examples$Ssetup.py-e
ifeq ($(SYSTEM),win)
	cd temp\ortools_examples && ..\..\tools\tar.exe -C ..\.. -c -v --exclude *svn* --exclude *roadef* examples\data | ..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\or-tools_python_examples_v$(OR_TOOLS_VERSION).zip ortools_examples
else
	cd temp/ortools_examples && tar -C ../.. -c -v --exclude *svn* --exclude *roadef* examples/data | tar xvm
	cd temp && tar -c -v -z --no-same-owner -f ../or-tools_python_examples_v$(OR_TOOLS_VERSION).tar.gz ortools_examples
endif

PYPI_ARCHIVE_TEMP_DIR = temp-python$(PYTHON_VERSION)

OR_TOOLS_PYTHON_GEN_SCRIPTS = $(wildcard src/gen/ortools/*/*.py) $(wildcard src/gen/ortools/*/*.cc)

pypi_archive: python $(PYPI_ARCHIVE_TEMP_DIR)

$(PYPI_ARCHIVE_TEMP_DIR) : $(OR_TOOLS_PYTHON_GEN_SCRIPTS)
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sdummy
	$(COPY) src$Sgen$Sortools$Sconstraint_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(COPY) src$Sortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) src$Sgen$Sortools$Sgraph$Spywrapgraph.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	$(COPY) src$Sgen$Sortools$Salgorithms$Spywrapknapsack_solver.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(COPY) $(GEN_DIR)$Sortools$S__init__.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms$S__init__.py
	$(COPY) tools$Sdummy_ortools_dependency.cc $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sdummy
	$(COPY) tools$SREADME.pypi $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$SREADME.txt
	$(COPY) LICENSE-2.0.txt $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
	$(COPY) tools$Ssetup.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
ifeq ($(PYTHON3),true)
	$(SED) -i -e 's/ORTOOLS_PYTHON_VERSION/py3-ortools/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
else
	$(SED) -i -e 's/ORTOOLS_PYTHON_VERSION/ortools/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
endif
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	copy src\gen\ortools\constraint_solver\_pywrapcp.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	copy src\gen\ortools\linear_solver\_pywraplp.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	copy src\gen\ortools\graph\_pywrapgraph.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	copy src\gen\ortools\algorithms\_pywrapknapsack_solver.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(SED) -i -e 's/\.dll/\.pyd/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEWIN/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEUNIX/          /g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	-del $(PYPI_ARCHIVE_TEMP_DIR)\ortools\setup.py-e
else
	cp lib/_pywrapcp.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/constraint_solver
	cp lib/_pywraplp.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/linear_solver
	cp lib/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/graph
	cp lib/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/algorithms
	cp lib/libortools.$(LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	$(SED) -i -e 's/\.dll/\.so/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEWIN //g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEUNIX/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DLL/$(LIB_SUFFIX)/g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	-rm $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py-e
ifeq ($(PLATFORM),MACOSX)
	tools/fix_python_libraries_on_mac.sh $(PYPI_ARCHIVE_TEMP_DIR)
endif
ifeq ($(PLATFORM),LINUX)
	tools/fix_python_libraries_on_linux.sh $(PYPI_ARCHIVE_TEMP_DIR)
endif
endif

pypi_upload:
	@echo Uploading Pypi module for $(PYTHON_EXECUTABLE).
ifeq ($(SYSTEM),win)
	cd $(PYPI_ARCHIVE_TEMP_DIR)\ortools && $(PYTHON_EXECUTABLE) setup.py bdist_egg bdist_wheel bdist_wininst
else
  ifeq ($(PLATFORM),MACOSX)
	cd $(PYPI_ARCHIVE_TEMP_DIR)/ortools && $(PYTHON_EXECUTABLE) setup.py bdist_egg bdist_wheel
  else
	cd $(PYPI_ARCHIVE_TEMP_DIR)/ortools && $(PYTHON_EXECUTABLE) setup.py bdist_egg 
  endif
endif
	cd $(PYPI_ARCHIVE_TEMP_DIR)/ortools && twine upload dist/*

pypi_upload: $(PYPI_UPLOAD)

detect_python:
	@echo PYTHON3 = $(PYTHON3)
	@echo SWIG_PYTHON3_FLAG = $(SWIG_PYTHON3_FLAG)