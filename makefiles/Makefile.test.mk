# Targets to run tests
.PHONY: test_cc_examples
test_cc_examples: cc
	$(BIN_DIR)$Sgolomb$E --size=5
	$(BIN_DIR)$Scvrptw$E
	$(BIN_DIR)$Sflow_api$E
	$(BIN_DIR)$Slinear_programming$E
	$(BIN_DIR)$Sinteger_programming$E
	$(BIN_DIR)$Stsp$E

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
 run_RabbitsPheasants \
 run_FlowExample \
 run_Tsp \
 run_LinearProgramming \
 run_IntegerProgramming \
 run_Knapsack \
 run_MultiThreadIntegerProgramming

# csharp test
.PHONY: test_csharp_examples
test_csharp_examples: csharp
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
	$(warning F# tests unimplemented)
