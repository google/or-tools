# ---------- Python support using SWIG ----------
.PHONY: help_python # Generate list of Python targets with descriptions.
help_python:
	@echo Use one of the following Python targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.python.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.python.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t24
	@echo
endif

OR_TOOLS_PYTHONPATH = $(OR_ROOT_FULL)$(CPSEP)$(OR_ROOT_FULL)$Sdependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython

# Check for required build tools
ifeq ($(SYSTEM),win)
PYTHON_COMPILER ?= python.exe
ifneq ($(WINDOWS_PATH_TO_PYTHON),)
PYTHON_EXECUTABLE := $(WINDOWS_PATH_TO_PYTHON)\$(PYTHON_COMPILER)
else
PYTHON_EXECUTABLE := $(shell $(WHICH) $(PYTHON_COMPILER) 2>nul)
endif
TWINE_EXECUTABLE := $(shell $(WHICH) twine.exe 2>nul)
SET_PYTHONPATH = set PYTHONPATH=$(OR_TOOLS_PYTHONPATH) &&
GEN_MYPY := $(shell $(WHICH) protoc-gen-mypy 2> NUL)
else # UNIX
PYTHON_COMPILER ?= python$(UNIX_PYTHON_VER)
PYTHON_EXECUTABLE := $(shell which $(PYTHON_COMPILER))
TWINE_EXECUTABLE := $(shell command -v twine 2> /dev/null)
SET_PYTHONPATH = PYTHONPATH=$(OR_TOOLS_PYTHONPATH)
GEN_MYPY := $(shell command -v protoc-gen-mypy 2> /dev/null)
endif

ifneq ($(GEN_MYPY),)
MYPY_OUT=--mypy_out=$(GEN_PATH)
endif

# Detect python3
PYTHON3 := true
SWIG_PYTHON3_FLAG := -py3 -DPY3
PYTHON3_CFLAGS := -DPY3

# All libraries and dependecies
PYINIT_LIBS = $(LIB_DIR)/_pywrapinit.$(SWIG_PYTHON_LIB_SUFFIX)
PYALGORITHMS_LIBS = $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_PYTHON_LIB_SUFFIX)
PYGRAPH_LIBS = $(LIB_DIR)/_pywrapgraph.$(SWIG_PYTHON_LIB_SUFFIX)
PYCP_LIBS = $(LIB_DIR)/_pywrapcp.$(SWIG_PYTHON_LIB_SUFFIX)
PYLP_LIBS = $(LIB_DIR)/_pywraplp.$(SWIG_PYTHON_LIB_SUFFIX)
PYSAT_LIBS = $(LIB_DIR)/_pywrapsat.$(SWIG_PYTHON_LIB_SUFFIX)
PYPACKING_LIBS = $(GEN_DIR)/ortools/packing/vector_bin_packing_pb2.py
PYSCHEDULING_LIBS = $(LIB_DIR)/_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX)
PYSORTED_INTERVAL_LIST_LIBS = $(LIB_DIR)/_sorted_interval_list.$(SWIG_PYTHON_LIB_SUFFIX)
PYTHON_OR_TOOLS_LIBS = \
 $(GEN_DIR)/ortools/__init__.py \
 $(PYINIT_LIBS) \
 $(PYALGORITHMS_LIBS) \
 $(PYGRAPH_LIBS) \
 $(PYCP_LIBS) \
 $(PYLP_LIBS) \
 $(PYSAT_LIBS) \
 $(PYPACKING_LIBS) \
 $(PYSCHEDULING_LIBS) \
 $(PYSORTED_INTERVAL_LIST_LIBS)

# Main target
.PHONY: python # Build Python OR-Tools.
.PHONY: check_python # Quick check only running Python OR-Tools samples.
.PHONY: test_python # Run all Python OR-Tools test targets.
.PHONY: package_python # Create Python ortools wheel package.
.PHONY: publish_python # Publish Python OR-Tools wheel package to Pypi.org.
ifneq ($(PYTHON_EXECUTABLE),)
python: $(PYTHON_OR_TOOLS_LIBS)
check_python: check_python_pimpl
test_python: test_python_pimpl
publish_python: publish_python_pimpl
BUILT_LANGUAGES +=, Python$(PYTHON_VERSION)
else
python:
	@echo PYTHON_EXECUTABLE = "${PYTHON_EXECUTABLE}"
	$(warning Cannot find '$(PYTHON_COMPILER)' command which is needed for build. Please make sure it is installed and in system path.)
check_python: python
test_python: python
package_python: python
publish_python: python
endif

PROTOBUF_PYTHON_DESC = dependencies/sources/protobuf-$(PROTOBUF_TAG)/python/google/protobuf/descriptor_pb2.py
$(PROTOBUF_PYTHON_DESC): \
dependencies/sources/protobuf-$(PROTOBUF_TAG)/python/setup.py
ifeq ($(SYSTEM),win)
	copy dependencies$Sinstall$Sbin$Sprotoc.exe dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Ssrc
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython && \
 "$(PYTHON_EXECUTABLE)" setup.py build
endif
ifeq ($(PLATFORM),LINUX)
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython && \
 LD_LIBRARY_PATH="$(UNIX_PROTOBUF_DIR)/lib64":"$(UNIX_PROTOBUF_DIR)/lib":$(LD_LIBRARY_PATH) \
 PROTOC=$(PROTOC_BINARY) \
 "$(PYTHON_EXECUTABLE)" setup.py build
endif
ifeq ($(PLATFORM),MACOSX)
	cd dependencies$Ssources$Sprotobuf-$(PROTOBUF_TAG)$Spython && \
 DYLD_LIBRARY_PATH="$(UNIX_PROTOBUF_DIR)/lib":$(DYLD_LIBRARY_PATH) \
 PROTOC=$(PROTOC_BINARY) \
 "$(PYTHON_EXECUTABLE)" setup.py build
endif

$(GEN_DIR)/ortools/__init__.py: | $(GEN_DIR)/ortools
	$(COPY) ortools$S__init__.py $(GEN_PATH)$Sortools$S__init__.py

#######################
##  Python Wrappers  ##
#######################
# init
ifeq ($(PLATFORM),MACOSX)
PYINIT_LDFLAGS = -install_name @rpath/_pywrapinit.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/init/pywrapinit.py: \
 $(SRC_DIR)/ortools/init/init.h \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/init/python/init.i \
 $(INIT_DEPS) \
 | $(GEN_DIR)/ortools/init
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_DOXYGEN) $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Sinit$Sinit_python_wrap.cc \
 -module pywrapinit \
 ortools$Sinit$Spython$Sinit.i

$(GEN_DIR)/ortools/init/init_python_wrap.cc: \
 $(GEN_DIR)/ortools/init/pywrapinit.py

$(OBJ_DIR)/swig/init_python_wrap.$O: \
 $(GEN_DIR)/ortools/init/init_python_wrap.cc \
 $(INIT_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sinit$Sinit_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sinit_python_wrap.$O

$(PYINIT_LIBS): $(OBJ_DIR)/swig/init_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYINIT_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywrapinit.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Sinit_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywrapinit.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\init\\_pywrapinit.pyd
else
	cp $(PYINIT_LIBS) $(GEN_PATH)/ortools/init
endif

# pywrapknapsack_solver
ifeq ($(PLATFORM),MACOSX)
PYALGORITHMS_LDFLAGS = -install_name @rpath/_pywrapknapsack_solver.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/algorithms/python/knapsack_solver.i \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/algorithms
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc \
 -module pywrapknapsack_solver \
 ortools$Salgorithms$Spython$Sknapsack_solver.i

$(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc: \
 $(GEN_DIR)/ortools/algorithms/pywrapknapsack_solver.py

$(OBJ_DIR)/swig/knapsack_solver_python_wrap.$O: \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_python_wrap.cc \
 $(ALGORITHMS_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_python_wrap.$O

$(PYALGORITHMS_LIBS): $(OBJ_DIR)/swig/knapsack_solver_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYALGORITHMS_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywrapknapsack_solver.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywrapknapsack_solver.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\algorithms\\_pywrapknapsack_solver.pyd
else
	cp $(PYALGORITHMS_LIBS) $(GEN_PATH)/ortools/algorithms
endif

# pywrapgraph
ifeq ($(PLATFORM),MACOSX)
PYGRAPH_LDFLAGS = -install_name @rpath/_pywrapgraph.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/graph/pywrapgraph.py: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/graph/python/graph.i \
 $(SRC_DIR)/ortools/graph/min_cost_flow.h \
 $(SRC_DIR)/ortools/graph/max_flow.h \
 $(SRC_DIR)/ortools/graph/ebert_graph.h \
 $(SRC_DIR)/ortools/graph/shortestpaths.h \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/graph
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_python_wrap.cc \
 -module pywrapgraph \
 ortools$Sgraph$Spython$Sgraph.i

$(GEN_DIR)/ortools/graph/graph_python_wrap.cc: \
 $(GEN_DIR)/ortools/graph/pywrapgraph.py

$(OBJ_DIR)/swig/graph_python_wrap.$O: \
 $(GEN_DIR)/ortools/graph/graph_python_wrap.cc \
 $(GRAPH_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)/ortools/graph/graph_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_python_wrap.$O

$(PYGRAPH_LIBS): $(OBJ_DIR)/swig/graph_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYGRAPH_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywrapgraph.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Sgraph_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywrapgraph.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\graph\\_pywrapgraph.pyd
else
	cp $(PYGRAPH_LIBS) $(GEN_PATH)/ortools/graph
endif

# pywrapcp
ifeq ($(PLATFORM),MACOSX)
PYCP_LDFLAGS = -install_name @rpath/_pywrapcp.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/search_limit.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/assignment.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Sassignment.proto

$(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/pywrapcp.py: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/constraint_solver/python/constraint_solver.i \
 $(SRC_DIR)/ortools/constraint_solver/python/routing.i \
 $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h \
 $(SRC_DIR)/ortools/constraint_solver/constraint_solveri.h \
 $(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/search_limit_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/assignment.pb.h \
 $(GEN_DIR)/ortools/constraint_solver/search_limit.pb.h \
 $(CP_LIB_OBJS) \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python -nofastunpack $(SWIG_PYTHON3_FLAG) $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc \
 -module pywrapcp \
 $(SRC_DIR)/ortools/constraint_solver$Spython$Srouting.i

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc: \
 $(GEN_DIR)/ortools/constraint_solver/pywrapcp.py

$(OBJ_DIR)/swig/constraint_solver_python_wrap.$O: \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_python_wrap.cc \
 $(CP_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_python_wrap.$O

$(PYCP_LIBS): $(OBJ_DIR)/swig/constraint_solver_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYCP_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywrapcp.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywrapcp.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\constraint_solver\\_pywrapcp.pyd
else
	cp $(PYCP_LIBS) $(GEN_PATH)/ortools/constraint_solver
endif

# pywraplp
ifeq ($(PLATFORM),MACOSX)
PYLP_LDFLAGS = -install_name @rpath/_pywraplp.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/util/optional_boolean_pb2.py: \
 $(SRC_DIR)/ortools/util/optional_boolean.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/util
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)/ortools/util/optional_boolean.proto

$(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py: \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.proto \
 $(GEN_DIR)/ortools/util/optional_boolean_pb2.py \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/linear_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.proto

$(GEN_DIR)/ortools/linear_solver/pywraplp.py: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/linear_solver/python/linear_solver.i \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.h \
 $(GEN_DIR)/ortools/linear_solver/linear_solver.pb.h \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/linear_solver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc \
 -module pywraplp \
 $(SRC_DIR)/ortools/linear_solver$Spython$Slinear_solver.i

$(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc: \
 $(GEN_DIR)/ortools/linear_solver/pywraplp.py

$(OBJ_DIR)/swig/linear_solver_python_wrap.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_python_wrap.cc \
 $(LP_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_python_wrap.$O

$(PYLP_LIBS): $(OBJ_DIR)/swig/linear_solver_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYLP_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywraplp.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Slinear_solver_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywraplp.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\linear_solver\\_pywraplp.pyd
else
	cp $(PYLP_LIBS) $(GEN_PATH)/ortools/linear_solver
endif

# pywrapsat
ifeq ($(PLATFORM),MACOSX)
PYSAT_LDFLAGS = -install_name @rpath/_pywrapsat.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/sat/cp_model_pb2.py: \
 $(SRC_DIR)/ortools/sat/cp_model.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)/ortools/sat/cp_model.proto

$(GEN_DIR)/ortools/sat/sat_parameters_pb2.py: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto

$(GEN_DIR)/ortools/sat/pywrapsat.py: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/sat/python/sat.i \
 $(GEN_DIR)/ortools/sat/cp_model_pb2.py \
 $(GEN_DIR)/ortools/sat/sat_parameters_pb2.py \
 $(SAT_DEPS) \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/sat
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_python_wrap.cc \
 -module pywrapsat \
 $(SRC_DIR)/ortools/sat$Spython$Ssat.i

$(GEN_DIR)/ortools/sat/sat_python_wrap.cc: \
 $(GEN_DIR)/ortools/sat/pywrapsat.py

$(OBJ_DIR)/swig/sat_python_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_python_wrap.cc \
 $(SAT_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_python_wrap.$O

$(PYSAT_LIBS): $(OBJ_DIR)/swig/sat_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYSAT_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywrapsat.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Ssat_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywrapsat.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\sat\\_pywrapsat.pyd
else
	cp $(PYSAT_LIBS) $(GEN_PATH)/ortools/sat
endif

# packing
$(GEN_DIR)/ortools/packing/vector_bin_packing_pb2.py: \
 $(SRC_DIR)/ortools/packing/vector_bin_packing.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/packing
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)/ortools/packing/vector_bin_packing.proto

# pywraprcpsp
ifeq ($(PLATFORM),MACOSX)
PYRCPSP_LDFLAGS = -install_name @rpath/_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/scheduling/rcpsp_pb2.py: \
 $(SRC_DIR)/ortools/scheduling/rcpsp.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/scheduling
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) $(MYPY_OUT) \
 $(SRC_DIR)/ortools/scheduling/rcpsp.proto

$(GEN_DIR)/ortools/scheduling/pywraprcpsp.py: \
 $(SRC_DIR)/ortools/scheduling/rcpsp_parser.h \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/scheduling/python/rcpsp.i \
 $(GEN_DIR)/ortools/scheduling/rcpsp_pb2.py \
 $(DATA_DEPS) \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/scheduling
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Sscheduling$Srcpsp_python_wrap.cc \
 -module pywraprcpsp \
 $(SRC_DIR)/ortools/scheduling$Spython$Srcpsp.i

$(GEN_DIR)/ortools/scheduling/rcpsp_python_wrap.cc: \
 $(GEN_DIR)/ortools/scheduling/pywraprcpsp.py

$(OBJ_DIR)/swig/rcpsp_python_wrap.$O: \
 $(GEN_DIR)/ortools/scheduling/rcpsp_python_wrap.cc \
 $(DATA_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sscheduling$Srcpsp_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Srcpsp_python_wrap.$O

$(PYSCHEDULING_LIBS): $(OBJ_DIR)/swig/rcpsp_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYRCPSP_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Srcpsp_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\scheduling\\_pywraprcpsp.pyd
else
	cp $(PYSCHEDULING_LIBS) $(GEN_PATH)/ortools/scheduling
endif

# sorted_interval_list
ifeq ($(PLATFORM),MACOSX)
PYSORTED_INTERVAL_LIST_LDFLAGS = -install_name @rpath/_sorted_interval_list.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/util/sorted_interval_list.py: \
 $(SRC_DIR)/ortools/util/sorted_interval_list.h \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/python/vector.i \
 $(SRC_DIR)/ortools/util/python/sorted_interval_list.i \
 $(UTIL_DEPS) \
 | $(GEN_DIR)/ortools/util
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_DOXYGEN) $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Sutil$Ssorted_interval_list_python_wrap.cc \
 -module sorted_interval_list \
 $(SRC_DIR)$Sortools$Sutil$Spython$Ssorted_interval_list.i

$(GEN_DIR)/ortools/util/sorted_interval_list_python_wrap.cc: \
 $(GEN_DIR)/ortools/util/sorted_interval_list.py

$(OBJ_DIR)/swig/sorted_interval_list_python_wrap.$O: \
 $(GEN_DIR)/ortools/util/sorted_interval_list_python_wrap.cc \
 $(UTIL_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sutil$Ssorted_interval_list_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssorted_interval_list_python_wrap.$O

$(PYSORTED_INTERVAL_LIST_LIBS): $(OBJ_DIR)/swig/sorted_interval_list_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYSORTED_INTERVAL_LIST_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_sorted_interval_list.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Ssorted_interval_list_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_sorted_interval_list.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\util\\_sorted_interval_list.pyd
else
	cp $(PYSORTED_INTERVAL_LIST_LIBS) $(GEN_PATH)/ortools/util
endif

#######################
##  Python SOURCE  ##
#######################
ifeq ($(SOURCE_SUFFIX),.py) # Those rules will be used if SOURCE contains a .py file
.PHONY: build # Build a Python program.
build: $(SOURCE) $(PYTHON_OR_TOOLS_LIBS) ;

EXTRA_PYTHON_PATH=:$(shell dirname "$(SOURCE)")

.PHONY: run # Run a Python program.
run: build
	$(SET_PYTHONPATH)$(EXTRA_PYTHON_PATH) "$(PYTHON_EXECUTABLE)" $(SOURCE_PATH) $(ARGS)
endif

###############################
##  Python Examples/Samples  ##
###############################
rpy_%: $(TEST_DIR)/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(TEST_PATH)$S$*.py $(ARGS)

rpy_%: $(PYTHON_EX_DIR)/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(PYTHON_EX_PATH)$S$*.py $(ARGS)

rpy_%: $(CONTRIB_EX_DIR)/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(CONTRIB_EX_PATH)$S$*.py $(ARGS)

rpy_%: ortools/algorithms/samples/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" ortools$Salgorithms$Ssamples$S$*.py $(ARGS)

rpy_%: ortools/constraint_solver/samples/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" ortools$Sconstraint_solver$Ssamples$S$*.py $(ARGS)

rpy_%: ortools/graph/samples/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" ortools$Sgraph$Ssamples$S$*.py $(ARGS)

rpy_%: ortools/linear_solver/samples/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" ortools$Slinear_solver$Ssamples$S$*.py $(ARGS)

rpy_%: ortools/sat/samples/%.py $(PYTHON_OR_TOOLS_LIBS) FORCE
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" ortools$Ssat$Ssamples$S$*.py $(ARGS)

.PHONY: test_python_algorithms_samples # Run all Python Algorithms Samples (located in ortools/algorithms/samples)
test_python_algorithms_samples: \
 rpy_knapsack \
 rpy_simple_knapsack_program

.PHONY: test_python_constraint_solver_samples # Run all Python CP Samples (located in ortools/constraint_solver/samples)
test_python_constraint_solver_samples: \
 rpy_simple_cp_program \
 rpy_simple_routing_program \
 rpy_tsp \
 rpy_tsp_cities \
 rpy_tsp_circuit_board \
 rpy_tsp_distance_matrix \
 rpy_vrp \
 rpy_vrp_breaks \
 rpy_vrp_breaks_from_start \
 rpy_vrp_capacity \
 rpy_vrp_drop_nodes \
 rpy_vrp_global_span \
 rpy_vrp_initial_routes \
 rpy_vrp_nodes_indices \
 rpy_vrp_pickup_delivery \
 rpy_vrp_pickup_delivery_fifo \
 rpy_vrp_pickup_delivery_lifo \
 rpy_vrp_resources \
 rpy_vrp_starts_ends \
 rpy_vrp_time_windows \
 rpy_vrp_tokens \
 rpy_vrp_with_time_limit \
 \
 rpy_vrpgs \
 rpy_cvrp \
 rpy_cvrp_reload \
 rpy_cvrptw \
 rpy_cvrptw_break

.PHONY: test_python_graph_samples # Run all Python Graph Samples (located in ortools/graph/samples)
test_python_graph_samples: \
 rpy_simple_max_flow_program \
 rpy_simple_min_cost_flow_program

.PHONY: test_python_linear_solver_samples # Run all Python LP Samples (located in ortools/linear_solver/samples)
test_python_linear_solver_samples: \
 rpy_assignment_mip \
 rpy_basic_example \
 rpy_bin_packing_mip \
 rpy_integer_programming_example \
 rpy_linear_programming_example \
 rpy_mip_var_array \
 rpy_multiple_knapsack_mip \
 rpy_simple_lp_program \
 rpy_simple_mip_program \
 rpy_stigler_diet

.PHONY: test_python_sat_samples # Run all Python Sat Samples (located in ortools/sat/samples)
test_python_sat_samples: \
 rpy_assignment_sat \
 rpy_assumptions_sample_sat \
 rpy_binpacking_problem_sat \
 rpy_bool_or_sample_sat \
 rpy_channeling_sample_sat \
 rpy_cp_is_fun_sat \
 rpy_earliness_tardiness_cost_sample_sat \
 rpy_interval_sample_sat \
 rpy_literal_sample_sat \
 rpy_minimal_jobshop_sat \
 rpy_no_overlap_sample_sat \
 rpy_nurses_sat \
 rpy_optional_interval_sample_sat \
 rpy_rabbits_and_pheasants_sat \
 rpy_ranking_sample_sat \
 rpy_reified_sample_sat \
 rpy_simple_sat_program \
 rpy_search_for_all_solutions_sample_sat \
 rpy_solve_and_print_intermediate_solutions_sample_sat \
 rpy_solve_with_time_limit_sample_sat \
 rpy_step_function_sample_sat \
 rpy_stop_after_n_solutions_sample_sat

.PHONY: check_python_pimpl
check_python_pimpl: \
 test_python_algorithms_samples \
 test_python_constraint_solver_samples \
 test_python_graph_samples \
 test_python_linear_solver_samples \
 test_python_sat_samples \
# rpy_rabbits_pheasants_cp \
# rpy_cryptarithmetic_cp \
# rpy_cryptarithmetic_sat \
# rpy_nqueens_cp \
# rpy_nqueens_sat \
# rpy_integer_programming \
# rpy_max_flow \
# rpy_min_cost_flow \
# rpy_assignment \
# rpy_nurses_cp \
# rpy_job_shop_cp \

.PHONY: test_python_tests # Run all Python Tests (located in examples/tests)
test_python_tests: \
 rpy_lp_test \
 rpy_cp_model_test \
 rpy_sorted_interval_list_test \
 rpy_test_cp_api \
 rpy_test_routing_api \
 rpy_test_lp_api \
 rpy_pywrapcp_test \
 rpy_pywraplp_test \
 rpy_pywraprouting_test

.PHONY: test_python_contrib # Run all Python Contrib (located in examples/python and examples/contrib)
test_python_contrib: \
 rpy_3_jugs_mip \
 rpy_3_jugs_regular \
 rpy_alldifferent_except_0 \
 rpy_all_interval \
 rpy_alphametic \
 rpy_a_round_of_golf \
 rpy_assignment6_mip \
 rpy_assignment \
 rpy_bacp \
 rpy_blending \
 rpy_broken_weights \
 rpy_bus_schedule \
 rpy_car \
 rpy_check_dependencies \
 rpy_circuit \
 rpy_coins3 \
 rpy_coins_grid_mip \
 rpy_coloring_ip \
 rpy_combinatorial_auction2 \
 rpy_contiguity_regular \
 rpy_costas_array \
 rpy_covering_opl \
 rpy_crew \
 rpy_crossword2 \
 rpy_crypta \
 rpy_crypto \
 rpy_curious_set_of_integers \
 rpy_debruijn_binary \
 rpy_diet1_b \
 rpy_diet1_mip \
 rpy_diet1 \
 rpy_discrete_tomography \
 rpy_divisible_by_9_through_1 \
 rpy_dudeney \
 rpy_einav_puzzle2 \
 rpy_einav_puzzle \
 rpy_eq10 \
 rpy_eq20 \
 rpy_fill_a_pix \
 rpy_furniture_moving \
 rpy_futoshiki \
 rpy_game_theory_taha \
 rpy_grocery \
 rpy_just_forgotten \
 rpy_kakuro \
 rpy_kenken2 \
 rpy_killer_sudoku \
 rpy_knapsack_cp \
 rpy_knapsack_mip \
 rpy_labeled_dice \
 rpy_langford \
 rpy_least_diff \
 rpy_least_square \
 rpy_lectures \
 rpy_magic_sequence_sat \
 rpy_magic_square_and_cards \
 rpy_magic_square_mip \
 rpy_magic_square \
 rpy_map \
 rpy_marathon2 \
 rpy_max_flow_taha \
 rpy_max_flow_winston1 \
 rpy_minesweeper \
 rpy_mr_smith \
 rpy_nonogram_default_search \
 rpy_nonogram_regular \
 rpy_nonogram_table2 \
 rpy_nonogram_table \
 rpy_nqueens2 \
 rpy_nqueens3 \
 rpy_nqueens \
 rpy_nurse_rostering \
 rpy_nurses_cp \
 rpy_olympic \
 rpy_organize_day \
 rpy_pandigital_numbers \
 rpy_photo_problem \
 rpy_place_number_puzzle \
 rpy_p_median \
 rpy_post_office_problem2 \
 rpy_production \
 rpy_pyls_api \
 rpy_quasigroup_completion \
 rpy_regular \
 rpy_regular_table2 \
 rpy_regular_table \
 rpy_rogo2 \
 rpy_rostering_with_travel \
 rpy_safe_cracking \
 rpy_scheduling_speakers \
 rpy_secret_santa2 \
 rpy_send_more_money_any_base \
 rpy_send_most_money \
 rpy_seseman_b \
 rpy_seseman \
 rpy_set_covering2 \
 rpy_set_covering3 \
 rpy_set_covering4 \
 rpy_set_covering_deployment \
 rpy_set_covering \
 rpy_set_covering_skiena \
 rpy_set_partition \
 rpy_sicherman_dice \
 rpy_ski_assignment \
 rpy_slitherlink \
 rpy_stable_marriage \
 rpy_steel_lns \
 rpy_steel \
 rpy_stigler_contrib \
 rpy_strimko2 \
 rpy_subset_sum \
 rpy_survo_puzzle \
 rpy_toNum \
 rpy_traffic_lights \
 rpy_volsay2 \
 rpy_volsay3 \
 rpy_volsay \
 rpy_wedding_optimal_chart \
 rpy_who_killed_agatha \
 rpy_xkcd \
 rpy_young_tableaux
	$(MAKE) run SOURCE=examples/contrib/coins_grid.py ARGS="5 2"
	$(MAKE) run SOURCE=examples/contrib/hidato.py ARGS="3 3"
#	$(MAKE) rpy_cvrptw_plot # error: py3 failure, missing numpy.
#	$(MAKE) rpy_nontransitive_dice # error: too long
# warning: nurse_sat take 18s
#	$(MAKE) rpy_school_scheduling_sat # error: too long
#	$(MAKE) rpy_secret_santa # error: too long
#	$(MAKE) rpy_word_square # Not working on window since it rely on /usr/share/dict/words

.PHONY: test_python_python # Build and Run all Python Examples (located in ortools/examples/python)
test_python_python: \
 rpy_appointments \
 rpy_assignment2_sat \
 rpy_assignment_with_constraints_sat \
 rpy_balance_group_sat \
 rpy_chemical_balance_lp \
 rpy_chemical_balance_sat \
 rpy_flexible_job_shop_sat \
 rpy_gate_scheduling_sat \
 rpy_golomb8 \
 rpy_hidato_sat \
 rpy_integer_programming \
 rpy_jobshop_ft06_distance_sat \
 rpy_jobshop_ft06_sat \
 rpy_jobshop_with_maintenance_sat \
 rpy_linear_assignment_api \
 rpy_linear_programming \
 rpy_magic_sequence_distribute \
 rpy_nqueens_sat \
 rpy_pyflow_example \
 rpy_reallocate_sat \
 rpy_rcpsp_sat \
 rpy_shift_scheduling_sat \
 rpy_single_machine_scheduling_with_setup_release_due_dates_sat \
 rpy_steel_mill_slab_sat \
 rpy_sudoku_sat \
 rpy_tasks_and_workers_assignment_sat \
 rpy_transit_time \
 rpy_vendor_scheduling_sat \
 rpy_wedding_optimal_chart_sat \
 rpy_worker_schedule_sat \
 rpy_zebra_sat

.PHONY: test_python_pimpl
test_python_pimpl: \
 check_python_pimpl \
 test_python_tests \
 test_python_contrib \
 test_python_python

#####################
##  Pypi artifact  ##
#####################
PYPI_ARCHIVE_TEMP_DIR = temp_python$(PYTHON_VERSION)

# PEP 513 auditwheel repair overwrite rpath to $ORIGIN/<ortools_root>/.libs
# We need to copy all dynamic libs here
ifneq ($(SYSTEM),win)
PYPI_ARCHIVE_LIBS = $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/.libs
endif

MISSING_PYPI_FILES = \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/README.txt \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/LICENSE-2.0.txt \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/__init__.py \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/init \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/algorithms \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/graph \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/constraint_solver \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/linear_solver \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/packing \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/sat \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/scheduling \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/util \
 $(PYPI_ARCHIVE_LIBS)

$(PYPI_ARCHIVE_TEMP_DIR):
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)

$(PYPI_ARCHIVE_TEMP_DIR)/ortools: | $(PYPI_ARCHIVE_TEMP_DIR)
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools: | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/README.txt: tools/README.pypi.txt | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(COPY) tools$SREADME.pypi.txt $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$SREADME.txt

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/LICENSE-2.0.txt: LICENSE-2.0.txt | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(COPY) LICENSE-2.0.txt $(PYPI_ARCHIVE_TEMP_DIR)$Sortools

ifndef PRE_RELEASE
OR_TOOLS_PYTHON_VERSION := $(OR_TOOLS_VERSION)
else
OR_TOOLS_PYTHON_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR)b$(GIT_REVISION)
endif


$(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py: tools/setup.py.in | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(COPY) tools$Ssetup.py.in $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/ORTOOLS_PYTHON_VERSION/ortools$(PYPI_OS)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_PYTHON_VERSION)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	$(SED) -i -e 's/\.dll/\.pyd/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEWIN/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEUNIX //g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	-del $(PYPI_ARCHIVE_TEMP_DIR)\ortools\setup.py-e
else
	$(SED) -i -e 's/\.dll/\.so/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEWIN //g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEUNIX/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DLL/$L/g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
endif

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/__init__.py: \
	$(GEN_DIR)/ortools/__init__.py | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	$(COPY) $(GEN_PATH)$Sortools$S__init__.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
ifeq ($(SYSTEM),win)
	echo __version__ = "$(OR_TOOLS_PYTHON_VERSION)" >> \
 $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
else
	echo "__version__ = \"$(OR_TOOLS_PYTHON_VERSION)\"" >> \
 $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S__init__.py
endif

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/init: $(PYINIT_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sinit
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sinit
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sinit$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sinit$Spywrapinit.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sinit
	$(COPY) $(GEN_PATH)$Sortools$Sinit$S_pywrapinit.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sinit

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/algorithms: $(PYALGORITHMS_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Salgorithms$Spywrapknapsack_solver.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(COPY) $(GEN_PATH)$Sortools$Salgorithms$S_pywrapknapsack_solver.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/graph: $(PYGRAPH_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sgraph$Spywrapgraph.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph
	$(COPY) $(GEN_PATH)$Sortools$Sgraph$S_pywrapgraph.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sgraph

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/constraint_solver: $(PYCP_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(COPY) $(GEN_PATH)$Sortools$Sconstraint_solver$S_pywrapcp.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/linear_solver: $(PYLP_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver$S__init__.py
	$(COPY) ortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) $(GEN_PATH)$Sortools$Slinear_solver$S*.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) $(GEN_PATH)$Sortools$Slinear_solver$S_pywraplp.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/sat: $(PYSAT_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$S__init__.py
	$(COPY) ortools$Ssat$Sdoc$S*.md $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) ortools$Ssat$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) $(GEN_PATH)$Sortools$Ssat$S*.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) $(GEN_PATH)$Sortools$Ssat$S_pywrapsat.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$Spython
	$(COPY) ortools$Ssat$Spython$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$Spython

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/packing: $(PYPACKING_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Spacking
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Spacking
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Spacking$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Spacking$S*.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Spacking

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/scheduling: $(PYSCHEDULING_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sscheduling
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sscheduling
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sscheduling$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sscheduling$S*.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sscheduling
	$(COPY) $(GEN_PATH)$Sortools$Sscheduling$S_pywraprcpsp.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sscheduling

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/util: $(PYSORTED_INTERVAL_LIST_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sutil$S*.py* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil
	$(COPY) $(GEN_PATH)$Sortools$Sutil$S_sorted_interval_list.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/.libs: | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs

ifneq ($(PYTHON_EXECUTABLE),)
.PHONY: pypi_archive
package_python pypi_archive: $(OR_TOOLS_LIBS) python $(MISSING_PYPI_FILES)
ifeq ($(SYSTEM),unix)
	cp $(OR_TOOLS_LIBS) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/.libs
endif
	cd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools && "$(PYTHON_EXECUTABLE)" setup.py bdist_wheel

.PHONY: test_package_python # Test Python "ortools" wheel package
test_package_python: package_python
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(PYTHON_EXECUTABLE) -m venv $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) test.py.in $(PYPI_ARCHIVE_TEMP_DIR)$Svenv$Stest.py
	$(COPY) ortools$Salgorithms$Ssamples$Ssimple_knapsack_program.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Sgraph$Ssamples$Ssimple_max_flow_program.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Sgraph$Ssamples$Ssimple_min_cost_flow_program.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Slinear_solver$Ssamples$Ssimple_lp_program.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Slinear_solver$Ssamples$Ssimple_mip_program.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Ssat$Ssamples$Ssimple_sat_program.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Sconstraint_solver$Ssamples$Stsp.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Sconstraint_solver$Ssamples$Svrp.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) ortools$Sconstraint_solver$Ssamples$Scvrptw_break.py $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
ifneq ($(SYSTEM),win)
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python -m pip install $(PYPI_ARCHIVE_TEMP_DIR)/ortools/dist/*.whl
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/test.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/simple_knapsack_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/simple_max_flow_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/simple_min_cost_flow_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/simple_lp_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/simple_mip_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/simple_sat_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/tsp.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/vrp.py
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/cvrptw_break.py
else
# wildcar not working on windows:  i.e. `pip install *.whl`:
# *.whl is not a valid wheel filename.
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python -m pip install --find-links=$(PYPI_ARCHIVE_TEMP_DIR)\ortools\dist ortools
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\test.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\simple_knapsack_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\simple_max_flow_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\simple_min_cost_flow_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\simple_lp_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\simple_mip_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\simple_sat_program.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\tsp.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\vrp.py
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\cvrptw_break.py
endif
endif # ifneq ($(PYTHON_EXECUTABLE),)

.PHONY: publish_python_pimpl
publish_python_pimpl: package_python
	cd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools && "$(TWINE_EXECUTABLE)" upload "*.whl"

.PHONY: install_python # Install Python OR-Tools on the host system
install_python: pypi_archive
	cd "$(PYPI_ARCHIVE_TEMP_DIR)$Sortools" && "$(PYTHON_EXECUTABLE)" setup.py install --user

.PHONY: uninstall_python # Uninstall Python OR-Tools from the host system
uninstall_python:
	"$(PYTHON_EXECUTABLE)" -m pip uninstall ortools

#######################
##  EXAMPLE ARCHIVE  ##
#######################
TEMP_PYTHON_DIR=temp_python

$(TEMP_PYTHON_DIR):
	$(MKDIR) $(TEMP_PYTHON_DIR)

$(TEMP_PYTHON_DIR)/ortools_examples: | $(TEMP_PYTHON_DIR)
	$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples

$(TEMP_PYTHON_DIR)/ortools_examples/examples: | $(TEMP_PYTHON_DIR)/ortools_examples
	$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples

$(TEMP_PYTHON_DIR)/ortools_examples/examples/python: | $(TEMP_PYTHON_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython

$(TEMP_PYTHON_DIR)/ortools_examples/examples/data: | $(TEMP_PYTHON_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Sdata

$(TEMP_PYTHON_DIR)/ortools_examples/examples/notebook: | $(TEMP_PYTHON_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook

.PHONY: python_examples_archive # Build stand-alone Python examples archive file for redistribution.
python_examples_archive: | \
 $(TEMP_PYTHON_DIR)/ortools_examples/examples/python \
 $(TEMP_PYTHON_DIR)/ortools_examples/examples/notebook \
 $(TEMP_PYTHON_DIR)/ortools_examples/examples/data
	$(COPY) $(PYTHON_EX_PATH)$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) $(CONTRIB_EX_PATH)$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) ortools$Salgorithms$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) ortools$Sgraph$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) ortools$Slinear_solver$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) ortools$Sconstraint_solver$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) ortools$Ssat$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	$(COPY) examples$Snotebook$Salgorithms$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$Sconstraint_solver$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$Scontrib$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$Sexamples$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$Sgraph$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$Slinear_solver$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$Ssat$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) examples$Snotebook$S*.md $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	$(COPY) tools$SREADME.python.md $(TEMP_PYTHON_DIR)$Sortools_examples$SREADME.md
	$(COPY) LICENSE-2.0.txt $(TEMP_PYTHON_DIR)$Sortools_examples
ifeq ($(SYSTEM),win)
	cd $(TEMP_PYTHON_DIR)\ortools_examples \
 && ..\..\$(TAR) -C ..\.. -c -v \
 --exclude *svn* --exclude *roadef* --exclude *vector_packing* \
 examples\data | ..\..\$(TAR) xvm
	cd $(TEMP_PYTHON_DIR) \
 && ..\$(ZIP) \
 -r ..\or-tools_python_examples_v$(OR_TOOLS_VERSION).zip \
 ortools_examples
else
	cd $(TEMP_PYTHON_DIR)/ortools_examples \
 && tar -C ../.. -c -v \
 --exclude *svn* --exclude *roadef* --exclude *vector_packing* \
 examples/data | tar xvm
	cd $(TEMP_PYTHON_DIR) \
 && tar -c -v -z --no-same-owner \
 -f ../or-tools_python_examples$(PYPI_OS)_v$(OR_TOOLS_VERSION).tar.gz \
 ortools_examples
endif
	-$(DELREC) $(TEMP_PYTHON_DIR)$Sortools_examples

################
##  Cleaning  ##
################
.PHONY: clean_python # Clean Python output from previous build.
clean_python:
	-$(DEL) $(GEN_PATH)$Sortools$S__init__.py
	-$(DEL) ortools$S*.pyc
	-$(DELREC) ortools$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sinit$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sinit$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sinit$S__pycache__
	-$(DEL) ortools$Sinit$S*.pyc
	-$(DELREC) ortools$Sinit$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sinit$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sinit$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Salgorithms$S__pycache__
	-$(DEL) ortools$Salgorithms$S*.pyc
	-$(DELREC) ortools$Salgorithms$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sgraph$S__pycache__
	-$(DEL) ortools$Sgraph$S*.pyc
	-$(DELREC) ortools$Sgraph$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sconstraint_solver$S__pycache__
	-$(DEL) ortools$Sconstraint_solver$S*.pyc
	-$(DELREC) ortools$Sconstraint_solver$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Slinear_solver$S__pycache__
	-$(DEL) ortools$Slinear_solver$S*.pyc
	-$(DELREC) ortools$Slinear_solver$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Ssat$S__pycache__
	-$(DEL) ortools$Ssat$S*.pyc
	-$(DELREC) ortools$Ssat$S__pycache__
	-$(DEL) ortools$Ssat$Spython$S*.pyc
	-$(DELREC) ortools$Ssat$Spython$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sscheduling$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sscheduling$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sscheduling$S__pycache__
	-$(DEL) ortools$Spacking$S*.pyc
	-$(DELREC) ortools$Spacking$S__pycache__
	-$(DEL) ortools$Sscheduling$S*.pyc
	-$(DELREC) ortools$Sscheduling$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sscheduling$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sscheduling$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sutil$S__pycache__
	-$(DEL) ortools$Sutil$S*.pyc
	-$(DELREC) ortools$Sutil$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S_*
	-$(DEL) $(LIB_DIR)$S_*.$(SWIG_PYTHON_LIB_SUFFIX)
	-$(DEL) $(OBJ_DIR)$Sswig$S*python_wrap.$O
	-$(DELREC) temp_python*

#############
##  DEBUG  ##
#############
.PHONY: detect_python # Show variables used to build Python OR-Tools.
detect_python:
	@echo Relevant info for the Python build:
ifeq ($(SYSTEM),win)
	@echo WINDOWS_PATH_TO_PYTHON = "$(WINDOWS_PATH_TO_PYTHON)"
else
	@echo UNIX_PYTHON_VER = "$(UNIX_PYTHON_VER)"
endif
	@echo PYTHON_COMPILER = $(PYTHON_COMPILER)
	@echo PYTHON_EXECUTABLE = "$(PYTHON_EXECUTABLE)"
	@echo PYTHON_VERSION = $(PYTHON_VERSION)
	@echo PYTHON3 = $(PYTHON3)
	@echo PYTHON_INC = $(PYTHON_INC)
	@echo PYTHON_LNK = $(PYTHON_LNK)
	@echo PYTHON_LDFLAGS = $(PYTHON_LDFLAGS)
	@echo SWIG_BINARY = $(SWIG_BINARY)
	@echo SWIG_INC = $(SWIG_INC)
	@echo SWIG_PYTHON3_FLAG = $(SWIG_PYTHON3_FLAG)
	@echo SWIG_PYTHON_LIB_SUFFIX = $(SWIG_PYTHON_LIB_SUFFIX)
	@echo SET_PYTHONPATH = "$(SET_PYTHONPATH)"
	@echo MYPY_OUT = "$(MYPY_OUT)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
