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
SET_PYTHONPATH = set PYTHONPATH=$(OR_TOOLS_PYTHONPATH) &&
else # UNIX
PYTHON_COMPILER ?= python$(UNIX_PYTHON_VER)
PYTHON_EXECUTABLE := $(shell which $(PYTHON_COMPILER))
SET_PYTHONPATH = PYTHONPATH=$(OR_TOOLS_PYTHONPATH)
endif

# Detect python3
ifneq ($(PYTHON_EXECUTABLE),)
ifeq ($(shell "$(PYTHON_EXECUTABLE)" -c "from sys import version_info as v; print (str(v[0]))"),3)
PYTHON3 := true
SWIG_PYTHON3_FLAG := -py3 -DPY3
PYTHON3_CFLAGS := -DPY3
endif
endif

# All libraries and dependecies
PYALGORITHMS_LIBS = $(LIB_DIR)/_pywrapknapsack_solver.$(SWIG_PYTHON_LIB_SUFFIX)
PYGRAPH_LIBS = $(LIB_DIR)/_pywrapgraph.$(SWIG_PYTHON_LIB_SUFFIX)
PYCP_LIBS = $(LIB_DIR)/_pywrapcp.$(SWIG_PYTHON_LIB_SUFFIX)
PYLP_LIBS = $(LIB_DIR)/_pywraplp.$(SWIG_PYTHON_LIB_SUFFIX)
PYSAT_LIBS = $(LIB_DIR)/_pywrapsat.$(SWIG_PYTHON_LIB_SUFFIX)
PYDATA_LIBS = $(LIB_DIR)/_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX)
PYTHON_OR_TOOLS_LIBS = \
 $(GEN_DIR)/ortools/__init__.py \
 $(PYALGORITHMS_LIBS) \
 $(PYGRAPH_LIBS) \
 $(PYCP_LIBS) \
 $(PYLP_LIBS) \
 $(PYSAT_LIBS) \
 $(PYDATA_LIBS)

# Main target
.PHONY: python # Build Python OR-Tools.
.PHONY: check_python # Quick check only running Python OR-Tools samples.
.PHONY: test_python # Run all Python OR-Tools test targets.
ifneq ($(PYTHON_EXECUTABLE),)
python: $(PYTHON_OR_TOOLS_LIBS)
check_python: check_python_pimpl
test_python: test_python_pimpl
BUILT_LANGUAGES +=, Python$(PYTHON_VERSION)
else
python:
	@echo PYTHON_EXECUTABLE = "${PYTHON_EXECUTABLE}"
	$(warning Cannot find '$(PYTHON_COMPILER)' command which is needed for build. Please make sure it is installed and in system path.)
check_python: python
test_python: python
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
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
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
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/ortools/constraint_solver/assignment_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/assignment.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Sassignment.proto

$(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/ortools/constraint_solver/routing_parameters_pb2.py: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 $(GEN_DIR)/ortools/constraint_solver/solver_parameters_pb2.py \
 $(GEN_DIR)/ortools/constraint_solver/routing_enums_pb2.py \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
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
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc \
 -module pywrapcp \
 $(SRC_DIR)/ortools/constraint_solver$Spython$Srouting.i
	$(SED) -i -e 's/< long long >/< int64 >/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc
	$(SED) -i -e 's/< long long,long long >/< int64, int64 >/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc
	$(SED) -i -e 's/< long long,std::allocator/< int64, std::allocator/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc
	$(SED) -i -e 's/,long long,/,int64,/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_python_wrap.cc

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
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)/ortools/util/optional_boolean.proto

$(GEN_DIR)/ortools/linear_solver/linear_solver_pb2.py: \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.proto \
 $(GEN_DIR)/ortools/util/optional_boolean_pb2.py \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/linear_solver
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
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
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
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
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)/ortools/sat/cp_model.proto

$(GEN_DIR)/ortools/sat/sat_parameters_pb2.py: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
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

# pywraprcpsp
ifeq ($(PLATFORM),MACOSX)
PYRCPSP_LDFLAGS = -install_name @rpath/_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX) #
endif

$(GEN_DIR)/ortools/data/rcpsp_pb2.py: \
 $(SRC_DIR)/ortools/data/rcpsp.proto \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/data
	$(PROTOC) --proto_path=$(INC_DIR) --python_out=$(GEN_PATH) \
 $(SRC_DIR)/ortools/data/rcpsp.proto

$(GEN_DIR)/ortools/data/pywraprcpsp.py: \
 $(SRC_DIR)/ortools/data/rcpsp_parser.h \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/data/python/rcpsp.i \
 $(GEN_DIR)/ortools/data/rcpsp_pb2.py \
 $(DATA_DEPS) \
 $(PROTOBUF_PYTHON_DESC) \
 | $(GEN_DIR)/ortools/data
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -python $(SWIG_PYTHON3_FLAG) \
 -o $(GEN_PATH)$Sortools$Sdata$Srcpsp_python_wrap.cc \
 -module pywraprcpsp \
 $(SRC_DIR)/ortools/data$Spython$Srcpsp.i

$(GEN_DIR)/ortools/data/rcpsp_python_wrap.cc: \
 $(GEN_DIR)/ortools/data/pywraprcpsp.py

$(OBJ_DIR)/swig/rcpsp_python_wrap.$O: \
 $(GEN_DIR)/ortools/data/rcpsp_python_wrap.cc \
 $(DATA_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) $(PYTHON_INC) $(PYTHON3_CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sdata$Srcpsp_python_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Srcpsp_python_wrap.$O

$(PYDATA_LIBS): $(OBJ_DIR)/swig/rcpsp_python_wrap.$O $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(PYRCPSP_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Srcpsp_python_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(SYS_LNK) \
 $(PYTHON_LNK) \
 $(PYTHON_LDFLAGS)
ifeq ($(SYSTEM),win)
	copy $(LIB_DIR)$S_pywraprcpsp.$(SWIG_PYTHON_LIB_SUFFIX) $(GEN_PATH)\\ortools\\data\\_pywraprcpsp.pyd
else
	cp $(PYDATA_LIBS) $(GEN_PATH)/ortools/data
endif

#######################
##  Python SOURCE  ##
#######################
ifeq ($(SOURCE_SUFFIX),.py) # Those rules will be used if SOURCE contains a .py file
.PHONY: build # Build a Python program.
build: $(SOURCE) $(PYTHON_OR_TOOLS_LIBS) ;

.PHONY: run # Run a Python program.
run: build
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(SOURCE_PATH) $(ARGS)
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
 rpy_vrp_capacity \
 rpy_vrp_drop_nodes \
 rpy_vrp_global_span \
 rpy_vrp_initial_routes \
 rpy_vrp_pickup_delivery \
 rpy_vrp_pickup_delivery_fifo \
 rpy_vrp_pickup_delivery_lifo \
 rpy_vrp_resources \
 rpy_vrp_starts_ends \
 rpy_vrp_time_windows \
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
 rpy_simple_lp_program \
 rpy_simple_mip_program \
 rpy_linear_programming_example \
 rpy_integer_programming_example

.PHONY: test_python_sat_samples # Run all Python Sat Samples (located in ortools/sat/samples)
test_python_sat_samples: \
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
 \
 rpy_stigler_diet
# rpy_rabbits_pheasants_cp \
# rpy_cryptarithmetic_cp \
# rpy_cryptarithmetic_sat \
# rpy_nqueens_cp \
# rpy_nqueens_sat \
# rpy_integer_programming \
# rpy_knapsack \
# rpy_max_flow \
# rpy_min_cost_flow \
# rpy_assignment \
# rpy_nurses_cp \
# rpy_job_shop_cp \

.PHONY: test_python_tests # Run all Python Tests (located in examples/tests)
test_python_tests: \
 rpy_cp_model_test \
 rpy_test_cp_api \
 rpy_test_routing_api \
 rpy_test_lp_api

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
 rpy_stigler \
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
 rpy_assignment_sat \
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
 rpy_knapsack \
 rpy_linear_assignment_api \
 rpy_linear_programming \
 rpy_magic_sequence_distribute \
 rpy_nqueens_sat \
 rpy_pyflow_example \
 rpy_reallocate_sat \
 rpy_rcpsp_sat \
 rpy_single_machine_scheduling_with_setup_release_due_dates_sat \
 rpy_steel_mill_slab_sat \
 rpy_stigler_diet \
 rpy_sudoku_sat \
 rpy_tasks_and_workers_assignment_sat \
 rpy_transit_time \
 rpy_vendor_scheduling_sat \
 rpy_wedding_optimal_chart_sat \
 rpy_worker_schedule_sat \
 rpy_zebra_sat
	$(MAKE) run SOURCE=examples/python/shift_scheduling_sat.py ARGS="--params max_time_in_seconds:10.0" \


.PHONY: test_python_pimpl
test_python_pimpl: \
 check_python_pimpl \
 test_python_tests \
 test_python_contrib \
 test_python_python

################
##  Cleaning  ##
################
.PHONY: clean_python # Clean Python output from previous build.
clean_python:
	-$(DEL) $(GEN_PATH)$Sortools$S__init__.py
	-$(DEL) ortools$S*.pyc
	-$(DELREC) ortools$S__pycache__
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
	-$(DEL) $(GEN_PATH)$Sortools$Sdata$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sdata$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sdata$S__pycache__
	-$(DEL) ortools$Sdata$S*.pyc
	-$(DELREC) ortools$Sdata$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sdata$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sdata$S_pywrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*.py
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*.pyc
	-$(DELREC) $(GEN_PATH)$Sortools$Sutil$S__pycache__
	-$(DEL) ortools$Sutil$S*.pyc
	-$(DELREC) ortools$Sutil$S__pycache__
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*_python_wrap.*
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S_pywrap*
	-$(DEL) $(LIB_DIR)$S_pywrap*.$(SWIG_PYTHON_LIB_SUFFIX)
	-$(DEL) $(OBJ_DIR)$Sswig$S*python_wrap.$O
	-$(DELREC) temp_python*

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
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/algorithms \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/graph \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/constraint_solver \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/linear_solver \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/sat \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/data \
 $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/util \
 $(PYPI_ARCHIVE_LIBS)

$(PYPI_ARCHIVE_TEMP_DIR):
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)

$(PYPI_ARCHIVE_TEMP_DIR)/ortools: | $(PYPI_ARCHIVE_TEMP_DIR)
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools: | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/README.txt: tools/README.pypi | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(COPY) tools$SREADME.pypi $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$SREADME.txt

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/LICENSE-2.0.txt: LICENSE-2.0.txt | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(COPY) LICENSE-2.0.txt $(PYPI_ARCHIVE_TEMP_DIR)$Sortools

PYTHON_SETUP_DEPS=
ifeq ($(UNIX_GFLAGS_DIR),$(OR_TOOLS_TOP)/dependencies/install)
  ifeq ($(PLATFORM),MACOSX)
    PYTHON_SETUP_DEPS += , 'libgflags.2.2.$L'
  endif
  ifeq ($(PLATFORM),LINUX)
    PYTHON_SETUP_DEPS += , 'libgflags.$L.2.2'
  endif
endif
ifeq ($(UNIX_GLOG_DIR),$(OR_TOOLS_TOP)/dependencies/install)
  ifeq ($(PLATFORM),MACOSX)
    PYTHON_SETUP_DEPS += , 'libglog.0.3.5.$L'
  endif
  ifeq ($(PLATFORM),LINUX)
    PYTHON_SETUP_DEPS += , 'libglog.$L.0.3.5'
  endif
endif
ifeq ($(UNIX_PROTOBUF_DIR),$(OR_TOOLS_TOP)/dependencies/install)
  ifeq ($(PLATFORM),MACOSX)
    PYTHON_SETUP_DEPS += , 'libprotobuf.3.6.1.$L'
  endif
  ifeq ($(PLATFORM),LINUX)
    PYTHON_SETUP_DEPS += , 'libprotobuf.$L.3.6.1'
  endif
endif
ifeq ($(UNIX_ABSL_DIR),$(OR_TOOLS_TOP)/dependencies/install)
  ifeq ($(PLATFORM),MACOSX)
    PYTHON_SETUP_DEPS += , 'libabsl_*'
  endif
  ifeq ($(PLATFORM),LINUX)
    PYTHON_SETUP_DEPS += , 'libabsl_*'
  endif
endif
ifeq ($(UNIX_CBC_DIR),$(OR_TOOLS_TOP)/dependencies/install)
  ifeq ($(PLATFORM),MACOSX)
    PYTHON_SETUP_DEPS += , 'libCbcSolver.3.$L'
    PYTHON_SETUP_DEPS += , 'libCbc.3.$L'
    PYTHON_SETUP_DEPS += , 'libOsiCbc.3.$L'
    PYTHON_SETUP_DEPS += , 'libCgl.1.$L'
    PYTHON_SETUP_DEPS += , 'libClpSolver.1.$L'
    PYTHON_SETUP_DEPS += , 'libClp.1.$L'
    PYTHON_SETUP_DEPS += , 'libOsiClp.1.$L'
    PYTHON_SETUP_DEPS += , 'libOsi.1.$L'
    PYTHON_SETUP_DEPS += , 'libCoinUtils.3.$L'
  endif
  ifeq ($(PLATFORM),LINUX)
    PYTHON_SETUP_DEPS += , 'libCbcSolver.$L.3'
    PYTHON_SETUP_DEPS += , 'libCbc.$L.3'
    PYTHON_SETUP_DEPS += , 'libOsiCbc.$L.3'
    PYTHON_SETUP_DEPS += , 'libCgl.$L.1'
    PYTHON_SETUP_DEPS += , 'libClpSolver.$L.1'
    PYTHON_SETUP_DEPS += , 'libClp.$L.1'
    PYTHON_SETUP_DEPS += , 'libOsiClp.$L.1'
    PYTHON_SETUP_DEPS += , 'libOsi.$L.1'
    PYTHON_SETUP_DEPS += , 'libCoinUtils.$L.3'
  endif
endif

ifndef PRE_RELEASE
OR_TOOLS_PYTHON_VERSION := $(OR_TOOLS_VERSION)
else
OR_TOOLS_PYTHON_VERSION := $(OR_TOOLS_MAJOR).$(OR_TOOLS_MINOR)b$(GIT_REVISION)
endif


$(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py: tools/setup.py | $(PYPI_ARCHIVE_TEMP_DIR)/ortools
	$(COPY) tools$Ssetup.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools
	$(SED) -i -e 's/ORTOOLS_PYTHON_VERSION/ortools$(PYPI_OS)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/VVVV/$(OR_TOOLS_PYTHON_VERSION)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
	$(SED) -i -e 's/PROTOBUF_TAG/$(PROTOBUF_TAG)/' $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Ssetup.py
ifeq ($(SYSTEM),win)
	$(SED) -i -e 's/\.dll/\.pyd/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEWIN/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEUNIX/          /g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	-del $(PYPI_ARCHIVE_TEMP_DIR)\ortools\setup.py-e
else
	$(SED) -i -e 's/\.dll/\.so/' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DELETEWIN //g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e '/DELETEUNIX/d' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e 's/DLL/$L/g' $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
	$(SED) -i -e "s/DDDD/$(PYTHON_SETUP_DEPS)/g" $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py
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

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/algorithms: $(PYALGORITHMS_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Salgorithms$Spywrapknapsack_solver.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Salgorithms
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
	$(COPY) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver
	$(COPY) $(GEN_PATH)$Sortools$Sconstraint_solver$S_pywrapcp.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sconstraint_solver

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/linear_solver: $(PYLP_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver$S__init__.py
	$(COPY) ortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) $(GEN_PATH)$Sortools$Slinear_solver$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver
	$(COPY) $(GEN_PATH)$Sortools$Slinear_solver$S_pywraplp.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Slinear_solver

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/sat: $(PYSAT_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$S__init__.py
	$(COPY) ortools$Ssat$Sdoc$S*.md $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) ortools$Ssat$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) $(GEN_PATH)$Sortools$Ssat$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	$(COPY) $(GEN_PATH)$Sortools$Ssat$S_pywrapsat.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$Spython
	$(COPY) ortools$Ssat$Spython$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Ssat$Spython

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/data: $(PYDATA_LIBS) | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sdata$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata
	$(COPY) $(GEN_PATH)$Sortools$Sdata$S_pywraprcpsp.* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sdata

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/util: | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil
	$(TOUCH) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil$S__init__.py
	$(COPY) $(GEN_PATH)$Sortools$Sutil$S*.py $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$Sutil

$(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/.libs: | $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
	-$(MKDIR) $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs

ifneq ($(PYTHON_EXECUTABLE),)
.PHONY: pypi_archive # Create Python "ortools" wheel package
pypi_archive: $(OR_TOOLS_LIBS) python $(MISSING_PYPI_FILES)
ifneq ($(SYSTEM),win)
	cp $(OR_TOOLS_LIBS) $(PYPI_ARCHIVE_TEMP_DIR)/ortools/ortools/.libs
endif
ifeq ($(UNIX_GFLAGS_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Slib$Slibgflags* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
endif
ifeq ($(UNIX_GLOG_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Slib$Slibglog* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
endif
ifeq ($(UNIX_PROTOBUF_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) $(subst /,$S,$(_PROTOBUF_LIB_DIR))libproto* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
endif
ifeq ($(UNIX_ABSL_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	-$(COPYREC) $(subst /,$S,$(_ABSL_LIB_DIR))$Slibabsl* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
endif
ifeq ($(UNIX_CBC_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Slib$SlibCbc* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
	$(COPYREC) dependencies$Sinstall$Slib$SlibCgl* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
	$(COPYREC) dependencies$Sinstall$Slib$SlibClp* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
	$(COPYREC) dependencies$Sinstall$Slib$SlibOsi* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
	$(COPYREC) dependencies$Sinstall$Slib$SlibCoinUtils* $(PYPI_ARCHIVE_TEMP_DIR)$Sortools$Sortools$S.libs
endif
	cd $(PYPI_ARCHIVE_TEMP_DIR)$Sortools && "$(PYTHON_EXECUTABLE)" setup.py bdist_wheel

.PHONY: test_pypi_archive # Test Python "ortools" wheel package
test_pypi_archive: pypi_archive
	-$(DELREC) $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(PYTHON_EXECUTABLE) -m virtualenv $(PYPI_ARCHIVE_TEMP_DIR)$Svenv
	$(COPY) test.py.in $(PYPI_ARCHIVE_TEMP_DIR)$Svenv$Stest.py
ifneq ($(SYSTEM),win)
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python -m pip install $(PYPI_ARCHIVE_TEMP_DIR)/ortools/dist/*.whl
	$(PYPI_ARCHIVE_TEMP_DIR)/venv/bin/python $(PYPI_ARCHIVE_TEMP_DIR)/venv/test.py
else
# wildcar not working on windows:  i.e. `pip install *.whl`:
# *.whl is not a valid wheel filename.
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python -m pip install --find-links=$(PYPI_ARCHIVE_TEMP_DIR)\ortools\dist ortools
	$(PYPI_ARCHIVE_TEMP_DIR)\venv\Scripts\python $(PYPI_ARCHIVE_TEMP_DIR)\venv\test.py
endif
endif # ifneq ($(PYTHON_EXECUTABLE),)

.PHONY: install_python # Install Python OR-Tools on the host system
install_python: pypi_archive
	cd "$(PYPI_ARCHIVE_TEMP_DIR)$Sortools" && "$(PYTHON_EXECUTABLE)" setup.py install --user

.PHONY: uninstall_python # Uninstall Python OR-Tools from the host system
uninstall_python:
	"$(PYTHON_EXECUTABLE)" -m pip uninstall ortools

TEMP_PYTHON_DIR=temp_python

.PHONY: python_examples_archive # Build stand-alone Python examples archive file for redistribution.
python_examples_archive:
	-$(DELREC) $(TEMP_PYTHON_DIR)
	-$(MKDIR) $(TEMP_PYTHON_DIR)
	-$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples
	-$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples
	-$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	-$(MKDIR) $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Sdata
	-$(COPY) $(PYTHON_EX_PATH)$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(COPY) ortools$Salgorithms$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(COPY) ortools$Sgraph$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(COPY) ortools$Slinear_solver$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(COPY) ortools$Srouting$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(COPY) ortools$Ssat$Ssamples$S*.py $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Spython
	-$(COPY) examples$Snotebook$S*.ipynb $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	-$(COPY) examples$Snotebook$S*.md $(TEMP_PYTHON_DIR)$Sortools_examples$Sexamples$Snotebook
	-$(COPY) tools$SREADME.examples.python $(TEMP_PYTHON_DIR)$Sortools_examples$SREADME.md
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
	-$(DELREC) $(TEMP_PYTHON_DIR)

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
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
