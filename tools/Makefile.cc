# OR_ROOT is the minimal prefix to define the root of or-tools, if we
# are compiling in the or-tools root, it is empty. Otherwise, it is
# $(OR_TOOLS_TOP)/ or $(OR_TOOLS_TOP)\\ depending on the platform. It
# contains the trailing separator if not empty.
#
# INC_DIR is like OR_ROOT, but with a default of '.' instead of
# empty.  It is used for instance in include directives (-I.).
#
# OR_ROOT_FULL is always the complete path to or-tools. It is useful
# to store path informations inside libraries for instance.
ifeq ($(OR_TOOLS_TOP),)
  OR_ROOT =
else
  ifeq "$(SHELL)" "cmd.exe"
    OR_ROOT = $(OR_TOOLS_TOP)\\
  else
    ifeq "$(SHELL)" "sh.exe"
      OR_ROOT = $(OR_TOOLS_TOP)\\
    else
      OR_ROOT = $(OR_TOOLS_TOP)/
    endif
  endif
endif

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

# Directories.
BIN_DIR = $(OR_ROOT)bin
OBJ_DIR = $(OR_ROOT)objs
EX_DIR  = $(OR_ROOT)examples
INC_DIR = $(OR_ROOT)include


# Unix specific part.
ifeq ("$(SYSTEM)","unix")
  OR_TOOLS_TOP ?= $(shell pwd)
  OR_ROOT_FULL=$(OR_TOOLS_TOP)

  LD_FLAGS = -lz -lrt -lpthread
  OS = $(shell uname -s)
  ifeq ($(OS),Linux)
    CCC = g++ -fPIC -std=c++0x
    ORTOOLS_LIB = -Wl,-rpath $(OR_ROOT_FULL)/lib -L$(OR_ROOT_FULL)/lib -lortools
    PLATFORM = LINUX
    LBITS = $(shell getconf LONG_BIT)
    ifeq ($(LBITS),64)
      PORT = Linux64
      PTRLENGTH = 64
      ARCH = -DARCH_K8
    else
      PORT = Linux32
      PTRLENGTH = 32
    endif
  endif
  ifeq ($(OS),Darwin) # Assume Mac Os X
    CCC = clang++ -fPIC -std=c++11
    ORTOOLS_LIB = -L$(OR_ROOT)lib -lortools
    PLATFORM = MACOSX
    PTRLENGTH = 64
    ARCH = -DARCH_K8
  endif
  O = o
  E =
  OBJ_OUT = -o #
  EXE_OUT = -o #
  DEL = rm -f
  S = /
  CLP_INC = -DUSE_CLP
  CBC_INC = -DUSE_CBC
  DEBUG = -O4 -DNDEBUG
  CFLAGS = $(DEBUG) -I$(INC_DIR) -I$(EX_DIR) $(ARCH) -Wno-deprecated \
      $(CBC_INC) $(CLP_INC)
endif

# Windows specific part.
ifeq ("$(SYSTEM)","win")
  ifeq ("$(Platform)", "X64")
    PLATFORM = x64
  endif
  ifeq ("$(Platform)", "x64")
    PLATFORM = x64
  endif
  ifeq ("$(PLATFORM)", "x64")
    CBC_PLATFORM = x64-$(VS_RELEASE)-Release
    PTRLENGTH = 64
  else
    PTRLENGTH = 32
    PORT = VisualStudio$(VISUAL_STUDIO)-32b
  endif
  OS = Windows
  OR_TOOLS_TOP_AUX = $(shell cd)
  OR_TOOLS_TOP = $(shell echo $(OR_TOOLS_TOP_AUX) | tools\\sed.exe -e "s/\\/\\\\/g" | tools\\sed.exe -e "s/ //g")
  CFLAGS= -nologo $(DEBUG) $(CBC_INC) $(CLP_INC)\
      /D__WIN32__ /I$(INC_DIR)\\src\\windows /DGFLAGS_DLL_DECL= \
      /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG= \
      /I$(INC_DIR) /I$(EX_DIR)
  LD_FLAGS = psapi.lib ws2_32.lib
  O=obj
  E=.exe
  OBJ_OUT = /Fo
  EXE_OUT = /Fe
  DEL = del
  S = \\
  DEBUG=/O2 -DNDEBUG
  CCC=cl /EHsc /MD /nologo
endif

# Cleaning

clean:
	-$(DEL) $(OR_ROOT)objs/*
	-$(DEL) $(OR_ROOT)bin/*

# Constraint Programming and Routing examples.

$(OBJ_DIR)/costas_array.$O: $(EX_DIR)/costas_array.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scostas_array.cc $(OBJ_OUT)$(OBJ_DIR)$Scostas_array.$O

$(BIN_DIR)/costas_array$E: $(OBJ_DIR)/costas_array.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/costas_array.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scostas_array$E

$(OBJ_DIR)/cryptarithm.$O:$(EX_DIR)/cryptarithm.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scryptarithm.cc $(OBJ_OUT)$(OBJ_DIR)$Scryptarithm.$O

$(BIN_DIR)/cryptarithm$E: $(OBJ_DIR)/cryptarithm.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cryptarithm.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scryptarithm$E

$(OBJ_DIR)/cvrptw.$O: $(EX_DIR)/cvrptw.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(BIN_DIR)/cvrptw$E: $(OBJ_DIR)/cvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw$E

$(OBJ_DIR)/dobble_ls.$O:$(EX_DIR)/dobble_ls.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sdobble_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sdobble_ls.$O

$(BIN_DIR)/dobble_ls$E: $(OBJ_DIR)/dobble_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dobble_ls.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdobble_ls$E

$(OBJ_DIR)/flexible_jobshop.$O:$(EX_DIR)/flexible_jobshop.cc $(INC_DIR)/constraint_solver/constraint_solver.h $(EX_DIR)/flexible_jobshop.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sflexible_jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sflexible_jobshop.$O

$(BIN_DIR)/flexible_jobshop$E: $(OBJ_DIR)/flexible_jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flexible_jobshop.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflexible_jobshop$E

$(OBJ_DIR)/golomb.$O:$(EX_DIR)/golomb.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sgolomb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgolomb.$O

$(BIN_DIR)/golomb$E: $(OBJ_DIR)/golomb.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/golomb.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sgolomb$E

$(OBJ_DIR)/jobshop.$O:$(EX_DIR)/jobshop.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sjobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop.$O

$(BIN_DIR)/jobshop$E: $(OBJ_DIR)/jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop$E

$(OBJ_DIR)/jobshop_ls.$O:$(EX_DIR)/jobshop_ls.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sjobshop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_ls.$O

$(BIN_DIR)/jobshop_ls$E: $(OBJ_DIR)/jobshop_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_ls.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_ls$E

$(OBJ_DIR)/jobshop_earlytardy.$O:$(EX_DIR)/jobshop_earlytardy.cc $(INC_DIR)/constraint_solver/constraint_solver.h $(EX_DIR)/jobshop_earlytardy.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sjobshop_earlytardy.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_earlytardy.$O

$(BIN_DIR)/jobshop_earlytardy$E: $(OBJ_DIR)/jobshop_earlytardy.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_earlytardy.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_earlytardy$E

$(OBJ_DIR)/magic_square.$O:$(EX_DIR)/magic_square.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Smagic_square.cc $(OBJ_OUT)$(OBJ_DIR)$Smagic_square.$O

$(BIN_DIR)/magic_square$E: $(OBJ_DIR)/magic_square.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/magic_square.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smagic_square$E

$(OBJ_DIR)/model_util.$O:$(EX_DIR)/model_util.cc $(GEN_DIR)/constraint_solver/model.pb.h $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Smodel_util.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_util.$O

$(BIN_DIR)/model_util$E: $(OBJ_DIR)/model_util.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/model_util.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smodel_util$E

$(OBJ_DIR)/multidim_knapsack.$O:$(EX_DIR)/multidim_knapsack.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Smultidim_knapsack.cc $(OBJ_OUT)$(OBJ_DIR)$Smultidim_knapsack.$O

$(BIN_DIR)/multidim_knapsack$E: $(OBJ_DIR)/multidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/multidim_knapsack.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smultidim_knapsack$E

$(OBJ_DIR)/network_routing.$O:$(EX_DIR)/network_routing.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Snetwork_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Snetwork_routing.$O

$(BIN_DIR)/network_routing$E: $(GRAPH_DEPS) $(OBJ_DIR)/network_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/network_routing.$O $(ORTOOLS_LIB) $(GRAPH_LNK) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snetwork_routing$E

$(OBJ_DIR)/nqueens.$O: $(EX_DIR)/nqueens.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Snqueens.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens.$O

$(BIN_DIR)/nqueens$E: $(OBJ_DIR)/nqueens.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens$E

$(OBJ_DIR)/pdptw.$O: $(EX_DIR)/pdptw.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Spdptw.cc $(OBJ_OUT)$(OBJ_DIR)$Spdptw.$O

$(BIN_DIR)/pdptw$E: $(OBJ_DIR)/pdptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/pdptw.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Spdptw$E

$(OBJ_DIR)/sports_scheduling.$O:$(EX_DIR)/sports_scheduling.cc $(INC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Ssports_scheduling.cc $(OBJ_OUT)$(OBJ_DIR)$Ssports_scheduling.$O

$(BIN_DIR)/sports_scheduling$E: $(OBJ_DIR)/sports_scheduling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/sports_scheduling.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssports_scheduling$E

$(OBJ_DIR)/tsp.$O: $(EX_DIR)/tsp.cc $(INC_DIR)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stsp.cc $(OBJ_OUT)$(OBJ_DIR)$Stsp.$O

$(BIN_DIR)/tsp$E: $(OBJ_DIR)/tsp.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/tsp.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Stsp$E

# Flow and linear assignment cpp

$(OBJ_DIR)/linear_assignment_api.$O:$(EX_DIR)/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Slinear_assignment_api.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment_api.$O

$(BIN_DIR)/linear_assignment_api$E: $(OBJ_DIR)/linear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_assignment_api.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_assignment_api$E

$(OBJ_DIR)/flow_api.$O:$(EX_DIR)/flow_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sflow_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sflow_api.$O

$(BIN_DIR)/flow_api$E: $(OBJ_DIR)/flow_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flow_api.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflow_api$E

# Linear Programming Examples

$(OBJ_DIR)/strawberry_fields_with_column_generation.$O: $(EX_DIR)/strawberry_fields_with_column_generation.cc $(INC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sstrawberry_fields_with_column_generation.cc $(OBJ_OUT)$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O

$(BIN_DIR)/strawberry_fields_with_column_generation$E: $(OBJ_DIR)/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sstrawberry_fields_with_column_generation$E

$(OBJ_DIR)/linear_programming.$O: $(EX_DIR)/linear_programming.cc $(INC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Slinear_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_programming.$O

$(BIN_DIR)/linear_programming$E: $(OBJ_DIR)/linear_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_programming.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_programming$E

$(OBJ_DIR)/linear_solver_protocol_buffers.$O: $(EX_DIR)/linear_solver_protocol_buffers.cc $(INC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Slinear_solver_protocol_buffers.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O

$(BIN_DIR)/linear_solver_protocol_buffers$E: $(OBJ_DIR)/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_solver_protocol_buffers$E

$(OBJ_DIR)/integer_programming.$O: $(EX_DIR)/integer_programming.cc $(INC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Sinteger_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Sinteger_programming.$O

$(BIN_DIR)/integer_programming$E: $(OBJ_DIR)/integer_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/integer_programming.$O $(ORTOOLS_LIB) $(LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sinteger_programming$E

# Debug

printport:
	@echo SHELL = $(SHELL)
	@echo OR_TOOLS_TOP = $(OR_TOOLS_TOP)
	@echo SYSTEM = $(SYSTEM)
	@echo PLATFORM = $(PLATFORM)
	@echo PTRLENGTH = $(PTRLENGTH)
	@echo SVNVERSION = $(SVNVERSION)
	@echo PORT = $(PORT)

# Include user makefile

-include $(OR_ROOT)Makefile.user
