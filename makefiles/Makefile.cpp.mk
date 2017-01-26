# Makefile targets.

.PHONY: ccc rcc clean_cc clean_compat ccexe

# Main target
cc: ortoolslibs ccexe

BUILT_LANGUAGES += C++

# Clean target

clean_cc:
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)*.a
	-$(DEL) $(OBJ_DIR)$S*.$O
	-$(DEL) $(OBJ_DIR)$Salgorithms$S*.$O
	-$(DEL) $(OBJ_DIR)$Sbase$S*.$O
	-$(DEL) $(OBJ_DIR)$Sflatzinc$S*.$O
	-$(DEL) $(OBJ_DIR)$Sbop$S*.$O
	-$(DEL) $(OBJ_DIR)$Sglop$S*.$O
	-$(DEL) $(OBJ_DIR)$Slp_data$S*.$O
	-$(DEL) $(OBJ_DIR)$Sgraph$S*.$O
	-$(DEL) $(OBJ_DIR)$Ssat$S*.$O
	-$(DEL) $(OBJ_DIR)$Sconstraint_solver$S*.$O
	-$(DEL) $(OBJ_DIR)$Slinear_solver$S*.$O
	-$(DEL) $(OBJ_DIR)$Sutil$S*.$O
	-$(DEL) $(BIN_DIR)$Sfz$E
	-$(DEL) $(BIN_DIR)$Ssat_runner$E
	-$(DEL) $(CP_BINARIES)
	-$(DEL) $(LP_BINARIES)
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*.pb.*
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*.pb.*
	-$(DEL) $(GEN_DIR)$Sgraph$S*.pb.*
	-$(DEL) $(GEN_DIR)$Sbop$S*.pb.*
	-$(DEL) $(GEN_DIR)$Sflatzinc$Sparser.*
	-$(DEL) $(GEN_DIR)$Sglop$S*.pb.*
	-$(DEL) $(GEN_DIR)$Sflatzinc$S*.tab.*
	-$(DEL) $(GEN_DIR)$Sflatzinc$S*.yy.*
	-$(DEL) $(GEN_DIR)$Ssat$S*.pb.*
	-$(DEL) $(BIN_DIR)$S*.exp
	-$(DEL) $(BIN_DIR)$S*.lib
	-$(DELREC) $(OR_ROOT)src$Sgen$Sflatzinc$S*
	-$(DELREC) $(OR_ROOT)objs$Sflatzinc$S*

clean_compat:
	-$(DELREC) $(OR_ROOT)constraint_solver
	-$(DELREC) $(OR_ROOT)linear_solver
	-$(DELREC) $(OR_ROOT)algorithms
	-$(DELREC) $(OR_ROOT)graph
	-$(DELREC) $(OR_ROOT)gen

# All libraries and dependecies

include $(OR_ROOT)makefiles/Makefile.gen.mk

OR_TOOLS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
OR_TOOLS_LNK = $(PRE_LIB)ortools$(POST_LIB)
ortoolslibs: $(OR_TOOLS_LIBS)

# Specific libraries for examples, and flatzinc.

CVRPTW_LIBS   = $(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX)
CVRPTW_DEPS = \
	$(EX_DIR)/cpp/cvrptw_lib.h \
	$(ROUTING_DEPS)
CVRPTW_LNK = $(PRE_LIB)cvrptw_lib$(POST_LIB) $(OR_TOOLS_LNK)
cvrptwlibs: $(CVRPTW_LIBS)


DIMACS_LIBS   = $(LIB_DIR)/$(LIB_PREFIX)dimacs.$(LIB_SUFFIX)
DIMACS_DEPS = \
	$(EX_DIR)/cpp/parse_dimacs_assignment.h \
	$(EX_DIR)/cpp/print_dimacs_assignment.h \
	$(GRAPH_DEPS)
DIMACS_LNK = $(PRE_LIB)dimacs$(POST_LIB) $(OR_TOOLS_LNK)
dimacslibs: $(DIMACS_LIBS)

FAP_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fap.$(LIB_SUFFIX)
FAP_DEPS = \
	$(EX_DIR)/cpp/fap_model_printer.h \
	$(EX_DIR)/cpp/fap_parser.h \
	$(EX_DIR)/cpp/fap_utilities.h \
	$(CP_DEPS) \
	$(LP_DEPS)
FAP_LNK = $(PRE_LIB)fap$(POST_LIB) $(OR_TOOLS_LNK)
faplibs: $(FAP_LIBS)


FLATZINC_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fz.$(LIB_SUFFIX)
FLATZINC_DEPS = \
	$(SRC_DIR)/flatzinc/checker.h \
	$(SRC_DIR)/flatzinc/constraints.h \
	$(SRC_DIR)/flatzinc/flatzinc_constraints.h \
	$(SRC_DIR)/flatzinc/logging.h \
	$(SRC_DIR)/flatzinc/model.h \
	$(SRC_DIR)/flatzinc/parser.h \
	$(GEN_DIR)/flatzinc/parser.tab.hh \
	$(SRC_DIR)/flatzinc/presolve.h \
	$(SRC_DIR)/flatzinc/reporting.h \
	$(SRC_DIR)/flatzinc/sat_constraint.h \
	$(SRC_DIR)/flatzinc/sat_fz_solver.h \
	$(SRC_DIR)/flatzinc/solver_data.h \
	$(SRC_DIR)/flatzinc/solver.h \
	$(SRC_DIR)/flatzinc/solver_util.h \
	$(CP_DEPS) \
	$(SAT_DEPS)
FLATZINC_LNK = $(PRE_LIB)fz$(POST_LIB) $(OR_TOOLS_LNK)

# Binaries

CC_BINARIES = \
	$(BIN_DIR)/costas_array$E \
	$(BIN_DIR)/cryptarithm$E \
	$(BIN_DIR)/cvrp_disjoint_tw$E \
	$(BIN_DIR)/cvrptw$E \
	$(BIN_DIR)/cvrptw_with_breaks$E \
	$(BIN_DIR)/cvrptw_with_refueling$E \
	$(BIN_DIR)/cvrptw_with_resources$E \
	$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E \
	$(BIN_DIR)/dimacs_assignment$E \
	$(BIN_DIR)/dobble_ls$E \
	$(BIN_DIR)/flow_api$E \
	$(BIN_DIR)/frequency_assignment_problem$E \
	$(BIN_DIR)/golomb$E \
	$(BIN_DIR)/integer_programming$E \
	$(BIN_DIR)/jobshop$E \
	$(BIN_DIR)/jobshop_earlytardy$E \
	$(BIN_DIR)/jobshop_ls$E \
	$(BIN_DIR)/jobshop_sat$E \
	$(BIN_DIR)/linear_assignment_api$E \
	$(BIN_DIR)/linear_programming$E \
	$(BIN_DIR)/linear_solver_protocol_buffers$E \
	$(BIN_DIR)/ls_api$E \
	$(BIN_DIR)/magic_square$E \
	$(BIN_DIR)/model_util$E \
	$(BIN_DIR)/mps_driver$E \
	$(BIN_DIR)/multidim_knapsack$E \
	$(BIN_DIR)/network_routing$E \
	$(BIN_DIR)/nqueens$E \
	$(BIN_DIR)/pdptw$E \
	$(BIN_DIR)/rcpsp_sat$E \
	$(BIN_DIR)/shift_minimization_sat$E \
	$(BIN_DIR)/solve$E \
	$(BIN_DIR)/sports_scheduling$E \
	$(BIN_DIR)/strawberry_fields_with_column_generation$E \
	$(BIN_DIR)/tsp$E \
	$(BIN_DIR)/weighted_tardiness_sat$E

ccexe: $(CC_BINARIES)

# CVRPTW common library

CVRPTW_OBJS=\
	$(OBJ_DIR)/cvrptw_lib.$O

$(OBJ_DIR)/cvrptw_lib.$O: $(EX_DIR)/cpp/cvrptw_lib.cc $(EX_DIR)/cpp/cvrptw_lib.h $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_lib.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_lib.$O

$(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX): $(CVRPTW_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX) $(CVRPTW_OBJS)

# DIMACS challenge problem format library

DIMACS_OBJS=\
	$(OBJ_DIR)/parse_dimacs_assignment.$O

$(OBJ_DIR)/parse_dimacs_assignment.$O: $(EX_DIR)/cpp/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/parse_dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sparse_dimacs_assignment.$O

$(LIB_DIR)/$(LIB_PREFIX)dimacs.$(LIB_SUFFIX): $(DIMACS_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX) $(DIMACS_OBJS)

# FAP challenge problem format library

FAP_OBJS=\
	$(OBJ_DIR)/fap_model_printer.$O \
	$(OBJ_DIR)/fap_parser.$O \
	$(OBJ_DIR)/fap_utilities.$O

$(OBJ_DIR)/fap_model_printer.$O: $(EX_DIR)/cpp/fap_model_printer.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_model_printer.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_model_printer.$O
$(OBJ_DIR)/fap_parser.$O: $(EX_DIR)/cpp/fap_parser.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_parser.$O
$(OBJ_DIR)/fap_utilities.$O: $(EX_DIR)/cpp/fap_utilities.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_utilities.$O

$(LIB_DIR)/$(LIB_PREFIX)fap.$(LIB_SUFFIX): $(FAP_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)fap.$(LIB_SUFFIX) $(FAP_OBJS)

# Flatzinc code

FLATZINC_OBJS=\
	$(OBJ_DIR)/flatzinc/checker.$O \
	$(OBJ_DIR)/flatzinc/constraints.$O \
	$(OBJ_DIR)/flatzinc/flatzinc_constraints.$O \
	$(OBJ_DIR)/flatzinc/logging.$O \
	$(OBJ_DIR)/flatzinc/model.$O \
	$(OBJ_DIR)/flatzinc/parser.$O \
	$(OBJ_DIR)/flatzinc/parser.tab.$O \
	$(OBJ_DIR)/flatzinc/parser.yy.$O \
	$(OBJ_DIR)/flatzinc/presolve.$O \
	$(OBJ_DIR)/flatzinc/reporting.$O \
	$(OBJ_DIR)/flatzinc/sat_constraint.$O \
	$(OBJ_DIR)/flatzinc/sat_fz_solver.$O \
	$(OBJ_DIR)/flatzinc/solver.$O \
	$(OBJ_DIR)/flatzinc/solver_data.$O \
	$(OBJ_DIR)/flatzinc/solver_util.$O


$(GEN_DIR)/flatzinc/parser.yy.cc: $(SRC_DIR)/flatzinc/parser.lex $(FLEX)
	$(FLEX) -o$(GEN_DIR)/flatzinc/parser.yy.cc $(SRC_DIR)/flatzinc/parser.lex

$(GEN_DIR)/flatzinc/parser.tab.cc: $(SRC_DIR)/flatzinc/parser.yy $(BISON)
	$(BISON) -t -o $(GEN_DIR)/flatzinc/parser.tab.cc -d $<

$(GEN_DIR)/flatzinc/parser.tab.hh: $(GEN_DIR)/flatzinc/parser.tab.cc

$(OBJ_DIR)/flatzinc/checker.$O: $(SRC_DIR)/flatzinc/checker.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Schecker.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Schecker.$O

$(OBJ_DIR)/flatzinc/constraints.$O: $(SRC_DIR)/flatzinc/constraints.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sconstraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sconstraints.$O

$(OBJ_DIR)/flatzinc/flatzinc_constraints.$O: $(SRC_DIR)/flatzinc/flatzinc_constraints.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sflatzinc_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sflatzinc_constraints.$O

$(OBJ_DIR)/flatzinc/logging.$O: $(SRC_DIR)/flatzinc/logging.cc $(SRC_DIR)/flatzinc/logging.h $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Slogging.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Slogging.$O

$(OBJ_DIR)/flatzinc/model.$O: $(SRC_DIR)/flatzinc/model.cc $(SRC_DIR)/flatzinc/model.h $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Smodel.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Smodel.$O

$(OBJ_DIR)/flatzinc/parser.$O: $(SRC_DIR)/flatzinc/parser.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparser.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.$O

$(OBJ_DIR)/flatzinc/parser.tab.$O: $(GEN_DIR)/flatzinc/parser.tab.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sflatzinc$Sparser.tab.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.tab.$O

$(OBJ_DIR)/flatzinc/parser.yy.$O: $(GEN_DIR)/flatzinc/parser.yy.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sflatzinc$Sparser.yy.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.yy.$O

$(OBJ_DIR)/flatzinc/presolve.$O: $(SRC_DIR)/flatzinc/presolve.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Spresolve.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Spresolve.$O

$(OBJ_DIR)/flatzinc/reporting.$O: $(SRC_DIR)/flatzinc/reporting.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sreporting.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sreporting.$O

$(OBJ_DIR)/flatzinc/sat_constraint.$O: $(SRC_DIR)/flatzinc/sat_constraint.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssat_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssat_constraint.$O

$(OBJ_DIR)/flatzinc/sat_fz_solver.$O: $(SRC_DIR)/flatzinc/sat_fz_solver.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssat_fz_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssat_fz_solver.$O

$(OBJ_DIR)/flatzinc/solver.$O: $(SRC_DIR)/flatzinc/solver.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssolver.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver.$O

$(OBJ_DIR)/flatzinc/solver_data.$O: $(SRC_DIR)/flatzinc/solver_data.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssolver_data.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver_data.$O

$(OBJ_DIR)/flatzinc/solver_util.$O: $(SRC_DIR)/flatzinc/solver_util.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssolver_util.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver_util.$O

$(LIB_DIR)/$(LIB_PREFIX)fz.$(LIB_SUFFIX): $(FLATZINC_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX) $(FLATZINC_OBJS)

$(OBJ_DIR)/flatzinc/fz.$O: $(SRC_DIR)/flatzinc/fz.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sfz.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sfz.$O

$(OBJ_DIR)/flatzinc/parser_main.$O: $(SRC_DIR)/flatzinc/parser_main.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparser_main.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser_main.$O

fz : $(BIN_DIR)/fz$E $(BIN_DIR)/parser_main$E

$(BIN_DIR)/fz$E: $(OBJ_DIR)/flatzinc/fz.$O $(FLATZINC_LIBS) $(OR_TOOLS_LIBS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflatzinc$Sfz.$O $(FLATZINC_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sfz$E

$(BIN_DIR)/parser_main$E: $(OBJ_DIR)/flatzinc/parser_main.$O $(FLATZINC_LIBS) $(OR_TOOLS_LIBS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sflatzinc$Sparser_main.$O $(FLATZINC_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sparser_main$E

# Flow and linear assignment cpp

$(OBJ_DIR)/linear_assignment_api.$O: $(EX_DIR)/cpp/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_assignment_api.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment_api.$O

$(BIN_DIR)/linear_assignment_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/linear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_assignment_api.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_assignment_api$E

$(OBJ_DIR)/flow_api.$O: $(EX_DIR)/cpp/flow_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flow_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sflow_api.$O

$(BIN_DIR)/flow_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/flow_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flow_api.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflow_api$E

$(OBJ_DIR)/dimacs_assignment.$O: $(EX_DIR)/cpp/dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sdimacs_assignment.$O

$(BIN_DIR)/dimacs_assignment$E: $(DIMACS_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/dimacs_assignment.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dimacs_assignment.$O $(DIMACS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdimacs_assignment$E

# Pure CP and Routing Examples

$(OBJ_DIR)/acp_challenge.$O: $(EX_DIR)/cpp/acp_challenge.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/acp_challenge.cc $(OBJ_OUT)$(OBJ_DIR)$Sacp_challenge.$O

$(BIN_DIR)/acp_challenge$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/acp_challenge.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/acp_challenge.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sacp_challenge$E

$(OBJ_DIR)/acp_challenge_routing.$O: $(EX_DIR)/cpp/acp_challenge_routing.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/acp_challenge_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Sacp_challenge_routing.$O

$(BIN_DIR)/acp_challenge_routing$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/acp_challenge_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/acp_challenge_routing.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sacp_challenge_routing$E

$(OBJ_DIR)/costas_array.$O: $(EX_DIR)/cpp/costas_array.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/costas_array.cc $(OBJ_OUT)$(OBJ_DIR)$Scostas_array.$O

$(BIN_DIR)/costas_array$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/costas_array.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/costas_array.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scostas_array$E

$(OBJ_DIR)/cryptarithm.$O: $(EX_DIR)/cpp/cryptarithm.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cryptarithm.cc $(OBJ_OUT)$(OBJ_DIR)$Scryptarithm.$O

$(BIN_DIR)/cryptarithm$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/cryptarithm.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cryptarithm.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scryptarithm$E

$(OBJ_DIR)/cvrp_disjoint_tw.$O: $(EX_DIR)/cpp/cvrp_disjoint_tw.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrp_disjoint_tw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrp_disjoint_tw.$O

$(BIN_DIR)/cvrp_disjoint_tw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrp_disjoint_tw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrp_disjoint_tw.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrp_disjoint_tw$E

$(OBJ_DIR)/cvrptw.$O: $(EX_DIR)/cpp/cvrptw.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(BIN_DIR)/cvrptw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw$E

$(OBJ_DIR)/cvrptw_with_breaks.$O: $(EX_DIR)/cpp/cvrptw_with_breaks.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_breaks.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_breaks.$O

$(BIN_DIR)/cvrptw_with_breaks$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_breaks.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_breaks.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_breaks$E

$(OBJ_DIR)/cvrptw_with_refueling.$O: $(EX_DIR)/cpp/cvrptw_with_refueling.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_refueling.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_refueling.$O

$(BIN_DIR)/cvrptw_with_refueling$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_refueling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_refueling.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_refueling$E

$(OBJ_DIR)/cvrptw_with_resources.$O: $(EX_DIR)/cpp/cvrptw_with_resources.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_resources.$O

$(BIN_DIR)/cvrptw_with_resources$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_resources.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_resources.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_resources$E

$(OBJ_DIR)/cvrptw_with_stop_times_and_resources.$O: $(EX_DIR)/cpp/cvrptw_with_stop_times_and_resources.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_with_stop_times_and_resources.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_with_stop_times_and_resources.$O

$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw_with_stop_times_and_resources.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw_with_stop_times_and_resources.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw_with_stop_times_and_resources$E

$(OBJ_DIR)/dobble_ls.$O: $(EX_DIR)/cpp/dobble_ls.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dobble_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sdobble_ls.$O

$(BIN_DIR)/dobble_ls$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/dobble_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dobble_ls.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdobble_ls$E

$(OBJ_DIR)/flexible_jobshop.$O: $(EX_DIR)/cpp/flexible_jobshop.cc $(EX_DIR)/cpp/flexible_jobshop.h $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flexible_jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sflexible_jobshop.$O

$(BIN_DIR)/flexible_jobshop$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/flexible_jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flexible_jobshop.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflexible_jobshop$E

$(OBJ_DIR)/golomb.$O: $(EX_DIR)/cpp/golomb.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/golomb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgolomb.$O

$(BIN_DIR)/golomb$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/golomb.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/golomb.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sgolomb$E

$(OBJ_DIR)/jobshop.$O: $(EX_DIR)/cpp/jobshop.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop.$O

$(BIN_DIR)/jobshop$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop$E

$(OBJ_DIR)/jobshop_ls.$O: $(EX_DIR)/cpp/jobshop_ls.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_ls.$O

$(BIN_DIR)/jobshop_ls$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_ls.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_ls$E

$(OBJ_DIR)/jobshop_earlytardy.$O: $(EX_DIR)/cpp/jobshop_earlytardy.cc $(EX_DIR)/cpp/jobshop_earlytardy.h $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_earlytardy.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_earlytardy.$O

$(BIN_DIR)/jobshop_earlytardy$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop_earlytardy.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_earlytardy.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_earlytardy$E

$(OBJ_DIR)/jobshop_sat.$O: $(EX_DIR)/cpp/jobshop_sat.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_sat.$O

$(BIN_DIR)/jobshop_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/jobshop_sat.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_sat$E

$(OBJ_DIR)/magic_square.$O: $(EX_DIR)/cpp/magic_square.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/magic_square.cc $(OBJ_OUT)$(OBJ_DIR)$Smagic_square.$O

$(BIN_DIR)/magic_square$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/magic_square.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/magic_square.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smagic_square$E

$(OBJ_DIR)/model_util.$O: $(EX_DIR)/cpp/model_util.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/model_util.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_util.$O

$(BIN_DIR)/model_util$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/model_util.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/model_util.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smodel_util$E

$(OBJ_DIR)/multidim_knapsack.$O: $(EX_DIR)/cpp/multidim_knapsack.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/multidim_knapsack.cc $(OBJ_OUT)$(OBJ_DIR)$Smultidim_knapsack.$O

$(BIN_DIR)/multidim_knapsack$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/multidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/multidim_knapsack.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smultidim_knapsack$E

$(OBJ_DIR)/network_routing.$O: $(EX_DIR)/cpp/network_routing.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/network_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Snetwork_routing.$O

$(BIN_DIR)/network_routing$E: $(OR_TOOLS_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/network_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/network_routing.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snetwork_routing$E

$(OBJ_DIR)/nqueens.$O: $(EX_DIR)/cpp/nqueens.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/nqueens.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens.$O

$(BIN_DIR)/nqueens$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/nqueens.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens$E

$(OBJ_DIR)/nqueens2.$O: $(EX_DIR)/cpp/nqueens2.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/nqueens2.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens2.$O

$(BIN_DIR)/nqueens2$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/nqueens2.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens2.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens2$E

$(OBJ_DIR)/pdptw.$O: $(EX_DIR)/cpp/pdptw.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/pdptw.cc $(OBJ_OUT)$(OBJ_DIR)$Spdptw.$O

$(BIN_DIR)/pdptw$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/pdptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/pdptw.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Spdptw$E

$(OBJ_DIR)/rcpsp_sat.$O: $(EX_DIR)/cpp/rcpsp_sat.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/rcpsp_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Srcpsp_sat.$O

$(BIN_DIR)/rcpsp_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/rcpsp_sat.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/rcpsp_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Srcpsp_sat$E

$(OBJ_DIR)/shift_minimization_sat.$O: $(EX_DIR)/cpp/shift_minimization_sat.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/shift_minimization_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Sshift_minimization_sat.$O

$(BIN_DIR)/shift_minimization_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/shift_minimization_sat.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/shift_minimization_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sshift_minimization_sat$E

$(OBJ_DIR)/slitherlink.$O: $(EX_DIR)/cpp/slitherlink.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/slitherlink.cc $(OBJ_OUT)$(OBJ_DIR)$Sslitherlink.$O

$(BIN_DIR)/slitherlink$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/slitherlink.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/slitherlink.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sslitherlink$E

$(OBJ_DIR)/sports_scheduling.$O: $(EX_DIR)/cpp/sports_scheduling.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/sports_scheduling.cc $(OBJ_OUT)$(OBJ_DIR)$Ssports_scheduling.$O

$(BIN_DIR)/sports_scheduling$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/sports_scheduling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/sports_scheduling.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssports_scheduling$E

$(OBJ_DIR)/tsp.$O: $(EX_DIR)/cpp/tsp.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/tsp.cc $(OBJ_OUT)$(OBJ_DIR)$Stsp.$O

$(BIN_DIR)/tsp$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/tsp.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/tsp.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Stsp$E

$(OBJ_DIR)/weighted_tardiness_sat.$O: $(EX_DIR)/cpp/weighted_tardiness_sat.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/weighted_tardiness_sat.cc $(OBJ_OUT)$(OBJ_DIR)$Sweighted_tardiness_sat.$O

$(BIN_DIR)/weighted_tardiness_sat$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/weighted_tardiness_sat.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/weighted_tardiness_sat.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sweighted_tardiness_sat$E

# CP tests.

$(OBJ_DIR)/bug_pack.$O: $(EX_DIR)/tests/bug_pack.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_pack.cc $(OBJ_OUT)$(OBJ_DIR)$Sbug_pack.$O

$(BIN_DIR)/bug_pack$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/bug_pack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_pack.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sbug_pack$E

$(OBJ_DIR)/bug_fz1.$O: $(EX_DIR)/tests/bug_fz1.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_fz1.cc $(OBJ_OUT)$(OBJ_DIR)$Sbug_fz1.$O

$(BIN_DIR)/bug_fz1$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/bug_fz1.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_fz1.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sbug_fz1$E

$(OBJ_DIR)/ac4r_table_test.$O: $(EX_DIR)/tests/ac4r_table_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/ac4r_table_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sac4r_table_test.$O

$(BIN_DIR)/ac4r_table_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/ac4r_table_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ac4r_table_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sac4r_table_test$E

$(OBJ_DIR)/forbidden_intervals_test.$O: $(EX_DIR)/tests/forbidden_intervals_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/forbidden_intervals_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sforbidden_intervals_test.$O

$(BIN_DIR)/forbidden_intervals_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/forbidden_intervals_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/forbidden_intervals_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sforbidden_intervals_test$E

$(OBJ_DIR)/gcc_test.$O: $(EX_DIR)/tests/gcc_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/gcc_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sgcc_test.$O

$(BIN_DIR)/gcc_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/gcc_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/gcc_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sgcc_test$E

$(OBJ_DIR)/min_max_test.$O: $(EX_DIR)/tests/min_max_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/min_max_test.cc $(OBJ_OUT)$(OBJ_DIR)$Smin_max_test.$O

$(BIN_DIR)/min_max_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/min_max_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/min_max_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smin_max_test$E

$(OBJ_DIR)/issue57.$O: $(EX_DIR)/tests/issue57.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/issue57.cc $(OBJ_OUT)$(OBJ_DIR)$Sissue57.$O

$(BIN_DIR)/issue57$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/issue57.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/issue57.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sissue57$E

$(OBJ_DIR)/issue173.$O: $(EX_DIR)/tests/issue173.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/issue173.cc $(OBJ_OUT)$(OBJ_DIR)$Sissue173.$O

$(BIN_DIR)/issue173$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/issue173.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/issue173.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sissue173$E

$(OBJ_DIR)/visitor_test.$O: $(EX_DIR)/tests/visitor_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/visitor_test.cc $(OBJ_OUT)$(OBJ_DIR)$Svisitor_test.$O

$(BIN_DIR)/visitor_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/visitor_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/visitor_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Svisitor_test$E

$(OBJ_DIR)/boolean_test.$O: $(EX_DIR)/tests/boolean_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/boolean_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sboolean_test.$O

$(BIN_DIR)/boolean_test$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/boolean_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/boolean_test.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sboolean_test$E

$(OBJ_DIR)/ls_api.$O: $(EX_DIR)/cpp/ls_api.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/ls_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sls_api.$O

$(BIN_DIR)/ls_api$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/ls_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ls_api.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sls_api$E

$(OBJ_DIR)/cpp11_test.$O: $(EX_DIR)/tests/cpp11_test.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/cpp11_test.cc $(OBJ_OUT)$(OBJ_DIR)$Scpp11_test.$O

$(BIN_DIR)/cpp11_test$E: $(OBJ_DIR)/cpp11_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cpp11_test.$O $(EXE_OUT)$(BIN_DIR)$Scpp11_test$E

# Frequency Assignment Problem

$(OBJ_DIR)/frequency_assignment_problem.$O: $(EX_DIR)/cpp/frequency_assignment_problem.cc $(FAP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/frequency_assignment_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Sfrequency_assignment_problem.$O

$(BIN_DIR)/frequency_assignment_problem$E: $(FAP_LIBS) $(OR_TOOLS_LIBS) $(OBJ_DIR)/frequency_assignment_problem.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/frequency_assignment_problem.$O $(FAP_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sfrequency_assignment_problem$E

# Linear Programming Examples

$(OBJ_DIR)/strawberry_fields_with_column_generation.$O: $(EX_DIR)/cpp/strawberry_fields_with_column_generation.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/strawberry_fields_with_column_generation.cc $(OBJ_OUT)$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O

$(BIN_DIR)/strawberry_fields_with_column_generation$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sstrawberry_fields_with_column_generation$E

$(OBJ_DIR)/linear_programming.$O: $(EX_DIR)/cpp/linear_programming.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_programming.$O

$(BIN_DIR)/linear_programming$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/linear_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_programming.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_programming$E

$(OBJ_DIR)/linear_solver_protocol_buffers.$O: $(EX_DIR)/cpp/linear_solver_protocol_buffers.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_solver_protocol_buffers.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O

$(BIN_DIR)/linear_solver_protocol_buffers$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_solver_protocol_buffers$E

$(OBJ_DIR)/integer_programming.$O: $(EX_DIR)/cpp/integer_programming.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/integer_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Sinteger_programming.$O

$(BIN_DIR)/integer_programming$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/integer_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/integer_programming.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sinteger_programming$E

$(OBJ_DIR)/glop/mps_driver.$O: $(EX_DIR)/cpp/mps_driver.cc $(GEN_DIR)/glop/parameters.pb.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Smps_driver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Smps_driver.$O

$(BIN_DIR)/mps_driver$E: $(OBJ_DIR)/glop/mps_driver.$O $(OR_TOOLS_LIBS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sglop$Smps_driver.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smps_driver$E

$(OBJ_DIR)/glop/solve.$O: $(EX_DIR)/cpp/solve.cc $(GEN_DIR)/glop/parameters.pb.h $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssolve.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Ssolve.$O

$(BIN_DIR)/solve$E: $(OBJ_DIR)/glop/solve.$O $(OR_TOOLS_LIBS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sglop$Ssolve.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssolve$E

# Sat solver

sat: bin/sat_runner$E

$(OBJ_DIR)/sat/sat_runner.$O: $(EX_DIR)/cpp/sat_runner.cc $(EX_DIR)/cpp/opb_reader.h $(EX_DIR)/cpp/sat_cnf_reader.h $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssat_runner.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_runner.$O

$(BIN_DIR)/sat_runner$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/sat/sat_runner.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Ssat$Ssat_runner.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssat_runner$E

# OR Tools unique library.

$(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX): \
    $(BASE_LIB_OBJS) \
    $(UTIL_LIB_OBJS) \
    $(LP_DATA_LIB_OBJS) \
    $(GLOP_LIB_OBJS) \
    $(GRAPH_LIB_OBJS) \
    $(ALGORITHMS_LIB_OBJS) \
    $(SAT_LIB_OBJS) \
    $(BOP_LIB_OBJS) \
    $(LP_LIB_OBJS) \
    $(CP_LIB_OBJS)
	$(LINK_CMD) \
	  $(LDOUT)$(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) \
	  $(BASE_LIB_OBJS) \
	  $(UTIL_LIB_OBJS) \
	  $(LP_DATA_LIB_OBJS) \
	  $(GLOP_LIB_OBJS) \
	  $(GRAPH_LIB_OBJS) \
	  $(ALGORITHMS_LIB_OBJS) \
	  $(SAT_LIB_OBJS) \
	  $(BOP_LIB_OBJS) \
	  $(LP_LIB_OBJS) \
	  $(CP_LIB_OBJS) \
	  $(DEPENDENCIES_LNK) \
	  $(OR_TOOLS_LD_FLAGS)

# compile and run C++ examples

ccc: $(BIN_DIR)$S$(basename $(notdir $(EX)))$E

rcc: $(BIN_DIR)$S$(basename $(notdir $(EX)))$E
	@echo running $(BIN_DIR)$S$(basename $(notdir $(EX)))$E
	$(BIN_DIR)$S$(basename $(notdir $(EX)))$E $(ARGS)

# Debug
printdir:
	@echo LIB_DIR = $(LIB_DIR)
	@echo BIN_DIR = $(BIN_DIR)
	@echo GEN_DIR = $(GEN_DIR)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo SRC_DIR = $(SRC_DIR)
	@echo EX_DIR  = $(EX_DIR)