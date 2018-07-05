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

JAVA_ORTOOLS_LIBS= $(LIB_DIR)/com.google.ortools.jar $(LIB_DIR)/$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT)

# Main target
.PHONY: java # Build Java OR-Tools.
.PHONY: test_java # Test Java OR-Tools using various examples.
ifeq ($(JAVA_BIN),)
java:
	$@echo JAVA_HOME = ${JAVA_HOME}
	$@echo JAVA_BIN = ${JAVA_BIN}
	$@echo JAVAC_BIN = ${JAVAC_BIN}
	$(warning Cannot find 'java' command which is needed for build. Please make sure it is installed and in system path. Check Makefile.local for more information.)
test_java: java
else
java: ortoolslibs $(JAVA_ORTOOLS_LIBS)
test_java: test_java_examples
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

$(OBJ_DIR)/com/google/ortools:
	$(MKDIR_P) $(OBJ_DIR)$Scom$Sgoogle$Sortools

$(OBJ_DIR)/com/google/ortools/samples: | $(OBJ_DIR)/com/google/ortools
	$(MKDIR) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Ssamples

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
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -module operations_research_constraint_solver -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)$Sortools$Sconstraint_solver$Sjava$Srouting.i

$(OBJ_DIR)/swig/constraint_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_java_wrap.cc \
 $(CP_DEPS) \
 $(SRC_DIR)/ortools/constraint_solver/routing.h \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/algorithms/java/knapsack_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h \
 | $(GEN_DIR)/ortools/algorithms $(GEN_DIR)/com/google/ortools/algorithms
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_java_wrap.cc -package com.google.ortools.algorithms -module operations_research_algorithms -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms $(SRC_DIR)$Sortools$Salgorithms$Sjava$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_java_wrap.$O

$(GEN_DIR)/ortools/graph/graph_java_wrap.cc: \
 $(SRC_DIR)/ortools/graph/java/graph.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(GRAPH_DEPS) \
 | $(GEN_DIR)/ortools/graph $(GEN_DIR)/com/google/ortools/graph
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_java_wrap.cc -package com.google.ortools.graph -module operations_research_graph -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph $(SRC_DIR)$Sortools$Sgraph$Sjava$Sgraph.i

$(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/java/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(LP_DEPS) \
 | $(GEN_DIR)/ortools/linear_solver $(GEN_DIR)/com/google/ortools/linearsolver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_java_wrap.cc -package com.google.ortools.linearsolver -module operations_research_linear_solver -outdir $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)$Sortools$Slinear_solver$Sjava$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc \
 $(LP_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_java_wrap.$O

$(OBJ_DIR)/swig/graph_java_wrap.$O: \
 $(GEN_DIR)/ortools/graph/graph_java_wrap.cc \
 $(GRAPH_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_PATH)$Sortools$Sgraph$Sgraph_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_java_wrap.$O

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

$(LIB_DIR)/protobuf.jar: dependencies/install/lib/protobuf.jar | $(LIB_DIR)
	$(COPY) dependencies$Sinstall$Slib$Sprotobuf.jar $(LIB_DIR)

$(LIB_DIR)/com.google.ortools.jar: \
 $(LIB_DIR)/protobuf.jar \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_java_wrap.cc \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc \
 $(GEN_DIR)/ortools/graph/graph_java_wrap.cc \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc \
 $(GEN_DIR)/com/google/ortools/constraintsolver/SolverParameters.java \
 $(GEN_DIR)/com/google/ortools/constraintsolver/SearchLimitProtobuf.java \
 $(GEN_DIR)/com/google/ortools/constraintsolver/RoutingParameters.java \
 $(GEN_DIR)/com/google/ortools/constraintsolver/RoutingEnums.java | $(OBJ_DIR)
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Sprotobuf.jar \
 $(SRC_DIR)$Sortools$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph$S*.java \
 $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	$(JAR_BIN) cf $(LIB_DIR)$Scom.google.ortools.jar -C $(OBJ_DIR) com$Sgoogle$Sortools$S

$(LIB_DIR)/$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT): \
 $(OBJ_DIR)/swig/constraint_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/graph_java_wrap.$O \
 $(OBJ_DIR)/swig/linear_solver_java_wrap.$O \
 $(OR_TOOLS_LIBS)
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT) \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/graph_java_wrap.$O \
 $(OBJ_DIR)/swig/linear_solver_java_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# Java CP Examples

ifeq ($(EX),) # Those rules will be used if EX variable is not set

compile_RabbitsPheasants: $(OBJ_DIR)/com/google/ortools/samples/RabbitsPheasants.class

$(OBJ_DIR)/com/google/ortools/samples/RabbitsPheasants.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/RabbitsPheasants.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SRabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.RabbitsPheasants

compile_LsApi: $(OBJ_DIR)/com/google/ortools/samples/LsApi.class

$(OBJ_DIR)/com/google/ortools/samples/LsApi.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/LsApi.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SLsApi.java

run_LsApi: compile_LsApi
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.LsApi

compile_GolombRuler: $(OBJ_DIR)/com/google/ortools/samples/GolombRuler.class

$(OBJ_DIR)/com/google/ortools/samples/GolombRuler.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/GolombRuler.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SGolombRuler.java

run_GolombRuler: compile_GolombRuler
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.GolombRuler

compile_Partition: $(OBJ_DIR)/com/google/ortools/samples/Partition.class

$(OBJ_DIR)/com/google/ortools/samples/Partition.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Partition.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SPartition.java

run_Partition: compile_Partition
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Partition

compile_SendMoreMoney: $(OBJ_DIR)/com/google/ortools/samples/SendMoreMoney.class

$(OBJ_DIR)/com/google/ortools/samples/SendMoreMoney.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/SendMoreMoney.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SSendMoreMoney.java

run_SendMoreMoney: compile_SendMoreMoney
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.SendMoreMoney

compile_SendMoreMoney2: $(OBJ_DIR)/com/google/ortools/samples/SendMoreMoney2.class

$(OBJ_DIR)/com/google/ortools/samples/SendMoreMoney2.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/SendMoreMoney2.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SSendMoreMoney2.java

run_SendMoreMoney2: compile_SendMoreMoney2
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.SendMoreMoney2

compile_LeastDiff: $(OBJ_DIR)/com/google/ortools/samples/LeastDiff.class

$(OBJ_DIR)/com/google/ortools/samples/LeastDiff.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/LeastDiff.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SLeastDiff.java

run_LeastDiff: compile_LeastDiff
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.LeastDiff

compile_MagicSquare: $(OBJ_DIR)/com/google/ortools/samples/MagicSquare.class

$(OBJ_DIR)/com/google/ortools/samples/MagicSquare.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/MagicSquare.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SMagicSquare.java

run_MagicSquare: compile_MagicSquare
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.MagicSquare

compile_NQueens: $(OBJ_DIR)/com/google/ortools/samples/NQueens.class

$(OBJ_DIR)/com/google/ortools/samples/NQueens.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/NQueens.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SNQueens.java

run_NQueens: compile_NQueens
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.NQueens

compile_NQueens2: $(OBJ_DIR)/com/google/ortools/samples/NQueens2.class

$(OBJ_DIR)/com/google/ortools/samples/NQueens2.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/NQueens2.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SNQueens2.java

run_NQueens2: compile_NQueens2
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.NQueens2


compile_AllDifferentExcept0: $(OBJ_DIR)/com/google/ortools/samples/AllDifferentExcept0.class

$(OBJ_DIR)/com/google/ortools/samples/AllDifferentExcept0.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/AllDifferentExcept0.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SAllDifferentExcept0.java

run_AllDifferentExcept0: compile_AllDifferentExcept0
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.AllDifferentExcept0


compile_Diet: $(OBJ_DIR)/com/google/ortools/samples/Diet.class

$(OBJ_DIR)/com/google/ortools/samples/Diet.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Diet.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SDiet.java

run_Diet: compile_Diet
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Diet


compile_Map: $(OBJ_DIR)/com/google/ortools/samples/Map.class

$(OBJ_DIR)/com/google/ortools/samples/Map.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Map.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SMap.java

run_Map: compile_Map
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Map


compile_Map2: $(OBJ_DIR)/com/google/ortools/samples/Map2.class

$(OBJ_DIR)/com/google/ortools/samples/Map2.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Map2.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SMap2.java

run_Map2: compile_Map2
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Map2


compile_Minesweeper: $(OBJ_DIR)/com/google/ortools/samples/Minesweeper.class

$(OBJ_DIR)/com/google/ortools/samples/Minesweeper.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Minesweeper.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SMinesweeper.java

run_Minesweeper: compile_Minesweeper
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Minesweeper


compile_QuasigroupCompletion: $(OBJ_DIR)/com/google/ortools/samples/QuasigroupCompletion.class

$(OBJ_DIR)/com/google/ortools/samples/QuasigroupCompletion.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/QuasigroupCompletion.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SQuasigroupCompletion.java

run_QuasigroupCompletion: compile_QuasigroupCompletion
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.QuasigroupCompletion


compile_SendMostMoney: $(OBJ_DIR)/com/google/ortools/samples/SendMostMoney.class

$(OBJ_DIR)/com/google/ortools/samples/SendMostMoney.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/SendMostMoney.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SSendMostMoney.java

run_SendMostMoney: compile_SendMostMoney
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.SendMostMoney


compile_Seseman: $(OBJ_DIR)/com/google/ortools/samples/Seseman.class

$(OBJ_DIR)/com/google/ortools/samples/Seseman.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Seseman.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SSeseman.java

run_Seseman: compile_Seseman
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Seseman


compile_Sudoku: $(OBJ_DIR)/com/google/ortools/samples/Sudoku.class

$(OBJ_DIR)/com/google/ortools/samples/Sudoku.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Sudoku.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SSudoku.java

run_Sudoku: compile_Sudoku
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Sudoku

compile_Tsp: $(OBJ_DIR)/com/google/ortools/samples/Tsp.class

$(OBJ_DIR)/com/google/ortools/samples/Tsp.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Tsp.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$STsp.java

run_Tsp: compile_Tsp
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Tsp $(ARGS)

compile_CapacitatedVehicleRoutingProblemWithTimeWindows: $(OBJ_DIR)/com/google/ortools/samples/CapacitatedVehicleRoutingProblemWithTimeWindows.class

$(OBJ_DIR)/com/google/ortools/samples/CapacitatedVehicleRoutingProblemWithTimeWindows.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/CapacitatedVehicleRoutingProblemWithTimeWindows.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SCapacitatedVehicleRoutingProblemWithTimeWindows.java

run_CapacitatedVehicleRoutingProblemWithTimeWindows: compile_CapacitatedVehicleRoutingProblemWithTimeWindows
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.CapacitatedVehicleRoutingProblemWithTimeWindows $(ARGS)

compile_Xkcd: $(OBJ_DIR)/com/google/ortools/samples/Xkcd.class

$(OBJ_DIR)/com/google/ortools/samples/Xkcd.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Xkcd.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SXkcd.java

run_Xkcd: compile_Xkcd
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Xkcd


compile_SurvoPuzzle: $(OBJ_DIR)/com/google/ortools/samples/SurvoPuzzle.class

$(OBJ_DIR)/com/google/ortools/samples/SurvoPuzzle.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/SurvoPuzzle.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SSurvoPuzzle.java

run_SurvoPuzzle: compile_SurvoPuzzle
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.SurvoPuzzle


compile_Circuit: $(OBJ_DIR)/com/google/ortools/samples/Circuit.class

$(OBJ_DIR)/com/google/ortools/samples/Circuit.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Circuit.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SCircuit.java

run_Circuit: compile_Circuit
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Circuit


compile_CoinsGrid: $(OBJ_DIR)/com/google/ortools/samples/CoinsGrid.class

$(OBJ_DIR)/com/google/ortools/samples/CoinsGrid.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/CoinsGrid.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SCoinsGrid.java

run_CoinsGrid: compile_CoinsGrid
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.CoinsGrid

# Java Algorithms Examples

compile_Knapsack: $(OBJ_DIR)/com/google/ortools/samples/Knapsack.class

$(OBJ_DIR)/com/google/ortools/samples/Knapsack.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Knapsack.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SKnapsack.java

run_Knapsack: compile_Knapsack
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Knapsack

# Java Algorithms Examples

compile_FlowExample: $(OBJ_DIR)/com/google/ortools/samples/FlowExample.class

$(OBJ_DIR)/com/google/ortools/samples/FlowExample.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/FlowExample.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SFlowExample.java

run_FlowExample: compile_FlowExample $(JAVA_ORTOOLS_LIBS)
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.FlowExample

compile_LinearAssignmentAPI: $(OBJ_DIR)/com/google/ortools/samples/LinearAssignmentAPI.class

$(OBJ_DIR)/com/google/ortools/samples/LinearAssignmentAPI.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/LinearAssignmentAPI.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)com$Sgoogle$Sortools$Ssamples$SLinearAssignmentAPI.java

run_LinearAssignmentAPI: compile_LinearAssignmentAPI $(JAVA_ORTOOLS_LIBS)
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.LinearAssignmentAPI

# Java Linear Programming Examples

compile_LinearProgramming: $(OBJ_DIR)/com/google/ortools/samples/LinearProgramming.class

$(OBJ_DIR)/com/google/ortools/samples/LinearProgramming.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/LinearProgramming.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SLinearProgramming.java

run_LinearProgramming: compile_LinearProgramming
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.LinearProgramming

compile_IntegerProgramming: $(OBJ_DIR)/com/google/ortools/samples/IntegerProgramming.class

$(OBJ_DIR)/com/google/ortools/samples/IntegerProgramming.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/IntegerProgramming.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SIntegerProgramming.java

run_IntegerProgramming: compile_IntegerProgramming
	$(JAVA_BIN) -Xss2048k -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.IntegerProgramming

compile_Issue173: $(OBJ_DIR)/com/google/ortools/samples/Issue173.class

$(OBJ_DIR)/com/google/ortools/samples/Issue173.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/Issue173.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SIssue173.java

run_Issue173: compile_Issue173
	$(JAVA_BIN) -Xss2048k -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.Issue173


# Integer programming Coin-CBC section
run_MultiThreadIntegerProgramming: compile_MultiThreadIntegerProgramming
	$(JAVA_BIN) -Xss2048k -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar com.google.ortools.samples.MultiThreadTest

compile_MultiThreadIntegerProgramming: $(OBJ_DIR)/com/google/ortools/samples/MultiThreadTest.class

$(OBJ_DIR)/com/google/ortools/samples/MultiThreadTest.class: \
 $(JAVA_ORTOOLS_LIBS) \
 $(EX_DIR)/com/google/ortools/samples/MultiThreadTest.java \
 | $(OBJ_DIR)/com/google/ortools/samples
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SMultiThreadTest.java

# Compile and Run CP java example:

else # This generic rule will be used if EX variable is set

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

$(EX_class_file): $(JAVA_ORTOOLS_LIBS) $(EX)
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX)

.PHONY: cjava
cjava: $(EX_class_file)

.PHONY: rjava
rjava: $(EX_class_file) $(JAVA_ORTOOLS_LIBS)
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar $(EX_class) $(ARGS)
endif

endif # ifeq ($(EX),)

.PHONY: clean_java # Clean Java output from previous build.
clean_java:
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)jni*.$(JNI_LIB_EXT)
	-$(DEL) $(LIB_DIR)$S*.jar
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Sgraph$S*.java
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Salgorithms$S*.java
	-$(DEL) $(GEN_PATH)$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*java_wrap*
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sgraph$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Salgorithms$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Ssamples$S*.class
	-$(DEL) $(OBJ_DIR)$Sswig$S*_java_wrap.$O

.PHONY: detect_java # Show variables used to build Java OR-Tools.
detect_java:
	@echo Relevant info for the Java build:
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo JAVA_INC = $(JAVA_INC)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JNIFLAGS = $(JNIFLAGS)
	@echo JAVA_ORTOOLS_LIBS = $(JAVA_ORTOOLS_LIBS)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
