# ---------- CSharp support using SWIG ----------

# Assembly Signing
# ----------------
# All C# assemblies can optionally be signed. This includes the
# DLLs and the test case EXEs. Signing is currently supported only
# on Windows with the .NET framework.
#
# To get signed assemblies, use for example:
# make CLR_KEYFILE="c:\full\path\to\keyfile.snk" csharp
#
# For delay signing, use for example:
# make CLR_KEYFILE="c:\full\path\to\keyfile_pub.snk" CLR_DELAYSIGN=1

ifeq ($(SYSTEM),win)
ifdef CLR_KEYFILE
ifdef CLR_DELAYSIGN
SIGNING_FLAGS:= /keyfile:"$(CLR_KEYFILE)" /delaysign
else
SIGNING_FLAGS:= /keyfile:"$(CLR_KEYFILE)"
endif
endif
endif

# The generated DLL name. Defaults to Google.OrTools
CLR_DLL_NAME?=Google.OrTools
BASE_CLR_DLL_NAME:= $(CLR_DLL_NAME)

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
      CLR_DLL_NAME:=$(CLR_DLL_NAME).x64
      CLR_EXE_SUFFIX?=_x64
    else
      CLR_DLL_NAME:=$(CLR_DLL_NAME).x86
    endif
  endif
endif

CSHARPEXE = \
	$(BIN_DIR)/csknapsack$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/csintegerprogramming$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/cslinearprogramming$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/csls_api$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/csflow$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/csrabbitspheasants$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/cstsp$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/furniture_moving_intervals$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/organize_day_intervals$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe \
	$(BIN_DIR)/cscvrptw$(CLR_EXE_SUFFIX).exe

csharpexe: $(CSHARPEXE)

# Main target.
ifeq ("$(SYSTEM)","unix")
ifeq ($(MCS),)
ifneq ($(REAL_MCS),)
csharp: warn_mcs
else   # REAL_MCS
csharp:
endif  # REAL_MCS
else   # MCS
csharp: csharportools csharpexe
endif  # MCS
else   # Windows
csharp: csharportools csharpexe
endif  # Windows

warn_mcs:
	$(warning mcs version 4.2.0.0 or greater is required, csharp files will not be compiled)

# Clean target.
clean_csharp:
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)
	-$(DEL) $(BIN_DIR)$S$(CLR_DLL_NAME)*.dll
	-$(DEL) $(BIN_DIR)$S$(CLR_DLL_NAME)*.mdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME)*.lib
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME)*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME)*.exp
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME)*.netmodule
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc*.lib
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc*.exp
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc*.netmodule
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Salgorithms$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Sgraph$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Sflatzinc$S*csharp_wrap*
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Salgorithms$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sflatzinc$S*.cs
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sutil$S*.cs
	-$(DEL) $(OBJ_DIR)$Sswig$S*csharp_wrap.$O
	-$(DEL) $(BIN_DIR)$S*$(CLR_EXE_SUFFIX).exe
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs

$(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs : $(GEN_DIR)/com/google/ortools/SvnVersion$(OR_TOOLS_SHORT_VERSION).txt
ifeq ("$(SYSTEM)","win")
	@echo using System.Reflection; > $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo using System.Runtime.CompilerServices; >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo using System.Runtime.InteropServices; >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyTitle( "OR-Tools Assembly" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyDescription( ".NET Assembly for the OR-Tools project" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyConfiguration( "" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyCompany( "Google" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyProduct( "OR-Tools" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyCopyright( "Copyright (c) 2010-2015 Google" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyCulture( "" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyVersion( "$(OR_TOOLS_SHORT_VERSION).*" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyFileVersion( "$(OR_TOOLS_SHORT_VERSION)" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:System.Reflection.AssemblyInformationalVersion( "OR-Tools $(OR_TOOLS_SHORT_VERSION)" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:ComVisible(false)] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly:Guid("0a227c4c-8bb3-4db0-808f-55dae227d8c5")] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
else
	@echo "using System.Reflection;" > $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "using System.Runtime.CompilerServices;" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "using System.Runtime.InteropServices;" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyTitle( \"OR-Tools Assembly\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyDescription( \".NET Assembly for the OR-Tools project\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyConfiguration( \"\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyCompany( \"Google\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyProduct( \"OR-Tools\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyCopyright( \"Copyright (c) 2010-2015 Google\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyCulture( \"\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyVersion( \"$(OR_TOOLS_SHORT_VERSION).*\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyFileVersion( \"$(OR_TOOLS_SHORT_VERSION).0\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:System.Reflection.AssemblyInformationalVersion( \"OR-Tools $(OR_TOOLS_SHORT_VERSION)-r$(GIT_HASH)\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:ComVisible(false)]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly:Guid(\"0a227c4c-8bb3-4db0-808f-55dae227d8c5\")]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
endif

$(GEN_DIR)/com/google/ortools/SvnVersion$(OR_TOOLS_SHORT_VERSION).txt:
	@echo $(OR_TOOLS_SHORT_VERSION) > $(GEN_DIR)$Scom$Sgoogle$Sortools$SSvnVersion$(OR_TOOLS_SHORT_VERSION).txt

# csharportools

csharportools: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(BIN_DIR)/Google.Protobuf.dll

$(BIN_DIR)/Google.Protobuf.dll: dependencies/install/bin/Google.Protobuf.dll
	$(COPY) dependencies$Sinstall$Sbin$SGoogle.Protobuf.dll $(BIN_DIR)

$(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc: \
	$(SRC_DIR)/linear_solver/csharp/linear_solver.swig \
	$(SRC_DIR)/base/base.swig $(SRC_DIR)/util/csharp/proto.swig \
	$(LP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research_linear_solver -namespace $(BASE_CLR_DLL_NAME).LinearSolver -dllimport "$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)/linear_solver$Scsharp$Slinear_solver.swig

$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O

$(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc: \
	$(SRC_DIR)/constraint_solver/csharp/routing.swig \
	$(SRC_DIR)/constraint_solver/csharp/constraint_solver.swig \
	$(SRC_DIR)/base/base.swig \
	$(SRC_DIR)/util/csharp/proto.swig \
	$(SRC_DIR)/util/csharp/functions.swig \
	$(CP_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc -module operations_research_constraint_solver -namespace $(BASE_CLR_DLL_NAME).ConstraintSolver -dllimport "$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)$Sconstraint_solver$Scsharp$Srouting.swig
	$(SED) -i -e 's/CSharp_new_Solver/CSharp_new_CpSolver/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Solver/CSharp_delete_CpSolver/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Solver/CSharp_CpSolver/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_new_Constraint/CSharp_new_CpConstraint/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_delete_Constraint/CSharp_delete_CpConstraint/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.*
	$(SED) -i -e 's/CSharp_Constraint/CSharp_CpConstraint/g' $(GEN_DIR)/com/google/ortools/constraintsolver/*cs $(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.*

$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O: \
	$(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O

$(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc: \
	$(SRC_DIR)/algorithms/csharp/knapsack_solver.swig \
	$(SRC_DIR)/base/base.swig \
	$(SRC_DIR)/util/csharp/proto.swig \
	$(SRC_DIR)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Salgorithms$Sknapsack_solver_csharp_wrap.cc -module operations_research_algorithms -namespace $(BASE_CLR_DLL_NAME).Algorithms -dllimport "$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Salgorithms $(SRC_DIR)$Salgorithms$Scsharp$Sknapsack_solver.swig

$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O: $(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/algorithms/knapsack_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O

$(GEN_DIR)/graph/graph_csharp_wrap.cc: \
	$(SRC_DIR)/graph/csharp/graph.swig \
	$(SRC_DIR)/base/base.swig \
	$(SRC_DIR)/util/csharp/proto.swig \
	$(GRAPH_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc -module operations_research_graph -namespace $(BASE_CLR_DLL_NAME).Graph -dllimport "$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph $(SRC_DIR)$Sgraph$Scsharp$Sgraph.swig

$(OBJ_DIR)/swig/graph_csharp_wrap.$O: $(GEN_DIR)/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O

# Protobufs

$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs: $(SRC_DIR)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs: $(SRC_DIR)/constraint_solver/solver_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs: $(SRC_DIR)/constraint_solver/routing_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs: $(SRC_DIR)/constraint_solver/routing_enums.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(SRC_DIR) --csharp_out=$(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver --csharp_opt=file_extension=.g.cs $(SRC_DIR)$Sconstraint_solver$Srouting_enums.proto

# Main DLL

$(BIN_DIR)/$(CLR_DLL_NAME).dll: \
	$(BIN_DIR)/Google.Protobuf.dll \
	$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O \
	$(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O \
	$(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O \
	$(OBJ_DIR)/swig/graph_csharp_wrap.$O \
	$(SRC_DIR)/com/google/ortools/algorithms/IntArrayHelper.cs \
	$(SRC_DIR)/com/google/ortools/constraintsolver/IntVarArrayHelper.cs \
	$(SRC_DIR)/com/google/ortools/constraintsolver/IntervalVarArrayHelper.cs \
	$(SRC_DIR)/com/google/ortools/constraintsolver/IntArrayHelper.cs \
	$(SRC_DIR)/com/google/ortools/constraintsolver/NetDecisionBuilder.cs \
	$(SRC_DIR)/com/google/ortools/constraintsolver/SolverHelper.cs \
	$(SRC_DIR)/com/google/ortools/constraintsolver/ValCstPair.cs \
	$(SRC_DIR)/com/google/ortools/linearsolver/DoubleArrayHelper.cs \
	$(SRC_DIR)/com/google/ortools/linearsolver/LinearExpr.cs \
	$(SRC_DIR)/com/google/ortools/linearsolver/LinearConstraint.cs \
	$(SRC_DIR)/com/google/ortools/linearsolver/SolverHelper.cs \
	$(SRC_DIR)/com/google/ortools/linearsolver/VariableHelper.cs \
	$(SRC_DIR)/com/google/ortools/util/NestedArrayHelper.cs \
	$(SRC_DIR)/com/google/ortools/util/ProtoHelper.cs \
	$(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs \
	$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimit.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.g.cs\
	$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.g.cs\
	$(OR_TOOLS_LIBS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME).netmodule /lib:$(BIN_DIR) /r:Google.Protobuf.dll /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\linearsolver\\*.cs $(SRC_DIR)\\com\\google\\ortools\\linearsolver\\*.cs $(GEN_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs $(SRC_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs $(GEN_DIR)\\com\\google\\ortools\\algorithms\\*.cs $(SRC_DIR)\\com\\google\\ortools\\algorithms\\*.cs $(GEN_DIR)\\com\\google\\ortools\\graph\\*.cs $(SRC_DIR)\\com\\google\\ortools\\util\\*.cs $(GEN_DIR)\\com\\google\\ortools\\CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)$S$(CLR_DLL_NAME).dll $(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME).netmodule $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/$(CLR_DLL_NAME).dll /lib:$(BIN_DIR) /r:Google.Protobuf.dll /warn:0 /nologo /debug $(SRC_DIR)/com/google/ortools/util/*.cs $(GEN_DIR)/com/google/ortools/linearsolver/*.cs $(SRC_DIR)/com/google/ortools/linearsolver/*.cs $(GEN_DIR)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/com/google/ortools/algorithms/*.cs $(GEN_DIR)/com/google/ortools/algorithms/*.cs $(GEN_DIR)/com/google/ortools/graph/*.cs $(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIB_PREFIX)$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O $(OBJ_DIR)/swig/graph_csharp_wrap.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS)
endif

# csharp linear solver examples

$(BIN_DIR)/cslinearprogramming$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/cslinearprogramming.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scslinearprogramming$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scslinearprogramming.cs

$(BIN_DIR)/csintegerprogramming$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csintegerprogramming.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsintegerprogramming$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scsintegerprogramming.cs

# csharp linear solver tests

$(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/testlp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /r:$(BIN_DIR)$S$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Stestlp.cs

testlp: $(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe

# csharp cp examples

$(BIN_DIR)/csrabbitspheasants$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csrabbitspheasants.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsrabbitspheasants$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scsrabbitspheasants.cs

$(BIN_DIR)/send_more_money$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/send_more_money.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Ssend_more_money$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Ssend_more_money.cs

$(BIN_DIR)/furniture_moving_intervals$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/furniture_moving_intervals.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Sfurniture_moving_intervals.cs

$(BIN_DIR)/organize_day_intervals$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/organize_day_intervals.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sorganize_day_intervals$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Sorganize_day_intervals.cs

$(BIN_DIR)/cstsp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/cstsp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scstsp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scstsp.cs

$(BIN_DIR)/cscvrptw$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/cscvrptw.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scscvrptw$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scscvrptw.cs

$(BIN_DIR)/csls_api$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csls_api.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsls_api$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scsls_api.cs

# csharp constraint solver tests

$(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/testcp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Stests$Stestcp.cs

testcp: $(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue18$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/issue18.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue18$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Stests$Sissue18.cs

issue18: $(BIN_DIR)/issue18$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue18$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue22$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/issue22.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue22$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Stests$Sissue22.cs

issue22: $(BIN_DIR)/issue22$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue22$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue33$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/issue33.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue33$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Stests$Sissue33.cs

issue33: $(BIN_DIR)/issue33$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue33$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/jobshop_bug$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/jobshop_bug.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sjobshop_bug$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Stests$Sjobshop_bug.cs

jobshop_bug: $(BIN_DIR)/jobshop_bug$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sjobshop_bug$(CLR_EXE_SUFFIX).exe

# csharp algorithm examples

$(BIN_DIR)/csknapsack$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csknapsack.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsknapsack$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scsknapsack.cs

# csharp graph examples

$(BIN_DIR)/csflow$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csflow.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsflow$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Scsflow.cs

# Examples using multiple libraries.

$(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/techtalk_scheduling.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$Stechtalk_scheduling.cs

techtalk_scheduling: $(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe

# Build and compile custome CP examples

ccs: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/$(EX).cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$S$(EX)$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX_DIR)$Scsharp$S$(EX).cs

rcs: ccs
	$(MONO) $(BIN_DIR)$S$(EX)$(CLR_EXE_SUFFIX).exe $(ARGS)

# C# Fz support

csharpfz: \
	$(BIN_DIR)/Google.OrTools.Flatzinc.dll \
	$(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe

$(GEN_DIR)/flatzinc/flatzinc_csharp_wrap.cc: \
	$(SRC_DIR)/flatzinc/csharp/flatzinc.swig \
	$(SRC_DIR)/base/base.swig $(SRC_DIR)/util/csharp/proto.swig \
	$(FLATZINC_DEPS)
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sflatzinc$Sflatzinc_csharp_wrap.cc -module operations_research_flatzinc -namespace $(BASE_CLR_DLL_NAME).Flatzinc -dllimport "Google.OrTools.Flatzinc.$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sflatzinc $(SRC_DIR)/flatzinc$Scsharp$Sflatzinc.swig

$(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O: $(GEN_DIR)/flatzinc/flatzinc_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/flatzinc/flatzinc_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sflatzinc_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.Flatzinc.dll: \
	$(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O \
	$(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs \
	$(FLATZINC_LIBS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\flatzinc\\*.cs $(GEN_DIR)\\com\\google\\ortools\\CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)$SGoogle.OrTools.Flatzinc.dll $(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc.netmodule $(OBJ_DIR)$Sswig$Sflatzinc_csharp_wrap.$O $(FLATZINC_LNK) $(OR_TOOLS_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.Flatzinc.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/flatzinc/*.cs $(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIB_PREFIX)Google.OrTools.Flatzinc.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O $(FLATZINC_LNK) $(OR_TOOLS_LD_FLAGS)
endif

$(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/Google.OrTools.Flatzinc.dll $(EX_DIR)/csharp/csfz.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsfz$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.Flatzinc.dll $(EX_DIR)$Scsharp$Scsfz.cs

rcsfz: $(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsfz$(CLR_EXE_SUFFIX).exe $(ARGS)


# Build archive.

nuget_upload:
	-$(DELREC) temp
ifeq ("$(SYSTEM)","win")
	tools\mkdir temp
	tools\mkdir temp\or-tools
	tools\mkdir temp\or-tools\bin
	tools\mkdir temp\or-tools\examples
	tools\mkdir temp\or-tools\examples\solution
	tools\mkdir temp\or-tools\examples\solution\Properties
	tools\mkdir temp\or-tools\examples\data
	tools\mkdir temp\or-tools\examples\data\discrete_tomography
	tools\mkdir temp\or-tools\examples\data\fill_a_pix
	tools\mkdir temp\or-tools\examples\data\minesweeper
	tools\mkdir temp\or-tools\examples\data\rogo
	tools\mkdir temp\or-tools\examples\data\survo_puzzle
	tools\mkdir temp\or-tools\examples\data\quasigroup_completion
	copy LICENSE-2.0.txt temp$Sor-tools
	copy tools\README.dotnet temp\or-tools\README
	copy bin\$(CLR_DLL_NAME).dll temp\or-tools\bin
	copy examples\csharp\*.cs temp\or-tools\examples
	copy examples\csharp\*.sln temp\or-tools\examples
	copy examples\csharp\solution\*.csproj temp\or-tools\examples\solution
	copy examples\csharp\solution\Properties\*.cs temp\or-tools\examples\solution\Properties
	copy examples\data\discrete_tomography\* temp\or-tools\examples\data\discrete_tomography
	copy examples\data\fill_a_pix\* temp\or-tools\examples\data\fill_a_pix
	copy examples\data\minesweeper\* temp\or-tools\examples\data\minesweeper
	copy examples\data\rogo\* temp\or-tools\examples\data\rogo
	copy examples\data\survo_puzzle\* temp\or-tools\examples\data\survo_puzzle
	copy examples\data\quasigroup_completion\* temp\or-tools\examples\data\quasigroup_completion
	copy tools\or-tools.nuspec temp\or-tools
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/g" temp\or-tools\or-tools.nuspec
	cd temp\or-tools && nuget pack or-tools.nuspec
	cd temp\or-tools && nuget push Google.OrTools-$(OR_TOOLS_VERSION).nupkg
endif

dotnet_archive: csharp
	-$(DELREC) temp
ifeq ("$(SYSTEM)","win")
	tools\mkdir temp
	tools\mkdir temp\$(INSTALL_DIR)
	tools\mkdir temp\$(INSTALL_DIR)\bin
	tools\mkdir temp\$(INSTALL_DIR)\examples
	tools\mkdir temp\$(INSTALL_DIR)\examples\solution
	tools\mkdir temp\$(INSTALL_DIR)\examples\solution\Properties
	tools\mkdir temp\$(INSTALL_DIR)\examples\data
	tools\mkdir temp\$(INSTALL_DIR)\examples\data\discrete_tomography
	tools\mkdir temp\$(INSTALL_DIR)\examples\data\fill_a_pix
	tools\mkdir temp\$(INSTALL_DIR)\examples\data\minesweeper
	tools\mkdir temp\$(INSTALL_DIR)\examples\data\rogo
	tools\mkdir temp\$(INSTALL_DIR)\examples\data\survo_puzzle
	tools\mkdir temp\$(INSTALL_DIR)\examples\data\quasigroup_completion
	copy LICENSE-2.0.txt temp$S$(INSTALL_DIR)
	copy tools\README.dotnet temp\$(INSTALL_DIR)\README
	copy bin\Google.Protobuf.dll temp\$(INSTALL_DIR)\bin
	copy bin\$(CLR_DLL_NAME).dll temp\$(INSTALL_DIR)\bin
	copy examples\csharp\*.cs temp\$(INSTALL_DIR)\examples
	copy examples\csharp\*.sln temp\$(INSTALL_DIR)\examples
	copy examples\csharp\solution\*.csproj temp\$(INSTALL_DIR)\examples\solution
	copy examples\csharp\solution\Properties\*.cs temp\$(INSTALL_DIR)\examples\solution\Properties
	copy examples\data\discrete_tomography\* temp\$(INSTALL_DIR)\examples\data\discrete_tomography
	copy examples\data\fill_a_pix\* temp\$(INSTALL_DIR)\examples\data\fill_a_pix
	copy examples\data\minesweeper\* temp\$(INSTALL_DIR)\examples\data\minesweeper
	copy examples\data\rogo\* temp\$(INSTALL_DIR)\examples\data\rogo
	copy examples\data\survo_puzzle\* temp\$(INSTALL_DIR)\examples\data\survo_puzzle
	copy examples\data\quasigroup_completion\* temp\$(INSTALL_DIR)\examples\data\quasigroup_completion
	cd temp && ..\tools\zip$(CLR_EXE_SUFFIX).exe -r ..\$(CLR_DLL_NAME).NET.$(INSTALL_PORT)-$(OR_TOOLS_VERSION).zip $(INSTALL_DIR)
else
	mkdir temp
	mkdir temp/$(INSTALL_DIR)
	mkdir temp/$(INSTALL_DIR)/bin
	mkdir temp/$(INSTALL_DIR)/examples
	mkdir temp/$(INSTALL_DIR)/examples/data
	mkdir temp/$(INSTALL_DIR)/examples/data/discrete_tomography
	mkdir temp/$(INSTALL_DIR)/examples/data/fill_a_pix
	mkdir temp/$(INSTALL_DIR)/examples/data/minesweeper
	mkdir temp/$(INSTALL_DIR)/examples/data/rogo
	mkdir temp/$(INSTALL_DIR)/examples/data/survo_puzzle
	mkdir temp/$(INSTALL_DIR)/examples/data/quasigroup_completion
	cp LICENSE-2.0.txt temp/$(INSTALL_DIR)
	cp tools/README.dotnet temp/$(INSTALL_DIR)/README
	cp bin/Google.Protobuf.dll temp/$(INSTALL_DIR)/bin
	cp bin/$(CLR_DLL_NAME).dll temp/$(INSTALL_DIR)/bin
	cp lib/lib$(CLR_DLL_NAME).so temp/$(INSTALL_DIR)/bin
	cp examples/csharp/*.cs temp/$(INSTALL_DIR)/examples
	cp examples/data/discrete_tomography/* temp/$(INSTALL_DIR)/examples/data/discrete_tomography
	cp examples/data/fill_a_pix/* temp/$(INSTALL_DIR)/examples/data/fill_a_pix
	cp examples/data/minesweeper/* temp/$(INSTALL_DIR)/examples/data/minesweeper
	cp examples/data/rogo/* temp/$(INSTALL_DIR)/examples/data/rogo
	cp examples/data/survo_puzzle/* temp/$(INSTALL_DIR)/examples/data/survo_puzzle
	cp examples/data/quasigroup_completion/* temp/$(INSTALL_DIR)/examples/data/quasigroup_completion
	cd temp && tar -c -v -z --no-same-owner -f ../$(CLR_DLL_NAME).NET.$(INSTALL_PORT)-$(OR_TOOLS_VERSION).tar.gz $(INSTALL_DIR)
endif
	-$(DELREC) temp
