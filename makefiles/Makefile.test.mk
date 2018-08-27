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
test_java_examples: java \
run_AllDifferentExcept0 \
run_AllInterval \
run_CapacitatedVehicleRoutingProblemWithTimeWindows \
run_Circuit \
run_CoinsGridMIP \
run_ColoringMIP \
run_CoveringOpl \
run_Crossword \
run_DeBruijn \
run_Diet \
run_DietMIP \
run_DivisibleBy9Through1 \
run_FlowExample \
run_GolombRuler \
run_IntegerProgramming \
run_Knapsack \
run_KnapsackMIP \
run_LeastDiff \
run_LinearAssignmentAPI \
run_LinearProgramming \
run_LsApi \
run_MagicSquare \
run_Map2 \
run_Map \
run_Minesweeper \
run_MultiThreadTest \
run_NQueens2 \
run_NQueens \
run_Partition \
run_QuasigroupCompletion \
run_RabbitsPheasants \
run_SendMoreMoney2 \
run_SendMoreMoney \
run_SendMostMoney \
run_Seseman \
run_SetCovering2 \
run_SetCovering3 \
run_SetCovering4 \
run_SetCoveringDeployment \
run_SetCovering \
run_SimpleRoutingTest \
run_StableMarriage \
run_StiglerMIP \
run_Strimko2 \
run_Sudoku \
run_SurvoPuzzle \
run_ToNum \
run_Tsp \
run_Vrp \
run_WhoKilledAgatha \
run_Xkcd \
run_YoungTableaux

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
