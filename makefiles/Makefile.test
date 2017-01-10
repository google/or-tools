.PHONY : test
test: test_cc test_python test_java test_csharp
	@echo Or-tools have been built and tested for $(BUILT_LANGUAGES)

test_cc: cc
	$(BIN_DIR)$Sgolomb$E --size=5
	$(BIN_DIR)$Scvrptw$E
	$(BIN_DIR)$Sflow_api$E
	$(BIN_DIR)$Slinear_programming$E
	$(BIN_DIR)$Sinteger_programming$E
	$(BIN_DIR)$Stsp$E

test_python_examples: python
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Spython$Shidato_table.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Spython$Stsp.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Spython$Spyflow_example.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Spython$Sknapsack.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Spython$Slinear_programming.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Spython$Sinteger_programming.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Stests$Stest_cp_api.py
	$(SET_PYTHONPATH) $(PYTHON_EXECUTABLE) $(EX_DIR)$Stests$Stest_lp_api.py

test_java_examples: java run_RabbitsPheasants run_FlowExample \
run_Tsp run_LinearProgramming run_IntegerProgramming \
run_Knapsack run_MultiThreadIntegerProgramming

# csharp test
test_csharp_examples: $(CSHARPEXE) $(BIN_DIR)/testlp$(CLR_EXE_SUFFIX).exe $(BIN_DIR)/testcp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scslinearprogramming$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsintegerprogramming$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsrabbitspheasants$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsflow$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsknapsack$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sorganize_day_intervals$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Sfurniture_moving_intervals$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stechtalk_scheduling$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scsls_api$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scstsp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Scscvrptw$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestlp$(CLR_EXE_SUFFIX).exe
	$(MONO) $(BIN_DIR)$Stestcp$(CLR_EXE_SUFFIX).exe