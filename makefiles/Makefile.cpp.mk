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
.PHONY: check_cc # Quick check only running few C++ OR-Tools examples.
.PHONY: test_cc # Run all C++ OR-Tools examples.
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

check_cc: check_cc_examples

test_cc: \
 test_cc_tests \
 test_cc_samples \
 test_cc_examples

test_fz: \
 test_fz_examples
BUILT_LANGUAGES += C++
endif

$(GEN_DIR):
	$(MKDIR_P) $(GEN_PATH)

$(GEN_DIR)/ortools: | $(GEN_DIR)
	$(MKDIR_P) $(GEN_PATH)$Sortools

$(GEN_DIR)/ortools/algorithms: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Salgorithms

$(GEN_DIR)/ortools/bop: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sbop

$(GEN_DIR)/ortools/constraint_solver: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sconstraint_solver

$(GEN_DIR)/ortools/data: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sdata

$(GEN_DIR)/ortools/flatzinc: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sflatzinc

$(GEN_DIR)/ortools/glop: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sglop

$(GEN_DIR)/ortools/graph: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sgraph

$(GEN_DIR)/ortools/linear_solver: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Slinear_solver

$(GEN_DIR)/ortools/sat: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Ssat

$(GEN_DIR)/ortools/util: | $(GEN_DIR)/ortools
	$(MKDIR) $(GEN_PATH)$Sortools$Sutil

$(BIN_DIR):
	$(MKDIR) $(BIN_DIR)

$(LIB_DIR):
	$(MKDIR) $(LIB_DIR)

$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

$(OBJ_DIR)/algorithms: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Salgorithms

$(OBJ_DIR)/base: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sbase

$(OBJ_DIR)/bop: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sbop

$(OBJ_DIR)/constraint_solver: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sconstraint_solver

$(OBJ_DIR)/data: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sdata

$(OBJ_DIR)/flatzinc: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sflatzinc

$(OBJ_DIR)/glop: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sglop

$(OBJ_DIR)/graph: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sgraph

$(OBJ_DIR)/linear_solver: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Slinear_solver

$(OBJ_DIR)/lp_data: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Slp_data

$(OBJ_DIR)/port: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sport

$(OBJ_DIR)/sat: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Ssat

$(OBJ_DIR)/util: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sutil

$(OBJ_DIR)/swig: | $(OBJ_DIR)
	$(MKDIR_P) $(OBJ_DIR)$Sswig

###############
##  CPP LIB  ##
###############
include $(OR_ROOT)makefiles/Makefile.gen.mk

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
ifdef WINDOWS_SCIP_DIR
	$(COPY) $(WINDOWS_SCIP_DIR)$Sbin$Sscip.dll $(BIN_DIR)
endif


# Specific libraries for examples, and flatzinc.
CVRPTW_LIBS = $(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$L
CVRPTW_PATH = $(subst /,$S,$(CVRPTW_LIBS))
CVRPTW_DEPS = \
	$(CC_EX_DIR)/cvrptw_lib.h \
	$(CP_DEPS)
CVRPTW_LNK = $(PRE_LIB)cvrptw_lib$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
CVRPTW_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)cvrptw_lib.$L #
endif
cvrptwlibs: $(CVRPTW_LIBS)

DIMACS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)dimacs.$L
DIMACS_PATH = $(subst /,$S,$(DIMACS_LIBS))
DIMACS_DEPS = \
	$(CC_EX_DIR)/parse_dimacs_assignment.h \
	$(CC_EX_DIR)/print_dimacs_assignment.h \
	$(GRAPH_DEPS)
DIMACS_LNK = $(PRE_LIB)dimacs$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
DIMACS_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)dimacs.$L #
endif
dimacslibs: $(DIMACS_LIBS)

FAP_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fap.$L
FAP_PATH = $(subst /,$S,$(FAP_LIBS))
FAP_DEPS = \
	$(CC_EX_DIR)/fap_model_printer.h \
	$(CC_EX_DIR)/fap_parser.h \
	$(CC_EX_DIR)/fap_utilities.h \
	$(CP_DEPS) \
	$(LP_DEPS)
FAP_LNK = $(PRE_LIB)fap$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
FAP_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)fap.$L #
endif
faplibs: $(FAP_LIBS)

# CVRPTW common library
CVRPTW_OBJS = $(OBJ_DIR)/cvrptw_lib.$O
$(CVRPTW_OBJS): \
 $(CC_EX_DIR)/cvrptw_lib.cc \
 $(CC_EX_DIR)/cvrptw_lib.h \
 $(CP_DEPS) \
 | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$Scvrptw_lib.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_lib.$O

$(CVRPTW_LIBS): $(OR_TOOLS_LIBS) $(CVRPTW_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(CVRPTW_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$L \
 $(CVRPTW_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# DIMACS challenge problem format library
DIMACS_OBJS = $(OBJ_DIR)/parse_dimacs_assignment.$O $(OBJ_DIR)/print_dimacs_assignment.$O

$(DIMACS_LIBS): $(OR_TOOLS_LIBS) $(DIMACS_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(DIMACS_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)dimacs.$L \
 $(DIMACS_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# FAP challenge problem format library
FAP_OBJS = \
	$(OBJ_DIR)/fap_model_printer.$O \
	$(OBJ_DIR)/fap_parser.$O \
	$(OBJ_DIR)/fap_utilities.$O

$(FAP_LIBS): $(OR_TOOLS_LIBS) $(FAP_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(FAP_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)fap.$L \
 $(FAP_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# CVRP Problem
$(OBJ_DIR)/cvrp%.$O: $(CC_EX_DIR)/cvrp%.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$Scvrp$*.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrp$*.$O

$(BIN_DIR)/cvrp%$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrp%.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Scvrp$*.$O $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrp$*$E

# Dimacs Assignment Problem
$(OBJ_DIR)/dimacs_assignment.$O: $(CC_EX_DIR)/dimacs_assignment.cc $(DIMACS_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$Sdimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sdimacs_assignment.$O

$(BIN_DIR)/dimacs_assignment$E: $(DIMACS_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/dimacs_assignment.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sdimacs_assignment.$O $(DIMACS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sdimacs_assignment$E

# Frequency Assignment Problem
$(OBJ_DIR)/frequency_assignment_problem.$O: $(CC_EX_DIR)/frequency_assignment_problem.cc $(FAP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$Sfrequency_assignment_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Sfrequency_assignment_problem.$O

$(BIN_DIR)/frequency_assignment_problem$E: $(FAP_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/frequency_assignment_problem.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/frequency_assignment_problem.$O $(FAP_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sfrequency_assignment_problem$E

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

############################
##  CPP Examples/Samples  ##
############################
.PHONY: check_cc_examples
check_cc_examples: cc
	$(MAKE) rcc_linear_programming
	$(MAKE) rcc_stigler_diet
	$(MAKE) rcc_constraint_programming_cp
	$(MAKE) rcc_rabbits_pheasants_cp
	$(MAKE) rcc_integer_programming
	$(MAKE) rcc_tsp
	$(MAKE) rcc_vrp
	$(MAKE) rcc_knapsack
	$(MAKE) rcc_max_flow
	$(MAKE) rcc_min_cost_flow
	$(MAKE) rcc_nurses_cp
	$(MAKE) rcc_job_shop_cp

.PHONY: test_cc_tests # Build and Run all C++ tests (located in examples/tests)
test_cc_tests: cc
	$(MAKE) rcc_ac4r_table_test
	$(MAKE) rcc_boolean_test
	$(MAKE) rcc_bug_fz1
	$(MAKE) rcc_cpp11_test
	$(MAKE) rcc_forbidden_intervals_test
	$(MAKE) rcc_gcc_test
#	$(MAKE) rcc_issue173 # error: too long
	$(MAKE) rcc_issue57
	$(MAKE) rcc_min_max_test
	$(MAKE) rcc_visitor_test

$(OBJ_DIR)/%.$O: $(TEST_DIR)/%.cc \
 $(BASE_DEPS) $(PORT_DEPS) $(UTIL_DEPS) \
 $(DATA_DEPS) $(LP_DATA_DEPS) \
 $(LP_DEPS) $(GLOP_DEPS) $(BOP_DEPS) \
 $(CP_DEPS) $(SAT_DEPS) \
 $(GRAPH_DEPS) $(ALGORITHMS_DEPS) \
 | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(TEST_PATH)$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

.PHONY: test_cc_examples # Build and Run all C++ Examples (located in examples/cpp)
test_cc_examples: cc
	$(MAKE) rcc_linear_programming
	$(MAKE) rcc_integer_programming
	$(MAKE) rcc_constraint_programming_cp
	$(MAKE) rcc_rabbits_pheasants_cp
	$(MAKE) rcc_tsp
	$(MAKE) rcc_vrp
	$(MAKE) rcc_knapsack
	$(MAKE) rcc_max_flow
	$(MAKE) rcc_min_cost_flow
	$(MAKE) rcc_nurses_cp
	$(MAKE) rcc_job_shop_cp
	$(MAKE) rcc_costas_array
	$(MAKE) rcc_cryptarithm
	$(MAKE) rcc_cvrp_disjoint_tw
	$(MAKE) rcc_cvrptw
	$(MAKE) rcc_cvrptw_with_breaks
	$(MAKE) rcc_cvrptw_with_refueling
	$(MAKE) rcc_cvrptw_with_resources
	$(MAKE) rcc_cvrptw_with_stop_times_and_resources
	$(MAKE) rcc_dimacs_assignment ARGS=examples/data/dimacs/assignment/small.asn
	$(MAKE) rcc_dobble_ls
	$(MAKE) rcc_flexible_jobshop ARGS="--data_file examples/data/flexible_jobshop/hurink_data/edata/la01.fjs"
	$(MAKE) rcc_flow_api
#	$(MAKE) rcc_frequency_assignment_problem  # Need data file
	$(MAKE) rcc_golomb ARGS="--size=5"
	$(MAKE) rcc_jobshop ARGS="--data_file=examples/data/jobshop/ft06"
	$(MAKE) rcc_jobshop_earlytardy ARGS="--machine_count=6 --job_count=6"
	$(MAKE) rcc_jobshop_ls ARGS="--data_file=examples/data/jobshop/ft06"
	$(MAKE) rcc_jobshop_sat ARGS="--input=examples/data/jobshop/ft06"
	$(MAKE) rcc_linear_assignment_api
	$(MAKE) rcc_linear_solver_protocol_buffers
	$(MAKE) rcc_ls_api
	$(MAKE) rcc_magic_square
#	$(MAKE) rcc_model_util  # Need data file
	$(MAKE) rcc_mps_driver
	$(MAKE) rcc_multidim_knapsack ARGS="--data_file examples/data/multidim_knapsack/PB1.DAT"
	$(MAKE) rcc_network_routing ARGS="--clients=10 --backbones=5 --demands=10 --traffic_min=5 --traffic_max=10 --min_client_degree=2 --max_client_degree=5 --min_backbone_degree=3 --max_backbone_degree=5 --max_capacity=20 --fixed_charge_cost=10"
	$(MAKE) rcc_nqueens
	$(MAKE) rcc_random_tsp
#	$(MAKE) rcc_pdptw ARGS="--pdp_file examples/data/pdptw/LC1_2_1.txt" # Fails on windows...
#	$(MAKE) rcc_shift_minimization_sat  # Port to new API.
#	$(MAKE) rcc_solve  # Need data file
	$(MAKE) rcc_sports_scheduling ARGS="--num_teams=8 --time_limit=10000"
	$(MAKE) rcc_strawberry_fields_with_column_generation
	$(MAKE) rcc_weighted_tardiness_sat

$(OBJ_DIR)/%.$O: $(CC_EX_DIR)/%.cc \
 $(BASE_DEPS) $(PORT_DEPS) $(UTIL_DEPS) \
 $(DATA_DEPS) $(LP_DATA_DEPS) \
 $(LP_DEPS) $(GLOP_DEPS) $(BOP_DEPS) \
 $(CP_DEPS) $(SAT_DEPS) \
 $(GRAPH_DEPS) $(ALGORITHMS_DEPS) \
 | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(CC_EX_PATH)$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O


.PHONY: test_cc_samples # Build and Run all C++ Samples (located in ortools/*/samples)
test_cc_samples: cc
	$(MAKE) rcc_binpacking_problem
	$(MAKE) rcc_bool_or_sample
	$(MAKE) rcc_channeling_sample
	$(MAKE) rcc_code_sample
	$(MAKE) rcc_interval_sample
	$(MAKE) rcc_literal_sample
	$(MAKE) rcc_no_overlap_sample
	$(MAKE) rcc_optional_interval_sample
	$(MAKE) rcc_rabbits_and_pheasants
	$(MAKE) rcc_ranking_sample
	$(MAKE) rcc_reified_sample
	$(MAKE) rcc_simple_solve
	$(MAKE) rcc_solve_all_solutions
	$(MAKE) rcc_solve_with_intermediate_solutions
	$(MAKE) rcc_solve_with_time_limit
	$(MAKE) rcc_stop_after_n_solutions

$(OBJ_DIR)/%.$O: ortools/sat/samples/%.cc \
 $(BASE_DEPS) $(PORT_DEPS) $(UTIL_DEPS) \
 $(DATA_DEPS) $(LP_DATA_DEPS) \
 $(LP_DEPS) $(GLOP_DEPS) $(BOP_DEPS) \
 $(CP_DEPS) $(SAT_DEPS) \
 $(GRAPH_DEPS) $(ALGORITHMS_DEPS) \
 | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c ortools$Ssat$Ssamples$S$*.cc $(OBJ_OUT)$(OBJ_DIR)$S$*.$O

$(BIN_DIR)/%$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/%.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$S$*.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$S$*$E

rcc_%: $(BIN_DIR)/%$E
	$(BIN_DIR)$S$*$E $(ARGS)

.PHONY: test_fz_examples # Build and Run few Flatzinc Samples (located in examples/flatzinc)
test_fz_examples: fz
	$(MAKE) rfz_golomb
	$(MAKE) rfz_alpha

rfz_%: fz $(FZ_EX_DIR)/%.fzn
	$(BIN_DIR)$Sfz$E $(FZ_EX_PATH)$S$*.fzn
####################
##  C++ Examples  ##
####################
ifeq ($(EX),) # Those rules will be used if EX variable is not set
.PHONY: rcc ccc
rcc ccc:
	@echo No C++ file was provided, the $@ target must be used like so: \
 make $@ EX=examples/cpp/example.cc
else # This generic rule will be used if EX variable is set
EX_NAME = $(basename $(notdir $(EX)))

.PHONY: ccc
ccc: $(BIN_DIR)/$(EX_NAME)$E

.PHONY: rcc
rcc: $(BIN_DIR)/$(EX_NAME)$E
	@echo running $<
	$(BIN_DIR)$S$(EX_NAME)$E $(ARGS)
endif # ifeq ($(EX),)

################
##  Cleaning  ##
################
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
	-$(DEL) $(addsuffix $E, $(addprefix $(BIN_DIR)$S, $(CC_SAMPLES) $(CC_EXAMPLES) $(CC_TESTS)))
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
ifdef WINDOWS_SCIP_DIR
	-$(DEL) $(BIN_DIR)$Sscip.dll
endif

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
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Salgorithms"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbase"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sbop"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sconstraint_solver"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sglop"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sgraph"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slinear_solver"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slp_data"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sport"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sdata"

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
	$(COPYREC) dependencies$Sinstall$Slib$Slibgflags* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Sbin$Sgflags_completions.sh "$(DESTDIR)$(prefix)$Sbin"
endif
ifeq ($(UNIX_GLOG_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Sglog "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) dependencies$Sinstall$Slib$Slibglog* "$(DESTDIR)$(prefix)$Slib"
endif
ifeq ($(UNIX_PROTOBUF_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Sgoogle "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) $(subst /,$S,$(_PROTOBUF_LIB_DIR))$Slibproto* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Sbin$Sprotoc "$(DESTDIR)$(prefix)$Sbin"
endif
ifeq ($(UNIX_CBC_DIR),$(OR_TOOLS_TOP)/dependencies/install)
	$(COPYREC) dependencies$Sinstall$Sinclude$Scoin "$(DESTDIR)$(prefix)$Sinclude"
	$(COPYREC) dependencies$Sinstall$Slib$SlibCbc* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib$SlibCgl* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib$SlibClp* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib$SlibOsi* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Slib$SlibCoinUtils* "$(DESTDIR)$(prefix)$Slib"
	$(COPYREC) dependencies$Sinstall$Sbin$Scbc "$(DESTDIR)$(prefix)$Sbin"
	$(COPYREC) dependencies$Sinstall$Sbin$Sclp "$(DESTDIR)$(prefix)$Sbin"
endif
ifeq ($(WINDOWS_ZLIB_DIR),$(OR_ROOT)dependencies/install)
	$(COPY) dependencies$Sinstall$Sinclude$Szlib.h "$(DESTDIR)$(prefix)$Sinclude"
	$(COPY) dependencies$Sinstall$Sinclude$Szconf.h "$(DESTDIR)$(prefix)$Sinclude"
endif
ifeq ($(WINDOWS_GFLAGS_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sgflags"
	$(COPYREC) dependencies$Sinstall$Sinclude$Sgflags "$(DESTDIR)$(prefix)$Sinclude$Sgflags"
endif
ifeq ($(WINDOWS_GLOG_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sglog"
	$(COPYREC) dependencies$Sinstall$Sinclude$Sglog "$(DESTDIR)$(prefix)$Sinclude$Sglog"
endif
ifeq ($(WINDOWS_PROTOBUF_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sgoogle"
	$(COPYREC) dependencies$Sinstall$Sinclude$Sgoogle "$(DESTDIR)$(prefix)$Sinclude$Sgoogle" /E
endif
ifeq ($(WINDOWS_CBC_DIR),$(OR_ROOT)dependencies/install)
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Scoin"
	$(COPYREC) dependencies$Sinstall$Sinclude$Scoin "$(DESTDIR)$(prefix)$Sinclude$Scoin"
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
