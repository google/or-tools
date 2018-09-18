# ---------- Java support using SWIG ----------
.PHONY: help_java # Generate list of Java targets with descriptions.
help_java:
	@echo Use one of the following Java targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.java.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.java.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t24
	@echo
endif

JAVA_OR_TOOLS_LIBS= $(LIB_DIR)/com.google.ortools$J
JAVA_OR_TOOLS_NATIVE_LIBS := $(LIB_DIR)/$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT)
JAVAFLAGS = -Djava.library.path=$(LIB_DIR)

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

# Main target
.PHONY: java # Build Java OR-Tools.
.PHONY: test_java # Test Java OR-Tools using various examples.
ifndef HAS_JAVA
java:
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	$(warning Cannot find 'java' command which is needed for build. Please make sure it is installed and in system path. Check Makefile.local for more information.)
test_java: java
else
java: $(JAVA_OR_TOOLS_LIBS)
test_java: \
 test_java_tests \
 test_java_samples \
 test_java_examples
BUILT_LANGUAGES +=, Java
endif

$(GEN_DIR)/com/google/ortools/algorithms:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms

$(GEN_DIR)/com/google/ortools/constraintsolver:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver

$(GEN_DIR)/com/google/ortools/graph:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph

$(GEN_DIR)/com/google/ortools/linearsolver:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver

$(GEN_DIR)/com/google/ortools/flatzinc:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sflatzinc

$(GEN_DIR)/com/google/ortools/sat:
	$(MKDIR_P) $(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat

$(CLASS_DIR):
	$(MKDIR_P) $(CLASS_DIR)

$(CLASS_DIR)/com: | $(CLASS_DIR)
	$(MKDIR) $(CLASS_DIR)$Scom

$(CLASS_DIR)/com/google: | $(CLASS_DIR)/com
	$(MKDIR) $(CLASS_DIR)$Scom$Sgoogle

$(CLASS_DIR)/com/google/ortools: | $(CLASS_DIR)/com/google
	$(MKDIR) $(CLASS_DIR)$Scom$Sgoogle$Sortools

################
##  JAVA JNI  ##
################
$(GEN_DIR)/ortools/constraint_solver/constraint_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/constraint_solver/java/constraint_solver.i \
 $(SRC_DIR)/ortools/constraint_solver/java/routing.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/proto.i \
 $(CP_DEPS) \
 $(SRC_DIR)/ortools/constraint_solver/routing.h \
 | $(GEN_DIR)/ortools/constraint_solver $(GEN_DIR)/com/google/ortools/constraintsolver
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_java_wrap.cc \
 -package com.google.ortools.constraintsolver \
 -module operations_research_constraint_solver \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Sjava$Srouting.i

$(OBJ_DIR)/swig/constraint_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_java_wrap.cc \
 $(CP_DEPS) \
 $(SRC_DIR)/ortools/constraint_solver/routing.h \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/algorithms/java/knapsack_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h \
 | $(GEN_DIR)/ortools/algorithms $(GEN_DIR)/com/google/ortools/algorithms
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_java_wrap.cc \
 -package com.google.ortools.algorithms \
 -module operations_research_algorithms \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms \
 $(SRC_DIR)$Sortools$Salgorithms$Sjava$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_java_wrap.$O

$(GEN_DIR)/ortools/graph/graph_java_wrap.cc: \
 $(SRC_DIR)/ortools/graph/java/graph.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(GRAPH_DEPS) \
 | $(GEN_DIR)/ortools/graph $(GEN_DIR)/com/google/ortools/graph
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_java_wrap.cc \
 -package com.google.ortools.graph \
 -module operations_research_graph \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph \
 $(SRC_DIR)$Sortools$Sgraph$Sjava$Sgraph.i

$(OBJ_DIR)/swig/graph_java_wrap.$O: \
 $(GEN_DIR)/ortools/graph/graph_java_wrap.cc \
 $(GRAPH_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Sgraph$Sgraph_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_java_wrap.$O

$(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/java/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(LP_DEPS) \
 | $(GEN_DIR)/ortools/linear_solver $(GEN_DIR)/com/google/ortools/linearsolver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_java_wrap.cc \
 -package com.google.ortools.linearsolver \
 -module operations_research_linear_solver \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver \
 $(SRC_DIR)$Sortools$Slinear_solver$Sjava$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc \
 $(LP_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_java_wrap.$O

$(GEN_DIR)/ortools/sat/sat_java_wrap.cc: \
 $(SRC_DIR)/ortools/sat/java/sat.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SAT_DEPS) \
 | $(GEN_DIR)/ortools/sat $(GEN_DIR)/com/google/ortools/sat
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_java_wrap.cc \
 -package com.google.ortools.sat \
 -module operations_research_sat \
 -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Sjava$Ssat.i

$(OBJ_DIR)/swig/sat_java_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_java_wrap.cc \
 $(SAT_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_java_wrap.$O

$(JAVA_OR_TOOLS_NATIVE_LIBS): \
 $(OR_TOOLS_LIBS) \
 $(OBJ_DIR)/swig/constraint_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/graph_java_wrap.$O \
 $(OBJ_DIR)/swig/linear_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/sat_java_wrap.$O
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT) \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Slinear_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_java_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

############
##  JAVA  ##
############
$(LIB_DIR)/protobuf.jar: \
 dependencies/install/lib/protobuf.jar \
 | $(LIB_DIR)
	$(COPY) dependencies$Sinstall$Slib$Sprotobuf.jar $(LIB_DIR)

$(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimitProtobuf.java: \
 $(SRC_DIR)/ortools/constraint_solver/search_limit.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH) $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.java: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH) $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.java: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH) $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.java: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto \
 | $(GEN_DIR)/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH) $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/com/google/ortools/sat/CpModel.java: \
 $(SRC_DIR)/ortools/sat/cp_model.proto \
 | $(GEN_DIR)/com/google/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH) $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/com/google/ortools/sat/SatParameters.java: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 | $(GEN_DIR)/com/google/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH) $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

$(JAVA_OR_TOOLS_LIBS): \
 $(JAVA_OR_TOOLS_NATIVE_LIBS) \
 $(LIB_DIR)/protobuf.jar \
 $(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.java \
 $(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimitProtobuf.java \
 $(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.java \
 $(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.java \
 $(GEN_DIR)/com/google/ortools/sat/SatParameters.java \
 $(GEN_DIR)/com/google/ortools/sat/CpModel.java | \
 $(CLASS_DIR)/com/google/ortools
	"$(JAVAC_BIN)" -d $(CLASS_DIR) \
 -cp $(LIB_DIR)$Sprotobuf.jar \
 $(SRC_DIR)$Sortools$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java \
 $(SRC_DIR)$Sortools$Scom$Sgoogle$Sortools$Ssat$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Ssat$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	"$(JAR_BIN)" cvf $(LIB_DIR)$Scom.google.ortools.jar -C $(CLASS_DIR) com$Sgoogle$Sortools$S

#############################
##  Java Examples/Samples  ##
#############################
.PHONY: test_java_tests # Build and Run all Java Tests (located in examples/tests)
test_java_tests: $(JAVA_OR_TOOLS_LIBS)
	$(MAKE) rjava_TestLp

$(CLASS_DIR)/%: $(TEST_DIR)/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(TEST_PATH)$S$*.java

.PHONY: test_java_examples # Build and Run all Java Examples (located in examples/java)
test_java_examples: $(JAVA_OR_TOOLS_LIBS)
	$(MAKE) rjava_AllDifferentExcept0
	$(MAKE) rjava_AllInterval
	$(MAKE) rjava_CapacitatedVehicleRoutingProblemWithTimeWindows
	$(MAKE) rjava_Circuit
	$(MAKE) rjava_CoinsGridMIP
	$(MAKE) rjava_ColoringMIP
	$(MAKE) rjava_CoveringOpl
	$(MAKE) rjava_Crossword
	$(MAKE) rjava_DeBruijn
	$(MAKE) rjava_Diet
	$(MAKE) rjava_DietMIP
	$(MAKE) rjava_DivisibleBy9Through1
	$(MAKE) rjava_FlowExample
	$(MAKE) rjava_GolombRuler
	$(MAKE) rjava_IntegerProgramming
	$(MAKE) rjava_Knapsack
	$(MAKE) rjava_KnapsackMIP
	$(MAKE) rjava_LeastDiff
	$(MAKE) rjava_LinearAssignmentAPI
	$(MAKE) rjava_LinearProgramming
	$(MAKE) rjava_LsApi
	$(MAKE) rjava_MagicSquare
	$(MAKE) rjava_Map2
	$(MAKE) rjava_Map
	$(MAKE) rjava_Minesweeper
	$(MAKE) rjava_MultiThreadTest
	$(MAKE) rjava_NQueens2
	$(MAKE) rjava_NQueens
	$(MAKE) rjava_Partition
	$(MAKE) rjava_QuasigroupCompletion
	$(MAKE) rjava_RabbitsPheasants
	$(MAKE) rjava_SendMoreMoney2
	$(MAKE) rjava_SendMoreMoney
	$(MAKE) rjava_SendMostMoney
	$(MAKE) rjava_Seseman
	$(MAKE) rjava_SetCovering2
	$(MAKE) rjava_SetCovering3
	$(MAKE) rjava_SetCovering4
	$(MAKE) rjava_SetCoveringDeployment
	$(MAKE) rjava_SetCovering
	$(MAKE) rjava_SimpleRoutingTest
	$(MAKE) rjava_StableMarriage
	$(MAKE) rjava_StiglerMIP
	$(MAKE) rjava_Strimko2
	$(MAKE) rjava_Sudoku
	$(MAKE) rjava_SurvoPuzzle
	$(MAKE) rjava_ToNum
	$(MAKE) rjava_Tsp
	$(MAKE) rjava_Vrp
	$(MAKE) rjava_WhoKilledAgatha
	$(MAKE) rjava_Xkcd
	$(MAKE) rjava_YoungTableaux

$(CLASS_DIR)/%: $(JAVA_EX_DIR)/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(JAVA_EX_PATH)$S$*.java

.PHONY: test_java_samples # Build and Run all Java Samples (located in ortools/*/samples)
test_java_samples: $(JAVA_OR_TOOLS_LIBS)
	$(MAKE) rjava_BinPackingProblem
	$(MAKE) rjava_BoolOrSample
	$(MAKE) rjava_ChannelingSample
	$(MAKE) rjava_CodeSample
	$(MAKE) rjava_IntervalSample
	$(MAKE) rjava_LiteralSample
	$(MAKE) rjava_NoOverlapSample
	$(MAKE) rjava_OptionalIntervalSample
	$(MAKE) rjava_RabbitsAndPheasants
	$(MAKE) rjava_RankingSample
	$(MAKE) rjava_ReifiedSample
	$(MAKE) rjava_SimpleSolve
	$(MAKE) rjava_SolveAllSolutions
	$(MAKE) rjava_SolveWithIntermediateSolutions
	$(MAKE) rjava_SolveWithTimeLimit
	$(MAKE) rjava_StopAfterNSolutions

$(CLASS_DIR)/%: $(SRC_DIR)/ortools/sat/samples/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 ortools$Ssat$Ssamples$S$*.java

$(LIB_DIR)/%$J: $(CLASS_DIR)/% | $(LIB_DIR)
	-$(DEL) $(LIB_DIR)$S$*.jar
	"$(JAR_BIN)" cvf $(LIB_DIR)$S$*.jar -C $(CLASS_DIR)$S$* .

rjava_%: $(LIB_DIR)/%$J
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*.jar$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $* $(ARGS)

.PHONY: rjava cjava
ifeq ($(EX),) # Those rules will be used if EX variable is not set
rjava cjava:
	@echo No java file was provided, the $@ target must be used like so: \
 make $@ EX=examples/java/example.java
else # This generic rule will be used if EX variable is set
EX_NAME = $(basename $(notdir $(EX)))

cjava: $(CLASS_DIR)/$(EX_NAME)

rjava: $(LIB_DIR)/$(EX_NAME)$J
	@echo running $<
	$(MAKE) rjava_$(EX_NAME)
endif # ifeq ($(EX),)

################
##  Cleaning  ##
################
.PHONY: clean_java # Clean Java output from previous build.
clean_java:
	-$(DELREC) $(GEN_PATH)$Scom
	-$(DELREC) $(OBJ_DIR)$Scom
	-$(DEL) $(CLASS_DIR)$S*.class
	-$(DELREC) $(CLASS_DIR)
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*java_wrap*
	-$(DEL) $(OBJ_DIR)$Sswig$S*_java_wrap.$O
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)jni*.$(JNI_LIB_EXT)
	-$(DEL) $(LIB_DIR)$S*.jar

#############
##  DEBUG  ##
#############
.PHONY: detect_java # Show variables used to build Java OR-Tools.
detect_java:
	@echo Relevant info for the Java build:
	@echo JAVA_INC = $(JAVA_INC)
	@echo JNIFLAGS = $(JNIFLAGS)
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo CLASS_DIR = $(CLASS_DIR)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo JAVAFLAGS = $(JAVAFLAGS)
	@echo JAVA_OR_TOOLS_LIBS = $(JAVA_OR_TOOLS_LIBS)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
