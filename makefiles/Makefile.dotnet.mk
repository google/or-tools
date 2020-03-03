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
dotnet: dotnet_csharp dotnet_fsharp
check_dotnet: check_dotnet_pimpl
test_dotnet: test_dotnet_pimpl
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

# All libraries and dependencies
DOTNET_ORTOOLS_SNK := $(SRC_DIR)/ortools/dotnet/or-tools.snk
DOTNET_ORTOOLS_SNK_PATH := $(subst /,$S,$(DOTNET_ORTOOLS_SNK))
OR_TOOLS_ASSEMBLY_NAME := Google.OrTools
OR_TOOLS_RUNTIME_ASSEMBLY_NAME := $(OR_TOOLS_ASSEMBLY_NAME).runtime.$(RUNTIME_IDENTIFIER)
OR_TOOLS_NATIVE_LIB := google-ortools-native
OR_TOOLS_FSHARP_ASSEMBLY_NAME := $(OR_TOOLS_ASSEMBLY_NAME).FSharp
OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME := $(OR_TOOLS_ASSEMBLY_NAME).FSharp.Tests
DOTNET_ORTOOLS_NUPKG := $(PACKAGE_DIR)/$(OR_TOOLS_ASSEMBLY_NAME).$(OR_TOOLS_VERSION).nupkg
DOTNET_ORTOOLS_NATIVE_NUPKG := $(PACKAGE_DIR)/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).$(OR_TOOLS_VERSION).nupkg
DOTNET_ORTOOLS_FSHARP_NUPKG := $(PACKAGE_DIR)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).$(OR_TOOLS_VERSION).nupkg
NUGET_PACK_ARGS := -c Release
DOTNET_BUILD_ARGS := -c Release /p:Platform=x64

######################
##  RUNTIME CSHARP  ##
######################
.PHONY: dotnet_runtime # Build C# runtime OR-Tools
dotnet_runtime: $(DOTNET_ORTOOLS_NATIVE_NUPKG)

###################################################################
# Protobuf: generate C# cs files from proto specification files. ##
###################################################################
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

$(GEN_DIR)/ortools/util/OptionalBoolean.pb.cs: \
 $(SRC_DIR)/ortools/util/optional_boolean.proto \
 | $(GEN_DIR)/ortools/util
	$(PROTOC) --proto_path=$(SRC_DIR) \
 --csharp_out=$(GEN_PATH)$Sortools$Sutil \
 --csharp_opt=file_extension=.pb.cs \
 $(SRC_DIR)$Sortools$Sutil$Soptional_boolean.proto

#############################################################################################
# Swig: Generate C# wrapper cs files from swig specification files and C wrapper .c files. ##
#############################################################################################
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
 -dllimport "$(OR_TOOLS_NATIVE_LIB)" \
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
 $(CP_DEPS) \
 | $(GEN_DIR)/ortools/constraint_solver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc \
 -module operations_research_constraint_solver \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).ConstraintSolver \
 -dllimport "$(OR_TOOLS_NATIVE_LIB)" \
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
 -dllimport "$(OR_TOOLS_NATIVE_LIB)" \
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
 -dllimport "$(OR_TOOLS_NATIVE_LIB)" \
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
 $(SAT_DEPS) \
 | $(GEN_DIR)/ortools/sat $(GEN_DIR)/ortools/sat
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 -module operations_research_sat \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).Sat \
 -dllimport "$(OR_TOOLS_NATIVE_LIB)" \
 -outdir $(GEN_PATH)$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O

$(GEN_DIR)/ortools/util/sorted_interval_list_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/sorted_interval_list.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(UTIL_DEPS) \
 |  $(GEN_DIR)/ortools/util
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_PATH)$Sortools$Sutil$Ssorted_interval_list_csharp_wrap.cc \
 -module operations_research_util \
 -namespace $(OR_TOOLS_ASSEMBLY_NAME).Util \
 -dllimport "$(OR_TOOLS_NATIVE_LIB)" \
 -outdir $(GEN_PATH)$Sortools$Sutil \
 $(SRC_DIR)$Sortools$Sutil$Scsharp$Ssorted_interval_list.i

$(OBJ_DIR)/swig/sorted_interval_list_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/util/sorted_interval_list_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Sutil$Ssorted_interval_list_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssorted_interval_list_csharp_wrap.$O

############
# Signing ##
############
ifneq ($(DOTNET_SNK),)
$(DOTNET_ORTOOLS_SNK):
	$(COPY) $(DOTNET_SNK) $(DOTNET_ORTOOLS_SNK_PATH)
else
$(DOTNET_ORTOOLS_SNK): ortools/dotnet/CreateSigningKey/CreateSigningKey.csproj
	"$(DOTNET_BIN)" run --project ortools$Sdotnet$SCreateSigningKey$SCreateSigningKey.csproj $S$(DOTNET_ORTOOLS_SNK_PATH)
endif

##########################################
# Swig: Generate native wrapper binary. ##
##########################################
$(LIB_DIR)/$(OR_TOOLS_NATIVE_LIB).$(SWIG_DOTNET_LIB_SUFFIX): \
 $(OR_TOOLS_LIBS) \
 $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/sat_csharp_wrap.$O \
 $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/graph_csharp_wrap.$O \
 $(OBJ_DIR)/swig/sorted_interval_list_csharp_wrap.$O \
 | $(LIB_DIR)
	$(DYNAMIC_LD) \
 $(LD_OUT)$(LIB_DIR)$S$(OR_TOOLS_NATIVE_LIB).$(SWIG_DOTNET_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O \
 $(OBJ_DIR)/swig/sorted_interval_list_csharp_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

$(SRC_DIR)/ortools/dotnet/orLogo.png: $(SRC_DIR)/tools/doc/orLogo.png
	$(COPY) $(SRC_DIR)$Stools$Sdoc$SorLogo.png $(SRC_DIR)$Sortools$Sdotnet$SorLogo.png

$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj

$(DOTNET_ORTOOLS_NATIVE_NUPKG): \
 $(LIB_DIR)/$(OR_TOOLS_NATIVE_LIB).$(SWIG_DOTNET_LIB_SUFFIX) \
 $(SRC_DIR)/ortools/dotnet/orLogo.png \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)/$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj \
 $(SRC_DIR)/ortools/constraint_solver/csharp/IntVarArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/IntervalVarArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/NetDecisionBuilder.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/SolverHelper.cs \
 $(SRC_DIR)/ortools/constraint_solver/csharp/ValCstPair.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/LinearExpr.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/LinearConstraint.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/SolverHelper.cs \
 $(SRC_DIR)/ortools/linear_solver/csharp/VariableHelper.cs \
 $(SRC_DIR)/ortools/sat/csharp/Constraints.cs \
 $(SRC_DIR)/ortools/sat/csharp/CpModel.cs \
 $(SRC_DIR)/ortools/sat/csharp/CpSolver.cs \
 $(SRC_DIR)/ortools/sat/csharp/IntegerExpressions.cs \
 $(SRC_DIR)/ortools/sat/csharp/IntervalVariables.cs \
 $(SRC_DIR)/ortools/sat/csharp/SearchHelpers.cs \
 $(SRC_DIR)/ortools/util/csharp/NestedArrayHelper.cs \
 $(SRC_DIR)/ortools/util/csharp/ProtoHelper.cs \
 $(GEN_DIR)/ortools/constraint_solver/SearchLimit.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/SolverParameters.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/RoutingParameters.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/RoutingEnums.pb.cs \
 $(GEN_DIR)/ortools/sat/CpModel.pb.cs \
 $(GEN_DIR)/ortools/util/OptionalBoolean.pb.cs \
 | $(DOTNET_ORTOOLS_SNK) $(PACKAGE_DIR)
	"$(DOTNET_BIN)" build $(DOTNET_BUILD_ARGS) ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj
	"$(DOTNET_BIN)" pack $(NUGET_PACK_ARGS) ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME).csproj

##############
##  CSHARP  ##
##############
.PHONY: dotnet_csharp # Build C# OR-Tools
dotnet_csharp: $(DOTNET_ORTOOLS_NUPKG)

#  create csproj for managed project
$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/$(OR_TOOLS_ASSEMBLY_NAME).csproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/$(OR_TOOLS_ASSEMBLY_NAME).csproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj

# Pack managed project nuget
$(DOTNET_ORTOOLS_NUPKG): \
 $(DOTNET_ORTOOLS_NATIVE_NUPKG) \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_ASSEMBLY_NAME)/$(OR_TOOLS_ASSEMBLY_NAME).csproj \
 | $(DOTNET_ORTOOLS_SNK) $(PACKAGE_DIR)
	"$(DOTNET_BIN)" build $(DOTNET_BUILD_ARGS) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" pack $(NUGET_PACK_ARGS) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)

##############
##  FSHARP  ##
##############
.PHONY: dotnet_fsharp # Build F# OR-Tools
dotnet_fsharp: $(DOTNET_ORTOOLS_FSHARP_NUPKG)

# create fsproj
$(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj: \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj

# build and pack nuget
$(DOTNET_ORTOOLS_FSHARP_NUPKG): \
 $(DOTNET_ORTOOLS_NUPKG) \
 $(SRC_DIR)/ortools/dotnet/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj \
 | $(DOTNET_ORTOOLS_SNK) $(PACKAGE_DIR)
	"$(DOTNET_BIN)" build $(DOTNET_BUILD_ARGS) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" pack $(NUGET_PACK_ARGS) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)

# create test fsproj
ortools/dotnet/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj: \
 ortools/dotnet/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj.in \
 > ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj

.PHONY: test_dotnet_fsharp # Run F# OrTools Tests
test_dotnet_fsharp: $(DOTNET_ORTOOLS_FSHARP_NUPKG) \
 ortools/dotnet/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)/$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME).fsproj
	"$(DOTNET_BIN)" build $(DOTNET_BUILD_ARGS) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)
	"$(DOTNET_BIN)" test $(DOTNET_BUILD_ARGS) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)

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

.PHONY: run # Run a .Net C# program (using 'dotnet run').
run: build
	"$(DOTNET_BIN)" run --no-build $(ARGS) --project $(SOURCE_PATH)proj

.PHONY: run_test # Run a .Net C# program (using 'dotnet test').
run_test: build
	"$(DOTNET_BIN)" test --blame --no-build $(ARGS) $(SOURCE_PATH)proj
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
	"$(DOTNET_BIN)" run --no-build --project $(SOURCE_PATH)proj -- $(ARGS)
endif

#############################
##  .NET Examples/Samples  ##
#############################
.PHONY: test_dotnet_algorithms_samples # Build and Run all .Net LP Samples (located in ortools/algorithms/samples)
test_dotnet_algorithms_samples:
	$(MAKE) run SOURCE=ortools/algorithms/samples/Knapsack.cs

.PHONY: test_dotnet_constraint_solver_samples # Build and Run all .Net CP Samples (located in ortools/constraint_solver/samples)
test_dotnet_constraint_solver_samples:
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/SimpleCpProgram.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/SimpleRoutingProgram.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/Tsp.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/TspCities.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/TspCircuitBoard.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/TspDistanceMatrix.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/Vrp.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpCapacity.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpDropNodes.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpGlobalSpan.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpInitialRoutes.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpPickupDelivery.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpPickupDeliveryFifo.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpPickupDeliveryLifo.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpResources.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpStartsEnds.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpTimeWindows.cs
	$(MAKE) run SOURCE=ortools/constraint_solver/samples/VrpWithTimeLimit.cs

.PHONY: test_dotnet_graph_samples # Build and Run all .Net LP Samples (located in ortools/graph/samples)
test_dotnet_graph_samples: ;
	$(MAKE) run SOURCE=ortools/graph/samples/SimpleMaxFlowProgram.cs
	$(MAKE) run SOURCE=ortools/graph/samples/SimpleMinCostFlowProgram.cs

.PHONY: test_dotnet_linear_solver_samples # Build and Run all .Net LP Samples (located in ortools/linear_solver/samples)
test_dotnet_linear_solver_samples:
	$(MAKE) run SOURCE=ortools/linear_solver/samples/SimpleLpProgram.cs
	$(MAKE) run SOURCE=ortools/linear_solver/samples/SimpleMipProgram.cs

.PHONY: test_dotnet_sat_samples # Build and Run all .Net SAT Samples (located in ortools/sat/samples)
test_dotnet_sat_samples:
	$(MAKE) run SOURCE=ortools/sat/samples/BinPackingProblemSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/BoolOrSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/ChannelingSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/CpIsFunSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/EarlinessTardinessCostSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/IntervalSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/LiteralSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/NoOverlapSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/OptionalIntervalSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/RabbitsAndPheasantsSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/RankingSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/ReifiedSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/SearchForAllSolutionsSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/SimpleSatProgram.cs
	$(MAKE) run SOURCE=ortools/sat/samples/SolutionHintingSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/SolveAndPrintIntermediateSolutionsSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/SolveWithTimeLimitSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/StepFunctionSampleSat.cs
	$(MAKE) run SOURCE=ortools/sat/samples/StopAfterNSolutionsSampleSat.cs

.PHONY: check_dotnet_pimpl
check_dotnet_pimpl: \
 test_dotnet_algorithms_samples \
 test_dotnet_constraint_solver_samples \
 test_dotnet_graph_samples \
 test_dotnet_linear_solver_samples \
 test_dotnet_sat_samples \

.PHONY: test_dotnet_tests # Build and Run all .Net Tests (located in examples/test)
test_dotnet_tests:
	$(MAKE) run_test SOURCE=examples/tests/LinearSolverTests.cs
	$(MAKE) run_test SOURCE=examples/tests/ConstraintSolverTests.cs
	$(MAKE) run_test SOURCE=examples/tests/RoutingSolverTests.cs
	$(MAKE) run_test SOURCE=examples/tests/SatSolverTests.cs
	$(MAKE) run_test SOURCE=examples/tests/issue18.cs
	$(MAKE) run_test SOURCE=examples/tests/issue22.cs
	$(MAKE) run_test SOURCE=examples/tests/issue33.cs

.PHONY: test_dotnet_contrib # Build and Run all .Net Examples (located in examples/contrib)
test_dotnet_contrib:
	$(MAKE) run SOURCE=examples/contrib/3_jugs_regular.cs
	$(MAKE) run SOURCE=examples/contrib/a_puzzle.cs
	$(MAKE) run SOURCE=examples/contrib/a_round_of_golf.cs
	$(MAKE) run SOURCE=examples/contrib/all_interval.cs
	$(MAKE) run SOURCE=examples/contrib/alldifferent_except_0.cs
	$(MAKE) run SOURCE=examples/contrib/assignment.cs
	$(MAKE) run SOURCE=examples/contrib/broken_weights.cs
	$(MAKE) run SOURCE=examples/contrib/bus_schedule.cs
	$(MAKE) run SOURCE=examples/contrib/circuit.cs
	$(MAKE) run SOURCE=examples/contrib/circuit2.cs
	$(MAKE) run SOURCE=examples/contrib/coins3.cs
	$(MAKE) run SOURCE=examples/contrib/coins_grid.cs ARGS="5 2"
	$(MAKE) run SOURCE=examples/contrib/combinatorial_auction2.cs
	$(MAKE) run SOURCE=examples/contrib/contiguity_regular.cs
	$(MAKE) run SOURCE=examples/contrib/contiguity_transition.cs
	$(MAKE) run SOURCE=examples/contrib/costas_array.cs
	$(MAKE) run SOURCE=examples/contrib/covering_opl.cs
	$(MAKE) run SOURCE=examples/contrib/crew.cs
	$(MAKE) run SOURCE=examples/contrib/crossword.cs
	$(MAKE) run SOURCE=examples/contrib/crypta.cs
	$(MAKE) run SOURCE=examples/contrib/crypto.cs
	$(MAKE) run SOURCE=examples/contrib/csdiet.cs
	$(MAKE) run SOURCE=examples/contrib/curious_set_of_integers.cs
	$(MAKE) run SOURCE=examples/contrib/debruijn.cs
	$(MAKE) run SOURCE=examples/contrib/discrete_tomography.cs
	$(MAKE) run SOURCE=examples/contrib/divisible_by_9_through_1.cs
	$(MAKE) run SOURCE=examples/contrib/dudeney.cs
	$(MAKE) run SOURCE=examples/contrib/einav_puzzle2.cs
	$(MAKE) run SOURCE=examples/contrib/eq10.cs
	$(MAKE) run SOURCE=examples/contrib/eq20.cs
	$(MAKE) run SOURCE=examples/contrib/fill_a_pix.cs
	$(MAKE) run SOURCE=examples/contrib/furniture_moving.cs
	$(MAKE) run SOURCE=examples/contrib/furniture_moving_intervals.cs
	$(MAKE) run SOURCE=examples/contrib/futoshiki.cs
	$(MAKE) run SOURCE=examples/contrib/golomb_ruler.cs
	$(MAKE) run SOURCE=examples/contrib/grocery.cs
	$(MAKE) run SOURCE=examples/contrib/hidato_table.cs
	$(MAKE) run SOURCE=examples/contrib/just_forgotten.cs
	$(MAKE) run SOURCE=examples/contrib/kakuro.cs
	$(MAKE) run SOURCE=examples/contrib/kenken2.cs
	$(MAKE) run SOURCE=examples/contrib/killer_sudoku.cs
	$(MAKE) run SOURCE=examples/contrib/labeled_dice.cs
	$(MAKE) run SOURCE=examples/contrib/langford.cs
	$(MAKE) run SOURCE=examples/contrib/least_diff.cs
	$(MAKE) run SOURCE=examples/contrib/lectures.cs
	$(MAKE) run SOURCE=examples/contrib/magic_sequence.cs
	$(MAKE) run SOURCE=examples/contrib/magic_square.cs
	$(MAKE) run SOURCE=examples/contrib/magic_square_and_cards.cs
	$(MAKE) run SOURCE=examples/contrib/map.cs
	$(MAKE) run SOURCE=examples/contrib/map2.cs
	$(MAKE) run SOURCE=examples/contrib/marathon2.cs
	$(MAKE) run SOURCE=examples/contrib/max_flow_taha.cs
	$(MAKE) run SOURCE=examples/contrib/max_flow_winston1.cs
	$(MAKE) run SOURCE=examples/contrib/minesweeper.cs
	$(MAKE) run SOURCE=examples/contrib/mr_smith.cs
	$(MAKE) run SOURCE=examples/contrib/nqueens.cs
	$(MAKE) run SOURCE=examples/contrib/nurse_rostering_regular.cs
	$(MAKE) run SOURCE=examples/contrib/nurse_rostering_transition.cs
	$(MAKE) run SOURCE=examples/contrib/olympic.cs
	$(MAKE) run SOURCE=examples/contrib/organize_day.cs
	$(MAKE) run SOURCE=examples/contrib/organize_day_intervals.cs
	$(MAKE) run SOURCE=examples/contrib/p_median.cs
	$(MAKE) run SOURCE=examples/contrib/pandigital_numbers.cs
	$(MAKE) run SOURCE=examples/contrib/perfect_square_sequence.cs
	$(MAKE) run SOURCE=examples/contrib/photo_problem.cs
	$(MAKE) run SOURCE=examples/contrib/place_number_puzzle.cs
	$(MAKE) run SOURCE=examples/contrib/post_office_problem2.cs
	$(MAKE) run SOURCE=examples/contrib/quasigroup_completion.cs
	$(MAKE) run SOURCE=examples/contrib/regex.cs
	$(MAKE) run SOURCE=examples/contrib/rogo2.cs
	$(MAKE) run SOURCE=examples/contrib/scheduling_speakers.cs
	$(MAKE) run SOURCE=examples/contrib/secret_santa2.cs
	$(MAKE) run SOURCE=examples/contrib/send_more_money.cs
	$(MAKE) run SOURCE=examples/contrib/send_more_money2.cs
	$(MAKE) run SOURCE=examples/contrib/send_most_money.cs
	$(MAKE) run SOURCE=examples/contrib/seseman.cs
	$(MAKE) run SOURCE=examples/contrib/set_covering.cs
	$(MAKE) run SOURCE=examples/contrib/set_covering2.cs
	$(MAKE) run SOURCE=examples/contrib/set_covering3.cs
	$(MAKE) run SOURCE=examples/contrib/set_covering4.cs
	$(MAKE) run SOURCE=examples/contrib/set_covering_deployment.cs
	$(MAKE) run SOURCE=examples/contrib/set_covering_skiena.cs
	$(MAKE) run SOURCE=examples/contrib/set_partition.cs
	$(MAKE) run SOURCE=examples/contrib/sicherman_dice.cs
	$(MAKE) run SOURCE=examples/contrib/ski_assignment.cs
	$(MAKE) run SOURCE=examples/contrib/stable_marriage.cs
	$(MAKE) run SOURCE=examples/contrib/strimko2.cs
	$(MAKE) run SOURCE=examples/contrib/subset_sum.cs
	$(MAKE) run SOURCE=examples/contrib/sudoku.cs
	$(MAKE) run SOURCE=examples/contrib/survo_puzzle.cs
	$(MAKE) run SOURCE=examples/contrib/to_num.cs
	$(MAKE) run SOURCE=examples/contrib/traffic_lights.cs
	$(MAKE) run SOURCE=examples/contrib/volsay.cs
	$(MAKE) run SOURCE=examples/contrib/volsay2.cs
	$(MAKE) run SOURCE=examples/contrib/volsay3.cs
	$(MAKE) run SOURCE=examples/contrib/wedding_optimal_chart.cs
	$(MAKE) run SOURCE=examples/contrib/who_killed_agatha.cs
	$(MAKE) run SOURCE=examples/contrib/xkcd.cs
	$(MAKE) run SOURCE=examples/contrib/young_tableaux.cs
	$(MAKE) run SOURCE=examples/contrib/zebra.cs
	$(MAKE) run SOURCE=examples/contrib/fsdiet.fs
	$(MAKE) run SOURCE=examples/contrib/fsequality-inequality.fs
	$(MAKE) run SOURCE=examples/contrib/fsequality.fs
	$(MAKE) run SOURCE=examples/contrib/fsinteger-linear-program.fs
	$(MAKE) run SOURCE=examples/contrib/fsintegerprogramming.fs
	$(MAKE) run SOURCE=examples/contrib/fsknapsack.fs
	$(MAKE) run SOURCE=examples/contrib/fslinearprogramming.fs
	$(MAKE) run SOURCE=examples/contrib/fsnetwork-max-flow-lpSolve.fs
	$(MAKE) run SOURCE=examples/contrib/fsnetwork-max-flow.fs
	$(MAKE) run SOURCE=examples/contrib/fsnetwork-min-cost-flow.fs
	$(MAKE) run SOURCE=examples/contrib/fsProgram.fs
	$(MAKE) run SOURCE=examples/contrib/fsrabbit-pheasant.fs
	$(MAKE) run SOURCE=examples/contrib/fsvolsay.fs
	$(MAKE) run SOURCE=examples/contrib/fsvolsay3-lpSolve.fs
	$(MAKE) run SOURCE=examples/contrib/fsvolsay3.fs
	$(MAKE) run SOURCE=examples/contrib/SimpleProgramFSharp.fs
#	$(MAKE) run SOURCE=examples/contrib/nontransitive_dice.cs # too long
#	$(MAKE) run SOURCE=examples/contrib/partition.cs # too long
#	$(MAKE) run SOURCE=examples/contrib/secret_santa.cs # too long
#	$(MAKE) run SOURCE=examples/contrib/word_square.cs # depends on /usr/share/dict/words

.PHONY: test_dotnet_dotnet # Build and Run all .Net Examples (located in examples/dotnet)
test_dotnet_dotnet:
	$(MAKE) run SOURCE=examples/dotnet/BalanceGroupSat.cs
	$(MAKE) run SOURCE=examples/dotnet/cscvrptw.cs
	$(MAKE) run SOURCE=examples/dotnet/csflow.cs
	$(MAKE) run SOURCE=examples/dotnet/csintegerprogramming.cs
	$(MAKE) run SOURCE=examples/dotnet/csknapsack.cs
	$(MAKE) run SOURCE=examples/dotnet/cslinearprogramming.cs
	$(MAKE) run SOURCE=examples/dotnet/csls_api.cs
	$(MAKE) run SOURCE=examples/dotnet/csrabbitspheasants.cs
	$(MAKE) run SOURCE=examples/dotnet/cstsp.cs
	$(MAKE) run SOURCE=examples/dotnet/GateSchedulingSat.cs
	$(MAKE) run SOURCE=examples/dotnet/JobshopFt06Sat.cs
	$(MAKE) run SOURCE=examples/dotnet/JobshopSat.cs
	$(MAKE) run SOURCE=examples/dotnet/NetworkRoutingSat.cs \
 ARGS="--clients=10 --backbones=5 --demands=10 --trafficMin=5 --trafficMax=10 --minClientDegree=2 --maxClientDegree=5 --minBackboneDegree=3 --maxBackboneDegree=5 --maxCapacity=20 --fixedChargeCost=10"
	$(MAKE) run SOURCE=examples/dotnet/NursesSat.cs
	$(MAKE) run SOURCE=examples/dotnet/ShiftSchedulingSat.cs
	$(MAKE) run SOURCE=examples/dotnet/SpeakerSchedulingSat.cs
	$(MAKE) run SOURCE=examples/dotnet/TaskSchedulingSat.cs

.PHONY: test_dotnet_pimpl
test_dotnet_pimpl: \
 check_dotnet_pimpl \
 test_dotnet_tests \
 test_dotnet_contrib \
 test_dotnet_dotnet

################
##  Cleaning  ##
################
.PHONY: clean_dotnet # Clean files
clean_dotnet:
	-$(DELREC) ortools$Sdotnet$SCreateSigningKey$Sbin
	-$(DELREC) ortools$Sdotnet$SCreateSigningKey$Sobj
	-$(DEL) $(DOTNET_ORTOOLS_SNK_PATH)
	-$(DEL) ortools$Sdotnet$SorLogo.png
	-$(DEL) ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)*.csproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)$Sobj
	-$(DEL) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME)*.csproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$Sobj
	-$(DEL) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)*.fsproj
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$Sobj
	-$(DEL) ortools$Sdotnet$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)*.fsproj
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
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*.cs
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*csharp_wrap*
	-$(DEL) $(OBJ_DIR)$Sswig$S*_csharp_wrap.$O
	-$(DEL) $(LIB_DIR)$S$(OR_TOOLS_NATIVE_LIB).*
	-$(DEL) $(BIN_DIR)$S$(OR_TOOLS_ASSEMBLY_NAME).*
	-$(DEL) $(BIN_DIR)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).*
	-$(DELREC) $(DOTNET_EX_PATH)$Sbin
	-$(DELREC) $(DOTNET_EX_PATH)$Sobj
	-$(DELREC) $(CONTRIB_EX_PATH)$Sbin
	-$(DELREC) $(CONTRIB_EX_PATH)$Sobj
	-$(DELREC) $(TEST_PATH)$Sbin
	-$(DELREC) $(TEST_PATH)$Sobj
	-$(DELREC) ortools$Salgorithms$Ssamples$Sbin
	-$(DELREC) ortools$Salgorithms$Ssamples$Sobj
	-$(DELREC) ortools$Sconstraint_solver$Ssamples$Sbin
	-$(DELREC) ortools$Sconstraint_solver$Ssamples$Sobj
	-$(DELREC) ortools$Sgraph$Ssamples$Sbin
	-$(DELREC) ortools$Sgraph$Ssamples$Sobj
	-$(DELREC) ortools$Slinear_solver$Ssamples$Sbin
	-$(DELREC) ortools$Slinear_solver$Ssamples$Sobj
	-$(DELREC) ortools$Ssat$Ssamples$Sbin
	-$(DELREC) ortools$Ssat$Ssamples$Sobj
	-$(DELREC) $(TEMP_DOTNET_DIR)
	-@"$(DOTNET_BIN)" nuget locals all --clear

######################
##  Nuget artifact  ##
######################
.PHONY: nuget_archive # Build .Net "Google.OrTools" Nuget Package
nuget_archive: dotnet | $(TEMP_DOTNET_DIR)
	"$(DOTNET_BIN)" publish $(DOTNET_BUILD_ARGS) --no-build --no-dependencies --no-restore -f netstandard2.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(OR_TOOLS_ASSEMBLY_NAME)$S$(OR_TOOLS_ASSEMBLY_NAME).csproj
	"$(DOTNET_BIN)" publish $(DOTNET_BUILD_ARGS) --no-build --no-dependencies --no-restore -f netstandard2.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME)$S$(OR_TOOLS_FSHARP_ASSEMBLY_NAME).fsproj
	"$(DOTNET_BIN)" pack -c Release $(NUGET_PACK_ARGS) --no-build \
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
	@echo DOTNET_SNK = $(DOTNET_SNK)
	@echo DOTNET_ORTOOLS_SNK = $(DOTNET_ORTOOLS_SNK)
	@echo SWIG_BINARY = $(SWIG_BINARY)
	@echo SWIG_INC = $(SWIG_INC)
	@echo SWIG_DOTNET_LIB_SUFFIX = $(SWIG_DOTNET_LIB_SUFFIX)
	@echo OR_TOOLS_NATIVE_LIB = $(OR_TOOLS_NATIVE_LIB)
	@echo OR_TOOLS_RUNTIME_ASSEMBLY_NAME =$(OR_TOOLS_RUNTIME_ASSEMBLY_NAME)
	@echo DOTNET_ORTOOLS_NATIVE_NUPKG = $(DOTNET_ORTOOLS_NATIVE_NUPKG)
	@echo OR_TOOLS_ASSEMBLY_NAME = $(OR_TOOLS_ASSEMBLY_NAME)
	@echo DOTNET_ORTOOLS_NUPKG = $(DOTNET_ORTOOLS_NUPKG)
	@echo OR_TOOLS_FSHARP_ASSEMBLY_NAME = $(OR_TOOLS_FSHARP_ASSEMBLY_NAME)
	@echo DOTNET_ORTOOLS_FSHARP_NUPKG = $(DOTNET_ORTOOLS_FSHARP_NUPKG)
	@echo OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME = $(OR_TOOLS_FSHARP_TESTS_ASSEMBLY_NAME)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
