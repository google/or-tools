# ---------- Java support using SWIG ----------

# javacp

javacp: com.google.ortools.constraintsolver.jar $(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT)
gen/constraint_solver/constraint_solver_java_wrap.cc: constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) -c++ -java -o gen$Sconstraint_solver$Sconstraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -outdir gen$Scom$Sgoogle$Sortools$Sconstraintsolver constraint_solver$Sconstraint_solver.swig
	$(FIX_SWIG)

objs/constraint_solver_java_wrap.$O: gen/constraint_solver/constraint_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/constraint_solver/constraint_solver_java_wrap.cc $(OBJOUT)objs/constraint_solver_java_wrap.$O

com.google.ortools.constraintsolver.jar: gen/constraint_solver/constraint_solver_java_wrap.cc
	$(JAVAC_BIN) -d objs com$Sgoogle$Sortools$Sconstraintsolver$S*.java gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	$(JAR_BIN) cf com.google.ortools.constraintsolver.jar -C objs com$Sgoogle$Sortools$Sconstraintsolver

$(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT): objs/constraint_solver_java_wrap.$O $(CP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniconstraintsolver.$(JNILIBEXT) objs/constraint_solver_java_wrap.$O $(CP_LIBS) $(BASE_LIBS) $(LDFLAGS)

# Java CP Examples

compile_RabbitsPheasants: objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class

objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class: javacp com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com$Sgoogle$Sortools$Sconstraintsolver$Ssamples$SRabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.$(JAR_BIN) com.google.ortools.constraintsolver.samples.RabbitsPheasants

compile_GolombRuler: objs/com/google/ortools/constraintsolver/samples/GolombRuler.class

objs/com/google/ortools/constraintsolver/samples/GolombRuler.class: javacp com/google/ortools/constraintsolver/samples/GolombRuler.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/GolombRuler.java

run_GolombRuler: compile_GolombRuler
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.GolombRuler

compile_Partition: objs/com/google/ortools/constraintsolver/samples/Partition.class

objs/com/google/ortools/constraintsolver/samples/Partition.class: javacp com/google/ortools/constraintsolver/samples/Partition.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Partition.java

run_Partition: compile_Partition
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Partition

compile_SendMoreMoney: objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class

objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class: javacp com/google/ortools/constraintsolver/samples/SendMoreMoney.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SendMoreMoney.java

run_SendMoreMoney: compile_SendMoreMoney
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney

compile_SendMoreMoney2: objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class

objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class: javacp com/google/ortools/constraintsolver/samples/SendMoreMoney2.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SendMoreMoney2.java

run_SendMoreMoney2: compile_SendMoreMoney2
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney2

compile_LeastDiff: objs/com/google/ortools/constraintsolver/samples/LeastDiff.class

objs/com/google/ortools/constraintsolver/samples/LeastDiff.class: javacp com/google/ortools/constraintsolver/samples/LeastDiff.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/LeastDiff.java

run_LeastDiff: compile_LeastDiff
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.LeastDiff

compile_MagicSquare: objs/com/google/ortools/constraintsolver/samples/MagicSquare.class

objs/com/google/ortools/constraintsolver/samples/MagicSquare.class: javacp com/google/ortools/constraintsolver/samples/MagicSquare.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/MagicSquare.java

run_MagicSquare: compile_MagicSquare
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.MagicSquare

compile_NQueens: objs/com/google/ortools/constraintsolver/samples/NQueens.class

objs/com/google/ortools/constraintsolver/samples/NQueens.class: javacp com/google/ortools/constraintsolver/samples/NQueens.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/NQueens.java

run_NQueens: compile_NQueens
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens

compile_NQueens2: objs/com/google/ortools/constraintsolver/samples/NQueens2.class

objs/com/google/ortools/constraintsolver/samples/NQueens2.class: javacp com/google/ortools/constraintsolver/samples/NQueens2.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/NQueens2.java

run_NQueens2: compile_NQueens2
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens2


compile_AllDifferentExcept0: objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class

objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class: javacp com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java

run_AllDifferentExcept0: compile_AllDifferentExcept0
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.AllDifferentExcept0


compile_Diet: objs/com/google/ortools/constraintsolver/samples/Diet.class

objs/com/google/ortools/constraintsolver/samples/Diet.class: javacp com/google/ortools/constraintsolver/samples/Diet.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Diet.java

run_Diet: compile_Diet
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Diet


compile_Map: objs/com/google/ortools/constraintsolver/samples/Map.class

objs/com/google/ortools/constraintsolver/samples/Map.class: javacp com/google/ortools/constraintsolver/samples/Map.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Map.java

run_Map: compile_Map
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map


compile_Map2: objs/com/google/ortools/constraintsolver/samples/Map2.class

objs/com/google/ortools/constraintsolver/samples/Map2.class: javacp com/google/ortools/constraintsolver/samples/Map2.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Map2.java

run_Map2: compile_Map2
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map2


compile_Minesweeper: objs/com/google/ortools/constraintsolver/samples/Minesweeper.class

objs/com/google/ortools/constraintsolver/samples/Minesweeper.class: javacp com/google/ortools/constraintsolver/samples/Minesweeper.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Minesweeper.java

run_Minesweeper: compile_Minesweeper
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Minesweeper


compile_QuasigroupCompletion: objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class

objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class: javacp com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java

run_QuasigroupCompletion: compile_QuasigroupCompletion
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.QuasigroupCompletion


compile_SendMostMoney: objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class

objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class: javacp com/google/ortools/constraintsolver/samples/SendMostMoney.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SendMostMoney.java

run_SendMostMoney: compile_SendMostMoney
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMostMoney


compile_Seseman: objs/com/google/ortools/constraintsolver/samples/Seseman.class

objs/com/google/ortools/constraintsolver/samples/Seseman.class: javacp com/google/ortools/constraintsolver/samples/Seseman.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Seseman.java

run_Seseman: compile_Seseman
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Seseman


compile_Sudoku: objs/com/google/ortools/constraintsolver/samples/Sudoku.class

objs/com/google/ortools/constraintsolver/samples/Sudoku.class: javacp com/google/ortools/constraintsolver/samples/Sudoku.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Sudoku.java

run_Sudoku: compile_Sudoku
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Sudoku


compile_Xkcd: objs/com/google/ortools/constraintsolver/samples/Xkcd.class

objs/com/google/ortools/constraintsolver/samples/Xkcd.class: javacp com/google/ortools/constraintsolver/samples/Xkcd.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Xkcd.java

run_Xkcd: compile_Xkcd
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Xkcd


compile_SurvoPuzzle: objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class

objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class: javacp com/google/ortools/constraintsolver/samples/SurvoPuzzle.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SurvoPuzzle.java

run_SurvoPuzzle: compile_SurvoPuzzle
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SurvoPuzzle


compile_Circuit: objs/com/google/ortools/constraintsolver/samples/Circuit.class

objs/com/google/ortools/constraintsolver/samples/Circuit.class: javacp com/google/ortools/constraintsolver/samples/Circuit.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Circuit.java

run_Circuit: compile_Circuit
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Circuit


compile_CoinsGrid: objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class

objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class: javacp com/google/ortools/constraintsolver/samples/CoinsGrid.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/CoinsGrid.java

run_CoinsGrid: compile_CoinsGrid
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.CoinsGrid

# javaalgorithms

javaalgorithms: com.google.ortools.knapsacksolver.jar $(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT)

gen/algorithms/knapsack_solver_java_wrap.cc: algorithms/knapsack_solver.swig base/base.swig util/data.swig algorithms/knapsack_solver.h
	$(SWIG_BINARY) -c++ -java -o gen/algorithms/knapsack_solver_java_wrap.cc -package com.google.ortools.knapsacksolver -outdir gen/com/google/ortools/knapsacksolver algorithms/knapsack_solver.swig

objs/knapsack_solver_java_wrap.$O: gen/algorithms/knapsack_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/algorithms/knapsack_solver_java_wrap.cc $(OBJOUT)objs/knapsack_solver_java_wrap.$O

com.google.ortools.knapsacksolver.jar: gen/algorithms/knapsack_solver_java_wrap.cc
	$(JAVAC_BIN) -d objs gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	$(JAR_BIN) cf com.google.ortools.knapsacksolver.jar -C objs com$Sgoogle$Sortools$Sknapsacksolver

$(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT): objs/knapsack_solver_java_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniknapsacksolver.$(JNILIBEXT) objs/knapsack_solver_java_wrap.$O $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# Java Algorithms Examples

compile_Knapsack: objs/com/google/ortools/knapsacksolver/samples/Knapsack.class

objs/com/google/ortools/knapsacksolver/samples/Knapsack.class: javaalgorithms com/google/ortools/knapsacksolver/samples/Knapsack.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.knapsacksolver.jar com/google/ortools/knapsacksolver/samples/Knapsack.java

run_Knapsack: compile_Knapsack
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.knapsacksolver.jar com.google.ortools.knapsacksolver.samples.Knapsack

# javagraph

javagraph: com.google.ortools.flow.jar $(LIBPREFIX)jniflow.$(JNILIBEXT)

gen/graph/flow_java_wrap.cc: graph/flow.swig base/base.swig util/data.swig graph/max_flow.h graph/min_cost_flow.h
	$(SWIG_BINARY) -c++ -java -o gen/graph/flow_java_wrap.cc -package com.google.ortools.flow -outdir gen/com/google/ortools/flow graph/flow.swig

objs/flow_java_wrap.$O: gen/graph/flow_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/graph/flow_java_wrap.cc $(OBJOUT)objs/flow_java_wrap.$O

com.google.ortools.flow.jar: gen/graph/flow_java_wrap.cc
	$(JAVAC_BIN) -d objs gen$Scom$Sgoogle$Sortools$Sflow$S*.java
	$(JAR_BIN) cf com.google.ortools.flow.jar -C objs com$Sgoogle$Sortools$Sflow

$(LIBPREFIX)jniflow.$(JNILIBEXT): objs/flow_java_wrap.$O $(GRAPH_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(LIBPREFIX)jniflow.$(JNILIBEXT) objs/flow_java_wrap.$O $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS)

# Java Algorithms Examples

compile_FlowExample: objs/com/google/ortools/flow/samples/FlowExample.class

objs/com/google/ortools/flow/samples/FlowExample.class: javagraph com/google/ortools/flow/samples/FlowExample.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.flow.jar com/google/ortools/flow/samples/FlowExample.java

run_FlowExample: compile_FlowExample javagraph
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.flow.jar com.google.ortools.flow.samples.FlowExample

# javalp

javalp: com.google.ortools.linearsolver.jar $(LIBPREFIX)jnilinearsolver.$(JNILIBEXT)

gen/linear_solver/linear_solver_java_wrap.cc: linear_solver/linear_solver.swig base/base.swig util/data.swig linear_solver/linear_solver.h gen/linear_solver/linear_solver.pb.h
	$(SWIG_BINARY) $(SWIG_INC) -c++ -java -o gen$Slinear_solver$Slinear_solver_java_wrap.cc -package com.google.ortools.linearsolver -outdir gen$Scom$Sgoogle$Sortools$Slinearsolver linear_solver$Slinear_solver.swig

objs/linear_solver_java_wrap.$O: gen/linear_solver/linear_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/linear_solver/linear_solver_java_wrap.cc $(OBJOUT)objs/linear_solver_java_wrap.$O

com.google.ortools.linearsolver.jar: gen/linear_solver/linear_solver_java_wrap.cc
	$(JAVAC_BIN) -d objs gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	$(JAR_BIN) cf com.google.ortools.linearsolver.jar -C objs com$Sgoogle$Sortools$Slinearsolver

$(LIBPREFIX)jnilinearsolver.$(JNILIBEXT): objs/linear_solver_java_wrap.$O $(LP_LIBS) $(BASE_LIBS)
	$(LD) $(LDOUT)$(LIBPREFIX)jnilinearsolver.$(JNILIBEXT) objs/linear_solver_java_wrap.$O $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# Java Algorithms Examples

compile_LinearProgramming: objs/com/google/ortools/linearsolver/samples/LinearProgramming.class

objs/com/google/ortools/linearsolver/samples/LinearProgramming.class: javalp com/google/ortools/linearsolver/samples/LinearProgramming.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.linearsolver.jar com/google/ortools/linearsolver/samples/LinearProgramming.java

run_LinearProgramming: compile_LinearProgramming
	$(JAVA_BIN) -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.LinearProgramming

compile_IntegerProgramming: objs/com/google/ortools/linearsolver/samples/IntegerProgramming.class

objs/com/google/ortools/linearsolver/samples/IntegerProgramming.class: javalp com/google/ortools/linearsolver/samples/IntegerProgramming.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.linearsolver.jar com/google/ortools/linearsolver/samples/IntegerProgramming.java

run_IntegerProgramming: compile_IntegerProgramming
	$(JAVA_BIN) -Xss2048k -Djava.library.path=. -cp objs$(CPSEP)com.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.IntegerProgramming
