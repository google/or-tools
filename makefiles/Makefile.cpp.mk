# ---------- C++ support ----------
.PHONY: help_cc # Generate list of C++ targets with descriptions.
help_cc:
	@echo Use one of the following C++ targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.cpp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.cpp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

# Checks if the user has overwritten default install prefix.
# cf https://www.gnu.org/prep/standards/html_node/Directory-Variables.html#index-prefix
ifeq ($(SYSTEM),win)
  prefix ?= C:\\Program Files\\or-tools
else
  prefix ?= /usr/local
endif

# All libraries and dependecies
OR_TOOLS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)ortools.$L
OR_TOOLS_LNK += $(PRE_LIB)ortools$(POST_LIB)

HAS_CCC = true
ifndef CCC
HAS_CCC =
endif

# Main target
.PHONY: cc # Build C++ OR-Tools library.
.PHONY: check_cc # Quick check only running C++ OR-Tools samples targets.
.PHONY: test_cc # Run all C++ OR-Tools test targets.
.PHONY: test_fz # Run all Flatzinc OR-Tools examples.
ifndef HAS_CCC
cc:
	@echo CCC = $(CCC)
	$(warning Cannot find '$@' command which is needed for build. Please make sure it is installed and in system PATH.)
check_cc: cc
test_cc: cc
test_fz: cc
else
cc: $(OR_TOOLS_LIBS)
check_cc: check_cc_pimpl
test_cc: test_cc_pimpl
test_fz: test_fz_pimpl
BUILT_LANGUAGES += C++
endif

$(GEN_DIR):
	-$(MKDIR_P) $(GEN_PATH)

$(GEN_DIR)/ortools: | $(GEN_DIR)
	-$(MKDIR_P) $(GEN_PATH)$Sortools

$(GEN_DIR)/ortools/algorithms: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Salgorithms

$(GEN_DIR)/ortools/bop: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sbop

$(GEN_DIR)/ortools/constraint_solver: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sconstraint_solver

$(GEN_DIR)/ortools/data: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sdata

$(GEN_DIR)/ortools/flatzinc: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sflatzinc

$(GEN_DIR)/ortools/glop: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sglop

$(GEN_DIR)/ortools/graph: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sgraph

$(GEN_DIR)/ortools/linear_solver: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Slinear_solver

$(GEN_DIR)/ortools/sat: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Ssat

$(GEN_DIR)/ortools/util: | $(GEN_DIR)/ortools
	-$(MKDIR) $(GEN_PATH)$Sortools$Sutil

$(BIN_DIR):
	-$(MKDIR) $(BIN_DIR)

$(LIB_DIR):
	-$(MKDIR) $(LIB_DIR)

$(OBJ_DIR):
	-$(MKDIR) $(OBJ_DIR)

$(OBJ_DIR)/algorithms: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Salgorithms

$(OBJ_DIR)/base: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sbase

$(OBJ_DIR)/bop: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sbop

$(OBJ_DIR)/constraint_solver: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sconstraint_solver

$(OBJ_DIR)/data: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sdata

$(OBJ_DIR)/flatzinc: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sflatzinc

$(OBJ_DIR)/glop: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sglop

$(OBJ_DIR)/graph: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sgraph

$(OBJ_DIR)/linear_solver: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Slinear_solver

$(OBJ_DIR)/lp_data: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Slp_data

$(OBJ_DIR)/port: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sport

$(OBJ_DIR)/sat: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Ssat

$(OBJ_DIR)/util: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sutil

$(OBJ_DIR)/swig: | $(OBJ_DIR)
	-$(MKDIR_P) $(OBJ_DIR)$Sswig

###############
##  CPP LIB  ##
###############
# build from: $> grep "pb\.h:" makefiles/Makefile.gen.mk
PROTO_DEPS = \
$(GEN_DIR)/ortools/util/optional_boolean.pb.h \
$(GEN_DIR)/ortools/data/jobshop_scheduling.pb.h \
$(GEN_DIR)/ortools/data/rcpsp.pb.h \
$(GEN_DIR)/ortools/glop/parameters.pb.h \
$(GEN_DIR)/ortools/graph/flow_problem.pb.h \
$(GEN_DIR)/ortools/sat/boolean_problem.pb.h \
$(GEN_DIR)/ortools/sat/cp_model.pb.h \
$(GEN_DIR)/ortools/sat/sat_parameters.pb.h \
$(GEN_DIR)/ortools/bop/bop_parameters.pb.h \
$(GEN_DIR)/ortools/linear_solver/linear_solver.pb.h \
$(GEN_DIR)/ortools/constraint_solver/assignment.pb.h \
$(GEN_DIR)/ortools/constraint_solver/demon_profiler.pb.h \
$(GEN_DIR)/ortools/constraint_solver/routing_enums.pb.h \
$(GEN_DIR)/ortools/constraint_solver/routing_parameters.pb.h \
$(GEN_DIR)/ortools/constraint_solver/search_limit.pb.h \
$(GEN_DIR)/ortools/constraint_solver/solver_parameters.pb.h
include $(OR_ROOT)makefiles/Makefile.gen.mk

all_protos: $(PROTO_DEPS)

# OR Tools unique library.
$(OR_TOOLS_LIBS): \
 dependencies/check.log \
 $(BASE_LIB_OBJS) \
 $(PORT_LIB_OBJS) \
 $(UTIL_LIB_OBJS) \
 $(DATA_LIB_OBJS) \
 $(LP_DATA_LIB_OBJS) \
 $(GLOP_LIB_OBJS) \
 $(BOP_LIB_OBJS) \
 $(LP_LIB_OBJS) \
 $(GRAPH_LIB_OBJS) \
 $(ALGORITHMS_LIB_OBJS) \
 $(SAT_LIB_OBJS) \
 $(CP_LIB_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)ortools.$L \
 $(BASE_LIB_OBJS) \
 $(PORT_LIB_OBJS) \
 $(UTIL_LIB_OBJS) \
 $(DATA_LIB_OBJS) \
 $(LP_DATA_LIB_OBJS) \
 $(GLOP_LIB_OBJS) \
 $(GRAPH_LIB_OBJS) \
 $(ALGORITHMS_LIB_OBJS) \
 $(SAT_LIB_OBJS) \
 $(BOP_LIB_OBJS) \
 $(LP_LIB_OBJS) \
 $(CP_LIB_OBJS) \
 $(DEPENDENCIES_LNK) \
 $(LDFLAGS)

#####################
##  Flatzinc code  ##
#####################
FLATZINC_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fz.$L
FLATZINC_PATH = $(subst /,$S,$(FLATZINC_LIBS))
FLATZINC_DEPS = \
	$(SRC_DIR)/ortools/flatzinc/checker.h \
	$(SRC_DIR)/ortools/flatzinc/constraints.h \
	$(SRC_DIR)/ortools/flatzinc/cp_model_fz_solver.h \
	$(SRC_DIR)/ortools/flatzinc/flatzinc_constraints.h \
	$(SRC_DIR)/ortools/flatzinc/logging.h \
	$(SRC_DIR)/ortools/flatzinc/model.h \
	$(SRC_DIR)/ortools/flatzinc/parser.h \
	$(SRC_DIR)/ortools/flatzinc/parser.tab.hh \
	$(SRC_DIR)/ortools/flatzinc/presolve.h \
	$(SRC_DIR)/ortools/flatzinc/reporting.h \
	$(SRC_DIR)/ortools/flatzinc/sat_constraint.h \
	$(SRC_DIR)/ortools/flatzinc/solver_data.h \
	$(SRC_DIR)/ortools/flatzinc/solver.h \
	$(SRC_DIR)/ortools/flatzinc/solver_util.h \
	$(CP_DEPS) \
	$(SAT_DEPS)
FLATZINC_LNK = $(PRE_LIB)fz$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
FLATZINK_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)fz.$L #
endif

FLATZINC_OBJS=\
	$(OBJ_DIR)/flatzinc/checker.$O \
	$(OBJ_DIR)/flatzinc/constraints.$O \
	$(OBJ_DIR)/flatzinc/cp_model_fz_solver.$O \
	$(OBJ_DIR)/flatzinc/flatzinc_constraints.$O \
	$(OBJ_DIR)/flatzinc/logging.$O \
	$(OBJ_DIR)/flatzinc/model.$O \
	$(OBJ_DIR)/flatzinc/parser.$O \
	$(OBJ_DIR)/flatzinc/parser.tab.$O \
	$(OBJ_DIR)/flatzinc/parser.yy.$O \
	$(OBJ_DIR)/flatzinc/presolve.$O \
	$(OBJ_DIR)/flatzinc/reporting.$O \
	$(OBJ_DIR)/flatzinc/sat_constraint.$O \
	$(OBJ_DIR)/flatzinc/solver.$O \
	$(OBJ_DIR)/flatzinc/solver_data.$O \
	$(OBJ_DIR)/flatzinc/solver_util.$O

FLATZINC_CC_TESTS = \
boolean_test

fz_parser: #$(SRC_DIR)/ortools/flatzinc/parser.lex $(SRC_DIR)/ortools/flatzinc/parser.yy
	flex -o $(SRC_DIR)/ortools/flatzinc/parser.yy.cc $(SRC_DIR)/ortools/flatzinc/parser.lex
	bison -t -o $(SRC_DIR)/ortools/flatzinc/parser.tab.cc -d $(SRC_DIR)/ortools/flatzinc/parser.yy

$(OBJ_DIR)/flatzinc/%.$O: $(SRC_DIR)/ortools/flatzinc/%.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$S$*.$O

$(FLATZINC_LIBS): $(OR_TOOLS_LIBS) $(FLATZINC_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(FLATZINK_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)fz.$L \
 $(FLATZINC_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

$(OBJ_DIR)/boolean_test.$O: $(TEST_DIR)/boolean_test.cc $(FLATZINC_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(TEST_PATH)$Sboolean_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sboolean_test.$O

$(BIN_DIR)/boolean_test$E: $(OBJ_DIR)/boolean_test.$O $(FLATZINC_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sboolean_test.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sboolean_test$E

.PHONY: fz # Build Flatzinc binaries.
fz: \
 $(BIN_DIR)/fz$E \
 $(BIN_DIR)/parser_main$E \
 $(addsuffix $E, $(addprefix $(BIN_DIR)/, $(FLATZINC_CC_TESTS)))

$(BIN_DIR)/fz$E: $(OBJ_DIR)/flatzinc/fz.$O $(FLATZINC_LIBS) $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflatzinc$Sfz.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sfz$E

$(BIN_DIR)/parser_main$E: $(OBJ_DIR)/flatzinc/parser_main.$O $(FLATZINC_LIBS) $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflatzinc$Sparser_main.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sparser_main$E

##################
##  Sat solver  ##
##################
sat: $(BIN_DIR)/sat_runner$E

$(OBJ_DIR)/sat_runner.$O: \
 $(CC_EX_DIR)/sat_runner.cc \
 $(CC_EX_DIR)/opb_reader.h \
 $(CC_EX_DIR)/sat_cnf_reader.h \
 $(SAT_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$Ssat_runner.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat_runner.$O

##################
##  C++ SOURCE  ##
##################
ifeq ($(SOURCE_SUFFIX),.cc) # Those rules will be used if SOURCE contain a .cc file
$(OBJ_DIR)/$(SOURCE_NAME).$O: $(SOURCE) $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) \
 -c $(SOURCE_PATH) \
 $(OBJ_OUT)$(OBJ_DIR)$S$(SOURCE_NAME).$O

$(BIN_DIR)/$(SOURCE_NAME)$E: $(OBJ_DIR)/$(SOURCE_NAME).$O $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) \
 $(OBJ_DIR)$S$(SOURCE_NAME).$O \
 $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) \
 $(EXE_OUT)$(BIN_DIR)$S$(SOURCE_NAME)$E

.PHONY: build # Build a C++ program.
build: $(BIN_DIR)/$(SOURCE_NAME)$E

.PHONY: run # Run a C++ program.
run: build
	$(BIN_DIR)$S$(SOURCE_NAME)$E $(ARGS)
endif

##################################
##  CPP Tests/Examples/Samples  ##
##################################
# Generic Command
$(OBJ_DIR)/%.$O: $(TEST_DIR)/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(TEST_PATH)$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: $(CC_EX_DIR)/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: $(CONTRIB_EX_DIR)/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CONTRIB_EX_PATH)$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: ortools/algorithms/samples/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Salgorithms$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: ortools/graph/samples/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Sgraph$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: ortools/linear_solver/samples/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Slinear_solver$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: ortools/constraint_solver/samples/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Sconstraint_solver$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: ortools/sat/samples/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Ssat$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(OBJ_DIR)/%.$O: ortools/routing/samples/%.cc $(OR_TOOLS_LIBS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Srouting$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(BIN_DIR)/%$E: $(OBJ_DIR)/%.$O $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$S$*.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$S$*$E

rcc_%: $(BIN_DIR)/%$E FORCE
	$(BIN_DIR)$S$*$E $(ARGS)

.PHONY: test_cc_algorithms_samples # Build and Run all C++ Algorithms Samples (located in ortools/algorithms/samples)
test_cc_algorithms_samples: \
 rcc_simple_knapsack_program

.PHONY: test_cc_graph_samples # Build and Run all C++ Graph Samples (located in ortools/graph/samples)
test_cc_graph_samples: \
 rcc_simple_max_flow_program \
 rcc_simple_min_cost_flow_program

.PHONY: test_cc_linear_solver_samples # Build and Run all C++ LP Samples (located in ortools/linear_solver/samples)
test_cc_linear_solver_samples: \
 rcc_simple_lp_program \
 rcc_simple_mip_program \
 rcc_linear_programming_example \
 rcc_integer_programming_example

.PHONY: test_cc_constraint_solver_samples # Build and Run all C++ CP Samples (located in ortools/constraint_solver/samples)
test_cc_constraint_solver_samples: \
 rcc_minimal_jobshop_cp \
 rcc_nurses_cp \
 rcc_rabbits_and_pheasants_cp \
 rcc_simple_ls_program \
 rcc_simple_cp_program \
 rcc_simple_routing_program \
 rcc_tsp \
 rcc_tsp_cities \
 rcc_tsp_circuit_board \
 rcc_tsp_distance_matrix \
 rcc_vrp \
 rcc_vrp_capacity \
 rcc_vrp_drop_nodes \
 rcc_vrp_global_span \
 rcc_vrp_initial_routes \
 rcc_vrp_pickup_delivery \
 rcc_vrp_pickup_delivery_fifo \
 rcc_vrp_pickup_delivery_lifo \
 rcc_vrp_resources \
 rcc_vrp_starts_ends \
 rcc_vrp_time_windows \
 rcc_vrp_with_time_limit

.PHONY: test_cc_sat_samples # Build and Run all C++ Sat Samples (located in ortools/sat/samples)
test_cc_sat_samples: \
 rcc_binpacking_problem_sat \
 rcc_bool_or_sample_sat \
 rcc_channeling_sample_sat \
 rcc_cp_is_fun_sat \
 rcc_earliness_tardiness_cost_sample_sat \
 rcc_interval_sample_sat \
 rcc_literal_sample_sat \
 rcc_no_overlap_sample_sat \
 rcc_optional_interval_sample_sat \
 rcc_rabbits_and_pheasants_sat \
 rcc_ranking_sample_sat \
 rcc_reified_sample_sat \
 rcc_search_for_all_solutions_sample_sat \
 rcc_simple_sat_program \
 rcc_solve_and_print_intermediate_solutions_sample_sat \
 rcc_solve_with_time_limit_sample_sat \
 rcc_step_function_sample_sat \
 rcc_stop_after_n_solutions_sample_sat

.PHONY: check_cc_pimpl
check_cc_pimpl: \
 test_cc_algorithms_samples \
 test_cc_constraint_solver_samples \
 test_cc_graph_samples \
 test_cc_linear_solver_samples \
 test_cc_sat_samples \
 \
 rcc_linear_programming \
 rcc_stigler_diet \
 rcc_constraint_programming_cp \
 rcc_integer_programming \
 rcc_knapsack \
 rcc_max_flow \
 rcc_min_cost_flow ;

.PHONY: test_cc_tests # Build and Run all C++ Tests (located in ortools/examples/tests)
test_cc_tests: \
 rcc_lp_test \
 rcc_boolean_test \
 rcc_bug_fz1 \
 rcc_cpp11_test \
 rcc_forbidden_intervals_test \
 rcc_issue57 \
 rcc_min_max_test
#	$(MAKE) rcc_issue173 # error: too long

.PHONY: test_cc_contrib # Build and Run all C++ Contrib (located in ortools/examples/contrib)
test_cc_contrib: ;

.PHONY: test_cc_cpp # Build and Run all C++ Examples (located in ortools/examples/cpp)
test_cc_cpp: \
 rcc_costas_array_sat \
 rcc_cvrp_disjoint_tw \
 rcc_cvrptw \
 rcc_cvrptw_with_breaks \
 rcc_cvrptw_with_refueling \
 rcc_cvrptw_with_resources \
 rcc_cvrptw_with_stop_times_and_resources \
 rcc_flow_api \
 rcc_linear_assignment_api \
 rcc_linear_solver_protocol_buffers \
 rcc_magic_square_sat \
 rcc_nqueens \
 rcc_random_tsp \
 rcc_slitherlink_sat \
 rcc_strawberry_fields_with_column_generation \
 rcc_weighted_tardiness_sat
	$(MAKE) run \
 SOURCE=examples/cpp/dimacs_assignment.cc \
 ARGS=examples/data/dimacs/assignment/small.asn
	$(MAKE) run \
 SOURCE=examples/cpp/dobble_ls.cc \
 ARGS="--time_limit_in_ms=10000"
	$(MAKE) run \
 SOURCE=examples/cpp/golomb_sat.cc \
 ARGS="--size=5"
	$(MAKE) run \
 SOURCE=examples/cpp/jobshop_sat.cc \
 ARGS="--input=examples/data/jobshop/ft06"
	$(MAKE) run \
 SOURCE=examples/cpp/mps_driver.cc \
 ARGS="--input examples/data/tests/test.mps"
	$(MAKE) run \
 SOURCE=examples/cpp/network_routing_sat.cc \
 ARGS="--clients=10 --backbones=5 --demands=10 --traffic_min=5 --traffic_max=10 --min_client_degree=2 --max_client_degree=5 --min_backbone_degree=3 --max_backbone_degree=5 --max_capacity=20 --fixed_charge_cost=10"
	$(MAKE) run \
 SOURCE=examples/cpp/sports_scheduling_sat.cc \
 ARGS="--params max_time_in_seconds:10.0"
#	$(MAKE) run SOURCE=examples/cpp/frequency_assignment_problem.cc  # Need data file
#	$(MAKE) run SOURCE=examples/cpp/pdptw.cc ARGS="--pdp_file examples/data/pdptw/LC1_2_1.txt" # Fails on windows...
	$(MAKE) run SOURCE=examples/cpp/shift_minimization_sat.cc  ARGS="--input examples/data/shift_scheduling/minimization/data_1_23_40_66.dat"
	$(MAKE) run \
 SOURCE=examples/cpp/solve.cc \
 ARGS="--input examples/data/tests/test2.mps"

.PHONY: test_cc_pimpl
test_cc_pimpl: \
 check_cc_pimpl \
 test_cc_tests \
 test_cc_contrib \
 test_cc_cpp

.PHONY: test_fz_pimpl
test_fz_pimpl: \
 rfz_golomb \
 rfz_alpha

rfz_%: fz $(FZ_EX_DIR)/%.fzn
	$(BIN_DIR)$Sfz$E $(FZ_EX_PATH)$S$*.fzn

################
##  Cleaning  ##
################
CC_SAMPLES := $(wildcard ortools/*/samples/*.cc)
CC_SAMPLES := $(notdir $(CC_SAMPLES))
CC_SAMPLES := $(addsuffix $E, $(addprefix $(BIN_DIR)$S, $(basename $(CC_SAMPLES))))

CC_EXAMPLES := $(wildcard $(CC_EX_DIR)/*.cc)
CC_EXAMPLES := $(notdir $(CC_EXAMPLES))
CC_EXAMPLES := $(addsuffix $E, $(addprefix $(BIN_DIR)$S, $(basename $(CC_EXAMPLES))))

CC_TESTS := $(wildcard $(TEST_DIR)/*.cc)
CC_TESTS := $(notdir $(CC_TESTS))
CC_TESTS := $(addsuffix $E, $(addprefix $(BIN_DIR)$S, $(basename $(CC_TESTS))))

.PHONY: clean_cc # Clean C++ output from previous build.
clean_cc:
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$L
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)dimacs.$L
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)fap.$L
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)fz.$L
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$L
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)*.a
	-$(DEL) $(OBJ_DIR)$S*.$O
	-$(DEL) $(OBJ_DIR)$Salgorithms$S*.$O
	-$(DEL) $(OBJ_DIR)$Sbase$S*.$O
	-$(DEL) $(OBJ_DIR)$Sbop$S*.$O
	-$(DEL) $(OBJ_DIR)$Sconstraint_solver$S*.$O
	-$(DEL) $(OBJ_DIR)$Sdata$S*.$O
	-$(DEL) $(OBJ_DIR)$Sflatzinc$S*.$O
	-$(DEL) $(OBJ_DIR)$Sglop$S*.$O
	-$(DEL) $(OBJ_DIR)$Sgraph$S*.$O
	-$(DEL) $(OBJ_DIR)$Slinear_solver$S*.$O
	-$(DEL) $(OBJ_DIR)$Slp_data$S*.$O
	-$(DEL) $(OBJ_DIR)$Sport$S*.$O
	-$(DEL) $(OBJ_DIR)$Ssat$S*.$O
	-$(DEL) $(OBJ_DIR)$Sutil$S*.$O
	-$(DEL) $(BIN_DIR)$Sfz$E
	-$(DEL) $(BIN_DIR)$Sparser_main$E
	-$(DEL) $(BIN_DIR)$Ssat_runner$E
	-$(DEL) $(CC_SAMPLES)
	-$(DEL) $(CC_EXAMPLES)
	-$(DEL) $(CC_TESTS)
	-$(DEL) $(GEN_PATH)$Sortools$Sbop$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Sdata$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Sflatzinc$S*.tab.*
	-$(DEL) $(GEN_PATH)$Sortools$Sflatzinc$S*.yy.*
	-$(DEL) $(GEN_PATH)$Sortools$Sflatzinc$Sparser.*
	-$(DEL) $(GEN_PATH)$Sortools$Sglop$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*.pb.*
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*.pb.*
	-$(DEL) $(BIN_DIR)$S*.exp
	-$(DEL) $(BIN_DIR)$S*.lib
	-$(DELREC) $(GEN_PATH)$Sflatzinc$S*
	-$(DELREC) $(OBJ_DIR)$Sflatzinc$S*

.PHONY: clean_compat
clean_compat:
	-$(DELREC) $(OR_ROOT)constraint_solver
	-$(DELREC) $(OR_ROOT)linear_solver
	-$(DELREC) $(OR_ROOT)algorithms
	-$(DELREC) $(OR_ROOT)graph
	-$(DELREC) $(OR_ROOT)gen

###############
##  INSTALL  ##
###############
# ref: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html#index-prefix
# ref: https://www.gnu.org/prep/standards/html_node/DESTDIR.html
install_dirs:
	-$(MKDIR_P) "$(DESTDIR)$(prefix)"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Slib"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sbin"

install_ortools_dirs: install_dirs
	-$(DELREC) "$(DESTDIR)$(prefix)$Sinclude$Sortools"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Salgorithms"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbase"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbop"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sconstraint_solver"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sglop"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sgraph"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slinear_solver"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slp_data"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sport"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sdata"

.PHONY: install_cc # Install C++ OR-Tools to $(DESTDIR)$(prefix)
install_cc: install_libortools install_third_party install_doc

.PHONY: install_libortools
install_libortools: $(OR_TOOLS_LIBS) install_ortools_dirs
	$(COPY) LICENSE-2.0.txt "$(DESTDIR)$(prefix)"
	$(COPY) ortools$Salgorithms$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Salgorithms"
	$(COPY) ortools$Sbase$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbase"
	$(COPY) ortools$Sconstraint_solver$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sconstraint_solver"
	$(COPY) $(GEN_PATH)$Sortools$Sconstraint_solver$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sconstraint_solver"
	$(COPY) ortools$Sbop$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbop"
	$(COPY) $(GEN_PATH)$Sortools$Sbop$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbop"
	$(COPY) ortools$Sglop$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sglop"
	$(COPY) $(GEN_PATH)$Sortools$Sglop$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sglop"
	$(COPY) ortools$Sgraph$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sgraph"
	$(COPY) $(GEN_PATH)$Sortools$Sgraph$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sgraph"
	$(COPY) ortools$Slinear_solver$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slinear_solver"
	$(COPY) $(GEN_PATH)$Sortools$Slinear_solver$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slinear_solver"
	$(COPY) ortools$Slp_data$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slp_data"
	$(COPY) ortools$Sport$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sport"
	$(COPY) ortools$Ssat$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	$(COPY) $(GEN_PATH)$Sortools$Ssat$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	$(COPY) ortools$Sutil$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"
	$(COPY) $(GEN_PATH)$Sortools$Sutil$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"
	$(COPY) ortools$Sdata$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sdata"
	$(COPY) $(GEN_PATH)$Sortools$Sdata$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sdata"
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$L "$(DESTDIR)$(prefix)$Slib"

.PHONY: install_third_party
install_third_party: install_dirs
ifeq ($(UNIX_GFLAGS_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Sgflags "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) dependencies$Sinstall$Slib*$Slibgflags* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Sbin$Sgflags_completions.sh "$(DESTDIR)$(prefix)$Sbin"
endif
ifeq ($(UNIX_GLOG_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Sglog "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) dependencies$Sinstall$Slib*$Slibglog* "$(DESTDIR)$(prefix)$Slib"
endif
ifeq ($(UNIX_PROTOBUF_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Sgoogle "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) $(subst /,$S,$(_PROTOBUF_LIB_DIR))$Slibproto* "$(DESTDIR)$(prefix)$Slib"
	$(COPY) dependencies$Sinstall$Sbin$Sprotoc* "$(DESTDIR)$(prefix)$Sbin"
endif
ifeq ($(UNIX_ABSL_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Sabsl "$(DESTDIR)$(prefix)$Sinclude"
	-$(COPYREC) $(subst /,$S,$(_ABSL_STATIC_LIB_DIR))$Slibabsl* "$(DESTDIR)$(prefix)$Slib"
	-$(COPYREC) $(subst /,$S,$(_ABSL_LIB_DIR))$Slibabsl* "$(DESTDIR)$(prefix)$Slib"
endif
ifeq ($(UNIX_CBC_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Scoin "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) dependencies$Sinstall$Slib*$SlibCbc* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib*$SlibCgl* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib*$SlibClp* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib*$SlibOsi* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib*$SlibCoinUtils* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Sbin$Scbc "$(DESTDIR)$(prefix)$Sbin"
	$(COPYREC) dependencies$Sinstall$Sbin$Sclp "$(DESTDIR)$(prefix)$Sbin"
endif
ifeq ($(WINDOWS_ZLIB_DIR),$(OR_ROOT)dependencies/install)
	$(COPY) dependencies$Sinstall$Sinclude$Szlib.h "$(DESTDIR)$(prefix)$Sinclude"
	$(COPY) dependencies$Sinstall$Sinclude$Szconf.h "$(DESTDIR)$(prefix)$Sinclude"
endif
ifeq ($(WINDOWS_GFLAGS_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sgflags"
	$(COPYREC) /E /Y dependencies$Sinstall$Sinclude$Sgflags "$(DESTDIR)$(prefix)$Sinclude$Sgflags"
endif
ifeq ($(WINDOWS_GLOG_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sglog"
	$(COPYREC) /E /Y dependencies$Sinstall$Sinclude$Sglog "$(DESTDIR)$(prefix)$Sinclude$Sglog"
endif
ifeq ($(WINDOWS_PROTOBUF_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sgoogle"
	$(COPYREC) /E /Y dependencies$Sinstall$Sinclude$Sgoogle "$(DESTDIR)$(prefix)$Sinclude$Sgoogle"
endif
ifeq ($(WINDOWS_ABSL_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sabsl"
	$(COPYREC) /E /Y dependencies$Sinstall$Sinclude$Sabsl "$(DESTDIR)$(prefix)$Sinclude$Sabsl"
endif
ifeq ($(WINDOWS_CBC_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Scoin"
	$(COPYREC) /E /Y dependencies$Sinstall$Sinclude$Scoin "$(DESTDIR)$(prefix)$Sinclude$Scoin"
endif

install_doc:
	-$(MKDIR_P) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat$Sdoc"
#$(COPY) ortools$Ssat$S*.md "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat"
	$(COPY) ortools$Ssat$Sdoc$S*.md "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat$Sdoc"

#############
##  DEBUG  ##
#############
.PHONY: detect_cc # Show variables used to build C++ OR-Tools.
detect_cc:
	@echo Relevant info for the C++ build:
	@echo PROTOC = $(PROTOC)
	@echo CCC = $(CCC)
	@echo CFLAGS = $(CFLAGS)
	@echo LDFLAGS = $(LDFLAGS)
	@echo LINK_CMD = $(LINK_CMD)
	@echo DEPENDENCIES_LNK = $(DEPENDENCIES_LNK)
	@echo SRC_DIR = $(SRC_DIR)
	@echo GEN_DIR = $(GEN_DIR)
	@echo CC_EX_DIR = $(CC_EX_DIR)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo LIB_DIR = $(LIB_DIR)
	@echo BIN_DIR = $(BIN_DIR)
	@echo prefix = $(prefix)
	@echo OR_TOOLS_LNK = $(OR_TOOLS_LNK)
	@echo OR_TOOLS_LDFLAGS = $(OR_TOOLS_LDFLAGS)
	@echo OR_TOOLS_LIBS = $(OR_TOOLS_LIBS)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
