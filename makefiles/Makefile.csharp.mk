# ---------- CSharp support using SWIG ----------
.PHONY: help_csharp # Generate list of C# targets with descriptions.
help_csharp:
	@echo Use one of the following C# targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.csharp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.csharp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

# Check for required build tools
ifeq ($(SYSTEM),win)
  ifneq ($(PATH_TO_CSHARP_COMPILER),)
  CSHARP_EXECUTABLE := $(PATH_TO_CSHARP_COMPILER)
  else
  CSHARP_COMPILER ?= csc.exe
  CSHARP_EXECUTABLE := $(shell $(WHICH) $(CSHARP_COMPILER) 2>nul)
  endif
else # UNIX
  ifneq ($(PATH_TO_CSHARP_COMPILER),)
  CSHARP_EXECUTABLE := $(PATH_TO_CSHARP_COMPILER)
  else
  CSHARP_COMPILER ?= mcs
  CSHARP_EXECUTABLE := $(shell which $(CSHARP_COMPILER))
  endif
endif

.PHONY: csharp # Build C# OR-Tools.
.PHONY: test_csharp # Test C# OR-Tools using various examples.
ifneq ($(CSHARP_EXECUTABLE),)
csharp: \
	ortoolslibs \
	csharpsolution \
	csharportools \
	csharpexe
test_csharp: test_csharp_examples
BUILT_LANGUAGES +=, C\#
else
csharp:
	$@echo CSHARP_EXECUTABLE = "${CSHARP_EXECUTABLE}"
	$(warning Cannot find '$(CSHARP_COMPILER)' command which is needed for build. Please make sure it is installed and in system path.)
test_csharp: csharp
endif

.PHONY: clean_csharp # Clean C# output from previous build.
clean_csharp: clean_dotnet_generated
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_DLL_NAME)*$(DLL)
	-$(DEL) $(BIN_DIR)$S$(CLR_ORTOOLS_DLL_NAME)*.mdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME)*.lib
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME)*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME)*.exp
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME)*.netmodule
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME)*.lib
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME)*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME)*.exp
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME)*.netmodule
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)
	-$(DEL) $(addsuffix $(CLR_EXE_SUFFIX).exe, $(addprefix $(BIN_DIR)$S, $(CSHARP_EXAMPLES)))
	-$(DEL) $(addsuffix $(CLR_EXE_SUFFIX).exe, $(addprefix $(BIN_DIR)$S, $(CSHARP_TESTS)))
	-$(DEL) examples$Scsharp$SCsharp_examples.sln
	-$(DEL) examples$Scsharp$Ssolution$S*.csproj

clean_netstandard: clean_dotnet_generated
	-$(DEL) $(BIN_DIR)$S$(NETSTANDARD_ORTOOLS_DLL_NAME)*$(DLL)
	-$(DEL) $(BIN_DIR)$S$(NETSTANDARD_ORTOOLS_DLL_NAME)*.mdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(NETSTANDARD_ORTOOLS_DLL_NAME)*.lib
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(NETSTANDARD_ORTOOLS_DLL_NAME)*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(NETSTANDARD_ORTOOLS_DLL_NAME)*.exp
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(NETSTANDARD_ORTOOLS_DLL_NAME)*.dll
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(NETSTANDARD_ORTOOLS_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)

clean_dotnet_generated:
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sflatzinc$S*csharp_wrap*
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties$S*cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$S*.cs
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sutil$S*.cs
	-$(DEL) $(OBJ_DIR)$Sswig$S*csharp_wrap.$O

# Assembly Signing
# ----------------
# All C# assemblies are strongly named by default. This includes the
# DLLs and the test case EXEs.
#
# If you would like to use none strongly named DLLs,
# then please do the following :
# make clean_csharp
# remove the definition of CLR_KEYFILE from Makefile.local
# make csharp
#
# For delay signing, use for example:
# make CLR_KEYFILE="c:\full\path\to\keyfile_pub.snk" CLR_DELAYSIGN=1

ifdef CLR_KEYFILE
ifdef CLR_DELAYSIGN
SIGNING_FLAGS:= /keyfile:"$(CLR_KEYFILE)" /delaysign
else
SIGNING_FLAGS:= /keyfile:"$(CLR_KEYFILE)"
endif
endif

# The generated DLL name. Defaults to Google.OrTools
CLR_PROTOBUF_DLL_NAME?=Google.Protobuf
CLR_ORTOOLS_DLL_NAME?=Google.OrTools
NETSTANDARD_ORTOOLS_DLL_NAME?=Google.OrTools.Core
BASE_CLR_ORTOOLS_DLL_NAME:= $(CLR_ORTOOLS_DLL_NAME)
NAMESPACE_ORTOOLS:= $(BASE_CLR_ORTOOLS_DLL_NAME)
CLR_ORTOOLS_FZ_DLL_NAME?=Google.OrTools.FlatZinc
BASE_CLR_ORTOOLS_FZ_DLL_NAME:= $(CLR_ORTOOLS_FZ_DLL_NAME)
NAMESPACE_ORTOOLS_FZ:=$(BASE_CLR_ORTOOLS_DLL_NAME).Flatzinc
CLR_ORTOOLS_IMPORT_DLL_NAME:=$(CLR_ORTOOLS_DLL_NAME)
NETSTANDARD_ORTOOLS_IMPORT_DLL_NAME:=$(CLR_ORTOOLS_IMPORT_DLL_NAME).Native
NETSTANDARD_OBJ_DIR = $(OBJ_DIR)$Snetstandard
NETSTANDARD_RUNTIME_IDENTIFIER=

# NuGet specification file name
ORTOOLS_NUSPEC_NAME := or-tools.nuspec
ORTOOLS_NUGET_DIR = temp$Sor-tools
FZ_NUSPEC_NAME := fz.nuspec
FZ_NUGET_DIR = temp$Sflatzinc
# TODO: TBD: add FlatZinc variables...

# Building to DLLs named per-platform
# -----------------------------------
# If CLR_PER_PLATFORM_ASSEMBLY_NAMING is defined, the generated .NET dll will
# be named with the platform as part of the name: Google.OrTools.x86.dll
# or Google.OrTools.x64.dll. If CLR_PER_PLATFORM_ASSEMBLY_NAMING is not defined,
# the generated .NET dll will be named Google.OrTools.dll regardless of
# the platform. Note that the name is signigicant due to SWIG-generated
# [DllImport] attributes which contain the name of the DLL. Executables will
# be suffixed _x64 in 64-bit builds, no suffix in 32-bit builds.
# This is only available on windows.

ifeq ($(SYSTEM),win)
  ifdef CLR_PER_PLATFORM_ASSEMBLY_NAMING
    ifeq ($(NETPLATFORM),x64)
      CLR_ORTOOLS_DLL_NAME:=$(CLR_ORTOOLS_DLL_NAME).x64
      CLR_ORTOOLS_FZ_DLL_NAME:=$(CLR_ORTOOLS_FZ_DLL_NAME).x64
      CLR_EXE_SUFFIX?=_x64
    else
      CLR_ORTOOLS_DLL_NAME:=$(CLR_ORTOOLS_DLL_NAME).x86
      CLR_ORTOOLS_FZ_DLL_NAME:=$(CLR_ORTOOLS_FZ_DLL_NAME).x86
    endif
  endif
endif

ifeq ($(SYSTEM),win)
	NETSTANDARD_RUNTIME_IDENTIFIER=win-$(NETPLATFORM)
endif
ifeq ($(SYSTEM),unix)
  ifeq ($(PLATFORM),LINUX)
	NETSTANDARD_RUNTIME_IDENTIFIER=linux-$(NETPLATFORM)
  else
	NETSTANDARD_RUNTIME_IDENTIFIER=osx
  endif
endif

# Binaries
CSHARP_EXAMPLES = \
csknapsack \
csintegerprogramming \
cslinearprogramming \
csls_api \
csflow \
csrabbitspheasants \
cstsp \
furniture_moving_intervals \
organize_day_intervals \
techtalk_scheduling \
nurses_sat \
jobshop_ft06_sat \
cscvrptw
CSHARP_TESTS = \
testlp \
testcp \
test_sat_model

csharpexe: \
$(addsuffix $(CLR_EXE_SUFFIX).exe, $(addprefix $(BIN_DIR)/, $(CSHARP_EXAMPLES))) \
$(addsuffix $(CLR_EXE_SUFFIX).exe, $(addprefix $(BIN_DIR)/, $(CSHARP_TESTS)))


# Assembly Info

$(GEN_DIR)/com/google/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt: \
	| $(GEN_DIR)/com/google/ortools/properties
	@echo $(OR_TOOLS_VERSION) > $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SGitVersion$(OR_TOOLS_VERSION).txt

# See for background on Windows Explorer File Info Details:
#  https://social.msdn.microsoft.com/Forums/vstudio/en-US/27894a09-1eed-48d9-8a0f-2198388d492c/csc-modulelink-or-just-csc-dll-plus-some-external-dllobj-references
#  also, https://blogs.msdn.microsoft.com/texblog/2007/04/05/linking-native-c-into-c-applications/
$(GEN_DIR)/com/google/ortools/properties/CommonAssemblyInfo.cs: \
	$(GEN_DIR)/com/google/ortools/properties/GitVersion$(OR_TOOLS_VERSION).txt
	$(COPY) tools$Scsharp$SCommonAssemblyInfo.cs $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)\.\*/" $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SCommonAssemblyInfo.cs
	$(SED) -i -e "s/XXXX/$(OR_TOOLS_VERSION)\.0/" $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SCommonAssemblyInfo.cs

# TODO: TBD: it seems perhaps an AssemblyInfo build step is poised? why is it not being invoked?
$(GEN_DIR)/com/google/ortools/properties/AssemblyInfo.cs: \
	$(CLR_KEYFILE) \
	$(GEN_DIR)/com/google/ortools/properties/CommonAssemblyInfo.cs
	$(COPY) tools$Scsharp$SAssemblyInfo.cs $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties
ifdef CLR_KEYFILE
ifeq ($(SYSTEM),win)
	@echo [assembly: AssemblyKeyFile("$(CLR_KEYFILE)")] >> $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SAssemblyInfo.cs
else
	@echo "[assembly: AssemblyKeyFile(\"$(CLR_KEYFILE)\")]" >> $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SAssemblyInfo.cs
endif
endif

# csharportools

csharportools: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL)

# setup path for dotnet sdk
CANONIC_PATH_TO_DOTNET_SDK = $(subst $(SPACE),$(BACKSLASH_SPACE),$(subst \,/,$(subst \\,/,$(DOTNET_INSTALL_PATH))))
PATH_TO_DOTNET_EXE = $(CANONIC_PATH_TO_DOTNET_SDK)$Sdotnet
ifeq ($(wildcard $(CANONIC_PATH_TO_DOTNET_SDK)),)
netstandard:
	@echo "The dotnet sdk path was not set properly. Check Makefile.local for more information."

else
netstandard: CLR_ORTOOLS_IMPORT_DLL_NAME = $(NETSTANDARD_ORTOOLS_IMPORT_DLL_NAME)
netstandard: clean_dotnet_generated netstandardortools
BUILT_LANGUAGES +=, .NETSTANDARD
endif

netstandardortools: ortoolslibs $(BIN_DIR)/$(NETSTANDARD_ORTOOLS_DLL_NAME)$(DLL) $(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL)

$(NETSTANDARD_OBJ_DIR)/AssemblyInfo.cs: \
	netstandard_keyfile \
	$(GEN_DIR)/com/google/ortools/properties/CommonAssemblyInfo.cs
	$(COPY) tools$Scsharp$SAssemblyInfo.cs $(NETSTANDARD_OBJ_DIR)$SAssemblyInfo.cs
ifdef CLR_KEYFILE
	$(COPY) $(CLR_KEYFILE) $(NETSTANDARD_OBJ_DIR)$S
	@echo [assembly: AssemblyKeyFile("$(notdir $(CLR_KEYFILE))")] >> $(NETSTANDARD_OBJ_DIR)$SAssemblyInfo.cs
endif

$(BIN_DIR)/$(CLR_PROTOBUF_DLL_NAME)$(DLL): tools/$(CLR_PROTOBUF_DLL_NAME)$(DLL)
	$(COPY) tools$S$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(BIN_DIR)

$(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/linear_solver/csharp/linear_solver.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(LP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research_linear_solver -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).LinearSolver -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)$Sortools$Slinear_solver$Scsharp$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O: $(GEN_DIR)/ortools/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_PATH)/ortools/linear_solver/linear_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/constraint_solver/csharp/routing.i \
	$(SRC_DIR)/ortools/constraint_solver/csharp/constraint_solver.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SRC_DIR)/ortools/util/csharp/functions.i \
	$(CP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research_constraint_solver -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).ConstraintSolver -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)$Sortools$Sconstraint_solver$Scsharp$Srouting.i
	$(SED) -i -e 's/CSharp_new_Solver/CSharp_new_CpSolver/g' $(GEN_PATH)/com/google/ortools/constraintsolver/*cs $(GEN_PATH)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Solver/CSharp_delete_CpSolver/g' $(GEN_PATH)/com/google/ortools/constraintsolver/*cs $(GEN_PATH)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Solver/CSharp_CpSolver/g' $(GEN_PATH)/com/google/ortools/constraintsolver/*cs $(GEN_PATH)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_new_Constraint/CSharp_new_CpConstraint/g' $(GEN_PATH)/com/google/ortools/constraintsolver/*cs $(GEN_PATH)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Constraint/CSharp_delete_CpConstraint/g' $(GEN_PATH)/com/google/ortools/constraintsolver/*cs $(GEN_PATH)/ortools/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Constraint/CSharp_CpConstraint/g' $(GEN_PATH)/com/google/ortools/constraintsolver/*cs $(GEN_PATH)/ortools/constraint_solver/constraint_solver_csharp_wrap.*

$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O: \
	$(GEN_DIR)/ortools/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/algorithms/csharp/knapsack_solver.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SRC_DIR)/ortools/algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research_algorithms -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Algorithms -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms $(SRC_DIR)$Sortools$Salgorithms$Scsharp$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O: $(GEN_DIR)/ortools/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_PATH)/ortools/algorithms/knapsack_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O

$(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/graph/csharp/graph.i \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(GRAPH_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_csharp_wrap.cc -module operations_research_graph -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Graph -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph $(SRC_DIR)$Sortools$Sgraph$Scsharp$Sgraph.i

$(OBJ_DIR)/swig/graph_csharp_wrap.$O: $(GEN_DIR)/ortools/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_PATH)$Sortools$Sgraph$Sgraph_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O

$(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/sat/csharp/sat.i \
	$(SRC_DIR)/ortools/sat/swig_helper.h \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SAT_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_PATH)$Sortools$Ssat$Ssat_csharp_wrap.cc -module operations_research_sat -namespace $(BASE_CLR_ORTOOLS_DLL_NAME).Sat -dllimport "$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat $(SRC_DIR)$Sortools$Ssat$Scsharp$Ssat.i

$(OBJ_DIR)/swig/sat_csharp_wrap.$O: $(GEN_DIR)/ortools/sat/sat_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_PATH)/ortools/sat/sat_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O


# Protobufs

$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs: $(SRC_DIR)/ortools/constraint_solver/search_limit.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs: $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Smodel.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs: $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs: $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs: $(SRC_DIR)/ortools/sat/cp_model.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/com/google/ortools/sat/SatParameters.g.cs: $(SRC_DIR)/ortools/sat/sat_parameters.proto
	$(PROTOC) --proto_path=$(SRC_DIR) --csharp_out=$(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

# Main DLL

$(CLR_KEYFILE): | $(BIN_DIR)
ifdef CLR_KEYFILE
	sn -k $(CLR_KEYFILE)
endif

netstandard_keyfile:
ifdef CLR_KEYFILE
	$(PATH_TO_DOTNET_EXE) run --project tools$Snetstandard$SCreateSigningKey$SCreateSigningKey.csproj $(CLR_KEYFILE)
endif

$(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL): \
	$(GEN_DIR)/com/google/ortools/properties/AssemblyInfo.cs \
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
	$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs\
	$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs \
	$(OR_TOOLS_LIBS)
ifeq ($(SYSTEM),win)
	"$(CSHARP_EXECUTABLE)" /target:module /out:$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME).netmodule /lib:$(BIN_DIR) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) /warn:0 /nologo /debug $(GEN_PATH)\\com\\google\\ortools\\linearsolver\\*.cs $(GEN_PATH)\\com\\google\\ortools\\sat\\*.cs $(SRC_DIR)\\ortools\\com\\google\\ortools\\linearsolver\\*.cs $(GEN_PATH)\\com\\google\\ortools\\constraintsolver\\*.cs $(SRC_DIR)\\ortools\\com\\google\\ortools\\constraintsolver\\*.cs $(GEN_PATH)\\com\\google\\ortools\\algorithms\\*.cs $(SRC_DIR)\\ortools\\com\\google\\ortools\\algorithms\\*.cs $(GEN_PATH)\\com\\google\\ortools\\graph\\*.cs $(SRC_DIR)\\ortools\\com\\google\\ortools\\sat\\*.cs $(SRC_DIR)\\ortools\\com\\google\\ortools\\util\\*.cs $(GEN_PATH)\\com\\google\\ortools\\properties\\*.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LD_OUT)$(BIN_DIR)$S$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME).netmodule $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Ssat_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS)
else
	"$(CSHARP_EXECUTABLE)" /target:library /out:$(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) /lib:$(BIN_DIR) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) /warn:0 /nologo /debug $(SRC_DIR)/ortools/com/google/ortools/util/*.cs $(GEN_PATH)/com/google/ortools/linearsolver/*.cs $(SRC_DIR)/ortools/com/google/ortools/linearsolver/*.cs $(SRC_DIR)/ortools/com/google/ortools/sat/*.cs $(GEN_PATH)/com/google/ortools/sat/*.cs $(GEN_PATH)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/ortools/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/ortools/com/google/ortools/algorithms/*.cs $(GEN_PATH)/com/google/ortools/algorithms/*.cs $(GEN_PATH)/com/google/ortools/graph/*.cs $(GEN_PATH)/com/google/ortools/properties/*.cs
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O $(OBJ_DIR)/swig/sat_csharp_wrap.$O $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O $(OBJ_DIR)/swig/graph_csharp_wrap.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS)
endif

$(BIN_DIR)/$(NETSTANDARD_ORTOOLS_DLL_NAME)$(DLL): \
	$(BIN_DIR)/$(NETSTANDARD_ORTOOLS_IMPORT_DLL_NAME)$(SWIG_DOTNET_LIB_SUFFIX) \
	netstandard_create_obj_dir \
	$(NETSTANDARD_OBJ_DIR)/AssemblyInfo.cs
	$(PATH_TO_DOTNET_EXE) restore $(NETSTANDARD_OBJ_DIR)$SOrTools.NetCore.csproj
	$(PATH_TO_DOTNET_EXE) build $(NETSTANDARD_OBJ_DIR)$SOrTools.NetCore.csproj -f netstandard2.0 -o:$(realpath $(BIN_DIR))$S
	$(PATH_TO_DOTNET_EXE) pack $(NETSTANDARD_OBJ_DIR)$SOrTools.NetCore.csproj -o:$(realpath $(BIN_DIR))$S /p:PackageVersion=$(OR_TOOLS_VERSION)\;TargetRid=$(NETSTANDARD_RUNTIME_IDENTIFIER)\;NativeDllName=$(NETSTANDARD_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)

$(BIN_DIR)/$(NETSTANDARD_ORTOOLS_IMPORT_DLL_NAME)$(SWIG_DOTNET_LIB_SUFFIX): \
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
	$(SRC_DIR)/ortools/com/google/ortools/util/NestedArrayHelper.cs \
	$(SRC_DIR)/ortools/com/google/ortools/util/ProtoHelper.cs \
	$(GEN_DIR)/com/google/ortools/constraintsolver/Model.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs\
	$(GEN_DIR)/com/google/ortools/sat/CpModel.g.cs \
	$(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LD_OUT)$(BIN_DIR)$S$(NETSTANDARD_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O $(OBJ_DIR)/swig/sat_csharp_wrap.$O $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O $(OBJ_DIR)/swig/graph_csharp_wrap.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS)

netstandard_create_obj_dir:
	$(MKDIR_P) $(NETSTANDARD_OBJ_DIR)
	$(COPY) tools$Snetstandard$SOrTools.NetCore.csproj $(NETSTANDARD_OBJ_DIR)

# csharp linear solver examples

ifeq ($(EX),) # Those rules will be used if EX variable is not set

$(BIN_DIR)/cslinearprogramming$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/cslinearprogramming.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scslinearprogramming$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scslinearprogramming.cs

$(BIN_DIR)/csintegerprogramming$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/csintegerprogramming.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsintegerprogramming$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scsintegerprogramming.cs

# csharp linear solver tests

$(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/testlp.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /r:$(BIN_DIR)$S$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Stestlp.cs

testlp: $(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe

# csharp cp examples

$(BIN_DIR)/csrabbitspheasants$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/csrabbitspheasants.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsrabbitspheasants$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scsrabbitspheasants.cs

$(BIN_DIR)/send_more_money$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/send_more_money.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Ssend_more_money$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Ssend_more_money.cs

$(BIN_DIR)/furniture_moving_intervals$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/furniture_moving_intervals.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Sfurniture_moving_intervals.cs

$(BIN_DIR)/organize_day_intervals$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/organize_day_intervals.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sorganize_day_intervals$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Sorganize_day_intervals.cs

$(BIN_DIR)/cstsp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/cstsp.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scstsp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scstsp.cs

$(BIN_DIR)/cscvrptw$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/cscvrptw.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scscvrptw$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scscvrptw.cs

$(BIN_DIR)/csls_api$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/csls_api.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsls_api$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scsls_api.cs

# csharp constraint solver tests

$(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/testcp.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Stestcp.cs

testcp: $(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/testsat$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/testsat.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestsat$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Stestsat.cs

testsat: $(BIN_DIR)/testsat$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestsat$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/test_sat_model$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/test_sat_model.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stest_sat_model$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Stest_sat_model.cs

test_sat_model: $(BIN_DIR)/test_sat_model$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stest_sat_model$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue18$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/issue18.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue18$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Sissue18.cs

issue18: $(BIN_DIR)/issue18$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue18$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue22$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/issue22.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue22$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Sissue22.cs

issue22: $(BIN_DIR)/issue22$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue22$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue33$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/issue33.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue33$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Sissue33.cs

issue33: $(BIN_DIR)/issue33$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue33$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/jobshop_bug$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/tests/jobshop_bug.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sjobshop_bug$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Stests$Sjobshop_bug.cs

jobshop_bug: $(BIN_DIR)/jobshop_bug$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sjobshop_bug$(CLR_EXE_SUFFIX).exe

# csharp algorithm examples

$(BIN_DIR)/csknapsack$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/csknapsack.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsknapsack$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scsknapsack.cs

# csharp graph examples

$(BIN_DIR)/csflow$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/csflow.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsflow$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scsflow.cs

# csharp sat examples

$(BIN_DIR)/jobshop_ft06_sat$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/jobshop_ft06_sat.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sjobshop_ft06_sat$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Sjobshop_ft06_sat.cs

$(BIN_DIR)/gate_scheduling_sat$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/gate_scheduling_sat.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sgate_scheduling_sat$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Sgate_scheduling_sat.cs

$(BIN_DIR)/nurses_sat$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/nurses_sat.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Snurses_sat$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Snurses_sat.cs

# Examples using multiple libraries.

$(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/techtalk_scheduling.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Stechtalk_scheduling.cs

techtalk_scheduling: $(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe

# Code samples

$(BIN_DIR)/code_samples_sat$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX_DIR)/csharp/code_samples_sat.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scode_samples_sat$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scode_samples_sat.cs

code_samples_sat: $(BIN_DIR)/code_samples_sat$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scode_samples_sat$(CLR_EXE_SUFFIX).exe

else # This generic rule will be used if EX variable is set

ifneq ($(CLR_EXE_SUFFIX),) # otherwise this rule has the same target than the one in use for cc files.
$(BIN_DIR)/$(basename $(notdir $(EX)))$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(EX)
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /debug /out:$(BIN_DIR)$S$(basename $(notdir $(EX)))$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_DLL_NAME)$(DLL) /r:$(CLR_PROTOBUF_DLL_NAME)$(DLL) $(EX)
endif # ifneq ($(CLR_EXE_SUFFIX),)

csc: $(BIN_DIR)/$(basename $(notdir $(EX)))$(CLR_EXE_SUFFIX).exe

rcs: $(BIN_DIR)/$(basename $(notdir $(EX)))$(CLR_EXE_SUFFIX).exe
	@echo running $(BIN_DIR)$S$(basename $(notdir $(EX)))$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$S$(basename $(notdir $(EX)))$(CLR_EXE_SUFFIX).exe $(ARGS)

endif # ifeq ($(EX),)

# C# Fz support

# Leverage the existing CommonAssemblyInfo from prior build targets.
$(GEN_DIR)/com/google/ortools/flatzinc/properties/CommonAssemblyInfo.cs: \
	$(GEN_DIR)/com/google/ortools/properties/CommonAssemblyInfo.cs
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties
	$(COPY) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sproperties$SCommonAssemblyInfo.cs $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties

$(GEN_DIR)/com/google/ortools/flatzinc/properties/AssemblyInfo.cs: \
	$(CLR_KEYFILE)
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties
	$(COPY) tools$Scsharp$SFlatZincAssemblyInfo.cs $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties$SAssemblyInfo.cs
ifdef CLR_KEYFILE
ifeq ($(SYSTEM),win)
	@echo [assembly: AssemblyKeyFile("$(CLR_KEYFILE)")] >> $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties$SAssemblyInfo.cs
else # ifeq ($(SYSTEM),win)
	@echo "[assembly: AssemblyKeyFile(\"$(CLR_KEYFILE)\")]" >> $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc$Sproperties$SAssemblyInfo.cs
endif # ifeq ($(SYSTEM),win)
endif # ifdef CLR_KEYFILE

csharpfz: \
	$(BIN_DIR)/$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL) \
	$(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe

$(GEN_DIR)/ortools/flatzinc/flatzinc_csharp_wrap.cc: \
	$(SRC_DIR)/ortools/base/base.i \
	$(SRC_DIR)/ortools/util/csharp/proto.i \
	$(SRC_DIR)/ortools/flatzinc/csharp/flatzinc.i \
	$(FLATZINC_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -nodefaultctor -csharp -o $(GEN_PATH)$Sortools$Sflatzinc$Sflatzinc_csharp_wrap.cc -module operations_research_flatzinc -namespace $(CLR_ORTOOLS_FZ_DLL_NAME) -dllimport "$(CLR_ORTOOLS_FZ_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX)" -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc $(SRC_DIR)$Sortools$Sflatzinc$Scsharp$Sflatzinc.i

$(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O: \
	$(GEN_DIR)/ortools/flatzinc/flatzinc_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_PATH)/ortools/flatzinc/flatzinc_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sflatzinc_csharp_wrap.$O

$(BIN_DIR)/$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL): \
	$(GEN_DIR)/com/google/ortools/flatzinc/properties/CommonAssemblyInfo.cs \
	$(GEN_DIR)/com/google/ortools/flatzinc/properties/AssemblyInfo.cs \
	$(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O \
	$(OR_TOOLS_LIBS) $(FLATZINC_LIBS)
ifeq ($(SYSTEM),win)
	"$(CSHARP_EXECUTABLE)" /target:module /out:$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME).netmodule /warn:0 /nologo /debug $(GEN_PATH)\\com\\google\\ortools\\flatzinc\\*.cs $(GEN_PATH)\\com\\google\\ortools\\flatzinc\\properties\\*cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LD_OUT)$(BIN_DIR)$S$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME).netmodule $(OBJ_DIR)$Sswig$Sflatzinc_csharp_wrap.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS)
else # ifeq ($(SYSTEM),win)
	"$(CSHARP_EXECUTABLE)" /target:library /out:$(BIN_DIR)/$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL) /warn:0 /nologo /debug $(GEN_PATH)/com/google/ortools/flatzinc/*.cs $(GEN_PATH)/com/google/ortools/flatzinc/properties/*cs
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_ORTOOLS_FZ_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) $(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS)
endif # ifeq ($(SYSTEM),win)

$(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe: \
	$(BIN_DIR)/$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL) \
	$(EX_DIR)/csharp/csfz.cs
	"$(CSHARP_EXECUTABLE)" $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsfz$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL) $(EX_DIR)$Scsharp$Scsfz.cs

rcsfz: $(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsfz$(CLR_EXE_SUFFIX).exe $(ARGS)

###############################
##  NuGet Package Management ##
###############################

ifeq ($(SYSTEM),win)
NUGET_COMPILER ?= nuget.exe
NUGET_EXECUTABLE := $(shell $(WHICH) $(NUGET_COMPILER) 2>nul)
else #UNIX
NUGET_COMPILER ?= nuget
NUGET_EXECUTABLE := $(shell which $(NUGET_COMPILER))
endif
NUGET_SRC = https://www.nuget.org/api/v2/package

clean_csharp_nuget:
ifeq ($(SYSTEM),win)
	if not exist temp $(MKDIR) temp
	if exist $(ORTOOLS_NUGET_DIR) $(ATTRIB) -r /s temp
	-$(RM_RECURSE_FORCED) $(ORTOOLS_NUGET_DIR)
endif # ($(SYSTEM),win)

csharp_nuget_stage: \
	$(BIN_DIR)/$(CLR_ORTOOLS_DLL_NAME)$(DLL)
ifeq ($(SYSTEM),win)
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\$(BIN_DIR)
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\solution\Properties
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\data\discrete_tomography
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\data\fill_a_pix
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\data\minesweeper
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\data\rogo
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\data\survo_puzzle
	$(MKDIR_P) $(ORTOOLS_NUGET_DIR)\examples\data\quasigroup_completion
	$(COPY) LICENSE-2.0.txt $(ORTOOLS_NUGET_DIR)
	$(COPY) tools\README.dotnet $(ORTOOLS_NUGET_DIR)\README
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_DLL_NAME)$(DLL) $(ORTOOLS_NUGET_DIR)\$(BIN_DIR)
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_DLL_NAME)$(PDB) $(ORTOOLS_NUGET_DIR)\$(BIN_DIR)
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_DLL_NAME)$(L) $(ORTOOLS_NUGET_DIR)\$(BIN_DIR)
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_DLL_NAME)$(EXP) $(ORTOOLS_NUGET_DIR)\$(BIN_DIR)
	$(COPY) examples\csharp\*.cs $(ORTOOLS_NUGET_DIR)\examples
	$(COPY) examples\csharp\*.sln $(ORTOOLS_NUGET_DIR)\examples
	$(COPY) examples\csharp\solution\*.csproj $(ORTOOLS_NUGET_DIR)\examples\solution
	$(COPY) examples\csharp\solution\Properties\*.cs $(ORTOOLS_NUGET_DIR)\examples\solution\Properties
	$(COPY) examples\data\discrete_tomography\* $(ORTOOLS_NUGET_DIR)\examples\data\discrete_tomography
	$(COPY) examples\data\fill_a_pix\* $(ORTOOLS_NUGET_DIR)\examples\data\fill_a_pix
	$(COPY) examples\data\minesweeper\* $(ORTOOLS_NUGET_DIR)\examples\data\minesweeper
	$(COPY) examples\data\rogo\* $(ORTOOLS_NUGET_DIR)\examples\data\rogo
	$(COPY) examples\data\survo_puzzle\* $(ORTOOLS_NUGET_DIR)\examples\data\survo_puzzle
	$(COPY) examples\data\quasigroup_completion\* $(ORTOOLS_NUGET_DIR)\examples\data\quasigroup_completion
endif # ($(SYSTEM),win)

tools\$(ORTOOLS_NUSPEC_NAME): csharp_nuget_stage
ifeq ($(SYSTEM),win)
	$(COPY) tools\$(ORTOOLS_NUSPEC_NAME) $(ORTOOLS_NUGET_DIR)
	$(SED) -i -e "s/NNNN/$(CLR_ORTOOLS_DLL_NAME)/" $(ORTOOLS_NUGET_DIR)\$(ORTOOLS_NUSPEC_NAME)
	$(SED) -i -e "s/MMMM/$(CLR_PROTOBUF_DLL_NAME)/" $(ORTOOLS_NUGET_DIR)\$(ORTOOLS_NUSPEC_NAME)
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" $(ORTOOLS_NUGET_DIR)\$(ORTOOLS_NUSPEC_NAME)
	$(SED) -i -e "s/PROTOBUF_TAG/$(PROTOBUF_TAG)/" $(ORTOOLS_NUGET_DIR)\$(ORTOOLS_NUSPEC_NAME)
endif # ($(SYSTEM),win)

.PHONY: nuget_archive
nuget_archive: \
	$(CLR_ORTOOLS_DLL_NAME).$(OR_TOOLS_VERSION).nupkg

$(CLR_ORTOOLS_DLL_NAME).$(OR_TOOLS_VERSION).nupkg: \
	tools\$(ORTOOLS_NUSPEC_NAME)
	cd $(ORTOOLS_NUGET_DIR) && "$(NUGET_EXECUTABLE)" pack $(ORTOOLS_NUSPEC_NAME)

.PHONY: nuget_upload
nuget_upload: nuget_archive
	cd $(ORTOOLS_NUGET_DIR) && "$(NUGET_EXECUTABLE)" push $(CLR_ORTOOLS_DLL_NAME).$(OR_TOOLS_VERSION).nupkg -Source $(NUGET_SRC)

clean_csharpfz_nuget:
ifeq ($(SYSTEM),win)
	if not exist temp $(MKDIR) temp
	if exist $(FZ_NUGET_DIR) $(ATTRIB) -r /s temp
	-$(RM_RECURSE_FORCED) $(FZ_NUGET_DIR)
endif # ($(SYSTEM),win)

csharpfz_nuget_stage: \
	$(BIN_DIR)/$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL)
ifeq ($(SYSTEM),win)
	$(MKDIR_P) $(FZ_NUGET_DIR)\$(BIN_DIR)
	$(MKDIR_P) $(FZ_NUGET_DIR)\examples
	$(COPY) LICENSE-2.0.txt $(FZ_NUGET_DIR)
	$(COPY) tools\README.dotnet $(FZ_NUGET_DIR)\README
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_FZ_DLL_NAME)$(DLL) $(FZ_NUGET_DIR)\$(BIN_DIR)
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_FZ_DLL_NAME)$(PDB) $(FZ_NUGET_DIR)\$(BIN_DIR)
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_FZ_DLL_NAME)$(L) $(FZ_NUGET_DIR)\$(BIN_DIR)
	$(COPY) $(BIN_DIR)\$(CLR_ORTOOLS_FZ_DLL_NAME)$(EXP) $(FZ_NUGET_DIR)\$(BIN_DIR)
	$(COPY) examples\flatzinc\*.fzn $(FZ_NUGET_DIR)\examples
endif # ($(SYSTEM),win)

tools\$(FZ_NUSPEC_NAME): \
	csharpfz_nuget_stage
ifeq ($(SYSTEM),win)
	$(COPY) tools\$(FZ_NUSPEC_NAME) $(FZ_NUGET_DIR)
	$(SED) -i -e "s/NNNN/$(CLR_ORTOOLS_FZ_DLL_NAME)/" $(FZ_NUGET_DIR)\$(FZ_NUSPEC_NAME)
	$(SED) -i -e "s/MMMM/$(CLR_ORTOOLS_DLL_NAME)/" $(FZ_NUGET_DIR)\$(FZ_NUSPEC_NAME)
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" $(FZ_NUGET_DIR)\$(FZ_NUSPEC_NAME)
endif # ($(SYSTEM),win)

csharpfz_nuget_pack: \
	tools\$(FZ_NUSPEC_NAME)
ifeq ($(SYSTEM),win)
	cd $(FZ_NUGET_DIR) && "$(NUGET_EXECUTABLE)" pack $(FZ_NUSPEC_NAME)
endif # ($(SYSTEM),win)

$(CLR_ORTOOLS_FZ_DLL_NAME).$(OR_TOOLS_VERSION).nupkg: \
	csharpfz_nuget_pack

csharpfz_nuget_push: \
	$(CLR_ORTOOLS_FZ_DLL_NAME).$(OR_TOOLS_VERSION).nupkg
	cd $(FZ_NUGET_DIR) && "$(NUGET_EXECUTABLE)" push $(CLR_ORTOOLS_FZ_DLL_NAME).$(OR_TOOLS_VERSION).nupkg -Source $(NUGET_SRC)

# Mirror the same approach for the root Google.OrTools
nuget_upload_fz: csharpfz_nuget_push


# csharpsolution
# create solution files for visual studio

ifeq ($(SYSTEM),win)
csharpsolution: examples/csharp/Csharp_examples.sln
else
csharpsolution:
endif

examples/csharp/solution/%.csproj: examples/csharp/%.cs tools/template.csproj
	$(COPY) tools$Stemplate.csproj examples$Scsharp$Ssolution$S$(@F)
# Replace all "SOURCEFILE" instances with the C# example source file
# AND
# Replace all "EXECUTABLE" instances with the name of the executable. the first letter is capitalized.
	$(SED) -i -e "s/SOURCEFILE/$*.cs/" -e "s/EXECUTABLE/\u$*/" examples$Scsharp$Ssolution$S$(@F)

ALL_CSPROJ= $(patsubst examples/csharp/%.cs, examples/csharp/solution/%.csproj, $(wildcard examples/csharp/*.cs))

examples/csharp/Csharp_examples.sln: tools/template.sln $(ALL_CSPROJ)
	-$(DEL) examples$Scsharp$SCsharp_examples.sln
	$(COPY) tools$Stemplate.sln examples$Scsharp$SCsharp_examples.sln
#Add the *csproj files to the solution
	$(SED) -i '/#End Projects/i $(join \
	$(patsubst examples/csharp/solution/%.csproj, Project\(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\"\)=\"%\"$(COMMA) , $(wildcard examples/csharp/solution/*.csproj)), \
$(patsubst examples/csharp/solution/%.csproj, \"solution%.csproj\"$(COMMA)\"{}\"EndProject, $(wildcard examples/csharp/solution/*.csproj)) \
)' examples/csharp/Csharp_examples.sln
	$(SED) -i -e "s/solution/solution\\/g" -e "s/EndProject/\r\nEndProject\r\n/g" -e "s/ Project/Project/g" examples$Scsharp$SCsharp_examples.sln

.PHONY: detect_csharp # Show variables used to build C# OR-Tools.
detect_csharp:
ifeq ($(SYSTEM),win)
	@echo Relevant info for the C# build:
else
	@echo Relevant info for the C\# build:
endif
	@echo NETSTANDARD_RUNTIME_IDENTIFIER = $(NETSTANDARD_RUNTIME_IDENTIFIER)
	@echo PATH_TO_CSHARP_COMPILER = $(PATH_TO_CSHARP_COMPILER)
	@echo CSHARP_COMPILER = $(CSHARP_COMPILER)
	@echo CSHARP_EXECUTABLE = "$(CSHARP_EXECUTABLE)"
	@echo CLR_EXE_SUFFIX = "$(CLR_EXE_SUFFIX)"
ifeq ($(SYSTEM),unix)
	@echo MONO_COMPILER = $(MONO_COMPILER)
	@echo MONO_EXECUTABLE = "$(MONO_EXECUTABLE)"
	@echo MONO = $(MONO)
endif
	@echo NUGET_COMPILER = $(NUGET_COMPILER)
	@echo NUGET_EXECUTABLE = "$(NUGET_EXECUTABLE)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
