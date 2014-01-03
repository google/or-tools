# Main target
java: javacp javaalgorithms javagraph javalp

# Clean target
clean_java:
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)jni*.$(JNI_LIB_EXT)
	-$(DEL) $(LIB_DIR)$S*.jar
	-$(DEL) $(GEN_DIR)$Salgorithms$S*java_wrap*
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph$S*.java
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	-$(DEL) $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*java_wrap*
	-$(DEL) $(GEN_DIR)$Sgraph$S*java_wrap*
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*java_wrap*
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sgraph$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sgraph$Ssamples$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$Ssamples$S*.class
	-$(DEL) $(OBJ_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$S*.class
	-$(DEL) $(OBJ_DIR)$S*java_wrap.$O

# ---------- Java support using SWIG ----------

# javacp

javacp: $(LIB_DIR)/com.google.ortools.constraintsolver.jar $(LIB_DIR)/$(LIBPREFIX)jniconstraintsolver.$(JNI_LIB_EXT)

$(GEN_DIR)/constraint_solver/constraint_solver_java_wrap.cc: $(SRC_DIR)/constraint_solver/constraint_solver.swig $(SRC_DIR)/constraint_solver/routing.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/constraint_solver/constraint_solver.h $(SRC_DIR)/constraint_solver/routing.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java -o $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver $(SRC_DIR)$Sconstraint_solver$Srouting.swig

$(OBJ_DIR)/constraint_solver_java_wrap.$O: $(GEN_DIR)/constraint_solver/constraint_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_DIR)$Sconstraint_solver$Sconstraint_solver_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver_java_wrap.$O

$(LIB_DIR)/com.google.ortools.constraintsolver.jar: $(GEN_DIR)/constraint_solver/constraint_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(OBJ_DIR) $(SRC_DIR)/com$Sgoogle$Sortools$Sconstraintsolver$S*.java $(GEN_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	$(JAR_BIN) cf $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar -C $(OBJ_DIR) $Scom$Sgoogle$Sortools$Sconstraintsolver

$(LIB_DIR)/$(LIBPREFIX)jniconstraintsolver.$(JNI_LIB_EXT): $(OBJ_DIR)/constraint_solver_java_wrap.$O $(STATIC_ROUTING_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)jniconstraintsolver.$(JNI_LIB_EXT) $(OBJ_DIR)$Sconstraint_solver_java_wrap.$O $(STATIC_ROUTING_LNK) $(STATIC_LD_FLAGS)

# Java CP Examples

compile_RabbitsPheasants: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SRabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.RabbitsPheasants

compile_LsApi: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/LsApi.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/LsApi.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/LsApi.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SLsApi.java

run_LsApi: compile_LsApi
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.LsApi

compile_GolombRuler: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/GolombRuler.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/GolombRuler.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/GolombRuler.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SGolombRuler.java

run_GolombRuler: compile_GolombRuler
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.GolombRuler

compile_Partition: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Partition.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Partition.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Partition.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SPartition.java

run_Partition: compile_Partition
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Partition

compile_SendMoreMoney: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SendMoreMoney.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SendMoreMoney.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/SendMoreMoney.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMoreMoney.java

run_SendMoreMoney: compile_SendMoreMoney
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney

compile_SendMoreMoney2: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/SendMoreMoney2.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMoreMoney2.java

run_SendMoreMoney2: compile_SendMoreMoney2
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney2

compile_LeastDiff: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/LeastDiff.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/LeastDiff.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/LeastDiff.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SLeastDiff.java

run_LeastDiff: compile_LeastDiff
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.LeastDiff

compile_MagicSquare: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/MagicSquare.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/MagicSquare.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/MagicSquare.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMagicSquare.java

run_MagicSquare: compile_MagicSquare
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.MagicSquare

compile_NQueens: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/NQueens.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/NQueens.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/NQueens.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SNQueens.java

run_NQueens: compile_NQueens
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens

compile_NQueens2: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/NQueens2.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/NQueens2.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/NQueens2.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SNQueens2.java

run_NQueens2: compile_NQueens2
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens2


compile_AllDifferentExcept0: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SAllDifferentExcept0.java

run_AllDifferentExcept0: compile_AllDifferentExcept0
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.AllDifferentExcept0


compile_Diet: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Diet.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Diet.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Diet.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SDiet.java

run_Diet: compile_Diet
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Diet


compile_Map: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Map.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Map.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Map.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMap.java

run_Map: compile_Map
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map


compile_Map2: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Map2.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Map2.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Map2.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMap2.java

run_Map2: compile_Map2
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map2


compile_Minesweeper: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Minesweeper.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Minesweeper.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Minesweeper.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMinesweeper.java

run_Minesweeper: compile_Minesweeper
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Minesweeper


compile_QuasigroupCompletion: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SQuasigroupCompletion.java

run_QuasigroupCompletion: compile_QuasigroupCompletion
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.QuasigroupCompletion


compile_SendMostMoney: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SendMostMoney.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SendMostMoney.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/SendMostMoney.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMostMoney.java

run_SendMostMoney: compile_SendMostMoney
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMostMoney


compile_Seseman: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Seseman.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Seseman.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Seseman.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSeseman.java

run_Seseman: compile_Seseman
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Seseman


compile_Sudoku: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Sudoku.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Sudoku.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Sudoku.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSudoku.java

run_Sudoku: compile_Sudoku
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Sudoku

compile_Tsp: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Tsp.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Tsp.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Tsp.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$STsp.java

run_Tsp: compile_Tsp
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Tsp $(ARGS)

compile_CapacitatedVehicleRoutingProblemWithTimeWindows: $(OBJ_DIR)/com/google/ortools/samples/CapacitatedVehicleRoutingProblemWithTimeWindows.class

$(OBJ_DIR)/com/google/ortools/samples/CapacitatedVehicleRoutingProblemWithTimeWindows.class: javacp $(EX_DIR)/com/google/ortools/samples/CapacitatedVehicleRoutingProblemWithTimeWindows.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Ssamples$SCapacitatedVehicleRoutingProblemWithTimeWindows.java

run_CapacitatedVehicleRoutingProblemWithTimeWindows: compile_CapacitatedVehicleRoutingProblemWithTimeWindows
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.samples.CapacitatedVehicleRoutingProblemWithTimeWindows $(ARGS)

compile_Xkcd: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Xkcd.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Xkcd.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Xkcd.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SXkcd.java

run_Xkcd: compile_Xkcd
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Xkcd


compile_SurvoPuzzle: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/SurvoPuzzle.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSurvoPuzzle.java

run_SurvoPuzzle: compile_SurvoPuzzle
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SurvoPuzzle


compile_Circuit: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Circuit.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/Circuit.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/Circuit.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SCircuit.java

run_Circuit: compile_Circuit
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Circuit


compile_CoinsGrid: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/CoinsGrid.class

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/CoinsGrid.class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/CoinsGrid.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SCoinsGrid.java

run_CoinsGrid: compile_CoinsGrid
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.CoinsGrid

# javaalgorithms

javaalgorithms: $(LIB_DIR)/com.google.ortools.knapsacksolver.jar $(LIB_DIR)/$(LIBPREFIX)jniknapsacksolver.$(JNI_LIB_EXT)

$(GEN_DIR)/algorithms/knapsack_solver_java_wrap.cc: $(SRC_DIR)/algorithms/knapsack_solver.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java -o $(GEN_DIR)$Salgorithms$Sknapsack_solver_java_wrap.cc -package com.google.ortools.knapsacksolver -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver $(SRC_DIR)$Salgorithms$Sknapsack_solver.swig

$(OBJ_DIR)/knapsack_solver_java_wrap.$O: $(GEN_DIR)/algorithms/knapsack_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_DIR)$Salgorithms$Sknapsack_solver_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sknapsack_solver_java_wrap.$O

$(LIB_DIR)/com.google.ortools.knapsacksolver.jar: $(GEN_DIR)/algorithms/knapsack_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(OBJ_DIR) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	$(JAR_BIN) cf $(LIB_DIR)$Scom.google.ortools.knapsacksolver.jar -C $(OBJ_DIR) com$Sgoogle$Sortools$Sknapsacksolver

$(LIB_DIR)/$(LIBPREFIX)jniknapsacksolver.$(JNI_LIB_EXT): $(OBJ_DIR)/knapsack_solver_java_wrap.$O $(STATIC_ALGORITHMS_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)jniknapsacksolver.$(JNI_LIB_EXT) $(OBJ_DIR)$Sknapsack_solver_java_wrap.$O $(STATIC_ALGORITHMS_LNK) $(STATIC_LD_FLAGS)

# Java Algorithms Examples

compile_Knapsack: $(OBJ_DIR)/com/google/ortools/knapsacksolver/samples/Knapsack.class

$(OBJ_DIR)/com/google/ortools/knapsacksolver/samples/Knapsack.class: javaalgorithms $(EX_DIR)/com/google/ortools/knapsacksolver/samples/Knapsack.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.knapsacksolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sknapsacksolver$Ssamples$SKnapsack.java

run_Knapsack: compile_Knapsack
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.knapsacksolver.jar com.google.ortools.knapsacksolver.samples.Knapsack

# javagraph

javagraph: $(LIB_DIR)/com.google.ortools.graph.jar $(LIB_DIR)/$(LIBPREFIX)jnigraph.$(JNI_LIB_EXT)

$(GEN_DIR)/graph/graph_java_wrap.cc: $(SRC_DIR)/graph/graph.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/graph/max_flow.h $(SRC_DIR)/graph/min_cost_flow.h $(SRC_DIR)/graph/linear_assignment.h
	$(SWIG_BINARY) -I$(INC_DIR) -c++ -java -o $(GEN_DIR)$Sgraph$Sgraph_java_wrap.cc -package com.google.ortools.graph -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph $(SRC_DIR)$Sgraph$Sgraph.swig

$(OBJ_DIR)/graph_java_wrap.$O: $(GEN_DIR)/graph/graph_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_DIR)$Sgraph$Sgraph_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph_java_wrap.$O

$(LIB_DIR)/com.google.ortools.graph.jar: $(GEN_DIR)/graph/graph_java_wrap.cc
	$(JAVAC_BIN) -d $(OBJ_DIR) $(GEN_DIR)$Scom$Sgoogle$Sortools$Sgraph$S*.java
	$(JAR_BIN) cf $(LIB_DIR)$Scom.google.ortools.graph.jar -C $(OBJ_DIR) com$Sgoogle$Sortools$Sgraph

$(LIB_DIR)/$(LIBPREFIX)jnigraph.$(JNI_LIB_EXT): $(OBJ_DIR)/graph_java_wrap.$O $(STATIC_GRAPH_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)jnigraph.$(JNI_LIB_EXT) $(OBJ_DIR)$Sgraph_java_wrap.$O $(STATIC_GRAPH_LNK) $(STATIC_LD_FLAGS)

# Java Algorithms Examples

compile_FlowExample: $(OBJ_DIR)/com/google/ortools/graph/samples/FlowExample.class

$(OBJ_DIR)/com/google/ortools/graph/samples/FlowExample.class: javagraph $(EX_DIR)/com/google/ortools/graph/samples/FlowExample.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.graph.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sgraph$Ssamples$SFlowExample.java

run_FlowExample: compile_FlowExample javagraph
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.graph.jar com.google.ortools.graph.samples.FlowExample

compile_LinearAssignmentAPI: $(OBJ_DIR)/com/google/ortools/graph/samples/LinearAssignmentAPI.class

$(OBJ_DIR)/com/google/ortools/graph/samples/LinearAssignmentAPI.class: javagraph $(EX_DIR)/com/google/ortools/graph/samples/LinearAssignmentAPI.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.graph.jar $(EX_DIR)com$Sgoogle$Sortools$Sgraph$Ssamples$SLinearAssignmentAPI.java

run_LinearAssignmentAPI: compile_LinearAssignmentAPI javagraph
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.graph.jar com.google.ortools.graph.samples.LinearAssignmentAPI

# javalp

javalp: $(LIB_DIR)/com.google.ortools.linearsolver.jar $(LIB_DIR)/$(LIBPREFIX)jnilinearsolver.$(JNI_LIB_EXT)

$(GEN_DIR)/linear_solver/linear_solver_java_wrap.cc: $(SRC_DIR)/linear_solver/linear_solver.swig $(SRC_DIR)/base/base.swig $(SRC_DIR)/util/data.swig $(SRC_DIR)/linear_solver/linear_solver.h $(GEN_DIR)/linear_solver/linear_solver2.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java -o $(GEN_DIR)$Slinear_solver$Slinear_solver_java_wrap.cc -package com.google.ortools.linearsolver -outdir $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver $(SRC_DIR)$Slinear_solver$Slinear_solver.swig

$(OBJ_DIR)/linear_solver_java_wrap.$O: $(GEN_DIR)/linear_solver/linear_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(GEN_DIR)$Slinear_solver$Slinear_solver_java_wrap.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_java_wrap.$O

$(LIB_DIR)/com.google.ortools.linearsolver.jar: $(GEN_DIR)/linear_solver/linear_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(OBJ_DIR) $(GEN_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	$(JAR_BIN) cf $(LIB_DIR)$Scom.google.ortools.linearsolver.jar -C $(OBJ_DIR) com$Sgoogle$Sortools$Slinearsolver

$(LIB_DIR)/$(LIBPREFIX)jnilinearsolver.$(JNI_LIB_EXT): $(OBJ_DIR)/linear_solver_java_wrap.$O $(STATIC_LP_DEPS)
	$(DYNAMIC_LD) $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)jnilinearsolver.$(JNI_LIB_EXT) $(OBJ_DIR)$Slinear_solver_java_wrap.$O $(STATIC_LP_LNK) $(STATIC_LD_FLAGS)

# Java Linear Programming Examples

compile_LinearProgramming: $(OBJ_DIR)/com/google/ortools/linearsolver/samples/LinearProgramming.class

$(OBJ_DIR)/com/google/ortools/linearsolver/samples/LinearProgramming.class: javalp $(EX_DIR)/com/google/ortools/linearsolver/samples/LinearProgramming.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.linearsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$SLinearProgramming.java

run_LinearProgramming: compile_LinearProgramming
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.LinearProgramming

compile_IntegerProgramming: $(OBJ_DIR)/com/google/ortools/linearsolver/samples/IntegerProgramming.class

$(OBJ_DIR)/com/google/ortools/linearsolver/samples/IntegerProgramming.class: javalp $(EX_DIR)/com/google/ortools/linearsolver/samples/IntegerProgramming.java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.linearsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$SIntegerProgramming.java

run_IntegerProgramming: compile_IntegerProgramming
	$(JAVA_BIN) -Xss2048k -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.IntegerProgramming

# Compile and Run CP java example:

$(OBJ_DIR)/com/google/ortools/constraintsolver/samples/$(EX).class: javacp $(EX_DIR)/com/google/ortools/constraintsolver/samples/$(EX).java
	$(JAVAC_BIN) -d $(OBJ_DIR) -cp $(LIB_DIR)$Scom.google.ortools.constraintsolver.jar $(EX_DIR)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$S$(EX).java

cjava: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/$(EX).class com.google.ortools.constraintsolver.jar

rjava: $(OBJ_DIR)/com/google/ortools/constraintsolver/samples/$(EX).class $(LIB_DIR)/$(LIBPREFIX)jniconstraintsolver.$(JNI_LIB_EXT) $(LIB_DIR)/com.google.ortools.constraintsolver.jar
	$(JAVA_BIN) -Djava.library.path=$(LIB_DIR) -cp $(OBJ_DIR)$(CPSEP)$(LIB_DIR)$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.$(EX)

# Build stand-alone archive file for redistribution.

java_archive: java
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sor-tools.$(PORT)
	$(MKDIR) temp$Sor-tools.$(PORT)$Slib
	$(MKDIR) temp$Sor-tools.$(PORT)$Sobjs
	$(COPY) LICENSE-2.0.txt temp$Sor-tools.$(PORT)
	$(COPY) tools$SREADME.java temp$Sor-tools.$(PORT)$SREADME
	$(COPY) lib$S*.jar temp$Sor-tools.$(PORT)$Slib
	$(COPY) lib$S$(LIBPREFIX)jni*.$(JNI_LIB_EXT) temp$Sor-tools.$(PORT)$Slib
ifeq ("$(SYSTEM)","win")
	tools\mkdir temp\or-tools.$(PORT)\examples
	tools\mkdir temp\or-tools.$(PORT)\examples\com
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\samples
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\constraintsolver
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\constraintsolver\samples
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\linearsolver
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\linearsolver\samples
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\graph
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\graph\samples
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\knapsacksolver
	tools\mkdir temp\or-tools.$(PORT)\examples\com\google\ortools\knapsacksolver\samples
	tools\mkdir temp\or-tools.$(PORT)\data
	tools\mkdir temp\or-tools.$(PORT)\data\discrete_tomography
	tools\mkdir temp\or-tools.$(PORT)\data\fill_a_pix
	tools\mkdir temp\or-tools.$(PORT)\data\minesweeper
	tools\mkdir temp\or-tools.$(PORT)\data\rogo
	tools\mkdir temp\or-tools.$(PORT)\data\survo_puzzle
	tools\mkdir temp\or-tools.$(PORT)\data\quasigroup_completion
	copy data\discrete_tomography\* temp\or-tools.$(PORT)\data\discrete_tomography
	copy data\fill_a_pix\* temp\or-tools.$(PORT)\data\fill_a_pix
	copy data\minesweeper\* temp\or-tools.$(PORT)\data\minesweeper
	copy data\rogo\* temp\or-tools.$(PORT)\data\rogo
	copy data\survo_puzzle\* temp\or-tools.$(PORT)\data\survo_puzzle
	copy data\quasigroup_completion\* temp\or-tools.$(PORT)\data\quasigroup_completion
	copy examples\com\google\ortools\constraintsolver\samples\*.java temp\or-tools.$(PORT)\examples\com\google\ortools\constraintsolver\samples
	copy examples\com\google\ortools\linearsolver\samples\*.java temp\or-tools.$(PORT)\examples\com\google\ortools\linearsolver\samples
	copy examples\com\google\ortools\graph\samples\*.java temp\or-tools.$(PORT)\examples\com\google\ortools\graph\samples
	copy examples\com\google\ortools\knapsacksolver\samples\*.java temp\or-tools.$(PORT)\examples\com\google\ortools\knapsacksolver\samples
	cd temp && ..$Stools$Szip.exe -r ..$SGoogle.OrTools.java.$(PORT).$(SVNVERSION).zip or-tools.$(PORT)
else
	mkdir temp/or-tools.$(PORT)/examples
	mkdir temp/or-tools.$(PORT)/examples/com
	mkdir temp/or-tools.$(PORT)/examples/com/google
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/constraintsolver
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/constraintsolver/samples
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/linearsolver
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/linearsolver/samples
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/graph
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/graph/samples
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/knapsacksolver
	mkdir temp/or-tools.$(PORT)/examples/com/google/ortools/knapsacksolver/samples
	mkdir temp/or-tools.$(PORT)/data
	mkdir temp/or-tools.$(PORT)/data/discrete_tomography
	mkdir temp/or-tools.$(PORT)/data/fill_a_pix
	mkdir temp/or-tools.$(PORT)/data/minesweeper
	mkdir temp/or-tools.$(PORT)/data/rogo
	mkdir temp/or-tools.$(PORT)/data/survo_puzzle
	mkdir temp/or-tools.$(PORT)/data/quasigroup_completion
	cp data/discrete_tomography/* temp/or-tools.$(PORT)/data/discrete_tomography
	cp data/fill_a_pix/* temp/or-tools.$(PORT)/data/fill_a_pix
	cp data/minesweeper/* temp/or-tools.$(PORT)/data/minesweeper
	cp data/rogo/* temp/or-tools.$(PORT)/data/rogo
	cp data/survo_puzzle/* temp/or-tools.$(PORT)/data/survo_puzzle
	cp data/quasigroup_completion/* temp/or-tools.$(PORT)/data/quasigroup_completion
	cp examples/com/google/ortools/constraintsolver/samples/*.java temp/or-tools.$(PORT)/examples/com/google/ortools/constraintsolver/samples
	cp examples/com/google/ortools/samples/*.java temp/or-tools.$(PORT)/examples/com/google/ortools/samples
	cp examples/com/google/ortools/linearsolver/samples/*.java temp/or-tools.$(PORT)/examples/com/google/ortools/linearsolver/samples
	cp examples/com/google/ortools/graph/samples/*.java temp/or-tools.$(PORT)/examples/com/google/ortools/graph/samples
	cp examples/com/google/ortools/knapsacksolver/samples/*.java temp/or-tools.$(PORT)/examples/com/google/ortools/knapsacksolver/samples
	cd temp && tar cvzf ../Google.OrTools.java.$(PORT).$(SVNVERSION).tar.gz or-tools.$(PORT)
endif
	-$(DELREC) temp
