# ---------- dotnet support using SWIG ----------
.PHONY: help_dotnet # Generate list of dotnet targets with descriptions.
help_dotnet:
	@echo Use one of the following dotnet targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.dotnet.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.dotnet.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

ORTOOLS_DLL_NAME := OrTools
ORTOOLS_TEST_DLL_NAME := $(ORTOOLS_DLL_NAME).Tests
ORTOOLS_FSHARP_DLL_NAME := $(ORTOOLS_DLL_NAME).FSharp
ORTOOLS_FSHARP_TEST_DLL_NAME := $(ORTOOLS_DLL_NAME).FSharp.Tests

ORTOOLS_NUSPEC_FILE := $(ORTOOLS_DLL_NAME).nuspec

CLR_PROTOBUF_DLL_NAME := Google.Protobuf
CLR_ORTOOLS_DLL_NAME := Google.$(ORTOOLS_DLL_NAME)
CLR_ORTOOLS_TEST_DLL_NAME := Google.$(ORTOOLS_TEST_DLL_NAME)
CLR_ORTOOLS_FSHARP_DLL_NAME := Google.$(ORTOOLS_FSHARP_DLL_NAME)
CLR_ORTOOLS_FSHARP_TEST_DLL_NAME := Google.$(ORTOOLS_FSHARP_TEST_DLL_NAME)
CLR_ORTOOLS_IMPORT_DLL_NAME := $(CLR_ORTOOLS_DLL_NAME).import

# relative to the project root folder
TEMP_DOTNET_TEST_DIR=temp_dotnet_test
TEMP_DOTNET_DIR=temp_dotnet

# Check for required build tools
ifeq ($(SYSTEM), win)
DOTNET_EXECUTABLE := $(shell $(WHICH) dotnet.exe 2>nul)
else # UNIX
ifeq ($(PLATFORM),MACOSX)
DOTNET_EXECUTABLE := $(PATH_TO_DOTNET_COMPILER)
else # LINUX
DOTNET_EXECUTABLE := $(shell $(WHICH) dotnet)
endif
endif

$(GEN_DIR)/com/google/ortools/properties:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties

.PHONY: csharp_dotnet # Build C# OR-Tools
csharp_dotnet: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL)

# Assembly Info
$(GEN_DIR)/com/google/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt: \
 | $(GEN_DIR)/com/google/ortools/properties
	@echo $(OR_TOOLS_VERSION) > $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SGitVersion$(OR_TOOLS_VERSION).txt

# Auto-generated code
$(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL): tools/$(CLR_PROTOBUF_DLL_NAME)$(DLL) | $(BIN_DIR)
	$(COPY) tools$S$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(BIN_DIR)

$(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/csharp/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(GLOP_DEPS) \
 $(LP_DEPS) \
 | $(GEN_DIR)/ortools/linear_solver $(GEN_DIR)/com/google/ortools/linearsolver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc \
 -module operations_research_linear_solver \
 -namespace $(CLR_ORTOOLS_DLL_NAME).LinearSolver \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver \
 $(SRC_DIR)$Sortools$Slinear_solver$Scsharp$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/constraint_solver/csharp/routing.i \
 $(SRC_DIR)/ortools/constraint_solver/csharp/constraint_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(SRC_DIR)/ortools/util/csharp/functions.i \
 $(CP_DEPS) \
 | $(GEN_DIR)/ortools/constraint_solver $(GEN_DIR)/com/google/ortools/constraintsolver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc \
 -module operations_research_constraint_solver \
 -namespace $(CLR_ORTOOLS_DLL_NAME).ConstraintSolver \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Scsharp$Srouting.i
	$(SED) -i -e 's/CSharp_new_Solver/CSharp_new_CpSolver/g' \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Solver/CSharp_delete_CpSolver/g' \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Solver/CSharp_CpSolver/g' \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_new_Constraint/CSharp_new_CpConstraint/g' \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Constraint/CSharp_delete_CpConstraint/g' \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Constraint/CSharp_CpConstraint/g' \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*

$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/algorithms/csharp/knapsack_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h \
 | $(GEN_DIR)/ortools/algorithms $(GEN_DIR)/com/google/ortools/algorithms
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc \
 -module operations_research_algorithms \
 -namespace $(CLR_ORTOOLS_DLL_NAME).Algorithms \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms \
 $(SRC_DIR)$Sortools$Salgorithms$Scsharp$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/graph/csharp/graph.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(GRAPH_DEPS) \
 | $(GEN_DIR)/ortools/graph $(GEN_DIR)/com/google/ortools/graph
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_csharp_wrap.cc \
 -module operations_research_graph \
 -namespace $(CLR_ORTOOLS_DLL_NAME).Graph \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph \
 $(SRC_DIR)$Sortools$Sgraph$Scsharp$Sgraph.i

$(OBJ_DIR)/swig/graph_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sgraph$Sgraph_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O

$(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/sat/csharp/sat.i \
 $(SRC_DIR)/ortools/sat/swig_helper.h \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(SAT_DEPS) \
 | $(GEN_DIR)/ortools/sat $(GEN_DIR)/com/google/ortools/sat
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 -module operations_research_sat \
 -namespace $(CLR_ORTOOLS_DLL_NAME).Sat \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O

# Protobufs
$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs: \
 $(SRC_DIR)/ortools/constraint_solver/search_limit.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs: \
 $(SRC_DIR)/ortools/sat/cp_model.proto \
 | $(GEN_DIR)/com/google/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/com/google/ortools/sat/SatParameters.g.cs: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat \
 --csharp_opt=file_extension=.g.cs \
 $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

$(CLR_KEYFILE): | $(BIN_DIR)
ifdef CLR_KEYFILE
	sn -k $(CLR_KEYFILE)
endif

# Main DLL
$(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL): \
 $(CLR_KEYFILE) \
 $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL) \
 $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/sat_csharp_wrap.$O \
 $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/graph_csharp_wrap.$O \
 $(SRC_DIR)/ortools/com/google/ortools/algorithms/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/IntVarArrayHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/NetDecisionBuilder.cs \
 $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/SolverHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/ValCstPair.cs \
 $(SRC_DIR)/ortools/com/google/ortools/linearsolver/DoubleArrayHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/linearsolver/LinearExpr.cs \
 $(SRC_DIR)/ortools/com/google/ortools/linearsolver/LinearConstraint.cs \
 $(SRC_DIR)/ortools/com/google/ortools/linearsolver/SolverHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/linearsolver/VariableHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/sat/CpModel.cs \
 $(SRC_DIR)/ortools/com/google/ortools/util/NestedArrayHelper.cs \
 $(SRC_DIR)/ortools/com/google/ortools/util/ProtoHelper.cs \
 $(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs \
 $(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs \
 $(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs \
 $(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs \
 $(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs \
 $(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs \
 $(OR_TOOLS_LIBS) \
 | \
 $(GEN_DIR)/com/google/ortools/algorithms \
 $(GEN_DIR)/com/google/ortools/constraintsolver \
 $(GEN_DIR)/com/google/ortools/graph \
 $(GEN_DIR)/com/google/ortools/linearsolver \
 $(GEN_DIR)/com/google/ortools/sat \
 $(BIN_DIR)
	$(DYNAMIC_LD) \
 $(LD_OUT)$(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)
	"$(DOTNET_EXECUTABLE)" build -c Debug ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
ifeq ($(SYSTEM),win)
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sbin$Sx64$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
else
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sbin$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
endif

.PHONY: fsharp_dotnet # Build F# OR-Tools
fsharp_dotnet: $(BIN_DIR)/$(CLR_ORTOOLS_FSHARP_DLL_NAME)$(DLL)

$(BIN_DIR)/$(CLR_ORTOOLS_FSHARP_DLL_NAME)$(DLL): \
 $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) \
 | $(BIN_DIR)
	"$(DOTNET_EXECUTABLE)" build -c Debug ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
ifeq ($(SYSTEM),win)
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sbin$Sx64$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
else
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sbin$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
endif

.PHONY: clean_dotnet # Clean files
clean_dotnet:
	-$(DELREC) $(GEN_PATH)
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$Spackage
	-$(DEL) $(OBJ_DIR)$Sswig$S*_csharp_wrap.$O
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).*
	-$(DEL) $(BIN_DIR)$S$(CLR_PROTOBUF_DLL_NAME).*
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_DLL_NAME).*
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_FSHARP_DLL_NAME).*
	-$(DEL) $(CLR_KEYFILE)
	-$(DELREC) .$S$(TEMP_DOTNET_DIR)
	-$(DELREC) .$S$(TEMP_DOTNET_TEST_DIR)

.PHONY: test_dotnet # Test dotnet version of OR-Tools
test_dotnet: dotnet
	"$(DOTNET_EXECUTABLE)" restore \
 --packages "ortools$Sdotnet$Spackages" \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$S$(ORTOOLS_TEST_DLL_NAME).csproj"
	"$(DOTNET_EXECUTABLE)" restore \
 --packages "ortools$Sdotnet$Spackages" \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$S$(ORTOOLS_FSHARP_TEST_DLL_NAME).fsproj"
	$(MKDIR_P) .$S$(TEMP_DOTNET_TEST_DIR)
	$(COPY) $(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) .$S$(TEMP_DOTNET_TEST_DIR)
	"$(DOTNET_EXECUTABLE)" clean \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$S$(ORTOOLS_TEST_DLL_NAME).csproj"
	"$(DOTNET_EXECUTABLE)" build \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$S$(ORTOOLS_TEST_DLL_NAME).csproj"
	"$(DOTNET_EXECUTABLE)" clean \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$S$(ORTOOLS_FSHARP_TEST_DLL_NAME).fsproj"
	"$(DOTNET_EXECUTABLE)" build \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$S$(ORTOOLS_FSHARP_TEST_DLL_NAME).fsproj"
	"$(DOTNET_EXECUTABLE)" test \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)"
	"$(DOTNET_EXECUTABLE)" test \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)"

.PHONY: dotnet # Build OrTools for .NET
dotnet: ortoolslibs csharp_dotnet fsharp_dotnet
	$(SED) -i -e "s/<Version>.*<\/Version>/<Version>$(OR_TOOLS_VERSION)<\/Version>/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	$(SED) -i -e "s/<AssemblyVersion>.*<\/AssemblyVersion>/<AssemblyVersion>$(OR_TOOLS_VERSION)<\/AssemblyVersion>/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	$(SED) -i -e "s/<FileVersion>.*<\/FileVersion>/<FileVersion>$(OR_TOOLS_VERSION)<\/FileVersion>/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	$(SED) -i -e "s/<Version>.*<\/Version>/<Version>$(OR_TOOLS_VERSION)<\/Version>/" ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
	$(SED) -i -e "s/<AssemblyVersion>.*<\/AssemblyVersion>/<AssemblyVersion>$(OR_TOOLS_VERSION)<\/AssemblyVersion>/" ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
	$(SED) -i -e "s/<FileVersion>.*<\/FileVersion>/<FileVersion>$(OR_TOOLS_VERSION)<\/FileVersion>/" ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
	"$(DOTNET_EXECUTABLE)" build -c Release -o $(BIN_DIR) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	"$(DOTNET_EXECUTABLE)" build -c Release -o $(BIN_DIR) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
	$(MKDIR_P) $(TEMP_DOTNET_DIR)
	"$(DOTNET_EXECUTABLE)" publish -c Release -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" -f netstandard2.0 ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	"$(DOTNET_EXECUTABLE)" publish -c Release -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" -f netstandard2.0 ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj

BUILT_LANGUAGES +=, dotnet \(netstandard2.0\)

ifeq ($(SYSTEM),win)
NUGET_COMPILER ?= nuget.exe
NUGET_EXECUTABLE := $(shell $(WHICH) $(NUGET_COMPILER) 2>nul)
else #UNIX
NUGET_COMPILER ?= nuget
NUGET_EXECUTABLE := $(shell which $(NUGET_COMPILER))
endif
NUGET_SRC = https://www.nuget.org/api/v2/package

.PHONY: pkg_dotnet # Build Nuget Package
pkg_dotnet:
	$(warning Not Implemented)

.PHONY: pkg_dotnet-upload # Upload Nuget Package
pkg_dotnet-upload: nuget_archive
	$(warning Not Implemented)

.PHONY: detect_dotnet # Show variables used to build dotnet OR-Tools.
detect_dotnet:
	@echo Relevant info for the dotnet build:
	@echo PROTOC = $(PROTOC)
	@echo DOTNET_EXECUTABLE = $(DOTNET_EXECUTABLE)
	@echo NUGET_EXECUTABLE = $(NUGET_EXECUTABLE)
	@echo SWIG_PYTHON_LIB_SUFFIX = $(SWIG_PYTHON_LIB_SUFFIX)
	@echo CLR_PROTOBUF_DLL_NAME = $(CLR_PROTOBUF_DLL_NAME)
	@echo CLR_ORTOOLS_IMPORT_DLL_NAME = $(CLR_ORTOOLS_IMPORT_DLL_NAME)
	@echo CLR_ORTOOLS_DLL_NAME = $(CLR_ORTOOLS_DLL_NAME)
	@echo CLR_ORTOOLS_FSHARP_DLL_NAME = $(CLR_ORTOOLS_FSHARP_DLL_NAME)
	@echo CLR_ORTOOLS_TEST_DLL_NAME = $(CLR_ORTOOLS_TEST_DLL_NAME)
	@echo CLR_ORTOOLS_FSHARP_TEST_DLL_NAME = $(CLR_ORTOOLS_FSHARP_TEST_DLL_NAME)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
