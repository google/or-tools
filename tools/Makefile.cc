# OR_ROOT is the minimal prefix to define the root of or-tools, if we
# are compiling in the or-tools root, it is empty. Otherwise, it is
# $(OR_TOOLS_TOP)/ or $(OR_TOOLS_TOP)\\ depending on the platform. It
# contains the trailing separator if not empty.

# Let's discover something about where we run
# Let's discover something about where we run
ifeq ($(OS), Windows_NT)
  SYSTEM = win
else
  SYSTEM = unix
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
BIN_DIR = $(OR_ROOT)bin
OBJ_DIR = $(OR_ROOT)objs
EX_DIR = $(OR_ROOT)examples
CPP_EX_DIR = $(OR_ROOT)examples$Scpp
CS_EX_DIR = $(OR_ROOT)examples$Scsharp
INC_EX_DIR = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT)include
LIB_DIR = $(OR_ROOT)lib

CLR_DLL_NAME?=Google.OrTools

JAVAC_BIN = javac
JAVA_BIN = java

# Unix specific part.
ifeq ("$(SYSTEM)","unix")
	# Defines OR_TOOLS_TOP if it is not already defined.
	OR_TOOLS_TOP ?= $(shell pwd)
	OS = $(shell uname -s)
	LIB_PREFIX = lib
	PRE_LIB = -Wl,-rpath $(OR_TOOLS_TOP)/lib -L$(OR_TOOLS_TOP)/lib
	OR_TOOLS_LNK = $(PRE_LIB) -lortools
	CVRPTW_LNK = $(PRE_LIB) -lcvrptw_lib $(PRE_LIB) -lortools
	DIMACS_LNK = $(PRE_LIB) -ldimacs $(PRE_LIB) -lortools
	FAP_LNK = $(PRE_LIB) -lfap $(PRE_LIB) -lortools
	ifeq ($(OS),Linux)
		CPP_COMPILER = g++
		CCC = $(CPP_COMPILER) -fPIC -std=c++0x
		LD_FLAGS = -lz -lrt -lpthread
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
		LIB_SUFFIX = so
	endif
	ifeq ($(OS),Darwin) # Assume Mac Os X
		CPP_COMPILER = clang++
		CCC = $(CPP_COMPILER) -fPIC -std=c++11
		LD_FLAGS = -lz
		ARCH = -DARCH_K8
		PORT = MacOsX64
		CSC = mcs
		MONO =	DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR):$(DYLD_LIBRARY_PATH) mono64
		NETPLATFORM = x64
		LIB_SUFFIX = dylib
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
	WHICH = which
	TO_NULL = 2\> /dev/null
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
	LIB_PREFIX =
	PRE_LIB =
	LIB_SUFFIX = lib
	OR_TOOLS_LNK = lib\\ortools.lib
	CVRPTW_LNK = lib\\cvrptw_lib.lib lib\\ortools.lib
	DIMACS_LNK = lib\\dimacs.lib lib\\ortools.lib
	O=obj
	E=.exe
	OBJ_OUT = /Fo
	EXE_OUT = /Fe
	DEL = del
	S = \\
	CPSEP = ;
	DEBUG=/O2 -DNDEBUG
	CPP_COMPILER = cl
	CCC = $(CPP_COMPILER) /EHsc /MD /nologo
	CSC=csc
	MONO=
	WHICH = where
	TO_NULL = 2> NUL
endif

OR_TOOLS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
CVRPTW_LIBS = $(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX)
DIMACS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)dimacs.$(LIB_SUFFIX)
FAP_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fap.$(LIB_SUFFIX)

.PHONY: all clean test

all: cc test

cc: \
	$(BIN_DIR)/costas_array$E \
	$(BIN_DIR)/cryptarithm$E \
	$(BIN_DIR)/cvrp_disjoint_tw$E \
	$(BIN_DIR)/cvrptw$E \
	$(BIN_DIR)/cvrptw_with_breaks$E \
	$(BIN_DIR)/cvrptw_with_refueling$E \
	$(BIN_DIR)/cvrptw_with_resources$E \
	$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E \
	$(BIN_DIR)/dimacs_assignment$E \
	$(BIN_DIR)/dobble_ls$E \
	$(BIN_DIR)/flexible_jobshop$E \
	$(BIN_DIR)/golomb$E \
	$(BIN_DIR)/jobshop$E \
	$(BIN_DIR)/jobshop_ls$E \
	$(BIN_DIR)/jobshop_earlytardy$E \
	$(BIN_DIR)/magic_square$E \
	$(BIN_DIR)/model_util$E \
	$(BIN_DIR)/multidim_knapsack$E \
	$(BIN_DIR)/network_routing$E \
	$(BIN_DIR)/nqueens$E \
	$(BIN_DIR)/pdptw$E \
	$(BIN_DIR)/sports_scheduling$E \
	$(BIN_DIR)/tsp$E \
	$(BIN_DIR)/linear_assignment_api$E \
	$(BIN_DIR)/strawberry_fields_with_column_generation$E \
	$(BIN_DIR)/linear_programming$E \
	$(BIN_DIR)/linear_solver_protocol_buffers$E \
	$(BIN_DIR)/integer_programming$E \
	$(BIN_DIR)/flow_api$E


clean:
	$(DEL) $(BIN_DIR)$S*
	$(DEL) $(OBJ_DIR)$S*$O

ifeq ($(shell $(WHICH) $(CPP_COMPILER) $(TO_NULL)),)
test_cc ccc rcc:
	@echo the $(CPP_COMPILER) command was not find in your Path 
else
test_cc: $(BIN_DIR)/golomb$E $(BIN_DIR)/cvrptw$E
	$(BIN_DIR)$Sgolomb$E
	$(BIN_DIR)/cvrptw$E
# C++ generic running command
  ifeq ($(EX),)
ccc rcc:
	@echo No C++ file was provided, the $@ target must be used like so: \
	make $@ EX=path/to/the/example/example.cc
  else
ccc: $(BIN_DIR)$S$(basename $(notdir $(EX)))$E

rcc: $(BIN_DIR)$S$(basename $(notdir $(EX)))$E
	@echo running $(BIN_DIR)$S$(basename $(notdir $(EX)))$E
	$(BIN_DIR)$S$(basename $(notdir $(EX)))$E $(ARGS)
  endif
endif

ifeq ($(shell $(WHICH) java $(TO_NULL)),)
test_java rjava cjava:
	@echo the java command was not find in your Path 
else
test_java: 
	javac -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$STsp.java
	java -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Tsp $(ARGS)
	@echo $(WHICH) javaa $(TO_NULL)

# Java generic compilation command.
  ifeq ($(EX),)
rjava cjava:
	@echo No java file was provided, the rjava target must be used like so: \
	make rjava EX=path/to/the/example/example.java
  else
    ifeq ($(SYSTEM),win)
      EX_read_package = $(shell findstr /r "^package.*\;" $(EX))
    else
      EX_read_package = $(shell grep '^package.*\;' $(EX))
    endif
    EX_name = $(basename $(notdir $(EX)))
    EX_package = $(subst ;,,$(subst package ,,$(EX_read_package)))
    ifeq ($(EX_read_package),)
      EX_class_file = $(OBJ_DIR)$S$(EX_name).class
      EX_class = $(EX_name)
    else
    EX_class_file = $(OBJ_DIR)$S$(subst .,$S,$(EX_package))$S$(EX_name).class
    EX_class = $(EX_package).$(EX_name)
    endif
$(EX_class_file): $(LIB_DIR)$Scom.google.ortools.jar $(LIB_DIR)$Sprotobuf.jar  $(EX)
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX)

cjava: $(EX_class_file)

rjava: $(EX_class_file)
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_class) $(ARGS)
  endif
endif

ifeq ($(shell $(WHICH) $(CSC) $(TO_NULL)),)
test_cs:
	@echo the $(CSC) command was not find in your Path 
else
test_cs: 
	$(CSC) /target:exe /out:$(BIN_DIR)$Scsflow.exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:Google.OrTools.dll /r:Google.Protobuf.dll $(CS_EX_DIR)$Scsflow.cs
	$(MONO) $(BIN_DIR)$Scsflow.exe $(ARGS)

test: test_cc test_java test_cs

  ifeq ($(EX),)
rcs:
	@echo No csharp file was provided, the rcs target must be used like so: \
	make rcs EX=path/to/the/example/example.cs
  else
# .NET generic compilation command.
$(BIN_DIR)$S$(basename $(notdir $(EX))).exe: $(BIN_DIR)/$(CLR_DLL_NAME).dll $(EX)
	$(CSC) $(SIGNING_FLAGS) /target:exe /out:$(BIN_DIR)$S$(basename $(notdir $(EX))).exe /platform:$(NETPLATFORM) /lib:$(BIN_DIR) /r:$(CLR_DLL_NAME).dll /r:Google.Protobuf.dll $(EX)

csc: $(BIN_DIR)$S$(basename $(notdir $(EX))).exe

rcs: $(BIN_DIR)$S$(basename $(notdir $(EX))).exe
	@echo running $(BIN_DIR)$S$(basename $(notdir $(EX))).exe
	$(MONO) $(BIN_DIR)$S$(basename $(notdir $(EX))).exe $(ARGS)
  endif
endif


# Constraint Programming and Routing examples.

$(OBJ_DIR)$Scostas_array.$O: $(CPP_EX_DIR)$Scostas_array.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scostas_array.cc $(OBJ_OUT)$(OBJ_DIR)$Scostas_array.$O

$(BIN_DIR)/costas_array$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Scostas_array.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scostas_array.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scostas_array$E

$(OBJ_DIR)$Scryptarithm.$O:$(CPP_EX_DIR)$Scryptarithm.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scryptarithm.cc $(OBJ_OUT)$(OBJ_DIR)$Scryptarithm.$O

$(BIN_DIR)/cryptarithm$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Scryptarithm.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scryptarithm.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scryptarithm$E

$(OBJ_DIR)$Scvrp_disjoint_tw.$O: $(CPP_EX_DIR)$Scvrp_disjoint_tw.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrp_disjoint_tw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrp_disjoint_tw.$O

$(BIN_DIR)/cvrp_disjoint_tw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)$Scvrp_disjoint_tw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrp_disjoint_tw.$O $(CVRPTW_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrp_disjoint_tw$E

$(OBJ_DIR)$Scvrptw.$O: $(CPP_EX_DIR)$Scvrptw.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(BIN_DIR)/cvrptw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)$Scvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw.$O $(CVRPTW_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw$E

$(OBJ_DIR)$Scvrptw_with_breaks.$O: $(CPP_EX_DIR)$Scvrptw_with_breaks.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_breaks.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_breaks.$O

$(BIN_DIR)/cvrptw_with_breaks$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)$Scvrptw_with_breaks.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_breaks.$O $(CVRPTW_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_breaks$E

$(OBJ_DIR)$Scvrptw_with_refueling.$O: $(CPP_EX_DIR)$Scvrptw_with_refueling.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_refueling.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_refueling.$O

$(BIN_DIR)/cvrptw_with_refueling$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)$Scvrptw_with_refueling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_refueling.$O $(CVRPTW_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_refueling$E

$(OBJ_DIR)$Scvrptw_with_resources.$O: $(CPP_EX_DIR)$Scvrptw_with_resources.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_resources.$O

$(BIN_DIR)/cvrptw_with_resources$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)$Scvrptw_with_resources.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_resources.$O $(CVRPTW_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_resources$E

$(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O: $(CPP_EX_DIR)$Scvrptw_with_stop_times_and_resources.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Scvrptw_with_stop_times_and_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O

$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O $(CVRPTW_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_stop_times_and_resources$E

$(OBJ_DIR)$Sdimacs_assignment.$O:$(CPP_EX_DIR)$Sdimacs_assignment.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sdimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sdimacs_assignment.$O

$(BIN_DIR)/dimacs_assignment$E: $(OR_TOOLS_LIBS) $(DIMACS_LIBS) $(OBJ_DIR)$Sdimacs_assignment.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sdimacs_assignment.$O $(DIMACS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdimacs_assignment$E

$(OBJ_DIR)$Sdobble_ls.$O:$(CPP_EX_DIR)$Sdobble_ls.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sdobble_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sdobble_ls.$O

$(BIN_DIR)/dobble_ls$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sdobble_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sdobble_ls.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdobble_ls$E

$(OBJ_DIR)$Sflexible_jobshop.$O:$(CPP_EX_DIR)$Sflexible_jobshop.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h $(CPP_EX_DIR)$Sflexible_jobshop.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sflexible_jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sflexible_jobshop.$O

$(BIN_DIR)/flexible_jobshop$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sflexible_jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflexible_jobshop.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflexible_jobshop$E

$(OBJ_DIR)$Sgolomb.$O:$(CPP_EX_DIR)$Sgolomb.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sgolomb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgolomb.$O

$(BIN_DIR)/golomb$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sgolomb.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sgolomb.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sgolomb$E

$(OBJ_DIR)$Sjobshop.$O:$(CPP_EX_DIR)$Sjobshop.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sjobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop.$O

$(BIN_DIR)/jobshop$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sjobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sjobshop.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop$E

$(OBJ_DIR)$Sjobshop_ls.$O:$(CPP_EX_DIR)$Sjobshop_ls.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sjobshop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_ls.$O

$(BIN_DIR)/jobshop_ls$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sjobshop_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sjobshop_ls.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_ls$E

$(OBJ_DIR)$Sjobshop_earlytardy.$O:$(CPP_EX_DIR)$Sjobshop_earlytardy.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h $(CPP_EX_DIR)$Sjobshop_earlytardy.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sjobshop_earlytardy.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_earlytardy.$O

$(BIN_DIR)/jobshop_earlytardy$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sjobshop_earlytardy.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sjobshop_earlytardy.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_earlytardy$E

$(OBJ_DIR)$Smagic_square.$O:$(CPP_EX_DIR)$Smagic_square.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Smagic_square.cc $(OBJ_OUT)$(OBJ_DIR)$Smagic_square.$O

$(BIN_DIR)/magic_square$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Smagic_square.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Smagic_square.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smagic_square$E

$(OBJ_DIR)$Smodel_util.$O:$(CPP_EX_DIR)$Smodel_util.cc $(INC_DIR)$Sconstraint_solver$Smodel.pb.h $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Smodel_util.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_util.$O

$(BIN_DIR)/model_util$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Smodel_util.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Smodel_util.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smodel_util$E

$(OBJ_DIR)$Smultidim_knapsack.$O:$(CPP_EX_DIR)$Smultidim_knapsack.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Smultidim_knapsack.cc $(OBJ_OUT)$(OBJ_DIR)$Smultidim_knapsack.$O

$(BIN_DIR)/multidim_knapsack$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Smultidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Smultidim_knapsack.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smultidim_knapsack$E

$(OBJ_DIR)$Snetwork_routing.$O:$(CPP_EX_DIR)$Snetwork_routing.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Snetwork_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Snetwork_routing.$O

$(BIN_DIR)/network_routing$E: $(OR_TOOLS_LIBS) $(GRAPH_DEPS) $(OBJ_DIR)$Snetwork_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Snetwork_routing.$O $(OR_TOOLS_LNK) $(GRAPH_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snetwork_routing$E

$(OBJ_DIR)$Snqueens.$O: $(CPP_EX_DIR)$Snqueens.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Snqueens.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens.$O

$(BIN_DIR)/nqueens$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Snqueens.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Snqueens.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens$E

$(OBJ_DIR)$Spdptw.$O: $(CPP_EX_DIR)$Spdptw.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Spdptw.cc $(OBJ_OUT)$(OBJ_DIR)$Spdptw.$O

$(BIN_DIR)/pdptw$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Spdptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Spdptw.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Spdptw$E

$(OBJ_DIR)$Ssports_scheduling.$O:$(CPP_EX_DIR)$Ssports_scheduling.cc $(INC_DIR)$Sconstraint_solver$Sconstraint_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Ssports_scheduling.cc $(OBJ_OUT)$(OBJ_DIR)$Ssports_scheduling.$O

$(BIN_DIR)/sports_scheduling$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Ssports_scheduling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Ssports_scheduling.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssports_scheduling$E

$(OBJ_DIR)$Stsp.$O: $(CPP_EX_DIR)$Stsp.cc $(INC_DIR)$Sconstraint_solver$Srouting.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Stsp.cc $(OBJ_OUT)$(OBJ_DIR)$Stsp.$O

$(BIN_DIR)/tsp$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Stsp.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Stsp.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Stsp$E

# Flow and linear assignment cpp

$(OBJ_DIR)$Slinear_assignment_api.$O:$(CPP_EX_DIR)$Slinear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Slinear_assignment_api.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment_api.$O

$(BIN_DIR)/linear_assignment_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Slinear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Slinear_assignment_api.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_assignment_api$E

$(OBJ_DIR)$Sflow_api.$O:$(CPP_EX_DIR)$Sflow_api.cc
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sflow_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sflow_api.$O

$(BIN_DIR)/flow_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sflow_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflow_api.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflow_api$E

# Linear Programming Examples

$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O: $(CPP_EX_DIR)$Sstrawberry_fields_with_column_generation.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sstrawberry_fields_with_column_generation.cc $(OBJ_OUT)$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O

$(BIN_DIR)/strawberry_fields_with_column_generation$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sstrawberry_fields_with_column_generation$E

$(OBJ_DIR)$Slinear_programming.$O: $(CPP_EX_DIR)$Slinear_programming.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Slinear_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_programming.$O

$(BIN_DIR)/linear_programming$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Slinear_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Slinear_programming.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_programming$E

$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O: $(CPP_EX_DIR)$Slinear_solver_protocol_buffers.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Slinear_solver_protocol_buffers.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O

$(BIN_DIR)/linear_solver_protocol_buffers$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Slinear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Slinear_solver_protocol_buffers.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_solver_protocol_buffers$E

$(OBJ_DIR)$Sinteger_programming.$O: $(CPP_EX_DIR)$Sinteger_programming.cc $(INC_DIR)$Slinear_solver$Slinear_solver.h
	$(CCC) $(CFLAGS) -c $(CPP_EX_DIR)$Sinteger_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Sinteger_programming.$O

$(BIN_DIR)/integer_programming$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)$Sinteger_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sinteger_programming.$O $(OR_TOOLS_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sinteger_programming$E

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
