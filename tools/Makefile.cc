# OR_ROOT is the minimal prefix to define the root of or-tools, if we
# are compiling in the or-tools root, it is empty. Otherwise, it is
# $(OR_TOOLS_TOP)/ or $(OR_TOOLS_TOP)\\ depending on the platform. It
# contains the trailing separator if not empty.

# Let's discover something about where we run
ifeq "$(SHELL)" "cmd.exe"
	SYSTEM = win
else
	ifeq "$(SHELL)" "sh.exe"
		SYSTEM = win
	else
		SYSTEM = unix
	endif
endif

# Define the OR_ROOT directory.
ifeq ($(OR_TOOLS_TOP),)
	OR_ROOT =
else
	ifeq "$(SYSTEM)" "win"
		OR_ROOT = $(OR_TOOLS_TOP)\\
	else
		OR_ROOT = $(OR_TOOLS_TOP)/
	endif
endif

# Useful directories.
CPP_BIN_DIR = $(OR_ROOT)bin/cpp
CS_BIN_DIR = $(OR_ROOT)bin$Scsharp
OBJ_DIR = $(OR_ROOT)objs
CPP_EX_DIR = $(OR_ROOT)examples$Scpp
JAVA_EX_DIR = $(OR_ROOL)examples$Scom
CS_EX_DIR = $(OR_ROOT)examples$Scsharp
INC_EX_DIR = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT)include
LIB_DIR = $(OR_ROOT)lib

# Unix specific part.
ifeq ("$(SYSTEM)","unix")
	OS = $(shell uname -s)
	ifeq ($(OS),Linux)
		CCC = g++ -fPIC -std=c++0x
		LD_FLAGS = -lz -lrt -lpthread
		# Defines OR_TOOLS_TOP if it is not already defined.
		OR_TOOLS_TOP ?= $(shell pwd)
		LIB_PREFIX = -Wl,-rpath $(OR_TOOLS_TOP)/lib -L$(OR_TOOLS_TOP)/lib
		OR_TOOLS_LIBS = $(LIB_PREFIX) -lortools
		CVRPTW_LIBS = $(LIB_PREFIX) -lcvrptw_lib $(LIB_PREFIX) -lortools
		DIMACS_LIBS = $(LIB_PREFIX) -ldimacs $(LIB_PREFIX) -lortools
		LBITS = $(shell getconf LONG_BIT)
		ifeq ($(LBITS),64)
			PORT = Linux64
			ARCH = -DARCH_K8
			NETPLATFORM = anycpu
		else
			PORT = Linux32
			NETPLATFORM = x86
		endif
		CSC = mcs
		MONO = LD_LIBRARY_PATH=$(LIB_DIR):$(LD_LIBRARY_PATH) mono
	endif
	ifeq ($(OS),Darwin) # Assume Mac Os X
		CCC = clang++ -fPIC -std=c++11
		LD_FLAGS = -lz
		OR_TOOLS_LIBS = -L$(OR_ROOT)lib -lortools
		CVRPTW_LIBS = -L$(OR_ROOT)lib -lcvrptw_lib -lortools
		DIMACS_LIBS = -L$(OR_ROOT)lib -ldimacs -lortools
		ARCH = -DARCH_K8
		PORT = MacOsX64
		CSC = mcs
		MONO =	DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR):$(DYLD_LIBRARY_PATH) mono64
		NETPLATFORM = x64
	endif
	O = o
	E =
	OBJ_OUT = -o #
	EXE_OUT = -o #
	DEL = rm -f
	S = /
	CPSEP = :
	CLP_INC = -DUSE_CLP
	CBC_INC = -DUSE_CBC
	GLOP_INC = -DUSE_GLOP
	BOP_INT = -DUSE_BOP
	DEBUG = -O4 -DNDEBUG
	CFLAGS = $(DEBUG) -I$(INC_DIR) -I$(INC_EX_DIR) $(ARCH) -Wno-deprecated \
		$(CBC_INC) $(CLP_INC) $(GLOP_INC) $(BOP_INC)
endif

# Windows specific part.
ifeq ("$(SYSTEM)","win")
	ifeq ("$(Platform)", "X64")
		PLATFORM = Win64
	endif
	ifeq ("$(Platform)", "x64")
		PLATFORM = Win64
	endif
	ifeq ("$(PLATFORM)", "Win64")
		PORT = VisualStudio$(VISUAL_STUDIO)-64b
		NETPLATFORM = x64
	else
		PORT = VisualStudio$(VISUAL_STUDIO)-32b
		NETPLATFORM = x86
	endif
	CLP_INC = -DUSE_CLP
	CBC_INC = -DUSE_CBC
	GLOP_INC = -DUSE_GLOP
	BOP_INC = -DUSE_BOP
	CFLAGS= /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS -nologo $(DEBUG) \
		$(CBC_INC) $(CLP_INC) /D__WIN32__ /I$(INC_DIR)\\src\\windows \
		/DGFLAGS_DLL_DECL= /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG= \
		/I$(INC_DIR) /I$(INC_EX_DIR) $(GLOP_INC) $(BOP_INC)
	LD_FLAGS = psapi.lib ws2_32.lib
	OR_TOOLS_LIBS = lib\\ortools.lib
	CVRPTW_LIBS = lib\\cvrptw_lib.lib lib\\ortools.lib
	DIMACS_LIBS = lib\\dimacs.lib lib\\ortools.lib
	O=obj
	E=.exe
	OBJ_OUT = /Fo
	EXE_OUT = /Fe
	DEL = del
	S = \\
	CPSEP = ;
	DEBUG=/O2 -DNDEBUG
	CCC=cl /EHsc /MD /nologo
	CSC=csc
	MONO=
endif

.PHONY: all clean test

all: \
	$(CPP_BIN_DIR)/costas_array$E \
	$(CPP_BIN_DIR)/cryptarithm$E \
	$(CPP_BIN_DIR)/cvrp_disjoint_tw$E \
	$(CPP_BIN_DIR)/cvrptw$E \
	$(CPP_BIN_DIR)/cvrptw_with_refueling$E \
	$(CPP_BIN_DIR)/cvrptw_with_resources$E \
	$(CPP_BIN_DIR)/cvrptw_with_stop_times_and_resources$E \
	$(CPP_BIN_DIR)/dimacs_assignment$E \
	$(CPP_BIN_DIR)/dobble_ls$E \
	$(CPP_BIN_DIR)/flexible_jobshop$E \
	$(CPP_BIN_DIR)/golomb$E \
	$(CPP_BIN_DIR)/jobshop$E \
	$(CPP_BIN_DIR)/jobshop_ls$E \
	$(CPP_BIN_DIR)/jobshop_earlytardy$E \
	$(CPP_BIN_DIR)/magic_square$E \
	$(CPP_BIN_DIR)/model_util$E \
	$(CPP_BIN_DIR)/multidim_knapsack$E \
	$(CPP_BIN_DIR)/network_routing$E \
	$(CPP_BIN_DIR)/nqueens$E \
	$(CPP_BIN_DIR)/pdptw$E \
	$(CPP_BIN_DIR)/sports_scheduling$E \
	$(CPP_BIN_DIR)/tsp$E \
	$(CPP_BIN_DIR)/linear_assignment_api$E \
	$(CPP_BIN_DIR)/strawberry_fields_with_column_generation$E \
	$(CPP_BIN_DIR)/linear_programming$E \
	$(CPP_BIN_DIR)/linear_solver_protocol_buffers$E \
	$(CPP_BIN_DIR)/integer_programming$E \
	$(CPP_BIN_DIR)/flow_api$E


clean:
	$(DEL) $(CPP_BIN_DIR)$S*
	$(DEL) $(OBJ_DIR)$S*$O

test_cc: $(CPP_BIN_DIR)/golomb$E
	$(CPP_BIN_DIR)$Sgolomb$E

test_java: EX:=Tsp
test_java: 
	javac -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(JAVA_EX_DIR)$Sgoogle$Sortools$Ssamples$S$(EX).java
	java -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.$(EX) $(ARGS)

test_cs: EX:=csflow
test_cs: 
	$(CSC) /target:exe /out:$(CS_BIN_DIR)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(CS_BIN_DIR) /r:Google.OrTools.dll /r:Google.Protobuf.dll $(CS_EX_DIR)$S$(EX).cs
	$(MONO) $(CS_BIN_DIR)$S$(EX)$(CLR_EXE_SUFFIX).exe $(ARGS)

test: test_cc test_java test_cs


#test_csharp:
# make rcs EX=csflow



# Constraint Programming and Routing examples.

$(OBJ_DIR)$Scostas_array.$O: $(CPP_EX_DIR)$Scostas_array.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scostas_array.cc $(OBJ_OUT)$(OBJ_DIR)$Scostas_array.$O

$(CPP_BIN_DIR)/costas_array$E: $(OBJ_DIR)$Scostas_array.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scostas_array.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scostas_array$E

$(OBJ_DIR)$Scryptarithm.$O:$(CPP_EX_DIR)$Scryptarithm.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scryptarithm.cc $(OBJ_OUT)$(OBJ_DIR)$Scryptarithm.$O

$(CPP_BIN_DIR)/cryptarithm$E: $(OBJ_DIR)$Scryptarithm.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scryptarithm.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scryptarithm$E

$(OBJ_DIR)$Scvrp_disjoint_tw.$O: $(CPP_EX_DIR)$Scvrp_disjoint_tw.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrp_disjoint_tw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrp_disjoint_tw.$O

$(CPP_BIN_DIR)/cvrp_disjoint_tw$E: $(OBJ_DIR)$Scvrp_disjoint_tw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrp_disjoint_tw.$O $(CVRPTW_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scvrp_disjoint_tw$E

$(OBJ_DIR)$Scvrptw.$O: $(CPP_EX_DIR)$Scvrptw.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(CPP_BIN_DIR)/cvrptw$E: $(OBJ_DIR)$Scvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw.$O $(CVRPTW_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scvrptw$E

$(OBJ_DIR)$Scvrptw_with_refueling.$O: $(CPP_EX_DIR)$Scvrptw_with_refueling.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_refueling.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_refueling.$O

$(CPP_BIN_DIR)/cvrptw_with_refueling$E: $(OBJ_DIR)$Scvrptw_with_refueling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_refueling.$O $(CVRPTW_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scvrptw_with_refueling$E

$(OBJ_DIR)$Scvrptw_with_resources.$O: $(CPP_EX_DIR)$Scvrptw_with_resources.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_resources.$O

$(CPP_BIN_DIR)/cvrptw_with_resources$E: $(OBJ_DIR)$Scvrptw_with_resources.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_resources.$O $(CVRPTW_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scvrptw_with_resources$E

$(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O: $(CPP_EX_DIR)$Scvrptw_with_stop_times_and_resources.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_stop_times_and_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O

$(CPP_BIN_DIR)/cvrptw_with_stop_times_and_resources$E: $(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O $(CVRPTW_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Scvrptw_with_stop_times_and_resources$E

$(OBJ_DIR)$Sdimacs_assignment.$O:$(CPP_EX_DIR)$Sdimacs_assignment.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sdimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sdimacs_assignment.$O

$(CPP_BIN_DIR)/dimacs_assignment$E: $(OBJ_DIR)$Sdimacs_assignment.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sdimacs_assignment.$O $(DIMACS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sdimacs_assignment$E

$(OBJ_DIR)$Sdobble_ls.$O:$(CPP_EX_DIR)$Sdobble_ls.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sdobble_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sdobble_ls.$O

$(CPP_BIN_DIR)/dobble_ls$E: $(OBJ_DIR)$Sdobble_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sdobble_ls.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sdobble_ls$E

$(OBJ_DIR)$Sflexible_jobshop.$O:$(CPP_EX_DIR)$Sflexible_jobshop.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h $(CPP_EX_DIR)$Sflexible_jobshop.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sflexible_jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sflexible_jobshop.$O

$(CPP_BIN_DIR)/flexible_jobshop$E: $(OBJ_DIR)$Sflexible_jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflexible_jobshop.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sflexible_jobshop$E

$(OBJ_DIR)$Sgolomb.$O:$(CPP_EX_DIR)$Sgolomb.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sgolomb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgolomb.$O

$(CPP_BIN_DIR)/golomb$E: $(OBJ_DIR)$Sgolomb.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sgolomb.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sgolomb$E

$(OBJ_DIR)$Sjobshop.$O:$(CPP_EX_DIR)$Sjobshop.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sjobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop.$O

$(CPP_BIN_DIR)/jobshop$E: $(OBJ_DIR)$Sjobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sjobshop.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sjobshop$E

$(OBJ_DIR)$Sjobshop_ls.$O:$(CPP_EX_DIR)$Sjobshop_ls.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sjobshop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_ls.$O

$(CPP_BIN_DIR)/jobshop_ls$E: $(OBJ_DIR)$Sjobshop_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sjobshop_ls.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sjobshop_ls$E

$(OBJ_DIR)$Sjobshop_earlytardy.$O:$(CPP_EX_DIR)$Sjobshop_earlytardy.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h $(CPP_EX_DIR)$Sjobshop_earlytardy.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sjobshop_earlytardy.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_earlytardy.$O

$(CPP_BIN_DIR)/jobshop_earlytardy$E: $(OBJ_DIR)$Sjobshop_earlytardy.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sjobshop_earlytardy.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sjobshop_earlytardy$E

$(OBJ_DIR)$Smagic_square.$O:$(CPP_EX_DIR)$Smagic_square.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Smagic_square.cc $(OBJ_OUT)$(OBJ_DIR)$Smagic_square.$O

$(CPP_BIN_DIR)/magic_square$E: $(OBJ_DIR)$Smagic_square.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Smagic_square.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Smagic_square$E

$(OBJ_DIR)$Smodel_util.$O:$(CPP_EX_DIR)$Smodel_util.cc $(INC_DIR)$Sconstraint_solver$Smodel.pb.h $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Smodel_util.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_util.$O

$(CPP_BIN_DIR)/model_util$E: $(OBJ_DIR)$Smodel_util.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Smodel_util.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Smodel_util$E

$(OBJ_DIR)$Smultidim_knapsack.$O:$(CPP_EX_DIR)$Smultidim_knapsack.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Smultidim_knapsack.cc $(OBJ_OUT)$(OBJ_DIR)$Smultidim_knapsack.$O

$(CPP_BIN_DIR)/multidim_knapsack$E: $(OBJ_DIR)$Smultidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Smultidim_knapsack.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Smultidim_knapsack$E

$(OBJ_DIR)$Snetwork_routing.$O:$(CPP_EX_DIR)$Snetwork_routing.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Snetwork_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Snetwork_routing.$O

$(CPP_BIN_DIR)/network_routing$E: $(GRAPH_DEPS) $(OBJ_DIR)$Snetwork_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Snetwork_routing.$O $(OR_TOOLS_LIBS) $(GRAPH_LNK) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Snetwork_routing$E

$(OBJ_DIR)$Snqueens.$O: $(CPP_EX_DIR)$Snqueens.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Snqueens.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens.$O

$(CPP_BIN_DIR)/nqueens$E: $(OBJ_DIR)$Snqueens.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Snqueens.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Snqueens$E

$(OBJ_DIR)$Spdptw.$O: $(CPP_EX_DIR)$Spdptw.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Spdptw.cc $(OBJ_OUT)$(OBJ_DIR)$Spdptw.$O

$(CPP_BIN_DIR)/pdptw$E: $(OBJ_DIR)$Spdptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Spdptw.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Spdptw$E

$(OBJ_DIR)$Ssports_scheduling.$O:$(CPP_EX_DIR)$Ssports_scheduling.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Ssports_scheduling.cc $(OBJ_OUT)$(OBJ_DIR)$Ssports_scheduling.$O

$(CPP_BIN_DIR)/sports_scheduling$E: $(OBJ_DIR)$Ssports_scheduling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Ssports_scheduling.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Ssports_scheduling$E

$(OBJ_DIR)$Stsp.$O: $(CPP_EX_DIR)$Stsp.cc $(INC_DIR)$Sconstraint_solver$Srouting.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Stsp.cc $(OBJ_OUT)$(OBJ_DIR)$Stsp.$O

$(CPP_BIN_DIR)/tsp$E: $(OBJ_DIR)$Stsp.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Stsp.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Stsp$E

# Flow and linear assignment cpp

$(OBJ_DIR)$Slinear_assignment_api.$O:$(CPP_EX_DIR)$Slinear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Slinear_assignment_api.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment_api.$O

$(CPP_BIN_DIR)/linear_assignment_api$E: $(OBJ_DIR)$Slinear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Slinear_assignment_api.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Slinear_assignment_api$E

$(OBJ_DIR)$Sflow_api.$O:$(CPP_EX_DIR)$Sflow_api.cc
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sflow_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sflow_api.$O

$(CPP_BIN_DIR)/flow_api$E: $(OBJ_DIR)$Sflow_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflow_api.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sflow_api$E

# Linear Programming Examples

$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O: $(CPP_EX_DIR)$Sstrawberry_fields_with_column_generation.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sstrawberry_fields_with_column_generation.cc $(OBJ_OUT)$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O

$(CPP_BIN_DIR)/strawberry_fields_with_column_generation$E: $(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sstrawberry_fields_with_column_generation$E

$(OBJ_DIR)$Slinear_programming.$O: $(CPP_EX_DIR)$Slinear_programming.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Slinear_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_programming.$O

$(CPP_BIN_DIR)/linear_programming$E: $(OBJ_DIR)$Slinear_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Slinear_programming.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Slinear_programming$E

$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O: $(CPP_EX_DIR)$Slinear_solver_protocol_buffers.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Slinear_solver_protocol_buffers.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O

$(CPP_BIN_DIR)/linear_solver_protocol_buffers$E: $(OBJ_DIR)$Slinear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Slinear_solver_protocol_buffers.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Slinear_solver_protocol_buffers$E

$(OBJ_DIR)$Sinteger_programming.$O: $(CPP_EX_DIR)$Sinteger_programming.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sinteger_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Sinteger_programming.$O

$(CPP_BIN_DIR)/integer_programming$E: $(OBJ_DIR)$Sinteger_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sinteger_programming.$O $(OR_TOOLS_LIBS) $(LD_FLAGS) $(EXE_OUT)$(CPP_BIN_DIR)$Sinteger_programming$E

# Java generic compilation command.

$(OBJ_DIR)$Scom$Sgoogle$Sortools$Ssamples$S$(EX).class: $(JAVA_EX_DIR)$Sgoogle$Sortools$Ssamples$S$(EX).java
	javac -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(JAVA_EX_DIR)$Sgoogle$Sortools$Ssamples$S$(EX).java

cjava: $(OBJ_DIR)$Scom$Sgoogle$Sortools$Ssamples$S$(EX).class

rjava: $(OBJ_DIR)$Scom$Sgoogle$Sortools$Ssamples$S$(EX).class
	java -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.$(EX) $(ARGS)

# .NET generic compilation command.

ccs: $(CS_EX_DIR)$S$(EX).cs
	$(CSC) /target:exe /out:$(CS_BIN_DIR)$S$(EX).exe /platform:$(NETPLATFORM) /lib:$(CS_BIN_DIR) /r:Google.OrTools.dll /r:Google.Protobuf.dll $(CS_EX_DIR)$S$(EX).cs

rcs: ccs
	$(MONO) $(CS_BIN_DIR)$S$(EX)$(CLR_EXE_SUFFIX).exe $(ARGS)

# Debug

printport:
	@echo SHELL = $(SHELL)
	@echo SYSTEM = $(SYSTEM)
	@echo PORT = $(PORT)
	@echo OS = $(OS)
	@echo CCC = $(CCC)

# Include user makefile

-include $(OR_ROOT)Makefile.user

print-%	: ; @echo $* = $($*)
