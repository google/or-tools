# ---------- dotnet support using SWIG ----------
.PHONY: help_dotnet # Generate list of dotnet targets with descriptions.
help_dotnet:
	@echo Use one of the following dotnet targets:
ifeq ($(SYSTEM),win)
	@tools\grep.exe "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.dotnet.mk | tools\sed.exe "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.dotnet.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

ORTOOLS_DLL_NAME=OrTools
ORTOOLS_DLL_TEST=$(ORTOOLS_DLL_NAME).Tests

FSHARP_ORTOOLS_DLL_NAME=$(ORTOOLS_DLL_NAME).FSharp
FSHARP_ORTOOLS_DLL_TEST=$(ORTOOLS_DLL_NAME).FSharp.Tests

ORTOOLS_NUSPEC_FILE=$(ORTOOLS_DLL_NAME).nuspec

CLR_PROTOBUF_DLL_NAME?=Google.Protobuf
CLR_ORTOOLS_DLL_NAME?=Google.$(ORTOOLS_DLL_NAME)
BASE_CLR_ORTOOLS_DLL_NAME:=$(CLR_ORTOOLS_DLL_NAME)
CLR_ORTOOLS_IMPORT_DLL_NAME:=$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME)

# relative to the project root folder
DOTNET_ORTOOLS_TEST_DIR=dotnet-test
DOTNET_BUILD_DIR=build

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

DOTNET_LIB_DIR :=
ifeq ($(PLATFORM),MACOSX)
DOTNET_LIB_DIR = DYLD_LIBRARY_PATH="$(LIB_DIR):$(DYLD_LIBRARY_PATH)"
endif
ifeq ($(PLATFORM),LINUX)
DOTNET_LIB_DIR = LD_LIBRARY_PATH="$(LIB_DIR):$(LD_LIBRARY_PATH)"
endif

CLEAN_FILES=$(CLR_PROTOBUF_DLL_NAME).* $(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME).* $(CLR_ORTOOLS_DLL_NAME).*

.PHONY: csharp_dotnet # Build C# OR-Tools
csharp_dotnet: ortoolslibs csharportools

# Assembly Info
$(GEN_DIR)/com/google/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt: \
 | $(GEN_DIR)/com/google/ortools/properties
	@echo $(OR_TOOLS_VERSION) > $(GEN_DIR)$Scom$Sgoogle$Sortools$Sproperties$SGitVersion$(OR_TOOLS_VERSION).txt

# csharp ortools
csharportools: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL)

# Auto-generated code
$(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL): tools/$(CLR_PROTOBUF_DLL_NAME)$(DLL)
	$(COPY) tools$S$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(BIN_DIR)

$(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/csharp/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(LP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc \
 -module operations_research_linear_solver \
 -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).LinearSolver \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" \
 -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver \
 $(SRC_DIR)$Sortools$Slinear_solver$Scsharp$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) \
 -c $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/constraint_solver/csharp/routing.i \
 $(SRC_DIR)/ortools/constraint_solver/csharp/constraint_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(SRC_DIR)/ortools/util/csharp/functions.i \
 $(CP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc \
 -module operations_research_constraint_solver \
 -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).ConstraintSolver \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" \
 -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Scsharp$Srouting.i
	$(SED) -i -e 's/CSharp_new_Solver/CSharp_new_CpSolver/g' \
 $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Solver/CSharp_delete_CpSolver/g' \
 $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Solver/CSharp_CpSolver/g' \
 $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_new_Constraint/CSharp_new_CpConstraint/g' \
 $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Constraint/CSharp_delete_CpConstraint/g' \
 $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Constraint/CSharp_CpConstraint/g' \
 $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*cs \
 $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.*

$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) \
 -c $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/algorithms/csharp/knapsack_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc \
 -module operations_research_algorithms \
 -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Algorithms \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" \
 -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Salgorithms \
 $(SRC_DIR)$Sortools$Salgorithms$Scsharp$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) \
 -c $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/graph/csharp/graph.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(GRAPH_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_DIR)$Sortools$Sgraph$Sgraph_csharp_wrap.cc \
 -module operations_research_graph \
 -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Graph \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" \
 -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph \
 $(SRC_DIR)$Sortools$Sgraph$Scsharp$Sgraph.i

$(OBJ_DIR)/swig/graph_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) \
 -c $(GEN_DIR)$Sortools$Sgraph$Sgraph_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O

$(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc: \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/sat/csharp/sat.i \
 $(SRC_DIR)/ortools/sat/swig_helper.h \
 $(SRC_DIR)/ortools/util/csharp/proto.i \
 $(SAT_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp \
 -o $(GEN_DIR)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 -module operations_research_sat \
 -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Sat \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" \
 -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc
	$(CCC) $(CFLAGS) \
 -c $(GEN_DIR)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O

# Protobufs
$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs: $(SRC_DIR)/ortools/constraint_solver/search_limit.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs: $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs: $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs: $(SRC_DIR)/ortools/sat/cp_model.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Ssat --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/com/google/ortools/sat/SatParameters.g.cs: $(SRC_DIR)/ortools/sat/sat_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Ssat --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

$(CLR_KEYFILE):
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
 $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME).$(SWIG_LIB_SUFFIX) \
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
fsharp_dotnet:
	"$(DOTNET_EXECUTABLE)" build -c Debug ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
ifeq ($(SYSTEM),win)
	$(COPY) ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$Sbin$Sx64$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
else
	$(COPY) ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$Sbin$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
endif

.PHONY: clean_dotnet # Clean files
clean_dotnet:
	-$(DELREC) $(GEN_DIR)
	-$(DELREC) .$S$(DOTNET_ORTOOLS_TEST_DIR)
	-$(foreach var,$(CLEAN_FILES),$(DEL) $(BIN_DIR)$S$(var)$(CMDSEP))

.PHONY: test_dotnet # Test dotnet version of OR-Tools
test_dotnet: csharp_dotnet fsharp_dotnet
	"$(DOTNET_EXECUTABLE)" restore --packages "ortools$Sdotnet$Spackages" "ortools$Sdotnet$S$(ORTOOLS_DLL_TEST)$S$(ORTOOLS_DLL_TEST).csproj"
	$(MKDIR_P) .$S$(DOTNET_ORTOOLS_TEST_DIR)
	"$(DOTNET_EXECUTABLE)" clean "ortools$Sdotnet$S$(ORTOOLS_DLL_TEST)$S$(ORTOOLS_DLL_TEST).csproj"
	"$(DOTNET_EXECUTABLE)" build -o "..$S..$S..$S$(DOTNET_ORTOOLS_TEST_DIR)" "ortools$Sdotnet$S$(ORTOOLS_DLL_TEST)$S$(ORTOOLS_DLL_TEST).csproj"
	"$(DOTNET_EXECUTABLE)" clean "ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_TEST)$S$(FSHARP_ORTOOLS_DLL_TEST).fsproj"
	"$(DOTNET_EXECUTABLE)" build -o "..$S..$S..$S$(DOTNET_ORTOOLS_TEST_DIR)" "ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_TEST)$S$(FSHARP_ORTOOLS_DLL_TEST).fsproj"
#	$(COPY) $(LIB_DIR)$S*.* .$S$(DOTNET_ORTOOLS_TEST_DIR)
ifeq ($(SYSTEM),win)
	$(DOTNET_LIB_DIR) "$(DOTNET_EXECUTABLE)" \
 "ortools$Sdotnet$Spackages$Sxunit.runner.console$S2.3.1$Stools$Snetcoreapp2.0$Sxunit.console.dll" \
 ".$S$(DOTNET_ORTOOLS_TEST_DIR)$SGoogle.$(ORTOOLS_DLL_TEST).dll" \
 ".$S$(DOTNET_ORTOOLS_TEST_DIR)$SGoogle.$(FSHARP_ORTOOLS_DLL_TEST).dll" -verbose
else
	$(DOTNET_LIB_DIR) "$(DOTNET_EXECUTABLE)" \
 "ortools$Sdotnet$Spackages$Sxunit.runner.console$S2.3.1$Stools$Snetcoreapp2.0$Sxunit.console.dll" \
 ".$S$(DOTNET_ORTOOLS_TEST_DIR)$SGoogle.$(ORTOOLS_DLL_TEST).dll" \
 ".$S$(DOTNET_ORTOOLS_TEST_DIR)$SGoogle.$(FSHARP_ORTOOLS_DLL_TEST).dll" -verbose
endif


.PHONY: dotnet # Build OrTools for .NET
dotnet: test_dotnet
	$(SED) -i -e "s/<Version>.*<\/Version>/<Version>$(OR_TOOLS_VERSION)<\/Version>/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	$(SED) -i -e "s/<AssemblyVersion>.*<\/AssemblyVersion>/<AssemblyVersion>$(OR_TOOLS_VERSION)<\/AssemblyVersion>/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	$(SED) -i -e "s/<FileVersion>.*<\/FileVersion>/<FileVersion>$(OR_TOOLS_VERSION)<\/FileVersion>/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	$(SED) -i -e "s/<Version>.*<\/Version>/<Version>$(OR_TOOLS_VERSION)<\/Version>/" ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
	$(SED) -i -e "s/<AssemblyVersion>.*<\/AssemblyVersion>/<AssemblyVersion>$(OR_TOOLS_VERSION)<\/AssemblyVersion>/" ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
	$(SED) -i -e "s/<FileVersion>.*<\/FileVersion>/<FileVersion>$(OR_TOOLS_VERSION)<\/FileVersion>/" ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
	"$(DOTNET_EXECUTABLE)" build -c Release -o $(BIN_DIR) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	"$(DOTNET_EXECUTABLE)" build -c Release -o $(BIN_DIR) ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
	$(MKDIR_P) $(DOTNET_BUILD_DIR)
	"$(DOTNET_EXECUTABLE)" publish -c Release -o "..$S..$S..$S$(DOTNET_BUILD_DIR)" -f netstandard2.0 ortools$Sdotnet$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj


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
	@echo DOTNET_LIB_DIR = "$(DOTNET_LIB_DIR)"
	@echo DOTNET_EXECUTABLE = "$(DOTNET_EXECUTABLE)"
	@echo MONO_EXECUTABLE = "$(MONO_EXECUTABLE)"
	@echo NUGET_EXECUTABLE = "$(NUGET_EXECUTABLE)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
