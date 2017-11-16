.PHONY : python install_python_modules pypi_archive pypi_archive_dir pyinit pycp pyalgorithms pygraph pylp pysat pydata

# Python support using SWIG

# Detect python3

OR_TOOLS_PYTHONPATH = $(OR_ROOT_FULL)$(CPSEP)$(OR_ROOT_FULL)$Sdependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython

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
  PYTHON3_CFLAGS=-DPY3
endif

# Main target
CANONIC_PYTHON_EXECUTABLE = $(subst $(SPACE),$(BACKSLASH_SPACE),$(subst \,/,$(subst \\,/,$(PYTHON_EXECUTABLE))))
ifeq ($(wildcard  $(CANONIC_PYTHON_EXECUTABLE)),)
python:
	@echo "The python executable was not set properly. Check Makefile.local for more information."
test_python: python

else
python: \
	install_python_modules \
	pyinit \
	pycp \
	pyalgorithms \
	pygraph \
	pylp \
	pysat \
	pyrcpsp

test_python: test_python_examples
BUILT_LANGUAGES +=, python
endif

# Clean target
clean_python:
	-$(DEL) $(GEN_DIR)$Sortools$S__init__.py
	-$(DEL) $(GEN_DIR)$Sortools$Salgorithms$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sconstraint_solver$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sdata$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sgraph$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Slinear_solver$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Ssat$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sutil$S*python_wrap*
	-$(DEL) $(GEN_DIR)$Sortools$Salgorithms$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Sconstraint_solver$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Sdata$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Sgraph$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Slinear_solver$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Ssat$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Sutil$S*.py
	-$(DEL) $(GEN_DIR)$Sortools$Salgorithms$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Sconstraint_solver$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Sdata$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Sgraph$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Slinear_solver$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Ssat$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Sutil$S*.pyc
	-$(DEL) $(GEN_DIR)$Sortools$Salgorithms$S_pywrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sconstraint_solver$S_pywrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sdata$S_pywrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sgraph$S_pywrap*
	-$(DEL) $(GEN_DIR)$Sortools$Slinear_solver$S_pywrap*
	-$(DEL) $(GEN_DIR)$Sortools$Ssat$S_pywrap*
	-$(DEL) $(GEN_DIR)$Sortools$Sutil$S_pywrap*
	-$(DEL) $(LIB_DIR)$S_pywrap*.$(SWIG_LIB_SUFFIX)
	-$(DEL) $(OBJ_DIR)$Sswig$S*python_wrap.$O
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)

install_python_modules: dependencies/sources/protobuf-$(PROTOBUF-TAG)/python/google/protobuf/descriptor_pb2.py

dependencies/sources/protobuf-$(PROTOBUF-TAG)/python/google/protobuf/descriptor_pb2.py: \
dependencies/sources/protobuf-$(PROTOBUF_TAG)/python/setup.py
ifeq ("$(SYSTEM)", "win")
	copy dependencies$Sinstall$Sbin$Sprotoc.exe dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Ssrc
else
	cp dependencies$Sinstall$Sbin$Sprotoc dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Ssrc
endif
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython && $(PYTHON_EXECUTABLE) setup.py build

pyinit: $(GEN_DIR)$Sortools$S__init__.py

$(GEN_DIR)$Sortools$S__init__.py:
	$(COPY) $(SRC_DIR)$Sortools$S__init__.py $(GEN_DIR)$Sortools$S__init__.py

# pywrapknapsack_solver
pyalgorithms: $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py: \
		$(SRC_DIR)/ortools/base/base.i \
		$(SRC_DIR)/ortools/util/python/vector.i \
		$(SRC_DIR)/ortools/algorithms/python/knapsack_solver.i \
		$(SRC_DIR)/ortools/algorithms/knapsack_solver.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc -module pywrapknapsack_solver $(SRC_DIR)/ortools/algorithms$Spython$Sknapsack_solver.i

$(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc: $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(OBJ_DIR)/swig/knapsack_solver_python_wrap.$O: $(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc $(ALGORITHMS_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) -c $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_python_wrap.$O

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
		$(SRC_DIR)/ortools/base/base.i \
		$(SRC_DIR)/ortools/util/python/vector.i \
		$(SRC_DIR)/ortools/graph/python/graph.i \
		$(SRC_DIR)/ortools/graph/min_cost_flow.h \
		$(SRC_DIR)/ortools/graph/max_flow.h \
		$(SRC_DIR)/ortools/graph/ebert_graph.h \
		$(SRC_DIR)/ortools/graph/shortestpaths.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sgraph$Sgraph_python_wrap.cc -module pywrapgraph $(SRC_DIR)/ortools/graph$Spython$Sgraph.i

$(GEN_DIR)/ortools/graph/graph_python_wrap.cc: $(GEN_DIR)/ortools/graph/pywrapgraph.py

$(OBJ_DIR)/swig/graph_python_wrap.$O: $(GEN_DIR)/ortools/graph/graph_python_wrap.cc $(GRAPH_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC)  $(PYTHON3_CFLAGS) -c $(GEN_DIR)/ortools/graph/graph_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_python_wrap.$O

$(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX): $(OBJ_DIR)/swig/graph_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapgraph.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Sgraph_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapgraph.dll $(GEN_DIR)\\ortools\\graph\\_pywrapgraph.pyd
else
	cp $(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/graph
endif

# pywrapcp

pycp: $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py $(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX)

$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py: $(SRC_DIR)/ortools/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/ortools/constraint_solver/model_pb2.py: $(SRC_DIR)/ortools/constraint_solver/model.proto $(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py: $(SRC_DIR)/ortools/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)$Sortools$Sconstraint_solver$Sassignment.proto

$(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py: $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py: $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto $(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py $(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/pywrapcp.py: \
		$(SRC_DIR)/ortools/base/base.i \
		$(SRC_DIR)/ortools/util/python/vector.i \
		$(SRC_DIR)/ortools/constraint_solver/python/constraint_solver.i \
		$(SRC_DIR)/ortools/constraint_solver/python/routing.i \
		$(SRC_DIR)/ortools/constraint_solver/constraint_solver.h \
		$(SRC_DIR)/ortools/constraint_solver/constraint_solveri.h \
		$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/model_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py \
		$(GEN_DIR)/ortools/constraint_solver/assignment.pb.h \
		$(GEN_DIR)/ortools/constraint_solver/model.pb.h \
		$(GEN_DIR)/ortools/constraint_solver/search_limit.pb.h \
		$(CP_LIB_OBJS)
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc -module pywrapcp $(SRC_DIR)/ortools/constraint_solver$Spython$Srouting.i

# TODO(user): Support pywraprouting as well.

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc: $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py

$(OBJ_DIR)/swig/constraint_solver_python_wrap.$O: $(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) -c $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_python_wrap.$O

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

$(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py: $(SRC_DIR)/ortools/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)/ortools/linear_solver/linear_solver.proto

$(GEN_DIR)/ortools/linear_solver/pywraplp.py: \
		$(SRC_DIR)/ortools/base/base.i \
		$(SRC_DIR)/ortools/util/python/vector.i \
		$(SRC_DIR)/ortools/linear_solver/python/linear_solver.i \
		$(SRC_DIR)/ortools/linear_solver/linear_solver.h \
		$(GEN_DIR)/ortools/linear_solver/linear_solver.pb.h \
		$(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc -module pywraplp $(SRC_DIR)/ortools/linear_solver$Spython$Slinear_solver.i

$(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc: $(GEN_DIR)/ortools/linear_solver/pywraplp.py

$(OBJ_DIR)/swig/linear_solver_python_wrap.$O: $(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) -c $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_python_wrap.$O

$(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX): \
		$(OBJ_DIR)/swig/linear_solver_python_wrap.$O \
			$(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywraplp.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Slinear_solver_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraplp.dll $(GEN_DIR)\\ortools\\linear_solver\\_pywraplp.pyd
else
	cp $(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/linear_solver
endif

# pywrapsat

pysat: $(LIB_DIR)/_pywrapsat.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/sat/pywrapsat.py

$(GEN_DIR)/ortools/sat/cp_model_pb2.py: $(SRC_DIR)/ortools/sat/cp_model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)/ortools/sat/cp_model.proto

$(GEN_DIR)/ortools/sat/sat_parameters_pb2.py: $(SRC_DIR)/ortools/sat/sat_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)/ortools/sat/sat_parameters.proto

$(GEN_DIR)/ortools/sat/pywrapsat.py: \
		$(SRC_DIR)/ortools/base/base.i \
		$(SRC_DIR)/ortools/util/python/vector.i \
		$(SRC_DIR)/ortools/sat/python/sat.i \
		$(GEN_DIR)/ortools/sat/cp_model_pb2.py \
		$(GEN_DIR)/ortools/sat/sat_parameters_pb2.py \
		$(SAT_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Ssat$Ssat_python_wrap.cc -module pywrapsat $(SRC_DIR)/ortools/sat$Spython$Ssat.i

$(GEN_DIR)/ortools/sat/sat_python_wrap.cc: $(GEN_DIR)/ortools/sat/pywrapsat.py

$(OBJ_DIR)/swig/sat_python_wrap.$O: $(GEN_DIR)/ortools/sat/sat_python_wrap.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) -c $(GEN_DIR)$Sortools$Ssat$Ssat_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_python_wrap.$O

$(LIB_DIR)/_pywrapsat.$(SWIG_LIB_SUFFIX): \
		$(OBJ_DIR)/swig/sat_python_wrap.$O \
			$(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywrapsat.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Ssat_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywrapsat.dll $(GEN_DIR)\\ortools\\sat\\_pywrapsat.pyd
else
	cp $(LIB_DIR)/_pywrapsat.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/sat
endif

# pywraprcpsp

pyrcpsp: $(LIB_DIR)/_pywraprcpsp.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/data/pywraprcpsp.py

$(GEN_DIR)/ortools/data/rcpsp_pb2.py: $(SRC_DIR)/ortools/data/rcpsp.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --python_out=$(GEN_DIR) $(SRC_DIR)/ortools/data/rcpsp.proto

$(GEN_DIR)/ortools/data/pywraprcpsp.py: \
		$(SRC_DIR)/ortools/data/rcpsp_parser.h \
		$(SRC_DIR)/ortools/base/base.i \
		$(SRC_DIR)/ortools/data/python/rcpsp.i \
		$(GEN_DIR)/ortools/data/rcpsp_pb2.py \
		$(DATA_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) -o $(GEN_DIR)$Sortools$Sdata$Srcpsp_python_wrap.cc -module pywraprcpsp $(SRC_DIR)/ortools/data$Spython$Srcpsp.i

$(GEN_DIR)/ortools/data/rcpsp_python_wrap.cc: $(GEN_DIR)/ortools/data/pywraprcpsp.py

$(OBJ_DIR)/swig/rcpsp_python_wrap.$O: $(GEN_DIR)/ortools/data/rcpsp_python_wrap.cc $(DATA_DEPS)
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) -c $(GEN_DIR)$Sortools$Sdata$Srcpsp_python_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Srcpsp_python_wrap.$O

$(LIB_DIR)/_pywraprcpsp.$(SWIG_LIB_SUFFIX): \
		$(OBJ_DIR)/swig/rcpsp_python_wrap.$O \
			$(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S_pywraprcpsp.$(SWIG_LIB_SUFFIX) $(OBJ_DIR)$Sswig$Srcpsp_python_wrap.$O $(OR_TOOLS_LNK) $(SYS_LNK) $(PYTHON_LNK)
ifeq "$(SYSTEM)" "win"
	copy $(LIB_DIR)\\_pywraprcpsp.dll $(GEN_DIR)\\ortools\\data\\_pywraprcpsp.pyd
else
	cp $(LIB_DIR)/_pywraprcpsp.$(SWIG_LIB_SUFFIX) $(GEN_DIR)/ortools/data
endif

# Run a single example

rpy: $(LIB_DIR)/_pywraplp.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapcp.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywrapsat.$(SWIG_LIB_SUFFIX) $(LIB_DIR)/_pywraprcpsp.$(SWIG_LIB_SUFFIX)  $(EX)
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
	$(COPY) LICENSE-2.0.txt temp$Sortools_examples
ifeq ($(SYSTEM),win)
	cd temp\ortools_examples && ..\..\tools\tar.exe -C ..\.. -c -v --exclude *svn* --exclude *roadef* examples\data | ..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\or-tools_python_examples_v$(OR_TOOLS_VERSION).zip ortools_examples
else
	cd temp/ortools_examples && tar -C ../.. -c -v --exclude *svn* --exclude *roadef* examples/data | tar xvm
	cd temp && tar -c -v -z --no-same-owner -f ../or-tools_python_examples$(PYPI_OS)_v$(OR_TOOLS_VERSION).tar.gz ortools_examples
endif

PYPI_ARCHIVE_TEMP_DIR = temp-python$(PYTHON_VERSION)

OR_TOOLS_PYTHON_GEN_SCRIPTS = $(wildcard src/gen/ortools/*/*.py) $(wildcard src/gen/ortools/*/*.cc)

# Stages all the files needed to build the python package.
pypi_archive_dir: python $(PYPI_ARCHIVE_TEMP_DIR)

# Patches the archive files to be able to build a pypi package.
# Graft libortools if needed and set RPATHs.
pypi_archive: pypi_archive_dir $(PATCHELF)
ifneq ($(SYSTEM),win)
	cp lib/libortools.$(LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
ifeq ($(PLATFORM),MACOSX)
	tools/fix_python_libraries_on_mac.sh $(PYPI_ARCHIVE_TEMP_DIR)
endif
ifeq ($(PLATFORM),LINUX)
	tools/fix_python_libraries_on_linux.sh $(PYPI_ARCHIVE_TEMP_DIR)
endif
endif

$(PYPI_ARCHIVE_TEMP_DIR) : $(OR_TOOLS_PYTHON_GEN_SCRIPTS)
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$Spython
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sdummy
	$(COPY) ortools$Sgen$Sortools$Sconstraint_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(COPY) ortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) ortools$Sgen$Sortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) ortools$Ssat$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) ortools$Ssat$Spython$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$Spython
	$(COPY) ortools$Sgen$Sortools$Ssat$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) ortools$Sgen$Sortools$Sgraph$Spywrapgraph.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	$(COPY) ortools$Sgen$Sortools$Salgorithms$Spywrapknapsack_solver.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(COPY) ortools$Sgen$Sortools$Sdata$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata
	$(COPY) $(GEN_DIR)$Sortools$S__init__.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
ifeq ($(SYSTEM),win)
	echo __version__ = "$(OR_TOOLS_VERSION)" >> $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
else
	echo "__version__ = \"$(OR_TOOLS_VERSION)\"" >> $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
endif
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms$S__init__.py
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata$S__init__.py
	$(COPY) tools$Sdummy_ortools_dependency.cc $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sdummy
	$(COPY) tools$SREADME.pypi $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$SREADME.txt
	$(COPY) LICENSE-2.0.txt $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
	$(COPY) tools$Ssetup.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
	$(SED) -i -e 's/ORTOOLS_PYTHON_VERSION/ortools$(PYPI_OS)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_VERSION)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	copy ortools\gen\ortools\constraint_solver\_pywrapcp.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	copy ortools\gen\ortools\linear_solver\_pywraplp.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	copy ortools\gen\ortools\sat\_pywrapsat.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	copy ortools\gen\ortools\graph\_pywrapgraph.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	copy ortools\gen\ortools\algorithms\_pywrapknapsack_solver.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	copy ortools\gen\ortools\data\_pywraprcpsp.pyd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata
	$(SED) -i -e 's/\.dll/\.pyd/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEWIN/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEUNIX/          /g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	-del $(PYPI_ARCHIVE_TEMP_DIR)\ortools\setup.py-e
else
	cp lib/_pywrapcp.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/constraint_solver
	cp lib/_pywraplp.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/linear_solver
	cp lib/_pywrapsat.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/sat
	cp lib/_pywrapgraph.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/graph
	cp lib/_pywrapknapsack_solver.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/algorithms
	cp lib/_pywraprcpsp.$(SWIG_LIB_SUFFIX) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/data
	$(SED) -i -e 's/\.dll/\.so/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEWIN //g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEUNIX/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DLL/$(LIB_SUFFIX)/g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
endif

pypi_upload: pypi_archive
	@echo Uploading Pypi module for $(PYTHON_EXECUTABLE).
ifeq ($(SYSTEM),win)
	cd $(PYPI_ARCHIVE_TEMP_DIR)\ortools && $(PYTHON_EXECUTABLE) setup.py bdist_wheel bdist_wininst
else
  ifeq ($(PLATFORM),MACOSX)
	cd $(PYPI_ARCHIVE_TEMP_DIR)/ortools && $(PYTHON_EXECUTABLE) setup.py bdist_wheel
  else
	cd $(PYPI_ARCHIVE_TEMP_DIR)/ortools && $(PYTHON_EXECUTABLE) setup.py bdist_egg
  endif
endif
	cd $(PYPI_ARCHIVE_TEMP_DIR)/ortools && twine upload dist/*

detect_python:
	@echo PYTHON3 = $(PYTHON3)
	@echo SWIG_PYTHON3_FLAG = $(SWIG_PYTHON3_FLAG)
