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
CLR_KEYFILE = $(BIN_DIR)/or-tools.snk
CLR_KEYFILE_PATH = $(subst /,$S,$(CLR_KEYFILE))

# relative to the project root folder
TEMP_DOTNET_TEST_DIR=temp_dotnet_test

# Check for required build tools
DOTNET = dotnet
ifeq ($(SYSTEM),win)
DOTNET_BIN := $(shell $(WHICH) $(DOTNET) 2> NUL)
else # UNIX
DOTNET_BIN := $(shell command -v $(DOTNET) 2> /dev/null)
endif
NUGET_BIN = $(DOTNET_BIN) nuget

HAS_DOTNET = true
ifndef DOTNET_BIN
HAS_DOTNET =
endif

# Main target
.PHONY: dotnet # Build OrTools for .NET
dotnet: ortoolslibs csharp_dotnet fsharp_dotnet

BUILT_LANGUAGES +=, dotnet \(netstandard2.0\)

# Assembly Info
$(GEN_DIR)/ortools/properties:
	$(MKDIR_P) $(GEN_PATH)$Sortools$Sproperties

$(GEN_DIR)/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt: \
 | $(GEN_DIR)/ortools/properties
	@echo $(OR_TOOLS_VERSION) > $(GEN_PATH)$Sortools$Sproperties$SGitVersion$(OR_TOOLS_VERSION).txt

.PHONY: csharp_dotnet # Build C# OR-Tools
csharp_dotnet: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$D $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$D

# Auto-generated code
$(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$D: tools/dotnet/$(CLR_PROTOBUF_DLL_NAME)$D | $(BIN_DIR)
	$(COPY) tools$Sdotnet$S$(CLR_PROTOBUF_DLL_NAME)$D $(BIN_DIR)

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
 -namespace $(CLR_ORTOOLS_DLL_NAME).LinearSolver \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
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
 -namespace $(CLR_ORTOOLS_DLL_NAME).ConstraintSolver \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
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
 -namespace $(CLR_ORTOOLS_DLL_NAME).Algorithms \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
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
 -namespace $(CLR_ORTOOLS_DLL_NAME).Graph \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
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
 -namespace $(CLR_ORTOOLS_DLL_NAME).Sat \
 -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" \
 -outdir $(GEN_PATH)$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O

# Protobufs
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

$(CLR_KEYFILE): | $(BIN_DIR)
	"$(DOTNET_BIN)" run --project tools$Sdotnet$SCreateSigningKey$SCreateSigningKey.csproj $S$(CLR_KEYFILE_PATH)

$(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$D: \
 $(SRC_DIR)/ortools/dotnet/$(ORTOOLS_DLL_NAME)/$(ORTOOLS_DLL_NAME).csproj \
 $(OR_TOOLS_LIBS) \
 $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$D \
 $(CLR_KEYFILE) \
 $(SRC_DIR)/ortools/dotnet/OrTools/algorithms/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/constraint_solver/IntVarArrayHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/constraint_solver/IntervalVarArrayHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/constraint_solver/IntArrayHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/constraint_solver/NetDecisionBuilder.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/constraint_solver/SolverHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/constraint_solver/ValCstPair.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/linear_solver/DoubleArrayHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/linear_solver/LinearExpr.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/linear_solver/LinearConstraint.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/linear_solver/SolverHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/linear_solver/VariableHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/sat/CpModel.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/util/NestedArrayHelper.cs \
 $(SRC_DIR)/ortools/dotnet/OrTools/util/ProtoHelper.cs \
 $(GEN_DIR)/ortools/constraint_solver/Model.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/SearchLimit.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/SolverParameters.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/RoutingParameters.pb.cs \
 $(GEN_DIR)/ortools/constraint_solver/RoutingEnums.pb.cs \
 $(GEN_DIR)/ortools/sat/CpModel.pb.cs \
 $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/sat_csharp_wrap.$O \
 $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)/swig/graph_csharp_wrap.$O \
 | $(BIN_DIR)
	$(DYNAMIC_LD) \
 $(LD_OUT)$(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)
	"$(DOTNET_BIN)" build -c Debug ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
ifeq ($(SYSTEM),win)
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sbin$Sx64$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
else
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sbin$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
endif

$(SRC_DIR)/ortools/dotnet/$(ORTOOLS_DLL_NAME)/$(ORTOOLS_DLL_NAME).csproj: \
 $(SRC_DIR)/ortools/dotnet/$(ORTOOLS_DLL_NAME)/$(ORTOOLS_DLL_NAME).csproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj.in \
 >ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj

##############
##  FSHARP  ##
##############
.PHONY: fsharp_dotnet # Build F# OR-Tools
fsharp_dotnet: $(BIN_DIR)/$(CLR_ORTOOLS_FSHARP_DLL_NAME)$D

$(BIN_DIR)/$(CLR_ORTOOLS_FSHARP_DLL_NAME)$D: \
 $(SRC_DIR)/ortools/dotnet/$(ORTOOLS_FSHARP_DLL_NAME)/$(ORTOOLS_FSHARP_DLL_NAME).fsproj \
 $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$D \
 | $(BIN_DIR)
	"$(DOTNET_BIN)" build -c Debug ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
ifeq ($(SYSTEM),win)
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sbin$Sx64$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
else
	$(COPY) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sbin$SDebug$Snetstandard2.0$S*.* $(BIN_DIR)
endif

$(SRC_DIR)/ortools/dotnet/$(ORTOOLS_FSHARP_DLL_NAME)/$(ORTOOLS_FSHARP_DLL_NAME).fsproj: \
 $(SRC_DIR)/ortools/dotnet/$(ORTOOLS_FSHARP_DLL_NAME)/$(ORTOOLS_FSHARP_DLL_NAME).fsproj.in
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj.in \
 >ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj

.PHONY: clean_dotnet # Clean files
clean_dotnet:
	-$(DELREC) tools$Sdotnet$SCreateSigningKey$Sbin
	-$(DELREC) tools$Sdotnet$SCreateSigningKey$Sobj
	-$(DEL) $(CLR_KEYFILE_PATH)
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$Sbin
	-$(DELREC) ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$Sobj
	-$(DELREC) ortools$Sdotnet$Spackages
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
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).*
	-$(DEL) $(BIN_DIR)$S$(CLR_PROTOBUF_DLL_NAME).*
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_DLL_NAME).*
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_FSHARP_DLL_NAME).*
	-$(DELREC) $(TEMP_DOTNET_DIR)
	-$(DELREC) $(TEMP_DOTNET_TEST_DIR)

.PHONY: test_dotnet # Test dotnet version of OR-Tools
test_dotnet: dotnet
	"$(DOTNET_BIN)" restore \
 --packages "ortools$Sdotnet$Spackages" \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$S$(ORTOOLS_TEST_DLL_NAME).csproj"
	"$(DOTNET_BIN)" restore \
 --packages "ortools$Sdotnet$Spackages" \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$S$(ORTOOLS_FSHARP_TEST_DLL_NAME).fsproj"
	$(MKDIR_P) .$S$(TEMP_DOTNET_TEST_DIR)
	$(COPY) $(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) .$S$(TEMP_DOTNET_TEST_DIR)
	"$(DOTNET_BIN)" clean \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$S$(ORTOOLS_TEST_DLL_NAME).csproj"
	"$(DOTNET_BIN)" build \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)$S$(ORTOOLS_TEST_DLL_NAME).csproj"
	"$(DOTNET_BIN)" clean \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$S$(ORTOOLS_FSHARP_TEST_DLL_NAME).fsproj"
	"$(DOTNET_BIN)" build \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)$S$(ORTOOLS_FSHARP_TEST_DLL_NAME).fsproj"
	"$(DOTNET_BIN)" test \
 --no-build -v n \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_TEST_DLL_NAME)"
	"$(DOTNET_BIN)" test \
 --no-build -v n \
 -o "..$S..$S..$S$(TEMP_DOTNET_TEST_DIR)" \
 "ortools$Sdotnet$S$(ORTOOLS_FSHARP_TEST_DLL_NAME)"

#####################
##  Nuget artifact ##
#####################
TEMP_DOTNET_DIR=temp_dotnet

$(TEMP_DOTNET_DIR):
	$(MKDIR_P) $(TEMP_DOTNET_DIR)

.PHONY: nuget_archive # Build .Net "Google.OrTools" Nuget Package
nuget_archive: dotnet $(SRC_DIR)/ortools/dotnet/$(ORTOOLS_NUSPEC_FILE).in | $(TEMP_DOTNET_DIR)
	"$(DOTNET_BIN)" publish -c Release --no-dependencies --no-restore -f netstandard2.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	"$(DOTNET_BIN)" publish -c Release --no-dependencies --no-restore -f netstandard2.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
	$(SED) -e "s/MMMM/$(CLR_ORTOOLS_DLL_NAME)/" \
 ortools$Sdotnet$S$(ORTOOLS_NUSPEC_FILE).in \
 >ortools$Sdotnet$S$(ORTOOLS_NUSPEC_FILE)
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" \
 ortools$Sdotnet$S$(ORTOOLS_NUSPEC_FILE)
	"$(NUGET_BIN)" pack ortools$Sdotnet$S$(ORTOOLS_NUSPEC_FILE)

.PHONY: nuget_upload # Upload Nuget Package
nuget_upload: nuget_archive
	@echo Uploading Nuget package for "netstandard2".
	$(warning Not Implemented)

.PHONY: detect_dotnet # Show variables used to build dotnet OR-Tools.
detect_dotnet:
	@echo Relevant info for the dotnet build:
	@echo PROTOC = $(PROTOC)
	@echo DOTNET_BIN = $(DOTNET_BIN)
	@echo CLR_KEYFILE = $(CLR_KEYFILE)
	@echo SWIG_PYTHON_LIB_SUFFIX = $(SWIG_PYTHON_LIB_SUFFIX)
	@echo CLR_PROTOBUF_DLL_NAME = $(CLR_PROTOBUF_DLL_NAME)
	@echo CLR_ORTOOLS_IMPORT_DLL_NAME = $(CLR_ORTOOLS_IMPORT_DLL_NAME)
	@echo CLR_ORTOOLS_DLL_NAME = $(CLR_ORTOOLS_DLL_NAME)
	@echo CLR_ORTOOLS_FSHARP_DLL_NAME = $(CLR_ORTOOLS_FSHARP_DLL_NAME)
	@echo CLR_ORTOOLS_TEST_DLL_NAME = $(CLR_ORTOOLS_TEST_DLL_NAME)
	@echo CLR_ORTOOLS_FSHARP_TEST_DLL_NAME = $(CLR_ORTOOLS_FSHARP_TEST_DLL_NAME)
	@echo NUGET_BIN = $(NUGET_BIN)
	@echo ORTOOLS_NUSPEC_FILE = $(ORTOOLS_NUSPEC_FILE)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
