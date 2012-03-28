# Main target
java: javacp javaalgorithms javagraph javalp

# Clean target
clean_java:
	-$(DEL) $(LIBPREFIX)jni*.$(JNILIBEXT)
	-$(DEL) $(OR_ROOT)lib$S*.jar
	-$(DEL) $(OR_ROOT)gen$Salgorithms$S*java_wrap*
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sgraph$S*.java
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	-$(DEL) $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	-$(DEL) $(OR_ROOT)gen$Sconstraint_solver$S*java_wrap*
	-$(DEL) $(OR_ROOT)gen$Sgraph$S*java_wrap*
	-$(DEL) $(OR_ROOT)gen$Slinear_solver$S*java_wrap*
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Sgraph$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Slinearsolver$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Sgraph$Ssamples$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Sknapsacksolver$Ssamples$S*.class
	-$(DEL) $(OR_ROOT)objs$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$S*.class
	-$(DEL) $(OR_ROOT)objs$S*java_wrap.$O

# ---------- Java support using SWIG ----------

# javacp

javacp: $(OR_ROOT)lib/com.google.ortools.constraintsolver.jar $(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT)
$(OR_ROOT)gen/constraint_solver/constraint_solver_java_wrap.cc: $(OR_ROOT)constraint_solver/constraint_solver.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)constraint_solver/constraint_solver.h
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -java -o $(OR_ROOT)gen$Sconstraint_solver$Sconstraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sconstraintsolver $(OR_ROOT)constraint_solver$Sconstraint_solver.swig

$(OR_ROOT)objs/constraint_solver_java_wrap.$O: $(OR_ROOT)gen/constraint_solver/constraint_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(OR_ROOT)gen$Sconstraint_solver$Sconstraint_solver_java_wrap.cc $(OBJOUT)objs/constraint_solver_java_wrap.$O

$(OR_ROOT)lib/com.google.ortools.constraintsolver.jar: $(OR_ROOT)gen/constraint_solver/constraint_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(OR_ROOT)objs $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$S*.java $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	$(JAR_BIN) cf $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar -C $(OR_ROOT)objs $Scom$Sgoogle$Sortools$Sconstraintsolver

$(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT): $(OR_ROOT)objs/constraint_solver_java_wrap.$O $(CP_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT) $(OR_ROOT)objs$Sconstraint_solver_java_wrap.$O $(CP_LNK) $(LDFLAGS)

# Java CP Examples

compile_RabbitsPheasants: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SRabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.RabbitsPheasants

compile_GolombRuler: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/GolombRuler.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/GolombRuler.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/GolombRuler.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SGolombRuler.java

run_GolombRuler: compile_GolombRuler
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.GolombRuler

compile_Partition: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Partition.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Partition.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Partition.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SPartition.java

run_Partition: compile_Partition
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Partition

compile_SendMoreMoney: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/SendMoreMoney.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMoreMoney.java

run_SendMoreMoney: compile_SendMoreMoney
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney

compile_SendMoreMoney2: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/SendMoreMoney2.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMoreMoney2.java

run_SendMoreMoney2: compile_SendMoreMoney2
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney2

compile_LeastDiff: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/LeastDiff.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/LeastDiff.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/LeastDiff.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SLeastDiff.java

run_LeastDiff: compile_LeastDiff
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.LeastDiff

compile_MagicSquare: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/MagicSquare.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/MagicSquare.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/MagicSquare.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMagicSquare.java

run_MagicSquare: compile_MagicSquare
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.MagicSquare

compile_NQueens: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/NQueens.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/NQueens.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/NQueens.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SNQueens.java

run_NQueens: compile_NQueens
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens

compile_NQueens2: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/NQueens2.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/NQueens2.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/NQueens2.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SNQueens2.java

run_NQueens2: compile_NQueens2
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens2


compile_AllDifferentExcept0: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SAllDifferentExcept0.java

run_AllDifferentExcept0: compile_AllDifferentExcept0
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.AllDifferentExcept0


compile_Diet: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Diet.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Diet.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Diet.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SDiet.java

run_Diet: compile_Diet
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Diet


compile_Map: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Map.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Map.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Map.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMap.java

run_Map: compile_Map
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map


compile_Map2: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Map2.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Map2.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Map2.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMap2.java

run_Map2: compile_Map2
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map2


compile_Minesweeper: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Minesweeper.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Minesweeper.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Minesweeper.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMinesweeper.java

run_Minesweeper: compile_Minesweeper
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Minesweeper


compile_QuasigroupCompletion: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SQuasigroupCompletion.java

run_QuasigroupCompletion: compile_QuasigroupCompletion
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.QuasigroupCompletion


compile_SendMostMoney: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/SendMostMoney.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMostMoney.java

run_SendMostMoney: compile_SendMostMoney
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMostMoney


compile_Seseman: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Seseman.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Seseman.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Seseman.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSeseman.java

run_Seseman: compile_Seseman
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Seseman


compile_Sudoku: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Sudoku.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Sudoku.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Sudoku.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSudoku.java

run_Sudoku: compile_Sudoku
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Sudoku


compile_Xkcd: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Xkcd.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Xkcd.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Xkcd.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SXkcd.java

run_Xkcd: compile_Xkcd
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Xkcd


compile_SurvoPuzzle: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/SurvoPuzzle.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSurvoPuzzle.java

run_SurvoPuzzle: compile_SurvoPuzzle
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SurvoPuzzle


compile_Circuit: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Circuit.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/Circuit.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/Circuit.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SCircuit.java

run_Circuit: compile_Circuit
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Circuit


compile_CoinsGrid: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/CoinsGrid.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SCoinsGrid.java

run_CoinsGrid: compile_CoinsGrid
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.CoinsGrid

# javaalgorithms

javaalgorithms: $(OR_ROOT)lib/com.google.ortools.knapsacksolver.jar $(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT)

$(OR_ROOT)gen/algorithms/knapsack_solver_java_wrap.cc: $(OR_ROOT)algorithms/knapsack_solver.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)algorithms/knapsack_solver.h
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -java -o $(OR_ROOT)gen$Salgorithms$Sknapsack_solver_java_wrap.cc -package com.google.ortools.knapsacksolver -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sknapsacksolver $(OR_ROOT)algorithms$Sknapsack_solver.swig

$(OR_ROOT)objs/knapsack_solver_java_wrap.$O: $(OR_ROOT)gen/algorithms/knapsack_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(OR_ROOT)gen$Salgorithms$Sknapsack_solver_java_wrap.cc $(OBJOUT)objs$Sknapsack_solver_java_wrap.$O

$(OR_ROOT)lib/com.google.ortools.knapsacksolver.jar: $(OR_ROOT)gen/algorithms/knapsack_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(OR_ROOT)objs $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	$(JAR_BIN) cf $(OR_ROOT)lib$Scom.google.ortools.knapsacksolver.jar -C $(OR_ROOT)objs com$Sgoogle$Sortools$Sknapsacksolver

$(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT): $(OR_ROOT)objs/knapsack_solver_java_wrap.$O $(ALGORITHMS_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT) $(OR_ROOT)objs$Sknapsack_solver_java_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)

# Java Algorithms Examples

compile_Knapsack: $(OR_ROOT)objs/com/google/ortools/knapsacksolver/samples/Knapsack.class

$(OR_ROOT)objs/com/google/ortools/knapsacksolver/samples/Knapsack.class: javaalgorithms $(OR_ROOT)com/google/ortools/knapsacksolver/samples/Knapsack.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.knapsacksolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sknapsacksolver$Ssamples$SKnapsack.java

run_Knapsack: compile_Knapsack
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.knapsacksolver.jar com.google.ortools.knapsacksolver.samples.Knapsack

# javagraph

javagraph: $(OR_ROOT)lib/com.google.ortools.graph.jar $(LIBPREFIX)jnigraph.$(JNILIBEXT)

$(OR_ROOT)gen/graph/graph_java_wrap.cc: $(OR_ROOT)graph/graph.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)graph/max_flow.h $(OR_ROOT)graph/min_cost_flow.h $(OR_ROOT)graph/linear_assignment.h
	$(SWIG_BINARY) -I$(OR_ROOT_INC) -c++ -java -o $(OR_ROOT)gen$Sgraph$Sgraph_java_wrap.cc -package com.google.ortools.graph -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

$(OR_ROOT)objs/graph_java_wrap.$O: $(OR_ROOT)gen/graph/graph_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(OR_ROOT)gen$Sgraph$Sgraph_java_wrap.cc $(OBJOUT)objs$Sgraph_java_wrap.$O

$(OR_ROOT)lib/com.google.ortools.graph.jar: $(OR_ROOT)gen/graph/graph_java_wrap.cc
	$(JAVAC_BIN) -d $(OR_ROOT)objs $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Sgraph$S*.java
	$(JAR_BIN) cf $(OR_ROOT)lib$Scom.google.ortools.graph.jar -C $(OR_ROOT)objs com$Sgoogle$Sortools$Sgraph

$(LIBPREFIX)jnigraph.$(JNILIBEXT): $(OR_ROOT)objs/graph_java_wrap.$O $(GRAPH_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jnigraph.$(JNILIBEXT) $(OR_ROOT)objs$Sgraph_java_wrap.$O $(GRAPH_LNK) $(LDFLAGS)

# Java Algorithms Examples

compile_FlowExample: $(OR_ROOT)objs/com/google/ortools/graph/samples/FlowExample.class

$(OR_ROOT)objs/com/google/ortools/graph/samples/FlowExample.class: javagraph $(OR_ROOT)com/google/ortools/graph/samples/FlowExample.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.graph.jar $(OR_ROOT)com$Sgoogle$Sortools$Sgraph$Ssamples$SFlowExample.java

run_FlowExample: compile_FlowExample javagraph
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.graph.jar com.google.ortools.graph.samples.FlowExample

compile_LinearAssignmentAPI: $(OR_ROOT)objs/com/google/ortools/graph/samples/LinearAssignmentAPI.class

$(OR_ROOT)objs/com/google/ortools/graph/samples/LinearAssignmentAPI.class: javagraph $(OR_ROOT)com/google/ortools/graph/samples/LinearAssignmentAPI.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.graph.jar $(OR_ROOT)com$Sgoogle$Sortools$Sgraph$Ssamples$SLinearAssignmentAPI.java

run_LinearAssignmentAPI: compile_LinearAssignmentAPI javagraph
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.graph.jar com.google.ortools.graph.samples.LinearAssignmentAPI

# javalp

javalp: $(OR_ROOT)lib/com.google.ortools.linearsolver.jar $(LIBPREFIX)jnilinearsolver.$(JNILIBEXT)

$(OR_ROOT)gen/linear_solver/linear_solver_java_wrap.cc: $(OR_ROOT)linear_solver/linear_solver.swig $(OR_ROOT)base/base.swig $(OR_ROOT)util/data.swig $(OR_ROOT)linear_solver/linear_solver.h $(OR_ROOT)gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(OR_ROOT_INC) -c++ -java -o $(OR_ROOT)gen$Slinear_solver$Slinear_solver_java_wrap.cc -package com.google.ortools.linearsolver -outdir $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Slinearsolver $(OR_ROOT)linear_solver$Slinear_solver.swig

$(OR_ROOT)objs/linear_solver_java_wrap.$O: $(OR_ROOT)gen/linear_solver/linear_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(OR_ROOT)gen$Slinear_solver$Slinear_solver_java_wrap.cc $(OBJOUT)objs$Slinear_solver_java_wrap.$O

$(OR_ROOT)lib/com.google.ortools.linearsolver.jar: $(OR_ROOT)gen/linear_solver/linear_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(OR_ROOT)objs $(OR_ROOT)gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	$(JAR_BIN) cf $(OR_ROOT)lib$Scom.google.ortools.linearsolver.jar -C $(OR_ROOT)objs com$Sgoogle$Sortools$Slinearsolver

$(LIBPREFIX)jnilinearsolver.$(JNILIBEXT): $(OR_ROOT)objs/linear_solver_java_wrap.$O $(LP_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jnilinearsolver.$(JNILIBEXT) $(OR_ROOT)objs$Slinear_solver_java_wrap.$O $(LP_LNK) $(LDFLAGS)

# Java Linear Programming Examples

compile_LinearProgramming: $(OR_ROOT)objs/com/google/ortools/linearsolver/samples/LinearProgramming.class

$(OR_ROOT)objs/com/google/ortools/linearsolver/samples/LinearProgramming.class: javalp $(OR_ROOT)com/google/ortools/linearsolver/samples/LinearProgramming.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.linearsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Slinearsolver$Ssamples$SLinearProgramming.java

run_LinearProgramming: compile_LinearProgramming
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.LinearProgramming

compile_IntegerProgramming: $(OR_ROOT)objs/com/google/ortools/linearsolver/samples/IntegerProgramming.class

$(OR_ROOT)objs/com/google/ortools/linearsolver/samples/IntegerProgramming.class: javalp $(OR_ROOT)com/google/ortools/linearsolver/samples/IntegerProgramming.java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.linearsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Slinearsolver$Ssamples$SIntegerProgramming.java

run_IntegerProgramming: compile_IntegerProgramming
	$(JAVA_BIN) -Xss2048k -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.IntegerProgramming

# Compile and Run CP java example:

$(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/$(EX).class: javacp $(OR_ROOT)com/google/ortools/constraintsolver/samples/$(EX).java
	$(JAVAC_BIN) -d $(OR_ROOT)objs -cp $(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar $(OR_ROOT)com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$S$(EX).java

cjava: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/$(EX).class com.google.ortools.constraintsolver.jar

rjava: $(OR_ROOT)objs/com/google/ortools/constraintsolver/samples/$(EX).class $(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT) com.google.ortools.constraintsolver.jar
	$(JAVA_BIN) -Djava.library.path=$(OR_ROOT)lib -cp $(OR_ROOT)objs$(CPSEP)$(OR_ROOT)lib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.$(EX)

# Build stand-alone archive file for redistribution.

java_archive: java
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$Sor-tools.$(PLATFORM)
	$(COPY) *.jar temp$Sor-tools.$(PLATFORM)
	$(COPY) $(LIBPREFIX)jni*.$(JNILIBEXT) temp$Sor-tools.$(PLATFORM)
ifeq ("$(SYSTEM)","win")
	tools\mkdir temp\or-tools.$(PLATFORM)\com
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\constraintsolver
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\constraintsolver\samples
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\linearsolver
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\linearsolver\samples
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\graph
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\graph\samples
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\knapsacksolver
	tools\mkdir temp\or-tools.$(PLATFORM)\com\google\ortools\knapsacksolver\samples
	tools\mkdir temp\or-tools.$(PLATFORM)\data
	tools\mkdir temp\or-tools.$(PLATFORM)\data\discrete_tomography
	tools\mkdir temp\or-tools.$(PLATFORM)\data\fill_a_pix
	tools\mkdir temp\or-tools.$(PLATFORM)\data\minesweeper
	tools\mkdir temp\or-tools.$(PLATFORM)\data\rogo
	tools\mkdir temp\or-tools.$(PLATFORM)\data\survo_puzzle
	tools\mkdir temp\or-tools.$(PLATFORM)\data\quasigroup_completion
	copy data\discrete_tomography\* temp\or-tools.$(PLATFORM)\data\discrete_tomography
	copy data\fill_a_pix\* temp\or-tools.$(PLATFORM)\data\fill_a_pix
	copy data\minesweeper\* temp\or-tools.$(PLATFORM)\data\minesweeper
	copy data\rogo\* temp\or-tools.$(PLATFORM)\data\rogo
	copy data\survo_puzzle\* temp\or-tools.$(PLATFORM)\data\survo_puzzle
	copy data\quasigroup_completion\* temp\or-tools.$(PLATFORM)\data\quasigroup_completion
	copy com\google\ortools\constraintsolver\samples\*.java temp\or-tools.$(PLATFORM)\com\google\ortools\constraintsolver\samples
	copy com\google\ortools\linearsolver\samples\*.java temp\or-tools.$(PLATFORM)\com\google\ortools\linearsolver\samples
	copy com\google\ortools\graph\samples\*.java temp\or-tools.$(PLATFORM)\com\google\ortools\graph\samples
	copy com\google\ortools\knapsacksolver\samples\*.java temp\or-tools.$(PLATFORM)\com\google\ortools\knapsacksolver\samples
	cd temp$Sor-tools.$(PLATFORM) && tar -C ..$S.. -c -v com | tar -x -v -m --exclude=*.cs --exclude=*svn*
	cd temp && ..$Stools$Szip.exe -r ..$SGoogle.OrTools.java.$(PLATFORM).$(SVNVERSION).zip or-tools.$(PLATFORM)
else
	cd temp$Sor-tools.$(PLATFORM) && tar -C ..$S.. -c -v com | tar -x -v -m --exclude=\*.cs --exclude=\*svn\*
	cd temp && tar cvzf ..$SGoogle.OrTools.java.$(PLATFORM).$(SVNVERSION).tar.gz or-tools.$(PLATFORM)
endif
	-$(DELREC) temp

