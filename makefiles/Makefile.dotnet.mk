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

# Check for required build tools
DOTNET = dotnet
ifeq ($(SYSTEM),win)
DOTNET_BIN := $(shell $(WHICH) $(DOTNET) 2> NUL)
else # UNIX
DOTNET_BIN := $(shell command -v $(DOTNET) 2> /dev/null)
endif
NUGET_BIN = "$(DOTNET_BIN)" nuget

HAS_DOTNET = true
ifndef DOTNET_BIN
HAS_DOTNET =
endif

# List Examples and Tests
TEMP_DOTNET_DIR=temp_dotnet

# Main target
.PHONY: dotnet # Build all .NET OrTools packages
.PHONY: test_dotnet # Run all test_dotnet_* targets
ifndef HAS_DOTNET
dotnet:
	@echo DOTNET_BIN = $(DOTNET_BIN)
	$(warning Cannot find '$@' command which is needed for build. Please make sure it is installed and in system path.)
check_dotnet: dotnet
test_dotnet: dotnet
else
dotnet: \
 dotnet_csharp \
 dotnet_fsharp
check_dotnet: check_dotnet_examples
test_dotnet: \
 test_dotnet_csharp \
 test_dotnet_fsharp \
 test_dotnet_tests \
 test_donet_samples \
 test_dotnet_examples
BUILT_LANGUAGES +=, dotnet \(netstandard2.0\)
endif

$(PACKAGE_DIR):
	$(MKDIR_P) $(PACKAGE_DIR)

$(TEMP_DOTNET_DIR):
	$(MKDIR_P) $(TEMP_DOTNET_DIR)

# Detect RuntimeIDentifier
ifeq ($(OS),Windows)
RUNTIME_IDENTIFIER=win-x64
else
  ifeq ($(OS),Linux)
  RUNTIME_IDENTIFIER=linux-x64
  else
    ifeq ($(OS),Darwin)
    RUNTIME_IDENTIFIER=osx-x64
    else
    $(error OS unknown !)
    endif
  endif
endif

# All libraries and dependecies
DOTNET_ORTOOLS_SNK := $(BIN_DIR)/or-tools.snk
DOTNET_ORTOOLS_SNK_PATH := $(subst /,$S,$(DOTNET_ORTOOLS_SNK))
OR_TOOLS_ASSEMBLY_NAME := Google.OrTools
OR_TOOLS_TESTS_ASSEMBLY_NAME := Google.OrTools.Tests
OR_TOOLS_NATIVE_ASSEMBLY_NAME := $(OR_TOOLS_ASSEMBLY_NAME).runtime.$(RUNTIME_IDENTIFIER)
OR_TOOLS_FSHARP_ASSEMBLY_NAME := $(OR_TOOLS_ASSEMBLY_NAME).FSharp
OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME := $(OR_TOOLS_ASSEMBLY_NAME).FSharp.Tests
DOTNET_ORTOOLS_NUPKG := $(PACKAGE_DIR)/$(OR_TOOLS_ASSEMBLY_NAME).$(OR_TOOLS_VERSION).nupkg
DOTNET_ORTOOLS_NATIVE_NUPKG := $(PACKAGE_DIR)/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(OR_TOOLS_VERSION).nupkg
DOTNET_ORTOOLS_FSHARP_NUPKG := $(PACKAGE_DIR)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).$(OR_TOOLS_VERSION).nupkg

######################
##  RUNTIME CSHARP  ##
######################
.PHONY: dotnet_runtime # Build C# runtime OR-Tools
dotnet_runtime: $(DOTNET_ORTOOLS_NATIVE_NUPKG)

# Protobufs generated code
$(GEN_DIR)/ortools/constraint_solver/SearchLimit.pb.cs: \
 $(SRC_DIR)/ortools/constraint_solver/search_limit.proto \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Sconstraint_solver \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/ortools/constraint_solver/SolverParameters.pb.cs: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Sconstraint_solver \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/Model.pb.cs: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Sconstraint_solver \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/ortools/constraint_solver/RoutingParameters.pb.cs: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Sconstraint_solver \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/ortools/constraint_solver/RoutingEnums.pb.cs: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto \
 | $(GEN_DIR)/ortools/constraint_solver
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Sconstraint_solver \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/ortools/sat/CpModel.pb.cs: \
 $(SRC_DIR)/ortools/sat/cp_model.proto \
 | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Ssat \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/ortools/sat/SatParameters.pb.cs: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 | $(GEN_DIR)/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Ssat \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

# Auto-generated rid dependent source code
$(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/csharp/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(GLOP_DEPS) \
 $(LP_DEPS) \
 | $(GEN_DIR)/ortools/linear_solver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc \
 -module operations_research_linear_solver \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).LinearSolver \
 -dllimport "$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Sortools$Slinear_solver \
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
 | $(GEN_DIR)/ortools/constraint_solver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc \
 -module operations_research_constraint_solver \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).ConstraintSolver \
 -dllimport "$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Sortools$Sconstraint_solver \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Scsharp$Srouting.i
	$(SED) -i -e 's/CSharp_new_Solver/CSharp_new_CpSolver/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Solver/CSharp_delete_CpSolver/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Solver/CSharp_CpSolver/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_new_Constraint/CSharp_new_CpConstraint/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Constraint/CSharp_delete_CpConstraint/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$S*cs \
 $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Constraint/CSharp_CpConstraint/g' \
 $(GEN_PATH)$Sortools$Sconstraint_solver$S*cs \
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
 | $(GEN_DIR)/ortools/algorithms
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc \
 -module operations_research_algorithms \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).Algorithms \
 -dllimport "$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Sortools$Salgorithms \
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
 | $(GEN_DIR)/ortools/graph $(GEN_DIR)/ortools/graph
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_csharp_wrap.cc \
 -module operations_research_graph \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).Graph \
 -dllimport "$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Sortools$Sgraph \
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
 | $(GEN_DIR)/ortools/sat $(GEN_DIR)/ortools/sat
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 -module operations_research_sat \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).Sat \
 -dllimport "$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O

$(DOTNET_ORTOOLS_SNK): ortools/dotnet/CreateSigningKey/CreateSigningKey.csproj | $(BIN_DIR)
	"$(DOTNET_BIN)" run --project ortools$Sdotnet$SCreateSigningKey$SCreateSigningKey.csproj $S$(DOTNET_ORTOOLS_SNK_PATH)

$(LIB_DIR)/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX): \
 $(OR_TOOLS_LIBS) \
 $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/sat_csharp_wrap.$O \
 $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/graph_csharp_wrap.$O \
 | $(LIB_DIR)
	$(DYNAMIC_LD) \
 $(LD_OUT)$(LIB_DIR)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj

$(DOTNET_ORTOOLS_NATIVE_NUPKG): \
 $(DOTNET_ORTOOLS_SNK) \
 $(LIB_DIR)/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).$(SWIG_DOTNET_LIB_SUFFIX) \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)/$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj \
 $(SRC_DIR)/ortools/algorithms/csharp/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/IntVarArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/IntervalVarArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/NetDecisionBuilder.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/SolverHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/ValCstPair.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/DoubleArrayHelper.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/LinearExpr.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/LinearConstraint.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/SolverHelper.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/VariableHelper.cs \
 $(SRC_DIR)/ortools/sat/csharp/CpModel.cs \
 $(SRC_DIR)/ortools/util/csharp/NestedArrayHelper.cs \
 $(SRC_DIR)/ortools/util/csharp/ProtoHelper.cs \
 $(GEN_DIR)/ortools/constraint_solver/Model.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/SearchLimit.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/SolverParameters.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/RoutingParameters.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/RoutingEnums.pb.cs \
 $(GEN_DIR)/ortools/sat/CpModel.pb.cs \
 | $(PACKAGE_DIR)
	"$(DOTNET_BIN)" build ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj
	"$(DOTNET_BIN)" pack ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj

##############
##  CSHARP  ##
##############
.PHONY: dotnet_csharp # Build C# OR-Tools
dotnet_csharp: $(DOTNET_ORTOOLS_NUPKG)

$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/$(OR_TOOLS_ASSEMBLY_NAME).csproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/$(OR_TOOLS_ASSEMBLY_NAME).csproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj

$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/runtime.json: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/runtime.json.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sruntime.json.in \
 > ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sruntime.json

$(DOTNET_ORTOOLS_NUPKG): \
 $(DOTNET_ORTOOLS_NATIVE_NUPKG) \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/$(OR_TOOLS_ASSEMBLY_NAME).csproj \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/runtime.json \
 | $(PACKAGE_DIR)
	"$(DOTNET_BIN)" build ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" pack ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)

ortools/dotnet/$(OR_TOOLS_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_TESTS_ASSEMBLY_NAME).csproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_TESTS_ASSEMBLY_NAME).csproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME).csproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME).csproj

.PHONY: test_dotnet_csharp # Run C# OrTools Tests
test_dotnet_csharp: $(DOTNET_ORTOOLS_NUPKG) \
 ortools/dotnet/$(OR_TOOLS_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_TESTS_ASSEMBLY_NAME).csproj
	"$(DOTNET_BIN)" build ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" test ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)

##############
##  FSHARP  ##
##############
.PHONY: dotnet_fsharp # Build F# OR-Tools
dotnet_fsharp: $(DOTNET_ORTOOLS_FSHARP_NUPKG)

$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj

$(DOTNET_ORTOOLS_FSHARP_NUPKG): \
 $(DOTNET_ORTOOLS_NUPKG) \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj \
 | $(PACKAGE_DIR)
	"$(DOTNET_BIN)" build ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" pack ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)

ortools/dotnet/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj: \
 ortools/dotnet/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj

.PHONY: test_dotnet_fsharp # Run F# OrTools Tests
test_dotnet_fsharp: $(DOTNET_ORTOOLS_FSHARP_NUPKG) \
 ortools/dotnet/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj
	"$(DOTNET_BIN)" build ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" test ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)

###################
##  .NET SOURCE  ##
###################
# .Net C#
ifeq ($(SOURCE_SUFFIX),.cs) # Those rules will be used if SOURCE contain a .cs file
ifeq (,$(wildcard $(SOURCE)proj))
$(error File "$(SOURCE)proj" does not exist !)
endif

.PHONY: build # Build a .Net C# program.
build: $(SOURCE) $(SOURCE)proj $(DOTNET_ORTOOLS_NUPKG)
	"$(DOTNET_BIN)" build $(SOURCE_PATH)proj

.PHONY: run # Run a .Net C# program.
run: build
	"$(DOTNET_BIN)" --no-build \
 --project $(SOURCE_PATH)proj -- $(ARGS)
endif

# .Net F#
ifeq ($(SOURCE_SUFFIX),.fs) # Those rules will be used if SOURCE contain a .cs file
ifeq (,$(wildcard $(SOURCE)proj))
$(error File "$(SOURCE)proj" does not exist !)
endif

.PHONY: build # Build a .Net F# program.
build: $(SOURCE) $(SOURCE)proj $(DOTNET_ORTOOLS_FSHARP_NUPKG)
	"$(DOTNET_BIN)" build $(SOURCE_PATH)proj

.PHONY: run # Run a .Net F# program.
run: build
	"$(DOTNET_BIN)" --no-build \
 --project $(SOURCE_PATH)proj -- $(ARGS)
endif

#############################
##  .NET Examples/Samples  ##
#############################
rdotnet_%: \
 $(TEST_DIR)/% \
 $(TEST_DIR)/%proj \
 $(DOTNET_ORTOOLS_NUPKG) FORCE
	"$(DOTNET_BIN)" build $(TEST_PATH)$S$*proj
	"$(DOTNET_BIN)" run --no-build --project $(TEST_PATH)$S$*proj -- $(ARGS)

rdotnet_%.cs: \
 $(DOTNET_EX_DIR)/%.cs \
 $(DOTNET_EX_DIR)/%.csproj \
 $(DOTNET_ORTOOLS_NUPKG) FORCE
	"$(DOTNET_BIN)" build $(DOTNET_EX_PATH)$S$*.csproj
	"$(DOTNET_BIN)" run --no-build --project $(DOTNET_EX_PATH)$S$*.csproj -- $(ARGS)

rdotnet_%.fs: \
 $(DOTNET_EX_DIR)/%.fs \
 $(DOTNET_EX_DIR)/%.fsproj \
 $(DOTNET_ORTOOLS_FSHARP_NUPKG) FORCE
	"$(DOTNET_BIN)" build $(DOTNET_EX_PATH)$S$*.fsproj
	"$(DOTNET_BIN)" run --no-build --project $(DOTNET_EX_PATH)$S$*.fsproj -- $(ARGS)

rdotnet_%.cs: \
 ortools/sat/samples/%.cs \
 ortools/sat/samples/%.csproj \
 $(DOTNET_ORTOOLS_NUPKG) FORCE
	"$(DOTNET_BIN)" build ortools$Ssat$Ssamples$S$*.csproj
	"$(DOTNET_BIN)" run --no-build --project ortools$Ssat$Ssamples$S$*.csproj -- $(ARGS)

.PHONY: test_dotnet_tests # Build and Run all .Net Tests (located in examples/test)
test_dotnet_tests: \
 rdotnet_issue18.cs \
 rdotnet_issue22.cs \
 rdotnet_issue33.cs \
 rdotnet_testcp.cs \
 rdotnet_testlp.cs \
 rdotnet_testsat.cs \
 rdotnet_test_sat_model.cs

.PHONY: test_dotnet_examples # Build and Run all .Net Examples (located in examples/dotnet)
test_dotnet_examples: \
 test_dotnet_examples_csharp \
 test_dotnet_examples_fsharp

.PHONY: test_dotnet_examples_csharp # Build and Run all CSharp Examples (located in examples/dotnet)
test_dotnet_examples_csharp: \
 rdotnet_3_jugs_regular.cs \
 rdotnet_alldifferent_except_0.cs \
 rdotnet_all_interval.cs \
 rdotnet_a_puzzle.cs \
 rdotnet_a_round_of_golf.cs \
 rdotnet_assignment.cs \
 rdotnet_broken_weights.cs \
 rdotnet_bus_schedule.cs \
 rdotnet_circuit2.cs \
 rdotnet_circuit.cs \
 rdotnet_coins3.cs \
 rdotnet_combinatorial_auction2.cs \
 rdotnet_contiguity_regular.cs \
 rdotnet_contiguity_transition.cs \
 rdotnet_costas_array.cs \
 rdotnet_covering_opl.cs \
 rdotnet_crew.cs \
 rdotnet_crossword.cs \
 rdotnet_crypta.cs \
 rdotnet_crypto.cs \
 rdotnet_cscvrptw.cs \
 rdotnet_csflow.cs \
 rdotnet_csintegerprogramming.cs \
 rdotnet_csjobshop.cs \
 rdotnet_csknapsack.cs \
 rdotnet_cslinearprogramming.cs \
 rdotnet_csls_api.cs \
 rdotnet_csrabbitspheasants.cs \
 rdotnet_cstsp.cs \
 rdotnet_curious_set_of_integers.cs \
 rdotnet_debruijn.cs \
 rdotnet_csdiet.cs \
 rdotnet_discrete_tomography.cs \
 rdotnet_divisible_by_9_through_1.cs \
 rdotnet_dudeney.cs \
 rdotnet_einav_puzzle2.cs \
 rdotnet_eq10.cs \
 rdotnet_eq20.cs \
 rdotnet_fill_a_pix.cs \
 rdotnet_furniture_moving.cs \
 rdotnet_furniture_moving_intervals.cs \
 rdotnet_futoshiki.cs \
 rdotnet_gate_scheduling_sat.cs \
 rdotnet_golomb_ruler.cs \
 rdotnet_grocery.cs \
 rdotnet_hidato_table.cs \
 rdotnet_jobshop_ft06_sat.cs \
 rdotnet_just_forgotten.cs \
 rdotnet_kakuro.cs \
 rdotnet_kenken2.cs \
 rdotnet_killer_sudoku.cs \
 rdotnet_labeled_dice.cs \
 rdotnet_langford.cs \
 rdotnet_least_diff.cs \
 rdotnet_lectures.cs \
 rdotnet_magic_sequence.cs \
 rdotnet_magic_square_and_cards.cs \
 rdotnet_magic_square.cs \
 rdotnet_map2.cs \
 rdotnet_map.cs \
 rdotnet_marathon2.cs \
 rdotnet_max_flow_taha.cs \
 rdotnet_max_flow_winston1.cs \
 rdotnet_minesweeper.cs \
 rdotnet_mr_smith.cs \
 rdotnet_nqueens.cs \
 rdotnet_nurse_rostering_regular.cs \
 rdotnet_nurse_rostering_transition.cs \
 rdotnet_nurses_sat.cs \
 rdotnet_olympic.cs \
 rdotnet_organize_day.cs \
 rdotnet_organize_day_intervals.cs \
 rdotnet_pandigital_numbers.cs \
 rdotnet_perfect_square_sequence.cs \
 rdotnet_photo_problem.cs \
 rdotnet_place_number_puzzle.cs \
 rdotnet_p_median.cs \
 rdotnet_post_office_problem2.cs \
 rdotnet_quasigroup_completion.cs \
 rdotnet_regex.cs \
 rdotnet_rogo2.cs \
 rdotnet_scheduling_speakers.cs \
 rdotnet_secret_santa2.cs \
 rdotnet_send_more_money2.cs \
 rdotnet_send_more_money.cs \
 rdotnet_send_most_money.cs \
 rdotnet_seseman.cs \
 rdotnet_set_covering2.cs \
 rdotnet_set_covering3.cs \
 rdotnet_set_covering4.cs \
 rdotnet_set_covering.cs \
 rdotnet_set_covering_deployment.cs \
 rdotnet_set_covering_skiena.cs \
 rdotnet_set_partition.cs \
 rdotnet_sicherman_dice.cs \
 rdotnet_ski_assignment.cs \
 rdotnet_slow_scheduling.cs \
 rdotnet_stable_marriage.cs \
 rdotnet_strimko2.cs \
 rdotnet_subset_sum.cs \
 rdotnet_sudoku.cs \
 rdotnet_survo_puzzle.cs \
 rdotnet_TaskScheduling.cs \
 rdotnet_techtalk_scheduling.cs \
 rdotnet_to_num.cs \
 rdotnet_traffic_lights.cs \
 rdotnet_tsp.cs \
 rdotnet_vrp.cs \
 rdotnet_volsay.cs \
 rdotnet_volsay2.cs \
 rdotnet_volsay3.cs \
 rdotnet_wedding_optimal_chart.cs \
 rdotnet_who_killed_agatha.cs \
 rdotnet_xkcd.cs \
 rdotnet_young_tableaux.cs \
 rdotnet_zebra.cs
	$(MAKE) run SOURCE=examples/dotnet/coins_grid.cs ARGS="5 2"
#	$(MAKE) rdotnet_nontransitive_dice # too long
#	$(MAKE) rdotnet_partition # too long
#	$(MAKE) rdotnet_secret_santa # too long
# $(MAKE) rdotnet_word_square # depends on /usr/share/dict/words

.PHONY: test_dotnet_examples_fsharp # Build and Run all FSharp Samples (located in examples/dotnet)
test_dotnet_examples_fsharp: \
 rdotnet_fsintegerprogramming.fs \
 rdotnet_fslinearprogramming.fs \
 rdotnet_fsdiet.fs \
 rdotnet_fsequality.fs \
 rdotnet_fsequality-inequality.fs \
 rdotnet_fsinteger-linear-program.fs \
 rdotnet_fsknapsack.fs \
 rdotnet_fsnetwork-max-flow.fs \
 rdotnet_fsnetwork-max-flow-lpSolve.fs \
 rdotnet_fsnetwork-min-cost-flow.fs \
 rdotnet_fsProgram.fs \
 rdotnet_fsrabbit-pheasant.fs \
 rdotnet_fsvolsay3.fs \
 rdotnet_fsvolsay3-lpSolve.fs \
 rdotnet_fsvolsay.fs

.PHONY: test_donet_samples # Build and Run all .Net Samples (located in ortools/*/samples)
test_dotnet_samples: \
 rdotnet_binpacking_problem.cs \
 rdotnet_bool_or_sample.cs \
 rdotnet_channeling_sample.cs \
 rdotnet_code_sample.cs \
 rdotnet_interval_sample.cs \
 rdotnet_literal_sample.cs \
 rdotnet_no_overlap_sample.cs \
 rdotnet_optional_interval_sample.cs \
 rdotnet_rabbits_and_pheasants.cs \
 rdotnet_ranking_sample.cs \
 rdotnet_reified_sample.cs \
 rdotnet_simple_solve.cs \
 rdotnet_solve_all_solutions.cs \
 rdotnet_solve_with_intermediate_solutions.cs \
 rdotnet_solve_with_time_limit.cs \
 rdotnet_stop_after_n_solutions.cs

################
##  Cleaning  ##
################
.PHONY: clean_dotnet # Clean files
clean_dotnet:
	-$(DELREC) ortools$Sdotnet$SCreateSigningKey$Sbin
	-$(DELREC) ortools$Sdotnet$SCreateSigningKey$Sobj
	-$(DEL) $(DOTNET_ORTOOLS_SNK_PATH)
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).csproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sruntime.json
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME).csproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_TESTS_ASSEMBLY_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$Sobj
	-$(DELREC) $(PACKAGE_DIR)
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*.cs
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*.cs
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.cs
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*.cs
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*.cs
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*csharp_wrap*
	-$(DEL) $(OBJ_DIR)$Sswig$S*_csharp_wrap.$O
	-$(DEL) $(LIB_DIR)$S$(OR_TOOLS_NATIVE_ASSEMBLY_NAME).*
	-$(DEL) $(BIN_DIR)$S$(OR_TOOLS_ASSEMBLY_NAME).*
	-$(DEL) $(BIN_DIR)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).*
	-$(DELREC) $(DOTNET_EX_PATH)$Sbin
	-$(DELREC) $(DOTNET_EX_PATH)$Sobj
	-$(DELREC) $(TEST_PATH)$Sbin
	-$(DELREC) $(TEST_PATH)$Sobj
	-$(DELREC) ortools$Ssat$Ssamples$Sbin
	-$(DELREC) ortools$Ssat$Ssamples$Sobj
	-$(DELREC) $(TEMP_DOTNET_DIR)
	-@"$(DOTNET_BIN)" nuget locals all --clear

######################
##  Nuget artifact  ##
######################
.PHONY: nuget_archive # Build .Net "Google.OrTools" Nuget Package
nuget_archive: dotnet | $(TEMP_DOTNET_DIR)
	"$(DOTNET_BIN)" publish -c Release --no-dependencies --no-restore -f netstandard2.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj
	"$(DOTNET_BIN)" publish -c Release --no-dependencies --no-restore -f netstandard2.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj
	"$(DOTNET_BIN)" pack -c Release \
 -o "..$S..$S..$S$(BIN_DIR)" \
 ortools$Sdotnet

.PHONY: nuget_upload # Upload Nuget Package
nuget_upload: nuget_archive
	@echo Uploading Nuget package for "netstandard2.0".
	$(warning Not Implemented)

#############
##  DEBUG  ##
#############
.PHONY: detect_dotnet # Show variables used to build dotnet OR-Tools.
detect_dotnet:
	@echo Relevant info for the dotnet build:
	@echo DOTNET_BIN = $(DOTNET_BIN)
	@echo NUGET_BIN = $(NUGET_BIN)
	@echo PROTOC = $(PROTOC)
	@echo DOTNET_ORTOOLS_SNK = $(DOTNET_ORTOOLS_SNK)
	@echo SWIG_DOTNET_LIB_SUFFIX = $(SWIG_DOTNET_LIB_SUFFIX)
	@echo OR_TOOLS_NATIVE_ASSEMBLY_NAME = $(OR_TOOLS_NATIVE_ASSEMBLY_NAME)
	@echo DOTNET_ORTOOLS_NATIVE_NUPKG = $(DOTNET_ORTOOLS_NATIVE_NUPKG)
	@echo OR_TOOLS_ASSEMBLY_NAME = $(OR_TOOLS_ASSEMBLY_NAME)
	@echo DOTNET_ORTOOLS_NUPKG = $(DOTNET_ORTOOLS_NUPKG)
	@echo OR_TOOLS_TESTS_ASSEMBLY_NAME = $(OR_TOOLS_TESTS_ASSEMBLY_NAME)
	@echo OR_TOOLS_FSHARP_ASSEMBLY_NAME = $(OR_TOOLS_FSHARP_ASSEMBLY_NAME)
	@echo DOTNET_ORTOOLS_FSHARP_NUPKG = $(DOTNET_ORTOOLS_FSHARP_NUPKG)
	@echo OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME = $(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
