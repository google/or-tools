# Let's discover something about where we run
ifeq ($(OS),Windows_NT)
OS = Windows
endif
ifeq ($(OS),Windows)
SYSTEM = win
else
SYSTEM = unix
endif

.SECONDARY:

# Useful directories.
INC_DIR = include
EX_DIR = examples
CPP_EX_DIR = examples/cpp
CPP_EX_PATH = $(subst /,$S,$(CPP_EX_DIR))
JAVA_EX_DIR = examples/java
JAVA_EX_PATH = $(subst /,$S,$(JAVA_EX_DIR))
DOTNET_EX_DIR = examples/dotnet
DOTNET_EX_PATH = $(subst /,$S,$(DOTNET_EX_DIR))
OBJ_DIR = objs
CLASS_DIR = classes
LIB_DIR = lib
BIN_DIR = bin

# Unix specific part.
ifeq ($(SYSTEM),unix)
  OS = $(shell uname -s)
# C++
  ifeq ($(OS),Linux)
    CXX = g++
    LDFLAGS = \
-Wl,-rpath,"\$$ORIGIN" \
-Wl,-rpath,"\$$ORIGIN/../lib64" \
-Wl,-rpath,"\$$ORIGIN/../lib" \
-lz -lrt -lpthread
    LBITS = $(shell getconf LONG_BIT)
    ifeq ($(LBITS),64)
      PORT = Linux64
      ARCH = -DARCH_K8
      NETPLATFORM = anycpu
    else
      PORT = Linux32
      ARCH =
      NETPLATFORM = x86
    endif
    MONO = LD_LIBRARY_PATH=$(LIB_DIR):$(LD_LIBRARY_PATH) mono
    L = .so
  endif # ifeq($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac Os X
    CXX = clang++
    LDFLAGS = \
-Wl,-rpath,@loader_path \
-Wl,-rpath,@loader_path/../lib \
-lz -framework CoreFoundation
    PORT = MacOsX64
    ARCH = -DARCH_K8
    NETPLATFORM = x64
    MONO = DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR):$(DYLD_LIBRARY_PATH) mono
    L = .dylib
  endif # ifeq($(OS),Darwin)
  CXX_BIN := $(shell command -v $(CXX) 2> /dev/null)
  DEBUG = -O4 -DNDEBUG
  CXXFLAGS = -fPIC -std=c++11 $(DEBUG) \
 -I$(INC_DIR) -I. $(ARCH) -Wno-deprecated \
 -DUSE_CBC -DUSE_CLP -DUSE_BOP -DUSE_GLOP
  LIB_PREFIX = lib
  PRE_LIB = -Llib -Llib64
  CBC_LNK = -lCbcSolver -lCbc -lOsiCbc -lCgl -lClpSolver -lClp -lOsiClp -lOsi -lCoinUtils
  ABSL_LNK = \
-labsl_bad_any_cast_impl \
-labsl_bad_optional_access \
-labsl_bad_variant_access \
-labsl_base \
-labsl_city \
-labsl_civil_time \
-labsl_debugging_internal \
-labsl_demangle_internal \
-labsl_dynamic_annotations \
-labsl_examine_stack \
-labsl_failure_signal_handler \
-labsl_graphcycles_internal \
-labsl_hash \
-labsl_hashtablez_sampler \
-labsl_int128 \
-labsl_leak_check \
-labsl_malloc_internal \
-labsl_optional \
-labsl_raw_hash_set \
-labsl_spinlock_wait \
-labsl_stacktrace \
-labsl_str_format_internal \
-labsl_strings \
-labsl_strings_internal \
-labsl_symbolize \
-labsl_synchronization \
-labsl_throw_delegate \
-labsl_time \
-labsl_time_zone
  OR_TOOLS_LNK = $(PRE_LIB) $(ABSL_LNK) -lprotobuf -lglog -lgflags $(CBC_LNK) -lortools
  OBJ_OUT = -o #
  EXE_OUT = -o #
  O = .o
  J = .jar
  D = .dll
  E =
# Java
ifneq ($(JAVA_HOME),)
  JAVAC_BIN := $(shell command -v $(JAVA_HOME)/bin/javac 2> /dev/null)
  JAR_BIN := $(shell command -v $(JAVA_HOME)/bin/jar 2> /dev/null)
  JAVA_BIN := $(shell command -v $(JAVA_HOME)/bin/java 2> /dev/null)
else
  JAVAC_BIN := $(shell command -v javac 2> /dev/null)
  JAR_BIN := $(shell command -v jar 2> /dev/null)
  JAVA_BIN := $(shell command -v java 2> /dev/null)
endif
  JAVAFLAGS = -Djava.library.path=$(LIB_DIR)
  CPSEP = :
# .Net
  DOTNET = dotnet
  DOTNET_BIN := $(shell command -v $(DOTNET) 2> /dev/null)
# Makefile
  S = /
  DEL = rm -f
  DEL_REC = rm -rf
  MKDIR = mkdir
endif # SYSTEM == unix

# Windows specific part.
ifeq ($(SYSTEM),win)
  WHICH = tools\\win\\which.exe
  ifeq ("$(Platform)","X64")
    PLATFORM = Win64
  endif
  ifeq ("$(Platform)","x64")
    PLATFORM = Win64
  endif
  ifeq ("$(PLATFORM)","Win64")
    PORT = VisualStudio$(VISUAL_STUDIO)-64b
    NETPLATFORM = x64
  else
    PORT = VisualStudio$(VISUAL_STUDIO)-32b
    NETPLATFORM = x86
  endif
  CXX = cl
  # We can't use `where` since it's return all matching pathnames
  # so we ship which.exe and use it
  CXX_BIN := $(shell $(WHICH) $(CXX) 2> NUL)
  DEBUG = /O2 -DNDEBUG
  CXXFLAGS = /EHsc /MD /nologo /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS -nologo $(DEBUG) \
    /D__WIN32__ /DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS \
    /DGFLAGS_DLL_DECL= /DGFLAGS_DLL_DECLARE_FLAG= /DGFLAGS_DLL_DEFINE_FLAG= \
    /I$(INC_DIR)\\src\\windows /I$(INC_DIR) /I. \
    -DUSE_CBC -DUSE_CLP -DUSE_BOP -DUSE_GLOP
  LDFLAGS = psapi.lib ws2_32.lib
  LIB_PREFIX =
  OR_TOOLS_LNK = lib\\ortools.lib
  OBJ_OUT = /Fo
  EXE_OUT = /Fe
  O = .obj
  L = .lib
  J = .jar
  D = .dll
  E = .exe
# Java
  JAVAC_BIN := $(shell $(WHICH) javac 2> NUL)
  JAR_BIN := $(shell $(WHICH) jar 2> NUL)
  JAVA_BIN := $(shell $(WHICH) java 2> NUL)
  JAVAFLAGS = -Djava.library.path=$(LIB_DIR)
  CPSEP = ;
# .Net
  DOTNET = dotnet
  DOTNET_BIN := $(shell $(WHICH) $(DOTNET) 2> NUL)
# Makefile
  S = \\
  DEL = del
  DEL_REC = rd /S /Q
  MKDIR = md
endif # SYSTEM == win

OR_TOOLS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)ortools$L

.PHONY: all
all: detect cc java dotnet test

.PHONY: detect
detect: detect_port detect_cc detect_java detect_dotnet
	@echo SOURCE = $(SOURCE)
	@echo SOURCE_PATH = $(SOURCE_PATH)
	@echo SOURCE_NAME = $(SOURCE_NAME)
	@echo SOURCE_SUFFIX = $(SOURCE_SUFFIX)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

.PHONY: test
test: test_cc test_java test_dotnet

.PHONY: clean
clean:
	-$(DEL) $(EXE)
	-$(DEL_REC) $(OBJ_DIR)
	-$(DEL_REC) $(CLASS_DIR)

.PHONY: detect_port
detect_port:
	@echo SHELL = $(SHELL)
	@echo SYSTEM = $(SYSTEM)
	@echo PORT = $(PORT)
	@echo OS = $(OS)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

##############
##  SOURCE  ##
##############
# Check SOURCE argument
SOURCE_SUFFIX = $(suffix $(SOURCE))
# will contain “/any/path/foo.cc” on unix and “\\any\\path\\foo.cc” on windows
SOURCE_PATH = $(subst /,$S,$(SOURCE))
SOURCE_NAME = $(basename $(notdir $(SOURCE)))
ifeq ($(SOURCE),) # Those rules will be used if SOURCE is empty
.PHONY: build run
build run:
	$(error no source file provided, the "$@" target must be used like so: \
 make $@ SOURCE=relative/path/to/filename.extension)
else
ifeq (,$(wildcard $(SOURCE)))
$(error File "$(SOURCE)" does not exist !)
endif
endif

###########
##  C++  ##
###########
EXE = \
$(BIN_DIR)/simple_ls_program$E \
$(BIN_DIR)/rabbits_and_pheasants_cp$E \
$(BIN_DIR)/nurses_cp$E \
$(BIN_DIR)/minimal_jobshop_cp$E \
\
$(BIN_DIR)/constraint_programming_cp$E \
$(BIN_DIR)/costas_array_sat$E \
$(BIN_DIR)/cvrp_disjoint_tw$E \
$(BIN_DIR)/cvrptw$E \
$(BIN_DIR)/cvrptw_with_breaks$E \
$(BIN_DIR)/cvrptw_with_refueling$E \
$(BIN_DIR)/cvrptw_with_resources$E \
$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E \
$(BIN_DIR)/dimacs_assignment$E \
$(BIN_DIR)/dobble_ls$E \
$(BIN_DIR)/flow_api$E \
$(BIN_DIR)/frequency_assignment_problem$E \
$(BIN_DIR)/golomb_sat$E \
$(BIN_DIR)/integer_programming$E \
$(BIN_DIR)/jobshop_sat$E \
$(BIN_DIR)/knapsack$E \
$(BIN_DIR)/linear_assignment_api$E \
$(BIN_DIR)/linear_programming$E \
$(BIN_DIR)/linear_solver_protocol_buffers$E \
$(BIN_DIR)/magic_square_sat$E \
$(BIN_DIR)/max_flow$E \
$(BIN_DIR)/min_cost_flow$E \
$(BIN_DIR)/mps_driver$E \
$(BIN_DIR)/network_routing_sat$E \
$(BIN_DIR)/nqueens$E \
$(BIN_DIR)/random_tsp$E \
$(BIN_DIR)/pdptw$E \
$(BIN_DIR)/shift_minimization_sat$E \
$(BIN_DIR)/solve$E \
$(BIN_DIR)/sports_scheduling_sat$E \
$(BIN_DIR)/strawberry_fields_with_column_generation$E \
$(BIN_DIR)/tsp$E \
$(BIN_DIR)/vrp$E \
$(BIN_DIR)/weighted_tardiness_sat$E

.PHONY: cc test_cc
ifndef CXX_BIN
cc test_cc:
	@echo the $(CXX) command was not found in your PATH
	exit 127
else
cc: $(EXE)
test_cc: detect_cc
	$(MAKE) run SOURCE=examples/cpp/simple_knapsack_program.cc
	$(MAKE) run SOURCE=examples/cpp/simple_max_flow_program.cc
	$(MAKE) run SOURCE=examples/cpp/simple_min_cost_flow_program.cc
	$(MAKE) run SOURCE=examples/cpp/simple_lp_program.cc
	$(MAKE) run SOURCE=examples/cpp/simple_mip_program.cc
	$(MAKE) run SOURCE=examples/cpp/simple_sat_program.cc
	$(MAKE) run SOURCE=examples/cpp/simple_ls_program.cc
	$(MAKE) run SOURCE=examples/cpp/tsp.cc
	$(MAKE) run SOURCE=examples/cpp/vrp.cc
	$(MAKE) run SOURCE=examples/cpp/nurses_cp.cc
	$(MAKE) run SOURCE=examples/cpp/minimal_jobshop_cp.cc

##################
##  C++ SOURCE  ##
##################
ifeq ($(SOURCE_SUFFIX),.cc) # Those rules will be used if SOURCE contain a .cc file
$(OBJ_DIR)/$(SOURCE_NAME).$O: $(SOURCE) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) \
 -c $(SOURCE_PATH) \
 $(OBJ_OUT)$(OBJ_DIR)$S$(SOURCE_NAME).$O

$(BIN_DIR)/$(SOURCE_NAME)$E: $(OBJ_DIR)/$(SOURCE_NAME).$O | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) \
 $(OBJ_DIR)$S$(SOURCE_NAME).$O \
 $(OR_TOOLS_LNK) $(LDFLAGS) \
 $(EXE_OUT)$(BIN_DIR)$S$(SOURCE_NAME)$E

.PHONY: build # Build a C++ program.
build: $(BIN_DIR)/$(SOURCE_NAME)$E

.PHONY: run # Run a C++ program.
run: build
	$(BIN_DIR)$S$(SOURCE_NAME)$E $(ARGS)
endif # ifeq ($(SOURCE_SUFFIX),.cc)

endif # ifndef CXX_BIN

$(OBJ_DIR):
	-$(MKDIR) $(OBJ_DIR)

$(OBJ_DIR)/%$O: $(CPP_EX_DIR)/%.cc | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $(CPP_EX_PATH)$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*$O

$(BIN_DIR)/%$E: $(OBJ_DIR)/%$O
	$(CXX) $(CXXFLAGS) $(OBJ_DIR)$S$*$O $(OR_TOOLS_LNK) $(LDFLAGS) $(EXE_OUT)$(BIN_DIR)$S$*$E

.PHONY: detect_cc
detect_cc:
	@echo CXX = $(CXX)
	@echo CXX_BIN = $(CXX_BIN)
	@echo CXXFLAGS = $(CXXFLAGS)
	@echo LDFLAGS = $(LDFLAGS)
	@echo OR_TOOLS_LNK = $(OR_TOOLS_LNK)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

############
##  JAVA  ##
############
JAR = \
$(LIB_DIR)/LinearProgramming$J \
$(LIB_DIR)/IntegerProgramming$J \
$(LIB_DIR)/RabbitsPheasants$J \
$(LIB_DIR)/Tsp$J \
$(LIB_DIR)/Vrp$J \
$(LIB_DIR)/Knapsack$J \
\
$(LIB_DIR)/AllDifferentExcept0$J \
$(LIB_DIR)/AllInterval$J \
$(LIB_DIR)/CapacitatedVehicleRoutingProblemWithTimeWindows$J \
$(LIB_DIR)/Circuit$J \
$(LIB_DIR)/CoinsGrid$J \
$(LIB_DIR)/CoinsGridMIP$J \
$(LIB_DIR)/ColoringMIP$J \
$(LIB_DIR)/CoveringOpl$J \
$(LIB_DIR)/Crossword$J \
$(LIB_DIR)/DeBruijn$J \
$(LIB_DIR)/Diet$J \
$(LIB_DIR)/DietMIP$J \
$(LIB_DIR)/DivisibleBy9Through1$J \
$(LIB_DIR)/FlowExample$J \
$(LIB_DIR)/GolombRuler$J \
$(LIB_DIR)/Issue173$J \
$(LIB_DIR)/KnapsackMIP$J \
$(LIB_DIR)/LeastDiff$J \
$(LIB_DIR)/LinearAssignmentAPI$J \
$(LIB_DIR)/MagicSquare$J \
$(LIB_DIR)/Map2$J \
$(LIB_DIR)/Map$J \
$(LIB_DIR)/Minesweeper$J \
$(LIB_DIR)/MultiThreadTest$J \
$(LIB_DIR)/NQueens2$J \
$(LIB_DIR)/NQueens$J \
$(LIB_DIR)/Partition$J \
$(LIB_DIR)/QuasigroupCompletion$J \
$(LIB_DIR)/SendMoreMoney2$J \
$(LIB_DIR)/SendMoreMoney$J \
$(LIB_DIR)/SendMostMoney$J \
$(LIB_DIR)/Seseman$J \
$(LIB_DIR)/SetCovering2$J \
$(LIB_DIR)/SetCovering3$J \
$(LIB_DIR)/SetCovering4$J \
$(LIB_DIR)/SetCoveringDeployment$J \
$(LIB_DIR)/SetCovering$J \
$(LIB_DIR)/SimpleRoutingTest$J \
$(LIB_DIR)/StableMarriage$J \
$(LIB_DIR)/StiglerMIP$J \
$(LIB_DIR)/Strimko2$J \
$(LIB_DIR)/Sudoku$J \
$(LIB_DIR)/SurvoPuzzle$J \
$(LIB_DIR)/ToNum$J \
$(LIB_DIR)/WhoKilledAgatha$J \
$(LIB_DIR)/Xkcd$J \
$(LIB_DIR)/YoungTableaux$J

HAS_JAVA = true
ifndef JAVAC_BIN
HAS_JAVA =
endif
ifndef JAR_BIN
HAS_JAVA =
endif
ifndef JAVA_BIN
HAS_JAVA =
endif

.PHONY: java test_java
ifndef HAS_JAVA
java test_java:
	@echo the command 'java', 'javac' or 'jar' was not found in your PATH
	exit 127
else
java: $(JAR)
test_java: detect_java
	$(MAKE) run SOURCE=examples/java/SimpleLpProgram.java
	$(MAKE) run SOURCE=examples/java/SimpleMipProgram.java
	$(MAKE) run SOURCE=examples/java/SimpleSatProgram.java
	$(MAKE) run SOURCE=examples/java/Tsp.java
	$(MAKE) run SOURCE=examples/java/Vrp.java
	$(MAKE) run SOURCE=examples/java/Knapsack.java

###################
##  Java SOURCE  ##
###################
ifeq ($(SOURCE_SUFFIX),.java) # Those rules will be used if SOURCE contain a .java file
$(CLASS_DIR)/$(SOURCE_NAME): $(SOURCE) | $(CLASS_DIR)
	-$(MKDIR) $(CLASS_DIR)$S$(SOURCE_NAME)
	"$(JAVAC_BIN)" -d $(CLASS_DIR)$S$(SOURCE_NAME) \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(SOURCE_PATH)

$(LIB_DIR)/$(SOURCE_NAME)$J: $(CLASS_DIR)/$(SOURCE_NAME) | $(LIB_DIR)
	-$(DEL) $(LIB_DIR)$S$(SOURCE_NAME)$J
	"$(JAR_BIN)" cvf $(LIB_DIR)$S$(SOURCE_NAME)$J -C $(CLASS_DIR)$S$(SOURCE_NAME) .

.PHONY: build # Build a Java program.
build: $(LIB_DIR)/$(SOURCE_NAME)$J

.PHONY: run # Run a Java program.
run: build
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$(SOURCE_NAME)$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(SOURCE_NAME) $(ARGS)
endif # ifeq ($(SOURCE_SUFFIX),.java)

endif # ifndef HAS_JAVA

$(CLASS_DIR):
	-$(MKDIR) $(CLASS_DIR)

$(CLASS_DIR)/%: $(JAVA_EX_DIR)/%.java | $(CLASS_DIR)
	-$(MKDIR) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools$J$(CPSEP)$(LIB_DIR)$Sprotobuf$J \
 $(JAVA_EX_PATH)$S$*.java

$(LIB_DIR)/%$J: $(CLASS_DIR)/% | $(LIB_DIR)
	"$(JAR_BIN)" cvf $(LIB_DIR)$S$*$J -C $(CLASS_DIR)$S$* .

.PHONY: detect_java
detect_java:
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo CLASS_DIR = $(CLASS_DIR)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo JAVAFLAGS = $(JAVAFLAGS)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

##############
##  DOTNET  ##
##############
.PHONY: dotnet test_dotnet
ifndef DOTNET_BIN
dotnet test_dotnet:
	@echo the command 'dotnet' was not found in your PATH
	exit 127
else
dotnet:
	$(MAKE) run SOURCE=examples/dotnet/3_jugs_regular.cs
# Linear Solver
	$(MAKE) run SOURCE=examples/dotnet/SimpleLpProgram.cs
	$(MAKE) run SOURCE=examples/dotnet/SimpleMipProgram.cs
# Constraint Solver
	$(MAKE) run SOURCE=examples/dotnet/Tsp.cs
	$(MAKE) run SOURCE=examples/dotnet/Vrp.cs
# Sat
	$(MAKE) run SOURCE=examples/dotnet/NursesSat.cs
	$(MAKE) run SOURCE=examples/dotnet/JobshopSat.cs
	$(MAKE) run SOURCE=examples/dotnet/JobshopFt06Sat.cs
	$(MAKE) run SOURCE=examples/dotnet/GateSchedulingSat.cs
	$(MAKE) run SOURCE=examples/dotnet/TaskSchedulingSat.cs
# Misc
	$(MAKE) run SOURCE=examples/dotnet/cslinearprogramming.cs
	$(MAKE) run SOURCE=examples/dotnet/csintegerprogramming.cs
	$(MAKE) run SOURCE=examples/dotnet/assignment.cs
	$(MAKE) run SOURCE=examples/dotnet/alldifferent_except_0.cs
	$(MAKE) run SOURCE=examples/dotnet/all_interval.cs
	$(MAKE) run SOURCE=examples/dotnet/a_puzzle.cs
	$(MAKE) run SOURCE=examples/dotnet/a_round_of_golf.cs
	$(MAKE) run SOURCE=examples/dotnet/broken_weights.cs
	$(MAKE) run SOURCE=examples/dotnet/bus_schedule.cs
	$(MAKE) run SOURCE=examples/dotnet/circuit2.cs
	$(MAKE) run SOURCE=examples/dotnet/circuit.cs
	$(MAKE) run SOURCE=examples/dotnet/coins3.cs
	$(MAKE) run SOURCE=examples/dotnet/coins_grid.cs ARGS="5 2"
	$(MAKE) run SOURCE=examples/dotnet/combinatorial_auction2.cs
	$(MAKE) run SOURCE=examples/dotnet/contiguity_regular.cs
	$(MAKE) run SOURCE=examples/dotnet/contiguity_transition.cs
	$(MAKE) run SOURCE=examples/dotnet/costas_array.cs
	$(MAKE) run SOURCE=examples/dotnet/covering_opl.cs
	$(MAKE) run SOURCE=examples/dotnet/crew.cs
	$(MAKE) run SOURCE=examples/dotnet/crossword.cs
	$(MAKE) run SOURCE=examples/dotnet/crypta.cs
	$(MAKE) run SOURCE=examples/dotnet/crypto.cs
	$(MAKE) run SOURCE=examples/dotnet/cscvrptw.cs
	$(MAKE) run SOURCE=examples/dotnet/csflow.cs
	$(MAKE) run SOURCE=examples/dotnet/csknapsack.cs
	$(MAKE) run SOURCE=examples/dotnet/csls_api.cs
	$(MAKE) run SOURCE=examples/dotnet/csrabbitspheasants.cs
	$(MAKE) run SOURCE=examples/dotnet/cstsp.cs
	$(MAKE) run SOURCE=examples/dotnet/curious_set_of_integers.cs
	$(MAKE) run SOURCE=examples/dotnet/debruijn.cs
	$(MAKE) run SOURCE=examples/dotnet/csdiet.cs
	$(MAKE) run SOURCE=examples/dotnet/discrete_tomography.cs
	$(MAKE) run SOURCE=examples/dotnet/divisible_by_9_through_1.cs
	$(MAKE) run SOURCE=examples/dotnet/dudeney.cs
	$(MAKE) run SOURCE=examples/dotnet/einav_puzzle2.cs
	$(MAKE) run SOURCE=examples/dotnet/eq10.cs
	$(MAKE) run SOURCE=examples/dotnet/eq20.cs
	$(MAKE) run SOURCE=examples/dotnet/fill_a_pix.cs
	$(MAKE) run SOURCE=examples/dotnet/furniture_moving.cs
	$(MAKE) run SOURCE=examples/dotnet/furniture_moving_intervals.cs
	$(MAKE) run SOURCE=examples/dotnet/futoshiki.cs
	$(MAKE) run SOURCE=examples/dotnet/golomb_ruler.cs
	$(MAKE) run SOURCE=examples/dotnet/grocery.cs
	$(MAKE) run SOURCE=examples/dotnet/hidato_table.cs
	$(MAKE) run SOURCE=examples/dotnet/just_forgotten.cs
	$(MAKE) run SOURCE=examples/dotnet/kakuro.cs
	$(MAKE) run SOURCE=examples/dotnet/kenken2.cs
	$(MAKE) run SOURCE=examples/dotnet/killer_sudoku.cs
	$(MAKE) run SOURCE=examples/dotnet/labeled_dice.cs
	$(MAKE) run SOURCE=examples/dotnet/langford.cs
	$(MAKE) run SOURCE=examples/dotnet/least_diff.cs
	$(MAKE) run SOURCE=examples/dotnet/lectures.cs
	$(MAKE) run SOURCE=examples/dotnet/magic_sequence.cs
	$(MAKE) run SOURCE=examples/dotnet/magic_square_and_cards.cs
	$(MAKE) run SOURCE=examples/dotnet/magic_square.cs
	$(MAKE) run SOURCE=examples/dotnet/map2.cs
	$(MAKE) run SOURCE=examples/dotnet/map.cs
	$(MAKE) run SOURCE=examples/dotnet/marathon2.cs
	$(MAKE) run SOURCE=examples/dotnet/max_flow_taha.cs
	$(MAKE) run SOURCE=examples/dotnet/max_flow_winston1.cs
	$(MAKE) run SOURCE=examples/dotnet/minesweeper.cs
	$(MAKE) run SOURCE=examples/dotnet/mr_smith.cs
#	$(MAKE) run SOURCE=examples/dotnet/nontransitive_dice.cs # too long
	$(MAKE) run SOURCE=examples/dotnet/nqueens.cs
	$(MAKE) run SOURCE=examples/dotnet/nurse_rostering_regular.cs
	$(MAKE) run SOURCE=examples/dotnet/nurse_rostering_transition.cs
	$(MAKE) run SOURCE=examples/dotnet/olympic.cs
	$(MAKE) run SOURCE=examples/dotnet/organize_day.cs
	$(MAKE) run SOURCE=examples/dotnet/organize_day_intervals.cs
	$(MAKE) run SOURCE=examples/dotnet/pandigital_numbers.cs
#	$(MAKE) run SOURCE=examples/dotnet/partition.cs # too long
	$(MAKE) run SOURCE=examples/dotnet/perfect_square_sequence.cs
	$(MAKE) run SOURCE=examples/dotnet/photo_problem.cs
	$(MAKE) run SOURCE=examples/dotnet/place_number_puzzle.cs
	$(MAKE) run SOURCE=examples/dotnet/p_median.cs
	$(MAKE) run SOURCE=examples/dotnet/post_office_problem2.cs
	$(MAKE) run SOURCE=examples/dotnet/quasigroup_completion.cs
	$(MAKE) run SOURCE=examples/dotnet/regex.cs
	$(MAKE) run SOURCE=examples/dotnet/rogo2.cs
	$(MAKE) run SOURCE=examples/dotnet/scheduling_speakers.cs
	$(MAKE) run SOURCE=examples/dotnet/secret_santa2.cs
#	$(MAKE) run SOURCE=examples/dotnet/secret_santa.cs # too long
	$(MAKE) run SOURCE=examples/dotnet/send_more_money2.cs
	$(MAKE) run SOURCE=examples/dotnet/send_more_money.cs
	$(MAKE) run SOURCE=examples/dotnet/send_most_money.cs
	$(MAKE) run SOURCE=examples/dotnet/seseman.cs
	$(MAKE) run SOURCE=examples/dotnet/set_covering2.cs
	$(MAKE) run SOURCE=examples/dotnet/set_covering3.cs
	$(MAKE) run SOURCE=examples/dotnet/set_covering4.cs
	$(MAKE) run SOURCE=examples/dotnet/set_covering.cs
	$(MAKE) run SOURCE=examples/dotnet/set_covering_deployment.cs
	$(MAKE) run SOURCE=examples/dotnet/set_covering_skiena.cs
	$(MAKE) run SOURCE=examples/dotnet/set_partition.cs
	$(MAKE) run SOURCE=examples/dotnet/sicherman_dice.cs
	$(MAKE) run SOURCE=examples/dotnet/ski_assignment.cs
	$(MAKE) run SOURCE=examples/dotnet/stable_marriage.cs
	$(MAKE) run SOURCE=examples/dotnet/strimko2.cs
	$(MAKE) run SOURCE=examples/dotnet/subset_sum.cs
	$(MAKE) run SOURCE=examples/dotnet/sudoku.cs
	$(MAKE) run SOURCE=examples/dotnet/survo_puzzle.cs
	$(MAKE) run SOURCE=examples/dotnet/to_num.cs
	$(MAKE) run SOURCE=examples/dotnet/traffic_lights.cs
	$(MAKE) run SOURCE=examples/dotnet/volsay.cs
	$(MAKE) run SOURCE=examples/dotnet/volsay2.cs
	$(MAKE) run SOURCE=examples/dotnet/volsay3.cs
	$(MAKE) run SOURCE=examples/dotnet/wedding_optimal_chart.cs
	$(MAKE) run SOURCE=examples/dotnet/who_killed_agatha.cs
	$(MAKE) run SOURCE=examples/dotnet/xkcd.cs
	$(MAKE) run SOURCE=examples/dotnet/young_tableaux.cs
	$(MAKE) run SOURCE=examples/dotnet/zebra.cs
	$(MAKE) run SOURCE=examples/dotnet/fsintegerprogramming.fs
	$(MAKE) run SOURCE=examples/dotnet/fslinearprogramming.fs
	$(MAKE) run SOURCE=examples/dotnet/fsdiet.fs
	$(MAKE) run SOURCE=examples/dotnet/fsequality.fs
	$(MAKE) run SOURCE=examples/dotnet/fsequality-inequality.fs
	$(MAKE) run SOURCE=examples/dotnet/fsinteger-linear-program.fs
	$(MAKE) run SOURCE=examples/dotnet/fsknapsack.fs
	$(MAKE) run SOURCE=examples/dotnet/fsnetwork-max-flow.fs
	$(MAKE) run SOURCE=examples/dotnet/fsnetwork-max-flow-lpSolve.fs
	$(MAKE) run SOURCE=examples/dotnet/fsnetwork-min-cost-flow.fs
	$(MAKE) run SOURCE=examples/dotnet/fsProgram.fs
	$(MAKE) run SOURCE=examples/dotnet/fsrabbit-pheasant.fs
	$(MAKE) run SOURCE=examples/dotnet/fsvolsay3.fs
	$(MAKE) run SOURCE=examples/dotnet/fsvolsay3-lpSolve.fs
	$(MAKE) run SOURCE=examples/dotnet/fsvolsay.fs
#	$(MAKE) run SOURCE=examples/dotnet/word_square.cs # depends on /usr/share/dict/words

test_dotnet: detect_dotnet
	$(MAKE) run SOURCE=examples/dotnet/SimpleLpProgram.cs
	$(MAKE) run SOURCE=examples/dotnet/SimpleMipProgram.cs
	$(MAKE) run SOURCE=examples/dotnet/Tsp.cs
	$(MAKE) run SOURCE=examples/dotnet/Vrp.cs
	$(MAKE) run SOURCE=examples/dotnet/JobshopSat.cs
	$(MAKE) run SOURCE=examples/dotnet/NursesSat.cs

###################
##  .NET SOURCE  ##
###################
# .Net C#
ifeq ($(SOURCE_SUFFIX),.cs) # Those rules will be used if SOURCE contain a .cs file
ifeq (,$(wildcard $(SOURCE)proj))
$(error File "$(SOURCE)proj" does not exist !)
endif

.PHONY: build # Build a .Net C# program.
build: $(SOURCE) $(SOURCE)proj
	"$(DOTNET_BIN)" build $(SOURCE_PATH)proj

.PHONY: run # Run a .Net C# program.
run: build
	"$(DOTNET_BIN)" run --no-build --project $(SOURCE_PATH)proj -- $(ARGS)
endif # ifeq ($(SOURCE_SUFFIX),.cs)

# .Net F#
ifeq ($(SOURCE_SUFFIX),.fs) # Those rules will be used if SOURCE contain a .cs file
ifeq (,$(wildcard $(SOURCE)proj))
$(error File "$(SOURCE)proj" does not exist !)
endif

.PHONY: build # Build a .Net F# program.
build: $(SOURCE) $(SOURCE)proj
	"$(DOTNET_BIN)" build $(SOURCE_PATH)proj

.PHONY: run # Run a .Net F# program.
run: build
	"$(DOTNET_BIN)" run --no-build --project $(SOURCE_PATH)proj -- $(ARGS)
endif # ifeq ($(SOURCE_SUFFIX),.fs)

endif # ifndef DOTNET_BIN

.PHONY: detect_dotnet
detect_dotnet:
	@echo DOTNET_BIN = $(DOTNET_BIN)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

############
##  MISC  ##
############
# Include user makefile
-include Makefile.user

print-%	: ; @echo $* = $($*)
