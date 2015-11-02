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
csharp: csharportools csharpexe

# Clean target.
clean_csharp:
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)
	-$(DEL) $(BIN_DIR)$S$(CLR_DLL_NAME)*.dll
	-$(DEL) $(BIN_DIR)$S$(CLR_DLL_NAME)*.mdb
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME)*.lib
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME)*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME)*.exp
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME)*.netmodule
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc*.lib
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc*.pdb
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc*.exp
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc*.netmodule
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

$(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs : $(GEN_DIR)/com/google/ortools/SvnVersion$(GIT_REVISION).txt
ifeq ("$(SYSTEM)","win")
	@echo using System.Reflection; > $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo using System.Runtime.CompilerServices; >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo using System.Runtime.InteropServices; >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyTitle( "OR-Tools Assembly" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyDescription( ".NET Assembly for the OR-Tools project" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyConfiguration( "" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyCompany( "Google" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyProduct( "OR-Tools" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyCopyright( "Copyright (c) 2010-2015 Google" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyCulture( "" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyVersion( "2.0.$(GIT_REVISION).*" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyFileVersion( "2.0.$(GIT_REVISION).0" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: System.Reflection.AssemblyInformationalVersion( "OR-Tools 2.0.$(GIT_REVISION)" )] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: ComVisible(false)] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo [assembly: Guid("0a227c4c-8bb3-4db0-808f-55dae227d8c5")] >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
else
	@echo "using System.Reflection;" > $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "using System.Runtime.CompilerServices;" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "using System.Runtime.InteropServices;" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyTitle( \"OR-Tools Assembly\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyDescription( \".NET Assembly for the OR-Tools project\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyConfiguration( \"\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyCompany( \"Google\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyProduct( \"OR-Tools\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyCopyright( \"Copyright (c) 2010-2015 Google\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyCulture( \"\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyVersion( \"2.0.$(GIT_REVISION).*\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyFileVersion( \"2.0.$(GIT_REVISION).0\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: System.Reflection.AssemblyInformationalVersion( \"OR-Tools 2.0.$(GIT_REVISION)-r$(GIT_HASH)\" )]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: ComVisible(false)]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
	@echo "[assembly: Guid(\"0a227c4c-8bb3-4db0-808f-55dae227d8c5\")]" >> $(GEN_DIR)$Scom$Sgoogle$Sortools$SCommonAssemblyAttributes.cs
endif

$(GEN_DIR)/com/google/ortools/SvnVersion$(GIT_REVISION).txt:
	@echo $(GIT_REVISION) > $(GEN_DIR)$Scom$Sgoogle$Sortools$SSvnVersion$(GIT_REVISION).txt

# csharportools

csharportools: $(BIN_DIR)/$(CLR_DLL_NAME).dll

$(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc: \
	$(SRC_DIR)/linear_solver/csharp/linear_solver.swig \
	$(SRC_DIR)/base/base.swig $(SRC_DIR)/util/csharp/proto.swig \
	$(SRC_DIR)/linear_solver/linear_solver.h \
	$(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Slinear_solver$Slinear_solver_csharp_wrap.cc -module operations_research_linear_solver -namespace $(BASE_CLR_DLL_NAME).LinearSolver -dllimport "$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)/linear_solver$Scsharp$Slinear_solver.swig

$(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O

$(GEN_DIR)/constraint_solver/constraint_solver_csharp_wrap.cc: \
	$(SRC_DIR)/constraint_solver/csharp/routing.swig \
	$(SRC_DIR)/constraint_solver/csharp/constraint_solver.swig \
	$(SRC_DIR)/base/base.swig \
	$(SRC_DIR)/util/csharp/proto.swig \
	$(SRC_DIR)/util/csharp/functions.swig \
	$(SRC_DIR)/constraint_solver/constraint_solver.h
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
	$(SRC_DIR)/graph/max_flow.h \
	$(SRC_DIR)/graph/min_cost_flow.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc -module operations_research_graph -namespace $(BASE_CLR_DLL_NAME).Graph -dllimport "$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph $(SRC_DIR)$Sgraph$Scsharp$Sgraph.swig

$(OBJ_DIR)/swig/graph_csharp_wrap.$O: $(GEN_DIR)/graph/graph_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sgraph$Sgraph_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O

$(BIN_DIR)/$(CLR_DLL_NAME).dll: \
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
	$(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs \
	$(STATIC_ALL_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME).netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\linearsolver\\*.cs $(SRC_DIR)\\com\\google\\ortools\\linearsolver\\*.cs $(GEN_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs $(SRC_DIR)\\com\\google\\ortools\\constraintsolver\\*.cs $(GEN_DIR)\\com\\google\\ortools\\algorithms\\*.cs $(SRC_DIR)\\com\\google\\ortools\\algorithms\\*.cs $(GEN_DIR)\\com\\google\\ortools\\graph\\*.cs $(SRC_DIR)\\com\\google\\ortools\\util\\*.cs $(GEN_DIR)\\com\\google\\ortools\\CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)$S$(CLR_DLL_NAME).dll $(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME).netmodule $(OBJ_DIR)$Sswig$Slinear_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sconstraint_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sknapsack_solver_csharp_wrap.$O $(OBJ_DIR)$Sswig$Sgraph_csharp_wrap.$O $(STATIC_ALL_LNK) $(STATIC_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/$(CLR_DLL_NAME).dll /warn:0 /nologo /debug $(SRC_DIR)/com/google/ortools/util/*.cs $(GEN_DIR)/com/google/ortools/linearsolver/*.cs $(SRC_DIR)/com/google/ortools/linearsolver/*.cs $(GEN_DIR)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/com/google/ortools/constraintsolver/*.cs $(SRC_DIR)/com/google/ortools/algorithms/*.cs $(GEN_DIR)/com/google/ortools/algorithms/*.cs $(GEN_DIR)/com/google/ortools/graph/*.cs $(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)$(CLR_DLL_NAME).$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/swig/linear_solver_csharp_wrap.$O $(OBJ_DIR)/swig/constraint_solver_csharp_wrap.$O $(OBJ_DIR)/swig/knapsack_solver_csharp_wrap.$O $(OBJ_DIR)/swig/graph_csharp_wrap.$O $(STATIC_ALL_LNK) $(STATIC_LD_FLAGS)
endif

# csharp linear solver examples

$(BIN_DIR)/cslinearprogramming$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/cslinearprogramming.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scslinearprogramming$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scslinearprogramming.cs

$(BIN_DIR)/csintegerprogramming$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csintegerprogramming.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsintegerprogramming$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scsintegerprogramming.cs

# csharp linear solver tests

$(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/testlp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /r:$(BIN_DIR)$S$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Stestlp.cs

testlp: $(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe

# csharp cp examples

$(BIN_DIR)/csrabbitspheasants$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csrabbitspheasants.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsrabbitspheasants$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scsrabbitspheasants.cs

$(BIN_DIR)/send_more_money$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/send_more_money.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Ssend_more_money$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Ssend_more_money.cs

$(BIN_DIR)/furniture_moving_intervals$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/furniture_moving_intervals.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Sfurniture_moving_intervals.cs

$(BIN_DIR)/organize_day_intervals$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/organize_day_intervals.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sorganize_day_intervals$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Sorganize_day_intervals.cs

$(BIN_DIR)/cstsp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/cstsp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scstsp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scstsp.cs

$(BIN_DIR)/cscvrptw$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/cscvrptw.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scscvrptw$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scscvrptw.cs

$(BIN_DIR)/csls_api$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csls_api.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsls_api$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scsls_api.cs

# csharp constraint solver tests

$(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/testcp.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Stestcp.cs

testcp: $(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue18$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/issue18.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue18$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Sissue18.cs

issue18: $(BIN_DIR)/issue18$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue18$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue22$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/issue22.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue22$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Sissue22.cs

issue22: $(BIN_DIR)/issue22$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue22$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/issue33$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/issue33.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sissue33$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Sissue33.cs

issue33: $(BIN_DIR)/issue33$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sissue33$(CLR_EXE_SUFFIX).exe

$(BIN_DIR)/jobshop_bug$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/tests/jobshop_bug.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Sjobshop_bug$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Stests$Sjobshop_bug.cs

jobshop_bug: $(BIN_DIR)/jobshop_bug$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sjobshop_bug$(CLR_EXE_SUFFIX).exe

# csharp algorithm examples

$(BIN_DIR)/csknapsack$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csknapsack.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsknapsack$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scsknapsack.cs

# csharp graph examples

$(BIN_DIR)/csflow$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/csflow.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Scsflow$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Scsflow.cs

# Examples using multiple libraries.

$(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/techtalk_scheduling.cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$Stechtalk_scheduling.cs

techtalk_scheduling: $(BIN_DIR)/techtalk_scheduling$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe

# Build and compile custome CP examples

ccs: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX_DIR)/csharp/$(EX).cs
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$S$(EX)$(CLR_EXE_SUFFIX).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll $(EX_DIR)$Scsharp$S$(EX).cs

rcs: ccs
	$(MONO) $(BIN_DIR)$S$(EX)$(CLR_EXE_SUFFIX).exe $(ARGS)

# C# Fz support

csharpfz: \
	$(BIN_DIR)/Google.OrTools.Flatzinc.dll \
	$(BIN_DIR)/csfz$(CLR_EXE_SUFFIX).exe

$(GEN_DIR)/flatzinc/flatzinc_csharp_wrap.cc: \
	$(SRC_DIR)/flatzinc/csharp/flatzinc.swig \
	$(SRC_DIR)/base/base.swig $(SRC_DIR)/util/csharp/proto.swig
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -csharp -o $(GEN_DIR)$Sflatzinc$Sflatzinc_csharp_wrap.cc -module operations_research_flatzinc -namespace $(BASE_CLR_DLL_NAME).Flatzinc -dllimport "Google.OrTools.Flatzinc.$(DYNAMIC_SWIG_LIB_SUFFIX)" -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sflatzinc $(SRC_DIR)/flatzinc$Scsharp$Sflatzinc.swig

$(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O: $(GEN_DIR)/flatzinc/flatzinc_csharp_wrap.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/flatzinc/flatzinc_csharp_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sflatzinc_csharp_wrap.$O

$(BIN_DIR)/Google.OrTools.Flatzinc.dll: \
	$(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O \
	$(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs \
	$(STATIC_FLATZINC_DEPS)
ifeq ($(SYSTEM),win)
	$(CSC) /target:module /out:$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc.netmodule /warn:0 /nologo /debug $(GEN_DIR)\\com\\google\\ortools\\flatzinc\\*.cs $(GEN_DIR)\\com\\google\\ortools\\CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(SIGNING_FLAGS) $(LDOUT)$(BIN_DIR)$SGoogle.OrTools.Flatzinc.dll $(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc.netmodule $(OBJ_DIR)$Sswig$Sflatzinc_csharp_wrap.$O $(STATIC_FLATZINC_LNK) $(STATIC_LD_FLAGS)
else
	$(CSC) /target:library /out:$(BIN_DIR)/Google.OrTools.Flatzinc.dll /warn:0 /nologo /debug $(GEN_DIR)/com/google/ortools/flatzinc/*.cs $(GEN_DIR)/com/google/ortools/CommonAssemblyAttributes.cs
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)Google.OrTools.Flatzinc.$(DYNAMIC_SWIG_LIB_SUFFIX) $(OBJ_DIR)/swig/flatzinc_csharp_wrap.$O $(STATIC_FLATZINC_LNK) $(STATIC_LD_FLAGS)
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
	tools\mkdir temp\or-tools\data
	tools\mkdir temp\or-tools\data\discrete_tomography
	tools\mkdir temp\or-tools\data\fill_a_pix
	tools\mkdir temp\or-tools\data\minesweeper
	tools\mkdir temp\or-tools\data\rogo
	tools\mkdir temp\or-tools\data\survo_puzzle
	tools\mkdir temp\or-tools\data\quasigroup_completion
	copy LICENSE-2.0.txt temp$Sor-tools
	copy tools\README.dotnet temp\or-tools\README
	copy bin\$(CLR_DLL_NAME).dll temp\or-tools\bin
	copy examples\csharp\*.cs temp\or-tools\examples
	copy examples\csharp\*.sln temp\or-tools\examples
	copy examples\csharp\solution\*.csproj temp\or-tools\examples\solution
	copy examples\csharp\solution\Properties\*.cs temp\or-tools\examples\solution\Properties
	copy data\discrete_tomography\* temp\or-tools\data\discrete_tomography
	copy data\fill_a_pix\* temp\or-tools\data\fill_a_pix
	copy data\minesweeper\* temp\or-tools\data\minesweeper
	copy data\rogo\* temp\or-tools\data\rogo
	copy data\survo_puzzle\* temp\or-tools\data\survo_puzzle
	copy data\quasigroup_completion\* temp\or-tools\data\quasigroup_completion
	copy tools\or-tools.nuspec temp\or-tools
	$(SED) -i -e "s/VVVV/$(GIT_REVISION)/g" temp\or-tools\or-tools.nuspec
	cd temp\or-tools && nuget pack or-tools.nuspec
	cd temp\or-tools && nuget push Google.OrTools.2.0.$(GIT_REVISION).nupkg
endif

dotnet_archive: csharp
	-$(DELREC) temp
ifeq ("$(SYSTEM)","win")
	tools\mkdir temp
	tools\mkdir temp\or-tools.$(PORT)
	tools\mkdir temp\or-tools.$(PORT)\bin
	tools\mkdir temp\or-tools.$(PORT)\examples
	tools\mkdir temp\or-tools.$(PORT)\examples\solution
	tools\mkdir temp\or-tools.$(PORT)\examples\solution\Properties
	tools\mkdir temp\or-tools.$(PORT)\data
	tools\mkdir temp\or-tools.$(PORT)\data\discrete_tomography
	tools\mkdir temp\or-tools.$(PORT)\data\fill_a_pix
	tools\mkdir temp\or-tools.$(PORT)\data\minesweeper
	tools\mkdir temp\or-tools.$(PORT)\data\rogo
	tools\mkdir temp\or-tools.$(PORT)\data\survo_puzzle
	tools\mkdir temp\or-tools.$(PORT)\data\quasigroup_completion
	copy LICENSE-2.0.txt temp$Sor-tools.$(PORT)
	copy tools\README.dotnet temp\or-tools.$(PORT)\README
	copy bin\$(CLR_DLL_NAME).dll temp\or-tools.$(PORT)\bin
	copy examples\csharp\*.cs temp\or-tools.$(PORT)\examples
	copy examples\csharp\*.sln temp\or-tools.$(PORT)\examples
	copy examples\csharp\solution\*.csproj temp\or-tools.$(PORT)\examples\solution
	copy examples\csharp\solution\Properties\*.cs temp\or-tools.$(PORT)\examples\solution\Properties
	copy data\discrete_tomography\* temp\or-tools.$(PORT)\data\discrete_tomography
	copy data\fill_a_pix\* temp\or-tools.$(PORT)\data\fill_a_pix
	copy data\minesweeper\* temp\or-tools.$(PORT)\data\minesweeper
	copy data\rogo\* temp\or-tools.$(PORT)\data\rogo
	copy data\survo_puzzle\* temp\or-tools.$(PORT)\data\survo_puzzle
	copy data\quasigroup_completion\* temp\or-tools.$(PORT)\data\quasigroup_completion
	cd temp && ..\tools\zip$(CLR_EXE_SUFFIX).exe -r ..\$(CLR_DLL_NAME).NET.$(PORT).$(GIT_REVISION).zip or-tools.$(PORT)
else
	mkdir temp
	mkdir temp/or-tools.$(PORT)
	mkdir temp/or-tools.$(PORT)/bin
	mkdir temp/or-tools.$(PORT)/examples
	mkdir temp/or-tools.$(PORT)/data
	mkdir temp/or-tools.$(PORT)/data/discrete_tomography
	mkdir temp/or-tools.$(PORT)/data/fill_a_pix
	mkdir temp/or-tools.$(PORT)/data/minesweeper
	mkdir temp/or-tools.$(PORT)/data/rogo
	mkdir temp/or-tools.$(PORT)/data/survo_puzzle
	mkdir temp/or-tools.$(PORT)/data/quasigroup_completion
	cp LICENSE-2.0.txt temp/or-tools.$(PORT)
	cp tools/README.dotnet temp/or-tools.$(PORT)/README
	cp bin/$(CLR_DLL_NAME).dll temp/or-tools.$(PORT)/bin
	cp lib/lib$(CLR_DLL_NAME).so temp/or-tools.$(PORT)/bin
	cp examples/csharp/*.cs temp/or-tools.$(PORT)/examples
	cp data/discrete_tomography/* temp/or-tools.$(PORT)/data/discrete_tomography
	cp data/fill_a_pix/* temp/or-tools.$(PORT)/data/fill_a_pix
	cp data/minesweeper/* temp/or-tools.$(PORT)/data/minesweeper
	cp data/rogo/* temp/or-tools.$(PORT)/data/rogo
	cp data/survo_puzzle/* temp/or-tools.$(PORT)/data/survo_puzzle
	cp data/quasigroup_completion/* temp/or-tools.$(PORT)/data/quasigroup_completion
	cd temp && tar cvzf ../$(CLR_DLL_NAME).NET.$(PORT).$(GIT_REVISION).tar.gz or-tools.$(PORT)
endif
	-$(DELREC) temp
