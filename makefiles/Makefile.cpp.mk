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

# Main target
.PHONY: cc # Build C++ OR-Tools and C++ Examples.
cc: ortoolslibs ccexe
.PHONY: test_cc # Test C++ OR-Tools using various examples.
test_cc: test_cc_examples
.PHONY: test_fz
test_fz: test_fz_examples
BUILT_LANGUAGES += C++

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
	-$(DEL) $(addsuffix $E, $(addprefix $(BIN_DIR)$S, $(CC_EXAMPLES)))
	-$(DEL) $(CP_BINARIES)
	-$(DEL) $(LP_BINARIES)
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

# All libraries and dependecies

include $(OR_ROOT)makefiles/Makefile.gen.mk

OR_TOOLS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)ortools.$L
OR_TOOLS_LNK += $(PRE_LIB)ortools$(POST_LIB)
ortoolslibs: third_party_check $(OR_TOOLS_LIBS)

# Specific libraries for examples, and flatzinc.
CVRPTW_LIBS = $(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$L
CVRPTW_PATH = $(subst /,$S,$(CVRPTW_LIBS))
CVRPTW_DEPS = \
	$(EX_DIR)/cpp/cvrptw_lib.h \
	$(CP_DEPS) $(SRC_DIR)/ortools/constraint_solver/routing.h
CVRPTW_LNK = $(PRE_LIB)cvrptw_lib$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
CVRPTW_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)cvrptw_lib.$L #
endif
cvrptwlibs: $(CVRPTW_LIBS)

DIMACS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)dimacs.$L
DIMACS_PATH = $(subst /,$S,$(DIMACS_LIBS))
DIMACS_DEPS = \
	$(EX_DIR)/cpp/parse_dimacs_assignment.h \
	$(EX_DIR)/cpp/print_dimacs_assignment.h \
	$(GRAPH_DEPS)
DIMACS_LNK = $(PRE_LIB)dimacs$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
DIMACS_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)cvrptw_lib.$L #
endif
dimacslibs: $(DIMACS_LIBS)

FAP_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fap.$L
FAP_PATH = $(subst /,$S,$(FAP_LIBS))
FAP_DEPS = \
	$(EX_DIR)/cpp/fap_model_printer.h \
	$(EX_DIR)/cpp/fap_parser.h \
	$(EX_DIR)/cpp/fap_utilities.h \
	$(CP_DEPS) \
	$(LP_DEPS)
FAP_LNK = $(PRE_LIB)fap$(POST_LIB) $(OR_TOOLS_LNK)
ifeq ($(PLATFORM),MACOSX)
FAP_LDFLAGS = -install_name @rpath/$(LIB_PREFIX)cvrptw_lib.$L #
endif
faplibs: $(FAP_LIBS)

FLATZINC_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fz.$L
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

# Binaries
CC_EXAMPLES = \
costas_array \
cryptarithm \
cvrp_disjoint_tw \
cvrptw \
cvrptw_with_breaks \
cvrptw_with_refueling \
cvrptw_with_resources \
cvrptw_with_stop_times_and_resources \
dimacs_assignment \
dobble_ls \
flexible_jobshop \
flow_api \
frequency_assignment_problem \
golomb \
integer_programming \
jobshop \
jobshop_earlytardy \
jobshop_ls \
jobshop_sat \
linear_assignment_api \
linear_programming \
linear_solver_protocol_buffers \
ls_api \
magic_square \
model_util \
mps_driver \
multidim_knapsack \
network_routing \
nqueens \
pdptw \
rcpsp_sat \
shift_minimization_sat \
solve \
sports_scheduling \
strawberry_fields_with_column_generation \
tsp \
weighted_tardiness_sat

.PHONY: ccexe
ccexe: $(addsuffix $E, $(addprefix $(BIN_DIR)/, $(CC_EXAMPLES)))

# CVRPTW common library
CVRPTW_OBJS= $(OBJ_DIR)/cvrptw_lib.$O
$(CVRPTW_OBJS): \
 $(EX_DIR)/cpp/cvrptw_lib.cc \
 $(EX_DIR)/cpp/cvrptw_lib.h \
 $(CP_DEPS) \
 $(SRC_DIR)/ortools/constraint_solver/routing.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_lib.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_lib.$O

$(CVRPTW_LIBS): $(OR_TOOLS_LIBS) $(CVRPTW_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(CVRPTW_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$L \
 $(CVRPTW_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# DIMACS challenge problem format library
DIMACS_OBJS= $(OBJ_DIR)/parse_dimacs_assignment.$O
$(DIMACS_OBJS): $(EX_DIR)/cpp/parse_dimacs_assignment.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/parse_dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sparse_dimacs_assignment.$O

$(DIMACS_LIBS): $(OR_TOOLS_LIBS) $(DIMACS_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(DIMACS_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)dimacs.$L \
 $(DIMACS_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# FAP challenge problem format library
FAP_OBJS=\
	$(OBJ_DIR)/fap_model_printer.$O \
	$(OBJ_DIR)/fap_parser.$O \
	$(OBJ_DIR)/fap_utilities.$O

$(OBJ_DIR)/fap_model_printer.$O: $(EX_DIR)/cpp/fap_model_printer.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_model_printer.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_model_printer.$O
$(OBJ_DIR)/fap_parser.$O: $(EX_DIR)/cpp/fap_parser.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_parser.$O
$(OBJ_DIR)/fap_utilities.$O: $(OR_TOOLS_LIBS) $(EX_DIR)/cpp/fap_utilities.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_utilities.$O

$(FAP_LIBS): $(OR_TOOLS_LIBS) $(FAP_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(FAP_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)fap.$L \
 $(FAP_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

# Flatzinc code
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

fz_parser: #$(SRC_DIR)/ortools/flatzinc/parser.lex $(SRC_DIR)/ortools/flatzinc/parser.yy
	flex -o$(SRC_DIR)/ortools/flatzinc/parser.yy.cc $(SRC_DIR)/ortools/flatzinc/parser.lex
	bison -t -o $(SRC_DIR)/ortools/flatzinc/parser.tab.cc -d $(SRC_DIR)/ortools/flatzinc/parser.yy

$(OBJ_DIR)/flatzinc/checker.$O: $(SRC_DIR)/ortools/flatzinc/checker.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Schecker.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Schecker.$O

$(OBJ_DIR)/flatzinc/constraints.$O: $(SRC_DIR)/ortools/flatzinc/constraints.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sconstraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sconstraints.$O

$(OBJ_DIR)/flatzinc/cp_model_fz_solver.$O: $(SRC_DIR)/ortools/flatzinc/cp_model_fz_solver.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Scp_model_fz_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Scp_model_fz_solver.$O

$(OBJ_DIR)/flatzinc/flatzinc_constraints.$O: $(SRC_DIR)/ortools/flatzinc/flatzinc_constraints.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sflatzinc_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sflatzinc_constraints.$O

$(OBJ_DIR)/flatzinc/logging.$O: $(SRC_DIR)/ortools/flatzinc/logging.cc $(SRC_DIR)/ortools/flatzinc/logging.h $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Slogging.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Slogging.$O

$(OBJ_DIR)/flatzinc/model.$O: $(SRC_DIR)/ortools/flatzinc/model.cc $(SRC_DIR)/ortools/flatzinc/model.h $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Smodel.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Smodel.$O

$(OBJ_DIR)/flatzinc/parser.$O: $(SRC_DIR)/ortools/flatzinc/parser.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sparser.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.$O

$(OBJ_DIR)/flatzinc/parser.tab.$O: $(SRC_DIR)/ortools/flatzinc/parser.tab.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sparser.tab.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.tab.$O

$(OBJ_DIR)/flatzinc/parser.yy.$O: $(SRC_DIR)/ortools/flatzinc/parser.yy.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sparser.yy.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.yy.$O

$(OBJ_DIR)/flatzinc/presolve.$O: $(SRC_DIR)/ortools/flatzinc/presolve.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Spresolve.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Spresolve.$O

$(OBJ_DIR)/flatzinc/reporting.$O: $(SRC_DIR)/ortools/flatzinc/reporting.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sreporting.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sreporting.$O

$(OBJ_DIR)/flatzinc/sat_constraint.$O: $(SRC_DIR)/ortools/flatzinc/sat_constraint.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Ssat_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssat_constraint.$O

$(OBJ_DIR)/flatzinc/solver.$O: $(SRC_DIR)/ortools/flatzinc/solver.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Ssolver.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver.$O

$(OBJ_DIR)/flatzinc/solver_data.$O: $(SRC_DIR)/ortools/flatzinc/solver_data.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Ssolver_data.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver_data.$O

$(OBJ_DIR)/flatzinc/solver_util.$O: $(SRC_DIR)/ortools/flatzinc/solver_util.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Ssolver_util.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver_util.$O

$(FLATZINC_LIBS): $(OR_TOOLS_LIBS) $(FLATZINC_OBJS) | $(LIB_DIR)
	$(LINK_CMD) \
 $(FLATZINK_LDFLAGS) \
 $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)fz.$L \
 $(FLATZINC_OBJS) \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

$(OBJ_DIR)/flatzinc/fz.$O: $(SRC_DIR)/ortools/flatzinc/fz.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sfz.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sfz.$O

$(OBJ_DIR)/flatzinc/parser_main.$O: $(SRC_DIR)/ortools/flatzinc/parser_main.cc $(FLATZINC_DEPS) | $(OBJ_DIR)/flatzinc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sortools$Sflatzinc$Sparser_main.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser_main.$O

fz: $(BIN_DIR)/fz$E $(BIN_DIR)/parser_main$E

$(BIN_DIR)/fz$E: $(OBJ_DIR)/flatzinc/fz.$O $(FLATZINC_LIBS) $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflatzinc$Sfz.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sfz$E

$(BIN_DIR)/parser_main$E: $(OBJ_DIR)/flatzinc/parser_main.$O $(FLATZINC_LIBS) $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflatzinc$Sparser_main.$O $(FLATZINC_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sparser_main$E

# Flow and linear assignment cpp

$(OBJ_DIR)/linear_assignment_api.$O: $(EX_DIR)/cpp/linear_assignment_api.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_assignment_api.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment_api.$O

$(BIN_DIR)/linear_assignment_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/linear_assignment_api.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_assignment_api.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_assignment_api$E

$(OBJ_DIR)/flow_api.$O: $(OR_TOOLS_LIBS) $(EX_DIR)/cpp/flow_api.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flow_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sflow_api.$O

$(BIN_DIR)/flow_api$E: $(OBJ_DIR)/flow_api.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flow_api.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sflow_api$E

$(OBJ_DIR)/dimacs_assignment.$O: $(EX_DIR)/cpp/dimacs_assignment.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sdimacs_assignment.$O

$(BIN_DIR)/dimacs_assignment$E: $(DIMACS_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/dimacs_assignment.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dimacs_assignment.$O $(DIMACS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sdimacs_assignment$E

# Pure CP and Routing Examples

$(OBJ_DIR)/acp_challenge.$O: $(EX_DIR)/cpp/acp_challenge.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/acp_challenge.cc $(OBJ_OUT)$(OBJ_DIR)$Sacp_challenge.$O

$(BIN_DIR)/acp_challenge$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/acp_challenge.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/acp_challenge.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sacp_challenge$E

$(OBJ_DIR)/acp_challenge_routing.$O: $(EX_DIR)/cpp/acp_challenge_routing.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/acp_challenge_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Sacp_challenge_routing.$O

$(BIN_DIR)/acp_challenge_routing$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/acp_challenge_routing.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/acp_challenge_routing.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sacp_challenge_routing$E

$(OBJ_DIR)/costas_array.$O: $(EX_DIR)/cpp/costas_array.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/costas_array.cc $(OBJ_OUT)$(OBJ_DIR)$Scostas_array.$O

$(BIN_DIR)/costas_array$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/costas_array.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/costas_array.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scostas_array$E

$(OBJ_DIR)/code_samples_sat.$O: $(EX_DIR)/cpp/code_samples_sat.cc $(SAT_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/code_samples_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Scode_samples_sat.$O

$(BIN_DIR)/code_samples_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/code_samples_sat.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/code_samples_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scode_samples_sat$E

$(OBJ_DIR)/cryptarithm.$O: $(EX_DIR)/cpp/cryptarithm.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cryptarithm.cc $(OBJ_OUT)$(OBJ_DIR)$Scryptarithm.$O

$(BIN_DIR)/cryptarithm$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/cryptarithm.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cryptarithm.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scryptarithm$E

$(OBJ_DIR)/cvrp_disjoint_tw.$O: $(EX_DIR)/cpp/cvrp_disjoint_tw.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrp_disjoint_tw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrp_disjoint_tw.$O

$(BIN_DIR)/cvrp_disjoint_tw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrp_disjoint_tw.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrp_disjoint_tw.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrp_disjoint_tw$E

$(OBJ_DIR)/cvrptw.$O: $(EX_DIR)/cpp/cvrptw.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(BIN_DIR)/cvrptw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw$E

$(OBJ_DIR)/cvrptw_with_breaks.$O: $(EX_DIR)/cpp/cvrptw_with_breaks.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_breaks.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_breaks.$O

$(BIN_DIR)/cvrptw_with_breaks$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_breaks.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_breaks.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_breaks$E

$(OBJ_DIR)/cvrptw_with_refueling.$O: $(EX_DIR)/cpp/cvrptw_with_refueling.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_refueling.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_refueling.$O

$(BIN_DIR)/cvrptw_with_refueling$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_refueling.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_refueling.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_refueling$E

$(OBJ_DIR)/cvrptw_with_resources.$O: $(EX_DIR)/cpp/cvrptw_with_resources.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_resources.$O

$(BIN_DIR)/cvrptw_with_resources$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_resources.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_resources.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_resources$E

$(OBJ_DIR)/cvrptw_with_stop_times_and_resources.$O: $(EX_DIR)/cpp/cvrptw_with_stop_times_and_resources.cc $(CVRPTW_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_stop_times_and_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O

$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_stop_times_and_resources.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_stop_times_and_resources.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_stop_times_and_resources$E

$(OBJ_DIR)/dobble_ls.$O: $(EX_DIR)/cpp/dobble_ls.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dobble_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sdobble_ls.$O

$(BIN_DIR)/dobble_ls$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/dobble_ls.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dobble_ls.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sdobble_ls$E

$(OBJ_DIR)/flexible_jobshop.$O: $(EX_DIR)/cpp/flexible_jobshop.cc $(EX_DIR)/cpp/flexible_jobshop.h $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flexible_jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sflexible_jobshop.$O

$(BIN_DIR)/flexible_jobshop$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/flexible_jobshop.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flexible_jobshop.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sflexible_jobshop$E

$(OBJ_DIR)/golomb.$O: $(EX_DIR)/cpp/golomb.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/golomb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgolomb.$O

$(BIN_DIR)/golomb$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/golomb.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/golomb.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sgolomb$E

$(OBJ_DIR)/jobshop.$O: $(EX_DIR)/cpp/jobshop.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop.$O

$(BIN_DIR)/jobshop$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop$E

$(OBJ_DIR)/jobshop_ls.$O: $(EX_DIR)/cpp/jobshop_ls.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_ls.$O

$(BIN_DIR)/jobshop_ls$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop_ls.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_ls.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_ls$E

$(OBJ_DIR)/jobshop_earlytardy.$O: $(EX_DIR)/cpp/jobshop_earlytardy.cc $(EX_DIR)/cpp/jobshop_earlytardy.h $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_earlytardy.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_earlytardy.$O

$(BIN_DIR)/jobshop_earlytardy$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop_earlytardy.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_earlytardy.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_earlytardy$E

$(OBJ_DIR)/jobshop_sat.$O: $(EX_DIR)/cpp/jobshop_sat.cc $(SAT_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_sat.$O

$(BIN_DIR)/jobshop_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop_sat.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_sat$E

$(OBJ_DIR)/magic_square.$O: $(EX_DIR)/cpp/magic_square.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/magic_square.cc $(OBJ_OUT)$(OBJ_DIR)$Smagic_square.$O

$(BIN_DIR)/magic_square$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/magic_square.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/magic_square.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Smagic_square$E

$(OBJ_DIR)/model_util.$O: $(EX_DIR)/cpp/model_util.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/model_util.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_util.$O

$(BIN_DIR)/model_util$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/model_util.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/model_util.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Smodel_util$E

$(OBJ_DIR)/multidim_knapsack.$O: $(EX_DIR)/cpp/multidim_knapsack.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/multidim_knapsack.cc $(OBJ_OUT)$(OBJ_DIR)$Smultidim_knapsack.$O

$(BIN_DIR)/multidim_knapsack$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/multidim_knapsack.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/multidim_knapsack.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Smultidim_knapsack$E

$(OBJ_DIR)/network_routing.$O: $(EX_DIR)/cpp/network_routing.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/network_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Snetwork_routing.$O

$(BIN_DIR)/network_routing$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/network_routing.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/network_routing.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Snetwork_routing$E

$(OBJ_DIR)/nqueens.$O: $(EX_DIR)/cpp/nqueens.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/nqueens.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens.$O

$(BIN_DIR)/nqueens$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/nqueens.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens$E

$(OBJ_DIR)/nqueens2.$O: $(EX_DIR)/cpp/nqueens2.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/nqueens2.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens2.$O

$(BIN_DIR)/nqueens2$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/nqueens2.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens2.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens2$E

$(OBJ_DIR)/pdptw.$O: $(EX_DIR)/cpp/pdptw.cc $(CP_DEPS) $(SRC_DIR)/ortools/constraint_solver/routing.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/pdptw.cc $(OBJ_OUT)$(OBJ_DIR)$Spdptw.$O

$(BIN_DIR)/pdptw$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/pdptw.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/pdptw.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Spdptw$E

$(OBJ_DIR)/rcpsp_sat.$O: $(EX_DIR)/cpp/rcpsp_sat.cc $(SAT_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/rcpsp_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Srcpsp_sat.$O

$(BIN_DIR)/rcpsp_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/rcpsp_sat.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/rcpsp_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Srcpsp_sat$E

$(OBJ_DIR)/shift_minimization_sat.$O: $(EX_DIR)/cpp/shift_minimization_sat.cc $(SAT_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/shift_minimization_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Sshift_minimization_sat.$O

$(BIN_DIR)/shift_minimization_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/shift_minimization_sat.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/shift_minimization_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sshift_minimization_sat$E

$(OBJ_DIR)/slitherlink.$O: $(EX_DIR)/cpp/slitherlink.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/slitherlink.cc $(OBJ_OUT)$(OBJ_DIR)$Sslitherlink.$O

$(BIN_DIR)/slitherlink$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/slitherlink.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/slitherlink.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sslitherlink$E

$(OBJ_DIR)/sports_scheduling.$O: $(EX_DIR)/cpp/sports_scheduling.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/sports_scheduling.cc $(OBJ_OUT)$(OBJ_DIR)$Ssports_scheduling.$O

$(BIN_DIR)/sports_scheduling$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/sports_scheduling.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/sports_scheduling.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Ssports_scheduling$E

$(OBJ_DIR)/tsp.$O: $(EX_DIR)/cpp/tsp.cc $(CP_DEPS) $(SRC_DIR)/ortools/constraint_solver/routing.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/tsp.cc $(OBJ_OUT)$(OBJ_DIR)$Stsp.$O

$(BIN_DIR)/tsp$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/tsp.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/tsp.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Stsp$E

$(OBJ_DIR)/weighted_tardiness_sat.$O: $(EX_DIR)/cpp/weighted_tardiness_sat.cc $(SAT_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/weighted_tardiness_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Sweighted_tardiness_sat.$O

$(BIN_DIR)/weighted_tardiness_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/weighted_tardiness_sat.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/weighted_tardiness_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sweighted_tardiness_sat$E

# CP tests.

$(OBJ_DIR)/bug_pack.$O: $(EX_DIR)/tests/bug_pack.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_pack.cc $(OBJ_OUT)$(OBJ_DIR)$Sbug_pack.$O

$(BIN_DIR)/bug_pack$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/bug_pack.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_pack.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sbug_pack$E

$(OBJ_DIR)/bug_fz1.$O: $(EX_DIR)/tests/bug_fz1.cc $(CP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_fz1.cc $(OBJ_OUT)$(OBJ_DIR)$Sbug_fz1.$O

$(BIN_DIR)/bug_fz1$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/bug_fz1.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_fz1.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sbug_fz1$E

$(OBJ_DIR)/ac4r_table_test.$O: $(EX_DIR)/tests/ac4r_table_test.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/ac4r_table_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sac4r_table_test.$O

$(BIN_DIR)/ac4r_table_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/ac4r_table_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ac4r_table_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sac4r_table_test$E

$(OBJ_DIR)/forbidden_intervals_test.$O: $(EX_DIR)/tests/forbidden_intervals_test.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/forbidden_intervals_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sforbidden_intervals_test.$O

$(BIN_DIR)/forbidden_intervals_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/forbidden_intervals_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/forbidden_intervals_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sforbidden_intervals_test$E

$(OBJ_DIR)/gcc_test.$O: $(EX_DIR)/tests/gcc_test.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/gcc_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sgcc_test.$O

$(BIN_DIR)/gcc_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/gcc_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/gcc_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sgcc_test$E

$(OBJ_DIR)/min_max_test.$O: $(EX_DIR)/tests/min_max_test.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/min_max_test.cc $(OBJ_OUT)$(OBJ_DIR)$Smin_max_test.$O

$(BIN_DIR)/min_max_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/min_max_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/min_max_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Smin_max_test$E

$(OBJ_DIR)/issue57.$O: $(EX_DIR)/tests/issue57.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/issue57.cc $(OBJ_OUT)$(OBJ_DIR)$Sissue57.$O

$(BIN_DIR)/issue57$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/issue57.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/issue57.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sissue57$E

$(OBJ_DIR)/issue173.$O: $(EX_DIR)/tests/issue173.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/issue173.cc $(OBJ_OUT)$(OBJ_DIR)$Sissue173.$O

$(BIN_DIR)/issue173$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/issue173.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/issue173.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sissue173$E

$(OBJ_DIR)/visitor_test.$O: $(EX_DIR)/tests/visitor_test.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/visitor_test.cc $(OBJ_OUT)$(OBJ_DIR)$Svisitor_test.$O

$(BIN_DIR)/visitor_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/visitor_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/visitor_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Svisitor_test$E

$(OBJ_DIR)/boolean_test.$O: $(EX_DIR)/tests/boolean_test.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/boolean_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sboolean_test.$O

$(BIN_DIR)/boolean_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/boolean_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/boolean_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sboolean_test$E

$(OBJ_DIR)/ls_api.$O: $(EX_DIR)/cpp/ls_api.cc $(SRC_DIR)/ortools/constraint_solver/constraint_solver.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/ls_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sls_api.$O

$(BIN_DIR)/ls_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/ls_api.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ls_api.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sls_api$E

$(OBJ_DIR)/cpp11_test.$O: $(EX_DIR)/tests/cpp11_test.cc | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/cpp11_test.cc $(OBJ_OUT)$(OBJ_DIR)$Scpp11_test.$O

$(BIN_DIR)/cpp11_test$E: $(OBJ_DIR)/cpp11_test.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cpp11_test.$O $(EXE_OUT)$(BIN_DIR)$Scpp11_test$E

# Frequency Assignment Problem

$(OBJ_DIR)/frequency_assignment_problem.$O: $(EX_DIR)/cpp/frequency_assignment_problem.cc $(FAP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/frequency_assignment_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Sfrequency_assignment_problem.$O

$(BIN_DIR)/frequency_assignment_problem$E: $(FAP_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/frequency_assignment_problem.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/frequency_assignment_problem.$O $(FAP_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sfrequency_assignment_problem$E

# Linear Programming Examples

$(OBJ_DIR)/strawberry_fields_with_column_generation.$O: $(EX_DIR)/cpp/strawberry_fields_with_column_generation.cc $(LP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/strawberry_fields_with_column_generation.cc $(OBJ_OUT)$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O

$(BIN_DIR)/strawberry_fields_with_column_generation$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sstrawberry_fields_with_column_generation$E

$(OBJ_DIR)/linear_programming.$O: $(EX_DIR)/cpp/linear_programming.cc $(LP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_programming.$O

$(BIN_DIR)/linear_programming$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/linear_programming.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_programming.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_programming$E

$(OBJ_DIR)/linear_solver_protocol_buffers.$O: $(EX_DIR)/cpp/linear_solver_protocol_buffers.cc $(LP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_solver_protocol_buffers.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O

$(BIN_DIR)/linear_solver_protocol_buffers$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_solver_protocol_buffers$E

$(OBJ_DIR)/integer_programming.$O: $(EX_DIR)/cpp/integer_programming.cc $(LP_DEPS) | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/integer_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Sinteger_programming.$O

$(BIN_DIR)/integer_programming$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/integer_programming.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/integer_programming.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Sinteger_programming$E

$(OBJ_DIR)/glop/mps_driver.$O: $(EX_DIR)/cpp/mps_driver.cc $(GEN_DIR)/ortools/glop/parameters.pb.h | $(OBJ_DIR)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Smps_driver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Smps_driver.$O

$(BIN_DIR)/mps_driver$E: $(OBJ_DIR)/glop/mps_driver.$O $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sglop$Smps_driver.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Smps_driver$E

$(OBJ_DIR)/glop/solve.$O: \
 $(EX_DIR)/cpp/solve.cc \
 $(GEN_DIR)/ortools/glop/parameters.pb.h \
 $(GEN_DIR)/ortools/linear_solver/linear_solver.pb.h | $(OBJ_DIR)/glop
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssolve.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Ssolve.$O

$(BIN_DIR)/solve$E: $(OBJ_DIR)/glop/solve.$O $(OR_TOOLS_LIBS) | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sglop$Ssolve.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Ssolve$E

# Sat solver

sat: bin/sat_runner$E

$(OBJ_DIR)/sat/sat_runner.$O: $(EX_DIR)/cpp/sat_runner.cc $(EX_DIR)/cpp/opb_reader.h $(EX_DIR)/cpp/sat_cnf_reader.h $(SAT_DEPS) | $(OBJ_DIR)/sat
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssat_runner.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_runner.$O

$(BIN_DIR)/sat_runner$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/sat/sat_runner.$O | $(BIN_DIR)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Ssat$Ssat_runner.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LDFLAGS) $(EXE_OUT)$(BIN_DIR)$Ssat_runner$E

# OR Tools unique library.
$(OR_TOOLS_LIBS): \
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

# compile and run C++ examples
.PHONY: ccc
ccc: $(BIN_DIR)/$(basename $(notdir $(EX)))$E

.PHONY: rcc
rcc: $(BIN_DIR)/$(basename $(notdir $(EX)))$E
	@echo running $(BIN_DIR)$S$(basename $(notdir $(EX)))$E
	$(BIN_DIR)$S$(basename $(notdir $(EX)))$E $(ARGS)

###############
##  INSTALL  ##
###############
# ref: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html#index-prefix
# ref: https://www.gnu.org/prep/standards/html_node/DESTDIR.html
install_dirs:
	-$(MKDIR) "$(DESTDIR)$(prefix)"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Slib"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sbin"
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
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"

.PHONY: install_cc # Install C++ OR-Tools to $(DESTDIR)$(prefix)
install_cc: install_libortools install_third_party install_doc

install_libortools: ortoolslibs install_dirs
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
	$(COPY) ortools$Slp_data$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slp_data"
	$(COPY) $(GEN_PATH)$Sortools$Slinear_solver$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Slinear_solver"
	$(COPY) ortools$Ssat$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	$(COPY) $(GEN_PATH)$Sortools$Ssat$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Ssat"
	$(COPY) ortools$Sutil$S*.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"
	$(COPY) $(GEN_PATH)$Sortools$Sutil$S*.pb.h "$(DESTDIR)$(prefix)$Sinclude$Sortools$Sutil"
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$L "$(DESTDIR)$(prefix)$Slib"

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
	$(COPYREC) dependencies$Sinstall$Slib$Slibproto* "$(DESTDIR)$(prefix)$Slib"
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

install_doc:
	-$(DELREC) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools"
	-$(MKDIR_P) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat$Sdoc"
#$(COPY) ortools$Ssat$S*.md "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat"
	$(COPY) ortools$Ssat$Sdoc$S*.md "$(DESTDIR)$(prefix)$Sshare$Sdoc$Sortools$Ssat$Sdoc"

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
	@echo EX_DIR = $(EX_DIR)
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
