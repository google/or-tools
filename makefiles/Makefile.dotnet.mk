# ---------- CSharp support using SWIG ----------
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
ORTOOLS_NUSPEC_FILE=$(ORTOOLS_DLL_NAME).nuspec


# Check for required build tools
ifeq ($(SYSTEM), win)
DOTNET_EXECUTABLE := $(shell $(WHICH) dotnet.exe 2>nul)
else # UNIX
ifeq ($(PLATFORM),MACOSX)
DOTNET_EXECUTABLE := $(shell dirname ${DOTNET_INSTALL_PATH})$Sdotnet
else # LINUX
DOTNET_EXECUTABLE := $(shell which dotnet)
endif
endif

.PHONY: csharp_dotnet # Build OrTools
csharp_dotnet: ortoolslib \
	$(GEN_DIR)/com/google/ortools/properties/CommonAssemblyInfo.cs \
	$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O \
	$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O \
	$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O \
	$(OBJ_DIR)/swig/graph_csharp_wrap.$O \
	$(OBJ_DIR)/swig/sat_csharp_wrap.$O \
	$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs \
	$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs \
	$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs \
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs \
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs \
	$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs \
	$(GEN_DIR)/com/google/ortools/sat/SatParameters.g.cs

	$(SED) -i -e "s/0.0.0.0/$(OR_TOOLS_VERSION)/" ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	"$(DOTNET_EXECUTABLE)" build ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj


$(GEN_DIR)/com/google/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt: \
	| $(GEN_DIR)/com/google/ortools/properties
	@echo $(OR_TOOLS_VERSION) > $(GEN_DIR)$Scom$Sgoogle$Sortools$Sproperties$SGitVersion$(OR_TOOLS_VERSION).txt

# See for background on Windows Explorer File Info Details:
#  https://social.msdn.microsoft.com/Forums/vstudio/en-US/27894a09-1eed-48d9-8a0f-2198388d492c/csc-modulelink-or-just-csc-dll-plus-some-external-dllobj-references
#  also, https://blogs.msdn.microsoft.com/texblog/2007/04/05/linking-native-c-into-c-applications/
$(GEN_DIR)/com/google/ortools/properties/CommonAssemblyInfo.cs: \
	$(GEN_DIR)/com/google/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt
	$(COPY) tools$Scsharp$SCommonAssemblyInfo.cs $(GEN_DIR)$Scom$Sgoogle$Sortools$Sproperties
	$(SED) -i -e "s/XXXX/$(OR_TOOLS_VERSION)/" $(GEN_DIR)$Scom$Sgoogle$Sortools$Sproperties$SCommonAssemblyInfo.cs


$(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/linear_solver/csharp/linear_solver.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(LP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research_linear_solver -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).LinearSolver -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)$Sortools$Slinear_solver$Scsharp$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O: $(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/constraint_solver/csharp/routing.i \
	$(SRC_DIR)/ortools/constraint_solver/csharp/constraint_solver.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SRC_DIR)/ortools/util/csharp/functions.i \
	$(CP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research_constraint_solver -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).ConstraintSolver -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)$Sortools$Sconstraint_solver$Scsharp$Srouting.i
	$(SED) -i -e 's/CSharp_new_Solver/CSharp_new_CpSolver/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Solver/CSharp_delete_CpSolver/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Solver/CSharp_CpSolver/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_new_Constraint/CSharp_new_CpConstraint/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Constraint/CSharp_delete_CpConstraint/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Constraint/CSharp_CpConstraint/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.*

$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O: \
	$(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/algorithms/csharp/knapsack_solver.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SRC_DIR)/ortools/algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research_algorithms -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Algorithms -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Salgorithms $(SRC_DIR)$Sortools$Salgorithms$Scsharp$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O: $(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/graph/csharp/graph.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(GRAPH_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sortools$Sgraph$Sgraph_csharp_wrap.cc -module operations_research_graph -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Graph -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph $(SRC_DIR)$Sortools$Sgraph$Scsharp$Sgraph.i

$(OBJ_DIR)/swig/graph_csharp_wrap.$O: $(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sortools$Sgraph$Sgraph_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O

$(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/sat/csharp/sat.i \
	$(SRC_DIR)/ortools/sat/swig_helper.h \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SAT_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sortools$Ssat$Ssat_csharp_wrap.cc -module operations_research_sat -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Sat -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Ssat $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O


# Protobufs

$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs: $(SRC_DIR)/ortools/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs: $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs: $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs: $(SRC_DIR)/ortools/sat/cp_model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Ssat --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/com/google/ortools/sat/SatParameters.g.cs: $(SRC_DIR)/ortools/sat/sat_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Ssat --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto



.PHONY: pkg_dotnet # Build Nuget Package for distribution.
pkg_dotnet:
	$(warning Not Implemented)

.PHONY: pkg_dotnet-upload # Upload Nuget Package
nuget-pkg_dotnet-upload: nuget_archive
	$(warning Not Implemented)


.PHONY: detect_dotnet # Show variables used to build dotnet OR-Tools.
detect_dotnet:
ifeq ($(SYSTEM),win)
	@echo Relevant info for the dotnet build:
else
	@echo Relevant info for the dotnet build:
endif
	@echo DOTNET_EXECUTABLE = "$(DOTNET_EXECUTABLE)"
	@echo MONO_EXECUTABLE = "$(MONO_EXECUTABLE)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
