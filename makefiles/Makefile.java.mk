# Main target
java: javacp javaalgorithms javagraph javalp

# Clean target
clean_java:
	-$(DEL) $(LIBPREFIX)jni*.$(JNILIBEXT)
	-$(DEL) $(TOP)$Slib$S*.jar
	-$(DEL) $(TOP)$Sgen$Salgorithms$S*java_wrap*
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sgraph$S*.java
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	-$(DEL) $(TOP)$Sgen$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	-$(DEL) $(TOP)$Sgen$Sconstraint_solver$S*java_wrap*
	-$(DEL) $(TOP)$Sgen$Sgraph$S*java_wrap*
	-$(DEL) $(TOP)$Sgen$Slinear_solver$S*java_wrap*
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Sgraph$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Slinearsolver$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Sgraph$Ssamples$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Sknapsacksolver$Ssamples$S*.class
	-$(DEL) $(TOP)$Sobjs$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$S*.class
	-$(DEL) $(TOP)$Sobjs$S*java_wrap.$O

# ---------- Java support using SWIG ----------

# javacp

javacp: $(TOP)/lib/com.google.ortools.constraintsolver.jar $(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT)
$(TOP)/gen/constraint_solver/constraint_solver_java_wrap.cc: $(TOP)/constraint_solver/constraint_solver.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/constraint_solver/constraint_solver.h
	$(SWIG_BINARY) -I$(TOP) -c++ -java -o $(TOP)$Sgen$Sconstraint_solver$Sconstraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver $(TOP)$Sconstraint_solver$Sconstraint_solver.swig

$(TOP)/objs/constraint_solver_java_wrap.$O: $(TOP)/gen/constraint_solver/constraint_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(TOP)$Sgen$Sconstraint_solver$Sconstraint_solver_java_wrap.cc $(OBJOUT)objs/constraint_solver_java_wrap.$O

$(TOP)/lib/com.google.ortools.constraintsolver.jar: $(TOP)/gen/constraint_solver/constraint_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(TOP)$Sobjs $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	$(JAR_BIN) cf $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar -C $(TOP)$Sobjs $Scom$Sgoogle$Sortools$Sconstraintsolver

$(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT): $(TOP)/objs/constraint_solver_java_wrap.$O $(CP_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT) $(TOP)$Sobjs$Sconstraint_solver_java_wrap.$O $(CP_LNK) $(LDFLAGS)

# Java CP Examples

compile_RabbitsPheasants: $(TOP)/objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SRabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.RabbitsPheasants

compile_GolombRuler: $(TOP)/objs/com/google/ortools/constraintsolver/samples/GolombRuler.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/GolombRuler.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/GolombRuler.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SGolombRuler.java

run_GolombRuler: compile_GolombRuler
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.GolombRuler

compile_Partition: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Partition.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Partition.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Partition.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$S$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SPartition.java

run_Partition: compile_Partition
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Partition

compile_SendMoreMoney: $(TOP)/objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/SendMoreMoney.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMoreMoney.java

run_SendMoreMoney: compile_SendMoreMoney
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney

compile_SendMoreMoney2: $(TOP)/objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/SendMoreMoney2.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMoreMoney2.java

run_SendMoreMoney2: compile_SendMoreMoney2
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney2

compile_LeastDiff: $(TOP)/objs/com/google/ortools/constraintsolver/samples/LeastDiff.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/LeastDiff.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/LeastDiff.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SLeastDiff.java

run_LeastDiff: compile_LeastDiff
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.LeastDiff

compile_MagicSquare: $(TOP)/objs/com/google/ortools/constraintsolver/samples/MagicSquare.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/MagicSquare.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/MagicSquare.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMagicSquare.java

run_MagicSquare: compile_MagicSquare
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.MagicSquare

compile_NQueens: $(TOP)/objs/com/google/ortools/constraintsolver/samples/NQueens.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/NQueens.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/NQueens.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SNQueens.java

run_NQueens: compile_NQueens
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens

compile_NQueens2: $(TOP)/objs/com/google/ortools/constraintsolver/samples/NQueens2.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/NQueens2.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/NQueens2.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SNQueens2.java

run_NQueens2: compile_NQueens2
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens2


compile_AllDifferentExcept0: $(TOP)/objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SAllDifferentExcept0.java

run_AllDifferentExcept0: compile_AllDifferentExcept0
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.AllDifferentExcept0


compile_Diet: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Diet.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Diet.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Diet.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SDiet.java

run_Diet: compile_Diet
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Diet


compile_Map: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Map.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Map.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Map.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMap.java

run_Map: compile_Map
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map


compile_Map2: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Map2.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Map2.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Map2.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMap2.java

run_Map2: compile_Map2
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map2


compile_Minesweeper: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Minesweeper.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Minesweeper.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Minesweeper.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SMinesweeper.java

run_Minesweeper: compile_Minesweeper
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Minesweeper


compile_QuasigroupCompletion: $(TOP)/objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SQuasigroupCompletion.java

run_QuasigroupCompletion: compile_QuasigroupCompletion
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.QuasigroupCompletion


compile_SendMostMoney: $(TOP)/objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/SendMostMoney.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSendMostMoney.java

run_SendMostMoney: compile_SendMostMoney
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMostMoney


compile_Seseman: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Seseman.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Seseman.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Seseman.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSeseman.java

run_Seseman: compile_Seseman
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Seseman


compile_Sudoku: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Sudoku.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Sudoku.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Sudoku.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSudoku.java

run_Sudoku: compile_Sudoku
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Sudoku


compile_Xkcd: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Xkcd.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Xkcd.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Xkcd.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SXkcd.java

run_Xkcd: compile_Xkcd
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Xkcd


compile_SurvoPuzzle: $(TOP)/objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/SurvoPuzzle.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SSurvoPuzzle.java

run_SurvoPuzzle: compile_SurvoPuzzle
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SurvoPuzzle


compile_Circuit: $(TOP)/objs/com/google/ortools/constraintsolver/samples/Circuit.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/Circuit.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/Circuit.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SCircuit.java

run_Circuit: compile_Circuit
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Circuit


compile_CoinsGrid: $(TOP)/objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class

$(TOP)/objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/CoinsGrid.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SCoinsGrid.java

run_CoinsGrid: compile_CoinsGrid
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.CoinsGrid

# javaalgorithms

javaalgorithms: $(TOP)/lib/com.google.ortools.knapsacksolver.jar $(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT)

$(TOP)/gen/algorithms/knapsack_solver_java_wrap.cc: $(TOP)/algorithms/knapsack_solver.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/algorithms/knapsack_solver.h
	$(SWIG_BINARY) -I$(TOP) -c++ -java -o $(TOP)$Sgen$Salgorithms$Sknapsack_solver_java_wrap.cc -package com.google.ortools.knapsacksolver -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sknapsacksolver $(TOP)$Salgorithms$Sknapsack_solver.swig

$(TOP)/objs/knapsack_solver_java_wrap.$O: $(TOP)/gen/algorithms/knapsack_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(TOP)$Sgen$Salgorithms$Sknapsack_solver_java_wrap.cc $(OBJOUT)objs$Sknapsack_solver_java_wrap.$O

$(TOP)/lib/com.google.ortools.knapsacksolver.jar: $(TOP)/gen/algorithms/knapsack_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(TOP)$Sobjs $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	$(JAR_BIN) cf $(TOP)$Slib$Scom.google.ortools.knapsacksolver.jar -C $(TOP)$Sobjs com$Sgoogle$Sortools$Sknapsacksolver

$(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT): $(TOP)/objs/knapsack_solver_java_wrap.$O $(ALGORITHMS_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT) $(TOP)$Sobjs$Sknapsack_solver_java_wrap.$O $(ALGORITHMS_LNK) $(LDFLAGS)

# Java Algorithms Examples

compile_Knapsack: $(TOP)/objs/com/google/ortools/knapsacksolver/samples/Knapsack.class

$(TOP)/objs/com/google/ortools/knapsacksolver/samples/Knapsack.class: javaalgorithms $(TOP)/com/google/ortools/knapsacksolver/samples/Knapsack.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.knapsacksolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sknapsacksolver$Ssamples$SKnapsack.java

run_Knapsack: compile_Knapsack
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.knapsacksolver.jar com.google.ortools.knapsacksolver.samples.Knapsack

# javagraph

javagraph: $(TOP)/lib/com.google.ortools.graph.jar $(LIBPREFIX)jnigraph.$(JNILIBEXT)

$(TOP)/gen/graph/graph_java_wrap.cc: $(TOP)/graph/graph.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/graph/max_flow.h $(TOP)/graph/min_cost_flow.h $(TOP)/graph/linear_assignment.h
	$(SWIG_BINARY) -I$(TOP) -c++ -java -o $(TOP)$Sgen$Sgraph$Sgraph_java_wrap.cc -package com.google.ortools.graph -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sgraph graph$Sgraph.swig

$(TOP)/objs/graph_java_wrap.$O: $(TOP)/gen/graph/graph_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(TOP)$Sgen$Sgraph$Sgraph_java_wrap.cc $(OBJOUT)objs$Sgraph_java_wrap.$O

$(TOP)/lib/com.google.ortools.graph.jar: $(TOP)/gen/graph/graph_java_wrap.cc
	$(JAVAC_BIN) -d $(TOP)$Sobjs $(TOP)$Sgen$Scom$Sgoogle$Sortools$Sgraph$S*.java
	$(JAR_BIN) cf $(TOP)$Slib$Scom.google.ortools.graph.jar -C $(TOP)$Sobjs com$Sgoogle$Sortools$Sgraph

$(LIBPREFIX)jnigraph.$(JNILIBEXT): $(TOP)/objs/graph_java_wrap.$O $(GRAPH_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jnigraph.$(JNILIBEXT) $(TOP)$Sobjs$Sgraph_java_wrap.$O $(GRAPH_LNK) $(LDFLAGS)

# Java Algorithms Examples

compile_FlowExample: $(TOP)/objs/com/google/ortools/graph/samples/FlowExample.class

$(TOP)/objs/com/google/ortools/graph/samples/FlowExample.class: javagraph $(TOP)/com/google/ortools/graph/samples/FlowExample.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.graph.jar $(TOP)$Scom$Sgoogle$Sortools$Sgraph$Ssamples$SFlowExample.java

run_FlowExample: compile_FlowExample javagraph
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.graph.jar com.google.ortools.graph.samples.FlowExample

compile_LinearAssignmentAPI: $(TOP)/objs/com/google/ortools/graph/samples/LinearAssignmentAPI.class

$(TOP)/objs/com/google/ortools/graph/samples/LinearAssignmentAPI.class: javagraph $(TOP)/com/google/ortools/graph/samples/LinearAssignmentAPI.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.graph.jar $(TOP)$Scom$Sgoogle$Sortools$Sgraph$Ssamples$SLinearAssignmentAPI.java

run_LinearAssignmentAPI: compile_LinearAssignmentAPI javagraph
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.graph.jar com.google.ortools.graph.samples.LinearAssignmentAPI

# javalp

javalp: $(TOP)/lib/com.google.ortools.linearsolver.jar $(LIBPREFIX)jnilinearsolver.$(JNILIBEXT)

$(TOP)/gen/linear_solver/linear_solver_java_wrap.cc: $(TOP)/linear_solver/linear_solver.swig $(TOP)/base/base.swig $(TOP)/util/data.swig $(TOP)/linear_solver/linear_solver.h $(TOP)/gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -I$(TOP) -c++ -java -o $(TOP)$Sgen$Slinear_solver$Slinear_solver_java_wrap.cc -package com.google.ortools.linearsolver -outdir $(TOP)$Sgen$Scom$Sgoogle$Sortools$Slinearsolver $(TOP)$Slinear_solver$Slinear_solver.swig

$(TOP)/objs/linear_solver_java_wrap.$O: $(TOP)/gen/linear_solver/linear_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c $(TOP)$Sgen$Slinear_solver$Slinear_solver_java_wrap.cc $(OBJOUT)objs$Slinear_solver_java_wrap.$O

$(TOP)/lib/com.google.ortools.linearsolver.jar: $(TOP)/gen/linear_solver/linear_solver_java_wrap.cc
	$(JAVAC_BIN) -d $(TOP)$Sobjs $(TOP)$Sgen$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	$(JAR_BIN) cf $(TOP)$Slib$Scom.google.ortools.linearsolver.jar -C $(TOP)$Sobjs com$Sgoogle$Sortools$Slinearsolver

$(LIBPREFIX)jnilinearsolver.$(JNILIBEXT): $(TOP)/objs/linear_solver_java_wrap.$O $(LP_DEPS)
	$(LD) $(LDOUT)$(LIBPREFIX)jnilinearsolver.$(JNILIBEXT) $(TOP)$Sobjs$Slinear_solver_java_wrap.$O $(LP_LNK) $(LDFLAGS)

# Java Linear Programming Examples

compile_LinearProgramming: $(TOP)/objs/com/google/ortools/linearsolver/samples/LinearProgramming.class

$(TOP)/objs/com/google/ortools/linearsolver/samples/LinearProgramming.class: javalp $(TOP)/com/google/ortools/linearsolver/samples/LinearProgramming.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.linearsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$SLinearProgramming.java

run_LinearProgramming: compile_LinearProgramming
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.LinearProgramming

compile_IntegerProgramming: $(TOP)/objs/com/google/ortools/linearsolver/samples/IntegerProgramming.class

$(TOP)/objs/com/google/ortools/linearsolver/samples/IntegerProgramming.class: javalp $(TOP)/com/google/ortools/linearsolver/samples/IntegerProgramming.java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.linearsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Slinearsolver$Ssamples$SIntegerProgramming.java

run_IntegerProgramming: compile_IntegerProgramming
	$(JAVA_BIN) -Xss2048k -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.IntegerProgramming

# Compile and Run CP java example:

$(TOP)/objs/com/google/ortools/constraintsolver/samples/$(EX).class: javacp $(TOP)/com/google/ortools/constraintsolver/samples/$(EX).java
	$(JAVAC_BIN) -d $(TOP)$Sobjs -cp $(TOP)$Slib$Scom.google.ortools.constraintsolver.jar $(TOP)$Scom$Sgoogle$Sortools$Sconstraintsolver$Ssamples$S$(EX).java

cjava: $(TOP)/objs/com/google/ortools/constraintsolver/samples/$(EX).class com.google.ortools.constraintsolver.jar

rjava: $(TOP)/objs/com/google/ortools/constraintsolver/samples/$(EX).class $(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT) com.google.ortools.constraintsolver.jar
	$(JAVA_BIN) -Djava.library.path=$(TOP)$Slib -cp $(TOP)$Sobjs$(CPSEP)$(TOP)$Slib$Scom.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.$(EX)

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

