# Targets to run tests
.PHONY: test_cc_examples
test_cc_examples: cc
	$(BIN_DIR)$Sgolomb$E --size=5
	$(BIN_DIR)$Scvrptw$E
	$(BIN_DIR)$Sflow_api$E
	$(BIN_DIR)$Slinear_programming$E
	$(BIN_DIR)$Sinteger_programming$E
	$(BIN_DIR)$Stsp$E
	$(BIN_DIR)$Sac4r_table_test$E
	$(BIN_DIR)$Sboolean_test$E
	$(BIN_DIR)$Sbug_fz1$E
	$(BIN_DIR)$Scpp11_test$E
	$(BIN_DIR)$Sforbidden_intervals_test$E
	$(BIN_DIR)$Sgcc_test$E
#	$(BIN_DIR)$Sissue173$E
	$(BIN_DIR)$Sissue57$E
	$(BIN_DIR)$Smin_max_test$E
	$(BIN_DIR)$Svisitor_test$E

.PHONY: test_fz_examples
test_fz_examples: fz
	$(BIN_DIR)$Sfz$E $(EX_DIR)$Sflatzinc$Sgolomb.fzn
	$(BIN_DIR)$Sfz$E $(EX_DIR)$Sflatzinc$Salpha.fzn

.PHONY: test_python_examples
test_python_examples: python
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Shidato_table.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Stsp.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Svrp.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Svrpgs.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Scvrp.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Scvrptw.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Spyflow_example.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Sknapsack.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Sjobshop_ft06_sat.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Slinear_programming.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Spython$Sinteger_programming.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Stests$Stest_cp_api.py
	$(SET_PYTHONPATH) "$(PYTHON_EXECUTABLE)" $(EX_DIR)$Stests$Stest_lp_api.py

.PHONY: test_java_examples
test_java_examples: java
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

.PHONY: test_donet_examples
test_dotnet_examples: dotnet
# C# tests
	"$(DOTNET_BIN)" $(BIN_DIR)$Sa_puzzle$D
	"$(DOTNET_BIN)" $(BIN_DIR)$Stsp$D
# F# tests
	"$(DOTNET_BIN)" $(BIN_DIR)$SProgram$D

# csharp test
.PHONY: test_csharp_examples
test_csharp_examples: csharp
	$(warning C# netfx not working)
	$(MONO) $(BIN_DIR)$Scslinearprogramming$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsintegerprogramming$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsrabbitspheasants$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsflow$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsknapsack$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sorganize_day_intervals$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Snurses_sat$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sjobshop_ft06_sat$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsls_api$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scstsp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scscvrptw$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stest_sat_model$(CLR_EXE_SUFFIX).exe

.PHONY: test_fsharp_examples
test_fsharp_examples: fsharp
	$(warning F# netfx tests unimplemented)
