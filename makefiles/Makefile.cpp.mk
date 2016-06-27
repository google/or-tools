# List libraries by module.

OR_TOOLS_LIBS = $(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX)

FLATZINC_LIBS = $(LIB_DIR)/$(LIB_PREFIX)fz.$(LIB_SUFFIX)

CVRPTW_LIBS   = $(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX)

DIMACS_LIBS   = $(LIB_DIR)/$(LIB_PREFIX)dimacs.$(LIB_SUFFIX)

FAP_LIBS      = $(LIB_DIR)/$(LIB_PREFIX)fap.$(LIB_SUFFIX)

# Header and protobuf dependencies when compiling .cc files.

CP_DEPS = \
	$(GEN_DIR)/constraint_solver/assignment.pb.h \
	$(GEN_DIR)/constraint_solver/demon_profiler.pb.h \
	$(GEN_DIR)/constraint_solver/model.pb.h \
	$(GEN_DIR)/constraint_solver/routing_enums.pb.h \
	$(GEN_DIR)/constraint_solver/routing_parameters.pb.h \
	$(GEN_DIR)/constraint_solver/search_limit.pb.h \
	$(GEN_DIR)/constraint_solver/solver_parameters.pb.h \
	$(SRC_DIR)/constraint_solver/constraint_solver.h \
	$(SRC_DIR)/constraint_solver/constraint_solveri.h

ROUTING_DEPS = \
	$(CP_DEPS) \
	$(SRC_DIR)/constraint_solver/routing.h

LP_DEPS = \
	$(GEN_DIR)/glop/parameters.pb.h \
	$(GEN_DIR)/linear_solver/linear_solver.pb.h \
	$(SRC_DIR)/linear_solver/linear_solver.h

GRAPH_DEPS = \
	$(GEN_DIR)/graph/flow_problem.pb.h \
	$(SRC_DIR)/graph/assignment.h \
	$(SRC_DIR)/graph/cliques.h \
	$(SRC_DIR)/graph/connectivity.h \
	$(SRC_DIR)/graph/ebert_graph.h \
	$(SRC_DIR)/graph/eulerian_path.h \
	$(SRC_DIR)/graph/graph.h \
	$(SRC_DIR)/graph/graphs.h \
	$(SRC_DIR)/graph/hamiltonian_path.h \
	$(SRC_DIR)/graph/linear_assignment.h \
	$(SRC_DIR)/graph/max_flow.h \
	$(SRC_DIR)/graph/min_cost_flow.h \
	$(SRC_DIR)/graph/minimum_spanning_tree.h \
	$(SRC_DIR)/graph/shortestpaths.h \
	$(SRC_DIR)/graph/util.h

SAT_DEPS = \
	$(GEN_DIR)/sat/boolean_problem.pb.h \
	$(GEN_DIR)/sat/sat_parameters.pb.h \
	$(SRC_DIR)/sat/boolean_problem.h \
	$(SRC_DIR)/sat/clause.h \
	$(SRC_DIR)/sat/drat.h \
	$(SRC_DIR)/sat/encoding.h \
	$(SRC_DIR)/sat/integer.h \
	$(SRC_DIR)/sat/lp_utils.h \
	$(SRC_DIR)/sat/model.h \
	$(SRC_DIR)/sat/optimization.h \
	$(SRC_DIR)/sat/pb_constraint.h \
	$(SRC_DIR)/sat/sat_base.h \
	$(SRC_DIR)/sat/sat_solver.h \
	$(SRC_DIR)/sat/simplification.h \
	$(SRC_DIR)/sat/symmetry.h

BOP_DEPS = \
	$(SAT_DEPS) \
	$(GEN_DIR)/bop/bop_parameters.pb.h \
	$(SRC_DIR)/bop/bop_base.h \
	$(SRC_DIR)/bop/bop_fs.h \
	$(SRC_DIR)/bop/bop_lns.h \
	$(SRC_DIR)/bop/bop_ls.h \
	$(SRC_DIR)/bop/bop_portfolio.h \
	$(SRC_DIR)/bop/bop_solution.h \
	$(SRC_DIR)/bop/bop_solver.h \
	$(SRC_DIR)/bop/bop_types.h \
	$(SRC_DIR)/bop/bop_util.h \
	$(SRC_DIR)/bop/complete_optimizer.h \
	$(SRC_DIR)/bop/integral_solver.h

FLATZINC_DEPS = \
	$(SRC_DIR)/flatzinc/flatzinc_constraints.h \
	$(SRC_DIR)/flatzinc/model.h \
	$(SRC_DIR)/flatzinc/parser.h \
	$(SRC_DIR)/flatzinc/presolve.h \
	$(SRC_DIR)/flatzinc/sat_constraint.h \
	$(SRC_DIR)/flatzinc/search.h \
	$(SRC_DIR)/flatzinc/solver.h \
	$(CP_DEPS) \
	$(SAT_DEPS)

CVRPTW_DEPS = \
	$(EX_DIR)/cpp/cvrptw_lib.h \
	$(ROUTING_DEPS)

DIMACS_DEPS = \
	$(EX_DIR)/cpp/parse_dimacs_assignment.h \
	$(EX_DIR)/cpp/print_dimacs_assignment.h

FAP_DEPS = \
	$(EX_DIR)/cpp/fap_model_printer.h \
	$(EX_DIR)/cpp/fap_parser.h \
	$(EX_DIR)/cpp/fap_utilities.h \
	$(CP_DEPS) \
	$(LP_DEPS)

# Link flags

OR_TOOLS_LNK = \
        $(PRE_LIB)ortools$(POST_LIB)

FLATZINC_LNK = \
        $(PRE_LIB)fz$(POST_LIB)\
        $(OR_TOOLS_LNK)

CVRPTW_LNK = \
        $(PRE_LIB)cvrptw_lib$(POST_LIB) \
        $(OR_TOOLS_LNK)

DIMACS_LNK = \
        $(PRE_LIB)dimacs$(POST_LIB) \
        $(OR_TOOLS_LNK)

FAP_LNK = \
        $(PRE_LIB)fap$(POST_LIB) \
        $(OR_TOOLS_LNK)

# Binaries

CPBINARIES = \
	$(BIN_DIR)/costas_array$E \
	$(BIN_DIR)/cryptarithm$E \
	$(BIN_DIR)/cvrptw$E \
	$(BIN_DIR)/cvrptw_with_refueling$E \
	$(BIN_DIR)/cvrptw_with_resources$E \
	$(BIN_DIR)/cvrptw_with_stop_times_and_resources$E \
	$(BIN_DIR)/dobble_ls$E \
	$(BIN_DIR)/flow_api$E \
	$(BIN_DIR)/golomb$E \
	$(BIN_DIR)/jobshop$E \
	$(BIN_DIR)/jobshop_ls$E \
	$(BIN_DIR)/linear_assignment_api$E \
	$(BIN_DIR)/ls_api$E \
	$(BIN_DIR)/magic_square$E \
	$(BIN_DIR)/model_util$E \
	$(BIN_DIR)/multidim_knapsack$E \
	$(BIN_DIR)/network_routing$E \
	$(BIN_DIR)/nqueens$E \
	$(BIN_DIR)/pdptw$E \
	$(BIN_DIR)/dimacs_assignment$E \
	$(BIN_DIR)/sports_scheduling$E \
	$(BIN_DIR)/tsp$E

LPBINARIES = \
	$(BIN_DIR)/integer_programming$E \
	$(BIN_DIR)/linear_programming$E \
	$(BIN_DIR)/linear_solver_protocol_buffers$E \
	$(BIN_DIR)/strawberry_fields_with_column_generation$E \
	$(BIN_DIR)/mps_driver$E \
	$(BIN_DIR)/solve$E


# Special dimacs example.

# Makefile targets.

# Main target
cc: ortoolslibs cpexe lpexe

# Clean target

clean_cc:
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)base.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)util.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)constraint_solver.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)linear_solver.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)bop.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)glop.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)graph.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)routing.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)algorithms.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)sat.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)shortestpaths.$(LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
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
	-$(DEL) $(CPBINARIES)
	-$(DEL) $(LPBINARIES)
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


# Individual targets.

ortoolslibs: $(OR_TOOLS_LIBS)

cpexe: $(CPBINARIES)

lpexe: $(LPBINARIES)

cvrptwlibs: $(CVRPTW_LIBS)

dimacslibs: $(DIMACS_LIBS)

faplibs: $(FAP_LIBS)

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OBJS = \
	$(OBJ_DIR)/constraint_solver/alldiff_cst.$O\
	$(OBJ_DIR)/constraint_solver/assignment.$O\
	$(OBJ_DIR)/constraint_solver/assignment.pb.$O\
	$(OBJ_DIR)/constraint_solver/ac4_mdd_reset_table.$O\
	$(OBJ_DIR)/constraint_solver/ac4r_table.$O\
	$(OBJ_DIR)/constraint_solver/collect_variables.$O\
	$(OBJ_DIR)/constraint_solver/constraint_solver.$O\
	$(OBJ_DIR)/constraint_solver/constraints.$O\
	$(OBJ_DIR)/constraint_solver/count_cst.$O\
	$(OBJ_DIR)/constraint_solver/default_search.$O\
	$(OBJ_DIR)/constraint_solver/demon_profiler.$O\
	$(OBJ_DIR)/constraint_solver/demon_profiler.pb.$O\
	$(OBJ_DIR)/constraint_solver/deviation.$O\
	$(OBJ_DIR)/constraint_solver/diffn.$O\
	$(OBJ_DIR)/constraint_solver/element.$O\
	$(OBJ_DIR)/constraint_solver/expr_array.$O\
	$(OBJ_DIR)/constraint_solver/expr_cst.$O\
	$(OBJ_DIR)/constraint_solver/expressions.$O\
	$(OBJ_DIR)/constraint_solver/gcc.$O\
	$(OBJ_DIR)/constraint_solver/hybrid.$O\
	$(OBJ_DIR)/constraint_solver/graph_constraints.$O\
	$(OBJ_DIR)/constraint_solver/interval.$O\
	$(OBJ_DIR)/constraint_solver/io.$O\
	$(OBJ_DIR)/constraint_solver/local_search.$O\
	$(OBJ_DIR)/constraint_solver/model.pb.$O\
	$(OBJ_DIR)/constraint_solver/model_cache.$O\
	$(OBJ_DIR)/constraint_solver/nogoods.$O\
	$(OBJ_DIR)/constraint_solver/pack.$O\
	$(OBJ_DIR)/constraint_solver/range_cst.$O\
	$(OBJ_DIR)/constraint_solver/resource.$O\
	$(OBJ_DIR)/constraint_solver/sat_constraint.$O\
	$(OBJ_DIR)/constraint_solver/sched_constraints.$O\
	$(OBJ_DIR)/constraint_solver/sched_expr.$O\
	$(OBJ_DIR)/constraint_solver/sched_search.$O\
	$(OBJ_DIR)/constraint_solver/search.$O\
	$(OBJ_DIR)/constraint_solver/search_limit.pb.$O\
	$(OBJ_DIR)/constraint_solver/solver_parameters.pb.$O\
	$(OBJ_DIR)/constraint_solver/table.$O\
	$(OBJ_DIR)/constraint_solver/timetabling.$O\
	$(OBJ_DIR)/constraint_solver/trace.$O\
	$(OBJ_DIR)/constraint_solver/tree_monitor.$O\
	$(OBJ_DIR)/constraint_solver/utilities.$O \
	$(OBJ_DIR)/constraint_solver/visitor.$O

$(OBJ_DIR)/constraint_solver/alldiff_cst.$O: $(SRC_DIR)/constraint_solver/alldiff_cst.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/alldiff_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Salldiff_cst.$O

$(OBJ_DIR)/constraint_solver/assignment.$O: $(SRC_DIR)/constraint_solver/assignment.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sassignment.$O

$(OBJ_DIR)/constraint_solver/assignment.pb.$O: $(GEN_DIR)/constraint_solver/assignment.pb.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/assignment.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sassignment.pb.$O

$(OBJ_DIR)/constraint_solver/ac4_mdd_reset_table.$O: $(SRC_DIR)/constraint_solver/ac4_mdd_reset_table.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/ac4_mdd_reset_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sac4_mdd_reset_table.$O

$(OBJ_DIR)/constraint_solver/ac4r_table.$O: $(SRC_DIR)/constraint_solver/ac4r_table.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/ac4r_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sac4r_table.$O

$(GEN_DIR)/constraint_solver/assignment.pb.cc: $(SRC_DIR)/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/assignment.proto

$(GEN_DIR)/constraint_solver/assignment.pb.h: $(GEN_DIR)/constraint_solver/assignment.pb.cc

$(OBJ_DIR)/constraint_solver/collect_variables.$O: $(SRC_DIR)/constraint_solver/collect_variables.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/collect_variables.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Scollect_variables.$O

$(GEN_DIR)/constraint_solver/solver_parameters.pb.cc: $(SRC_DIR)/constraint_solver/solver_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/solver_parameters.proto

$(GEN_DIR)/constraint_solver/solver_parameters.pb.h: $(GEN_DIR)/constraint_solver/solver_parameters.pb.cc

$(OBJ_DIR)/constraint_solver/solver_parameters.pb.$O: $(GEN_DIR)/constraint_solver/solver_parameters.pb.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/solver_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssolver_parameters.pb.$O

$(OBJ_DIR)/constraint_solver/constraint_solver.$O: $(SRC_DIR)/constraint_solver/constraint_solver.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraint_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sconstraint_solver.$O

$(OBJ_DIR)/constraint_solver/constraints.$O: $(SRC_DIR)/constraint_solver/constraints.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sconstraints.$O

$(OBJ_DIR)/constraint_solver/count_cst.$O: $(SRC_DIR)/constraint_solver/count_cst.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/count_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Scount_cst.$O

$(OBJ_DIR)/constraint_solver/default_search.$O: $(SRC_DIR)/constraint_solver/default_search.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/default_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdefault_search.$O

$(OBJ_DIR)/constraint_solver/demon_profiler.$O: $(SRC_DIR)/constraint_solver/demon_profiler.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/demon_profiler.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdemon_profiler.$O

$(OBJ_DIR)/constraint_solver/demon_profiler.pb.$O: $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdemon_profiler.pb.$O

$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc: $(SRC_DIR)/constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/demon_profiler.proto

$(GEN_DIR)/constraint_solver/demon_profiler.pb.h: $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc

$(OBJ_DIR)/constraint_solver/deviation.$O: $(SRC_DIR)/constraint_solver/deviation.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/deviation.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdeviation.$O

$(OBJ_DIR)/constraint_solver/diffn.$O: $(SRC_DIR)/constraint_solver/diffn.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/diffn.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sdiffn.$O

$(OBJ_DIR)/constraint_solver/element.$O: $(SRC_DIR)/constraint_solver/element.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/element.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Selement.$O

$(OBJ_DIR)/constraint_solver/expr_array.$O: $(SRC_DIR)/constraint_solver/expr_array.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_array.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpr_array.$O

$(OBJ_DIR)/constraint_solver/expr_cst.$O: $(SRC_DIR)/constraint_solver/expr_cst.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpr_cst.$O

$(OBJ_DIR)/constraint_solver/expressions.$O: $(SRC_DIR)/constraint_solver/expressions.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expressions.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sexpressions.$O

$(OBJ_DIR)/constraint_solver/gcc.$O: $(SRC_DIR)/constraint_solver/gcc.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/gcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sgcc.$O

$(OBJ_DIR)/constraint_solver/graph_constraints.$O: $(SRC_DIR)/constraint_solver/graph_constraints.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/graph_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sgraph_constraints.$O

$(OBJ_DIR)/constraint_solver/hybrid.$O: $(SRC_DIR)/constraint_solver/hybrid.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/hybrid.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Shybrid.$O

$(OBJ_DIR)/constraint_solver/interval.$O: $(SRC_DIR)/constraint_solver/interval.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/interval.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sinterval.$O

$(OBJ_DIR)/constraint_solver/io.$O: $(SRC_DIR)/constraint_solver/io.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/io.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sio.$O

$(OBJ_DIR)/constraint_solver/local_search.$O: $(SRC_DIR)/constraint_solver/local_search.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/local_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Slocal_search.$O

$(OBJ_DIR)/constraint_solver/model.pb.$O: $(GEN_DIR)/constraint_solver/model.pb.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/model.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Smodel.pb.$O

$(OBJ_DIR)/constraint_solver/model_cache.$O: $(SRC_DIR)/constraint_solver/model_cache.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/model_cache.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Smodel_cache.$O

$(GEN_DIR)/constraint_solver/model.pb.cc: $(SRC_DIR)/constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/model.proto

$(GEN_DIR)/constraint_solver/model.pb.h: $(GEN_DIR)/constraint_solver/model.pb.cc

$(OBJ_DIR)/constraint_solver/nogoods.$O: $(SRC_DIR)/constraint_solver/nogoods.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/nogoods.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Snogoods.$O

$(OBJ_DIR)/constraint_solver/pack.$O: $(SRC_DIR)/constraint_solver/pack.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/pack.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Spack.$O

$(OBJ_DIR)/constraint_solver/range_cst.$O: $(SRC_DIR)/constraint_solver/range_cst.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/range_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srange_cst.$O

$(OBJ_DIR)/constraint_solver/resource.$O: $(SRC_DIR)/constraint_solver/resource.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/resource.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sresource.$O

$(OBJ_DIR)/constraint_solver/sat_constraint.$O: $(SRC_DIR)/constraint_solver/sat_constraint.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sat_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssat_constraint.$O

$(OBJ_DIR)/constraint_solver/sched_constraints.$O: $(SRC_DIR)/constraint_solver/sched_constraints.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_constraints.$O

$(OBJ_DIR)/constraint_solver/sched_expr.$O: $(SRC_DIR)/constraint_solver/sched_expr.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_expr.$O

$(OBJ_DIR)/constraint_solver/sched_search.$O: $(SRC_DIR)/constraint_solver/sched_search.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssched_search.$O

$(OBJ_DIR)/constraint_solver/search.$O: $(SRC_DIR)/constraint_solver/search.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssearch.$O

$(OBJ_DIR)/constraint_solver/search_limit.pb.$O: $(GEN_DIR)/constraint_solver/search_limit.pb.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/search_limit.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssearch_limit.pb.$O

$(GEN_DIR)/constraint_solver/search_limit.pb.cc: $(SRC_DIR)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/search_limit.proto

$(GEN_DIR)/constraint_solver/search_limit.pb.h: $(GEN_DIR)/constraint_solver/search_limit.pb.cc

$(OBJ_DIR)/constraint_solver/softgcc.$O: $(SRC_DIR)/constraint_solver/softgcc.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/softgcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Ssoftgcc.$O

$(OBJ_DIR)/constraint_solver/table.$O: $(SRC_DIR)/constraint_solver/table.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/table.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stable.$O

$(OBJ_DIR)/constraint_solver/timetabling.$O: $(SRC_DIR)/constraint_solver/timetabling.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/timetabling.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stimetabling.$O

$(OBJ_DIR)/constraint_solver/trace.$O: $(SRC_DIR)/constraint_solver/trace.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/trace.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Strace.$O

$(OBJ_DIR)/constraint_solver/tree_monitor.$O: $(SRC_DIR)/constraint_solver/tree_monitor.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/tree_monitor.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Stree_monitor.$O

$(OBJ_DIR)/constraint_solver/utilities.$O: $(SRC_DIR)/constraint_solver/utilities.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Sutilities.$O

$(OBJ_DIR)/constraint_solver/visitor.$O: $(SRC_DIR)/constraint_solver/visitor.cc $(CP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/visitor.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Svisitor.$O


# Linear Solver Library

LINEAR_SOLVER_LIB_OBJS = \
	$(OBJ_DIR)/linear_solver/bop_interface.$O \
	$(OBJ_DIR)/linear_solver/glop_interface.$O \
	$(OBJ_DIR)/linear_solver/cbc_interface.$O \
	$(OBJ_DIR)/linear_solver/clp_interface.$O \
	$(OBJ_DIR)/linear_solver/glpk_interface.$O \
	$(OBJ_DIR)/linear_solver/gurobi_interface.$O \
	$(OBJ_DIR)/linear_solver/linear_solver.$O \
	$(OBJ_DIR)/linear_solver/linear_solver.pb.$O \
	$(OBJ_DIR)/linear_solver/model_exporter.$O \
	$(OBJ_DIR)/linear_solver/model_validator.$O \
	$(OBJ_DIR)/linear_solver/scip_interface.$O \
	$(OBJ_DIR)/linear_solver/cplex_interface.$O \
	$(OBJ_DIR)/linear_solver/sulum_interface.$O \

$(OBJ_DIR)/linear_solver/cbc_interface.$O: $(SRC_DIR)/linear_solver/cbc_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/cbc_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Scbc_interface.$O

$(OBJ_DIR)/linear_solver/clp_interface.$O: $(SRC_DIR)/linear_solver/clp_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/clp_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sclp_interface.$O

$(OBJ_DIR)/linear_solver/bop_interface.$O: $(SRC_DIR)/linear_solver/bop_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Sbop_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sbop_interface.$O

$(OBJ_DIR)/linear_solver/cplex_interface.$O: $(SRC_DIR)/linear_solver/cplex_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/cplex_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Scplex_interface.$O

$(OBJ_DIR)/linear_solver/glop_interface.$O: $(SRC_DIR)/linear_solver/glop_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Sglop_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglop_interface.$O

$(OBJ_DIR)/linear_solver/glpk_interface.$O: $(SRC_DIR)/linear_solver/glpk_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Sglpk_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sglpk_interface.$O

$(OBJ_DIR)/linear_solver/gurobi_interface.$O: $(SRC_DIR)/linear_solver/gurobi_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Sgurobi_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sgurobi_interface.$O

$(OBJ_DIR)/linear_solver/linear_solver.$O: $(SRC_DIR)/linear_solver/linear_solver.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Slinear_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_solver.$O

$(OBJ_DIR)/linear_solver/linear_solver.pb.$O: $(GEN_DIR)/linear_solver/linear_solver.pb.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Slinear_solver$Slinear_solver.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Slinear_solver.pb.$O

$(GEN_DIR)/linear_solver/linear_solver.pb.cc: $(SRC_DIR)/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)$Sbin$Sprotoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)$Slinear_solver$Slinear_solver.proto

$(GEN_DIR)/linear_solver/linear_solver.pb.h: $(GEN_DIR)/linear_solver/linear_solver.pb.cc

$(OBJ_DIR)/linear_solver/model_exporter.$O: $(SRC_DIR)/linear_solver/model_exporter.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Smodel_exporter.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Smodel_exporter.$O

$(OBJ_DIR)/linear_solver/model_validator.$O: $(SRC_DIR)/linear_solver/model_validator.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Smodel_validator.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Smodel_validator.$O

$(OBJ_DIR)/linear_solver/scip_interface.$O: $(SRC_DIR)/linear_solver/scip_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Sscip_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Sscip_interface.$O

$(OBJ_DIR)/linear_solver/sulum_interface.$O: $(SRC_DIR)/linear_solver/sulum_interface.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Slinear_solver$Ssulum_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver$Ssulum_interface.$O


# Util library.

UTIL_LIB_OBJS=\
	$(OBJ_DIR)/util/bitset.$O \
	$(OBJ_DIR)/util/cached_log.$O \
	$(OBJ_DIR)/util/fp_utils.$O \
	$(OBJ_DIR)/util/graph_export.$O \
	$(OBJ_DIR)/util/piecewise_linear_function.$O \
	$(OBJ_DIR)/util/proto_tools.$O \
	$(OBJ_DIR)/util/range_query_function.$O \
	$(OBJ_DIR)/util/rational_approximation.$O \
	$(OBJ_DIR)/util/sorted_interval_list.$O \
	$(OBJ_DIR)/util/stats.$O \
	$(OBJ_DIR)/util/time_limit.$O \
	$(OBJ_DIR)/util/xml_helper.$O

$(OBJ_DIR)/util/bitset.$O: $(SRC_DIR)/util/bitset.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/bitset.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sbitset.$O

$(OBJ_DIR)/util/cached_log.$O: $(SRC_DIR)/util/cached_log.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/cached_log.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Scached_log.$O

$(OBJ_DIR)/util/fp_utils.$O: $(SRC_DIR)/util/fp_utils.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/fp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sfp_utils.$O

$(OBJ_DIR)/util/graph_export.$O: $(SRC_DIR)/util/graph_export.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/graph_export.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sgraph_export.$O

$(OBJ_DIR)/util/piecewise_linear_function.$O: $(SRC_DIR)/util/piecewise_linear_function.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/piecewise_linear_function.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Spiecewise_linear_function.$O

$(OBJ_DIR)/util/proto_tools.$O: $(SRC_DIR)/util/proto_tools.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sutil$Sproto_tools.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sproto_tools.$O

$(OBJ_DIR)/util/range_query_function.$O: $(SRC_DIR)/util/range_query_function.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sutil$Srange_query_function.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srange_query_function.$O

$(OBJ_DIR)/util/rational_approximation.$O: $(SRC_DIR)/util/rational_approximation.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/rational_approximation.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Srational_approximation.$O

$(OBJ_DIR)/util/sorted_interval_list.$O: $(SRC_DIR)/util/sorted_interval_list.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/sorted_interval_list.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Ssorted_interval_list.$O

$(OBJ_DIR)/util/stats.$O: $(SRC_DIR)/util/stats.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/stats.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sstats.$O

$(OBJ_DIR)/util/time_limit.$O: $(SRC_DIR)/util/time_limit.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/time_limit.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Stime_limit.$O

$(OBJ_DIR)/util/xml_helper.$O: $(SRC_DIR)/util/xml_helper.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/xml_helper.cc $(OBJ_OUT)$(OBJ_DIR)$Sutil$Sxml_helper.$O


# Graph library.

GRAPH_LIB_OBJS=\
	$(OBJ_DIR)/graph/simple_assignment.$O \
	$(OBJ_DIR)/graph/linear_assignment.$O \
	$(OBJ_DIR)/graph/cliques.$O \
	$(OBJ_DIR)/graph/flow_problem.pb.$O \
	$(OBJ_DIR)/graph/max_flow.$O \
	$(OBJ_DIR)/graph/min_cost_flow.$O

$(OBJ_DIR)/graph/linear_assignment.$O: $(SRC_DIR)/graph/linear_assignment.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/linear_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Slinear_assignment.$O

$(OBJ_DIR)/graph/simple_assignment.$O: $(SRC_DIR)/graph/assignment.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Ssimple_assignment.$O

$(OBJ_DIR)/graph/cliques.$O: $(SRC_DIR)/graph/cliques.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/cliques.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Scliques.$O

$(GEN_DIR)/graph/flow_problem.pb.cc: $(SRC_DIR)/graph/flow_problem.proto
	 $(PROTOBUF_DIR)$Sbin$Sprotoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)$Sgraph$Sflow_problem.proto

$(GEN_DIR)/graph/flow_problem.pb.h: $(GEN_DIR)/graph/flow_problem.pb.cc

$(OBJ_DIR)/graph/flow_problem.pb.$O: $(GEN_DIR)/graph/flow_problem.pb.cc
	 $(CCC) $(CFLAGS) -c $(GEN_DIR)$Sgraph$Sflow_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sflow_problem.pb.$O

$(OBJ_DIR)/graph/max_flow.$O: $(SRC_DIR)/graph/max_flow.cc $(SRC_DIR)/util/stats.h $(GRAPH_DEPS)
	 $(PROTOBUF_DIR)$Sbin$Sprotoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)$Sgraph$Sflow_problem.proto
	 $(PROTOBUF_DIR)$Sbin$Sprotoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)$Sgraph$Sflow_problem.proto
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/max_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Smax_flow.$O

$(OBJ_DIR)/graph/min_cost_flow.$O: $(SRC_DIR)/graph/min_cost_flow.cc $(GRAPH_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/min_cost_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Smin_cost_flow.$O

# Shortestpaths library.

SHORTESTPATHS_LIB_OBJS=\
	$(OBJ_DIR)/graph/bellman_ford.$O \
	$(OBJ_DIR)/graph/dijkstra.$O \
	$(OBJ_DIR)/graph/astar.$O \
	$(OBJ_DIR)/graph/shortestpaths.$O

$(OBJ_DIR)/graph/bellman_ford.$O: $(SRC_DIR)/graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/bellman_ford.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sbellman_ford.$O

$(OBJ_DIR)/graph/dijkstra.$O: $(SRC_DIR)/graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/dijkstra.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sdijkstra.$O

$(OBJ_DIR)/graph/astar.$O: $(SRC_DIR)/graph/astar.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/astar.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sastar.$O

$(OBJ_DIR)/graph/shortestpaths.$O: $(SRC_DIR)/graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/shortestpaths.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph$Sshortestpaths.$O


# Routing library.

ROUTING_LIB_OBJS=\
	$(OBJ_DIR)/constraint_solver/routing.$O \
	$(OBJ_DIR)/constraint_solver/routing_enums.pb.$O \
	$(OBJ_DIR)/constraint_solver/routing_flags.$O \
	$(OBJ_DIR)/constraint_solver/routing_parameters.pb.$O \
	$(OBJ_DIR)/constraint_solver/routing_search.$O

$(GEN_DIR)/constraint_solver/routing_enums.pb.cc: $(SRC_DIR)/constraint_solver/routing_enums.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/routing_enums.proto

$(GEN_DIR)/constraint_solver/routing_enums.pb.h: $(GEN_DIR)/constraint_solver/routing_enums.pb.cc

$(OBJ_DIR)/constraint_solver/routing_enums.pb.$O: $(GEN_DIR)/constraint_solver/routing_enums.pb.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/routing_enums.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_enums.pb.$O

$(GEN_DIR)/constraint_solver/routing_parameters.pb.cc: $(SRC_DIR)/constraint_solver/routing_parameters.proto $(GEN_DIR)/constraint_solver/routing_enums.pb.h
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/routing_parameters.proto

$(GEN_DIR)/constraint_solver/routing_parameters.pb.h: $(GEN_DIR)/constraint_solver/routing_parameters.pb.cc

$(OBJ_DIR)/constraint_solver/routing_parameters.pb.$O: $(GEN_DIR)/constraint_solver/routing_parameters.pb.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/routing_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_parameters.pb.$O

$(OBJ_DIR)/constraint_solver/routing.$O: $(SRC_DIR)/constraint_solver/routing.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting.$O

$(OBJ_DIR)/constraint_solver/routing_flags.$O: $(SRC_DIR)/constraint_solver/routing_flags.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing_flags.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_flags.$O

$(OBJ_DIR)/constraint_solver/routing_search.$O: $(SRC_DIR)/constraint_solver/routing_search.cc $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver$Srouting_search.$O


# Algorithms library.

SPLIT_LIB_OBJS=\
	$(OBJ_DIR)/algorithms/dynamic_partition.$O \
	$(OBJ_DIR)/algorithms/dynamic_permutation.$O \
	$(OBJ_DIR)/algorithms/sparse_permutation.$O \
	$(OBJ_DIR)/algorithms/find_graph_symmetries.$O

ALGORITHMS_LIB_OBJS=\
	$(OBJ_DIR)/algorithms/hungarian.$O \
	$(OBJ_DIR)/algorithms/knapsack_solver.$O

$(OBJ_DIR)/algorithms/hungarian.$O: $(SRC_DIR)/algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/hungarian.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Shungarian.$O

$(OBJ_DIR)/algorithms/knapsack_solver.$O: $(SRC_DIR)/algorithms/knapsack_solver.cc $(LP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/knapsack_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sknapsack_solver.$O

$(OBJ_DIR)/algorithms/dynamic_partition.$O: $(SRC_DIR)/algorithms/dynamic_partition.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/dynamic_partition.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sdynamic_partition.$O

$(OBJ_DIR)/algorithms/dynamic_permutation.$O: $(SRC_DIR)/algorithms/dynamic_permutation.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/dynamic_permutation.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sdynamic_permutation.$O

$(OBJ_DIR)/algorithms/sparse_permutation.$O: $(SRC_DIR)/algorithms/sparse_permutation.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/sparse_permutation.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Ssparse_permutation.$O

$(OBJ_DIR)/algorithms/find_graph_symmetries.$O: $(SRC_DIR)/algorithms/find_graph_symmetries.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/find_graph_symmetries.cc $(OBJ_OUT)$(OBJ_DIR)$Salgorithms$Sfind_graph_symmetries.$O


# Base library.

BASE_LIB_OBJS=\
	$(OBJ_DIR)/base/bitmap.$O\
	$(OBJ_DIR)/base/callback.$O\
	$(OBJ_DIR)/base/file.$O\
	$(OBJ_DIR)/base/filelinereader.$O\
	$(OBJ_DIR)/base/join.$O\
	$(OBJ_DIR)/base/logging.$O\
	$(OBJ_DIR)/base/mutex.$O\
	$(OBJ_DIR)/base/numbers.$O\
	$(OBJ_DIR)/base/random.$O\
	$(OBJ_DIR)/base/recordio.$O\
	$(OBJ_DIR)/base/split.$O\
	$(OBJ_DIR)/base/stringpiece.$O\
	$(OBJ_DIR)/base/stringprintf.$O\
	$(OBJ_DIR)/base/sysinfo.$O\
	$(OBJ_DIR)/base/threadpool.$O\
	$(OBJ_DIR)/base/timer.$O \
	$(OBJ_DIR)/base/time_support.$O

$(OBJ_DIR)/base/bitmap.$O: $(SRC_DIR)/base/bitmap.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/bitmap.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sbitmap.$O
$(OBJ_DIR)/base/callback.$O: $(SRC_DIR)/base/callback.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/callback.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Scallback.$O
$(OBJ_DIR)/base/file.$O: $(SRC_DIR)/base/file.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/file.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sfile.$O
$(OBJ_DIR)/base/filelinereader.$O: $(SRC_DIR)/base/filelinereader.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/filelinereader.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sfilelinereader.$O
$(OBJ_DIR)/base/logging.$O: $(SRC_DIR)/base/logging.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/logging.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Slogging.$O
$(OBJ_DIR)/base/mutex.$O: $(SRC_DIR)/base/mutex.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/mutex.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Smutex.$O
$(OBJ_DIR)/base/numbers.$O: $(SRC_DIR)/base/numbers.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/numbers.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Snumbers.$O
$(OBJ_DIR)/base/join.$O: $(SRC_DIR)/base/join.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/join.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sjoin.$O
$(OBJ_DIR)/base/random.$O: $(SRC_DIR)/base/random.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/random.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Srandom.$O
$(OBJ_DIR)/base/recordio.$O: $(SRC_DIR)/base/recordio.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/recordio.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Srecordio.$O
$(OBJ_DIR)/base/threadpool.$O: $(SRC_DIR)/base/threadpool.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/threadpool.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sthreadpool.$O
$(OBJ_DIR)/base/split.$O: $(SRC_DIR)/base/split.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/split.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Ssplit.$O
$(OBJ_DIR)/base/stringpiece.$O: $(SRC_DIR)/base/stringpiece.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringpiece.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sstringpiece.$O
$(OBJ_DIR)/base/stringprintf.$O: $(SRC_DIR)/base/stringprintf.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringprintf.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Sstringprintf.$O
$(OBJ_DIR)/base/sysinfo.$O: $(SRC_DIR)/base/sysinfo.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/sysinfo.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Ssysinfo.$O
$(OBJ_DIR)/base/timer.$O: $(SRC_DIR)/base/timer.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/timer.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Stimer.$O
$(OBJ_DIR)/base/time_support.$O: $(SRC_DIR)/base/time_support.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/time_support.cc $(OBJ_OUT)$(OBJ_DIR)$Sbase$Stime_support.$O


# Glop library.

LP_DATA_OBJS= \
  $(OBJ_DIR)/lp_data/lp_data.$O \
  $(OBJ_DIR)/lp_data/lp_decomposer.$O \
  $(OBJ_DIR)/lp_data/lp_print_utils.$O \
  $(OBJ_DIR)/lp_data/lp_types.$O \
  $(OBJ_DIR)/lp_data/lp_utils.$O \
  $(OBJ_DIR)/lp_data/matrix_scaler.$O \
  $(OBJ_DIR)/lp_data/matrix_utils.$O \
  $(OBJ_DIR)/lp_data/mps_reader.$O \
  $(OBJ_DIR)/lp_data/sparse.$O \
  $(OBJ_DIR)/lp_data/sparse_column.$O \

$(OBJ_DIR)/lp_data/lp_data.$O: $(SRC_DIR)/lp_data/lp_data.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Slp_data.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_data.$O

$(OBJ_DIR)/lp_data/lp_decomposer.$O: $(SRC_DIR)/lp_data/lp_decomposer.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Slp_decomposer.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_decomposer.$O

$(OBJ_DIR)/lp_data/lp_print_utils.$O: $(SRC_DIR)/lp_data/lp_print_utils.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Slp_print_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_print_utils.$O

$(OBJ_DIR)/lp_data/lp_types.$O: $(SRC_DIR)/lp_data/lp_types.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Slp_types.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_types.$O

$(OBJ_DIR)/lp_data/lp_utils.$O: $(SRC_DIR)/lp_data/lp_utils.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Slp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Slp_utils.$O

$(OBJ_DIR)/lp_data/matrix_scaler.$O: $(SRC_DIR)/lp_data/matrix_scaler.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Smatrix_scaler.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smatrix_scaler.$O

$(OBJ_DIR)/lp_data/matrix_utils.$O: $(SRC_DIR)/lp_data/matrix_utils.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Smatrix_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smatrix_utils.$O

$(OBJ_DIR)/lp_data/mps_reader.$O: $(SRC_DIR)/lp_data/mps_reader.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Smps_reader.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smps_reader.$O

$(OBJ_DIR)/lp_data/mps_to_png.$O: $(SRC_DIR)/lp_data/mps_to_png.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Smps_to_png.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Smps_to_png.$O

$(OBJ_DIR)/lp_data/png_dump.$O: $(SRC_DIR)/lp_data/png_dump.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Spng_dump.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Spng_dump.$O

$(OBJ_DIR)/lp_data/sparse.$O: $(SRC_DIR)/lp_data/sparse.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Ssparse.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Ssparse.$O

$(OBJ_DIR)/lp_data/sparse_column.$O: $(SRC_DIR)/lp_data/sparse_column.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Slp_data$Ssparse_column.cc $(OBJ_OUT)$(OBJ_DIR)$Slp_data$Ssparse_column.$O

GLOP_LIB_OBJS= $(LP_DATA_OBJS) \
  $(OBJ_DIR)/glop/basis_representation.$O \
  $(OBJ_DIR)/glop/dual_edge_norms.$O \
  $(OBJ_DIR)/glop/entering_variable.$O \
  $(OBJ_DIR)/glop/initial_basis.$O \
  $(OBJ_DIR)/glop/lp_solver.$O \
  $(OBJ_DIR)/glop/lu_factorization.$O \
  $(OBJ_DIR)/glop/markowitz.$O \
  $(OBJ_DIR)/glop/parameters.pb.$O \
  $(OBJ_DIR)/glop/preprocessor.$O \
  $(OBJ_DIR)/glop/primal_edge_norms.$O \
  $(OBJ_DIR)/glop/proto_utils.$O \
  $(OBJ_DIR)/glop/reduced_costs.$O \
  $(OBJ_DIR)/glop/revised_simplex.$O \
  $(OBJ_DIR)/glop/status.$O \
  $(OBJ_DIR)/glop/update_row.$O \
  $(OBJ_DIR)/glop/variables_info.$O \
  $(OBJ_DIR)/glop/variable_values.$O

$(GEN_DIR)/glop/parameters.pb.cc: $(SRC_DIR)/glop/parameters.proto
	 $(PROTOBUF_DIR)$Sbin$Sprotoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)$Sglop$Sparameters.proto

$(GEN_DIR)/glop/parameters.pb.h: $(GEN_DIR)/glop/parameters.pb.cc

$(OBJ_DIR)/glop/parameters.pb.$O: $(GEN_DIR)/glop/parameters.pb.cc
	 $(CCC) $(CFLAGS) -c $(GEN_DIR)$Sglop$Sparameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sparameters.pb.$O

$(OBJ_DIR)/glop/basis_representation.$O: $(SRC_DIR)/glop/basis_representation.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sbasis_representation.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sbasis_representation.$O

$(OBJ_DIR)/glop/dual_edge_norms.$O: $(SRC_DIR)/glop/dual_edge_norms.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sdual_edge_norms.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sdual_edge_norms.$O

$(OBJ_DIR)/glop/entering_variable.$O: $(SRC_DIR)/glop/entering_variable.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sentering_variable.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sentering_variable.$O

$(OBJ_DIR)/glop/initial_basis.$O: $(SRC_DIR)/glop/initial_basis.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sinitial_basis.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sinitial_basis.$O

$(OBJ_DIR)/glop/lp_solver.$O: $(SRC_DIR)/glop/lp_solver.cc  $(GEN_DIR)/linear_solver/linear_solver.pb.h
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Slp_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Slp_solver.$O

$(OBJ_DIR)/glop/lu_factorization.$O: $(SRC_DIR)/glop/lu_factorization.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Slu_factorization.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Slu_factorization.$O

$(OBJ_DIR)/glop/markowitz.$O: $(SRC_DIR)/glop/markowitz.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Smarkowitz.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Smarkowitz.$O

$(OBJ_DIR)/glop/preprocessor.$O: $(SRC_DIR)/glop/preprocessor.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Spreprocessor.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Spreprocessor.$O

$(OBJ_DIR)/glop/primal_edge_norms.$O: $(SRC_DIR)/glop/primal_edge_norms.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sprimal_edge_norms.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sprimal_edge_norms.$O

$(OBJ_DIR)/glop/proto_driver.$O: $(SRC_DIR)/glop/proto_driver.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sproto_driver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sproto_driver.$O

$(OBJ_DIR)/glop/proto_txt_to_bin.$O: $(SRC_DIR)/glop/proto_txt_to_bin.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sproto_txt_to_bin.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sproto_txt_to_bin.$O

$(OBJ_DIR)/glop/proto_utils.$O: $(SRC_DIR)/glop/proto_utils.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sproto_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sproto_utils.$O

$(OBJ_DIR)/glop/reduced_costs.$O: $(SRC_DIR)/glop/reduced_costs.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sreduced_costs.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sreduced_costs.$O

$(OBJ_DIR)/glop/revised_simplex.$O: $(SRC_DIR)/glop/revised_simplex.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Srevised_simplex.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Srevised_simplex.$O

$(OBJ_DIR)/glop/status.$O: $(SRC_DIR)/glop/status.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Sstatus.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Sstatus.$O

$(OBJ_DIR)/glop/update_row.$O: $(SRC_DIR)/glop/update_row.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Supdate_row.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Supdate_row.$O

$(OBJ_DIR)/glop/variables_info.$O: $(SRC_DIR)/glop/variables_info.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Svariables_info.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Svariables_info.$O

$(OBJ_DIR)/glop/variable_values.$O: $(SRC_DIR)/glop/variable_values.cc
	 $(CCC) $(CFLAGS) -c $(SRC_DIR)$Sglop$Svariable_values.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Svariable_values.$O

$(OBJ_DIR)/glop/mps_driver.$O: $(EX_DIR)/cpp/mps_driver.cc $(GEN_DIR)/glop/parameters.pb.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Smps_driver.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Smps_driver.$O

$(BIN_DIR)/mps_driver$E: $(OBJ_DIR)/glop/mps_driver.$O $(OR_TOOLS_LIBS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sglop$Smps_driver.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smps_driver$E

$(OBJ_DIR)/glop/solve.$O: $(EX_DIR)/cpp/solve.cc $(GEN_DIR)/glop/parameters.pb.h $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssolve.cc $(OBJ_OUT)$(OBJ_DIR)$Sglop$Ssolve.$O

$(BIN_DIR)/solve$E: $(OBJ_DIR)/glop/solve.$O $(OR_TOOLS_LIBS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Sglop$Ssolve.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssolve$E


# CVRPTW common library

CVRPTW_LIB_OBJS=\
	$(OBJ_DIR)/cvrptw_lib.$O

$(OBJ_DIR)/cvrptw_lib.$O: $(EX_DIR)/cpp/cvrptw_lib.cc $(EX_DIR)/cpp/cvrptw_lib.h $(ROUTING_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw_lib.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw_lib.$O

$(LIB_DIR)/$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX): $(CVRPTW_LIB_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX) $(CVRPTW_LIB_OBJS)

# DIMACS challenge problem format library

DIMACS_LIB_OBJS=\
	$(OBJ_DIR)/parse_dimacs_assignment.$O

$(OBJ_DIR)/parse_dimacs_assignment.$O: $(EX_DIR)/cpp/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/parse_dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sparse_dimacs_assignment.$O

$(LIB_DIR)/$(LIB_PREFIX)dimacs.$(LIB_SUFFIX): $(DIMACS_LIB_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX) $(DIMACS_LIB_OBJS)

# FAP challenge problem format library

FAP_LIB_OBJS=\
	$(OBJ_DIR)/fap_model_printer.$O\
	$(OBJ_DIR)/fap_parser.$O\
	$(OBJ_DIR)/fap_utilities.$O

$(OBJ_DIR)/fap_model_printer.$O: $(EX_DIR)/cpp/fap_model_printer.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_model_printer.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_model_printer.$O
$(OBJ_DIR)/fap_parser.$O: $(EX_DIR)/cpp/fap_parser.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_parser.$O
$(OBJ_DIR)/fap_utilities.$O: $(EX_DIR)/cpp/fap_utilities.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Sfap_utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_utilities.$O


# Flatzinc code

FLATZINC_LIB_OBJS=\
	$(OBJ_DIR)/flatzinc/constraints.$O\
	$(OBJ_DIR)/flatzinc/flatzinc_constraints.$O\
	$(OBJ_DIR)/flatzinc/model.$O\
	$(OBJ_DIR)/flatzinc/parallel_support.$O\
	$(OBJ_DIR)/flatzinc/parser.$O\
	$(OBJ_DIR)/flatzinc/parser.tab.$O\
	$(OBJ_DIR)/flatzinc/parser.yy.$O\
	$(OBJ_DIR)/flatzinc/presolve.$O\
	$(OBJ_DIR)/flatzinc/sat_constraint.$O\
	$(OBJ_DIR)/flatzinc/search.$O\
	$(OBJ_DIR)/flatzinc/sequential_support.$O\
	$(OBJ_DIR)/flatzinc/solver.$O

$(GEN_DIR)/flatzinc/parser.yy.cc: $(SRC_DIR)/flatzinc/parser.lex $(FLEX)
	$(FLEX) -o$(GEN_DIR)/flatzinc/parser.yy.cc $(SRC_DIR)/flatzinc/parser.lex

$(GEN_DIR)/flatzinc/parser.tab.cc: $(SRC_DIR)/flatzinc/parser.yy $(BISON)
	$(BISON) -t -o $(GEN_DIR)/flatzinc/parser.tab.cc -d $<

$(GEN_DIR)/flatzinc/parser.tab.hh: $(GEN_DIR)/flatzinc/parser.tab.cc

$(OBJ_DIR)/flatzinc/constraints.$O: $(SRC_DIR)/flatzinc/constraints.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sconstraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sconstraints.$O

$(OBJ_DIR)/flatzinc/flatzinc_constraints.$O: $(SRC_DIR)/flatzinc/flatzinc_constraints.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sflatzinc_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sflatzinc_constraints.$O

$(OBJ_DIR)/flatzinc/model.$O: $(SRC_DIR)/flatzinc/model.cc $(SRC_DIR)/flatzinc/model.h $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Smodel.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Smodel.$O

$(OBJ_DIR)/flatzinc/parallel_support.$O: $(SRC_DIR)/flatzinc/parallel_support.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparallel_support.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparallel_support.$O

$(OBJ_DIR)/flatzinc/parser.$O: $(SRC_DIR)/flatzinc/parser.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparser.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.$O

$(OBJ_DIR)/flatzinc/parser.tab.$O: $(GEN_DIR)/flatzinc/parser.tab.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sflatzinc$Sparser.tab.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.tab.$O

$(OBJ_DIR)/flatzinc/parser.yy.$O: $(GEN_DIR)/flatzinc/parser.yy.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sflatzinc$Sparser.yy.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Sparser.yy.$O

$(OBJ_DIR)/flatzinc/presolve.$O: $(SRC_DIR)/flatzinc/presolve.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Spresolve.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Spresolve.$O

$(OBJ_DIR)/flatzinc/sat_constraint.$O: $(SRC_DIR)/flatzinc/sat_constraint.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssat_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssat_constraint.$O

$(OBJ_DIR)/flatzinc/search.$O: $(SRC_DIR)/flatzinc/search.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssearch.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssearch.$O

$(OBJ_DIR)/flatzinc/sequential_support.$O: $(SRC_DIR)/flatzinc/sequential_support.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssequential_support.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssequential_support.$O

$(OBJ_DIR)/flatzinc/solver.$O: $(SRC_DIR)/flatzinc/solver.cc $(FLATZINC_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Ssolver.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc$Ssolver.$O

$(LIB_DIR)/$(LIB_PREFIX)fz.$(LIB_SUFFIX): $(FLATZINC_LIB_OBJS)
	$(LINK_CMD) $(LINK_PREFIX)$(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX) $(FLATZINC_LIB_OBJS)

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

$(OBJ_DIR)/cvrptw.$O: $(EX_DIR)/cpp/cvrptw.cc $(CVRPTW_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(BIN_DIR)/cvrptw$E: $(OR_TOOLS_LIBS) $(CVRPTW_LIBS) $(OBJ_DIR)/cvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw.$O $(OR_TOOLS_LNK) $(CVRPTW_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw$E

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

# Sat solver

sat: bin/sat_runner$E

SAT_LIB_OBJS = \
	$(OBJ_DIR)/sat/boolean_problem.$O\
	$(OBJ_DIR)/sat/boolean_problem.pb.$O \
	$(OBJ_DIR)/sat/clause.$O\
	$(OBJ_DIR)/sat/drat.$O\
	$(OBJ_DIR)/sat/encoding.$O\
	$(OBJ_DIR)/sat/integer.$O\
	$(OBJ_DIR)/sat/lp_utils.$O\
	$(OBJ_DIR)/sat/optimization.$O\
	$(OBJ_DIR)/sat/pb_constraint.$O\
	$(OBJ_DIR)/sat/sat_parameters.pb.$O\
	$(OBJ_DIR)/sat/sat_solver.$O\
	$(OBJ_DIR)/sat/simplification.$O\
	$(OBJ_DIR)/sat/symmetry.$O


$(OBJ_DIR)/sat/sat_solver.$O: $(SRC_DIR)/sat/sat_solver.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/sat_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_solver.$O

$(OBJ_DIR)/sat/lp_utils.$O: $(SRC_DIR)/sat/lp_utils.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/lp_utils.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Slp_utils.$O

$(OBJ_DIR)/sat/simplification.$O: $(SRC_DIR)/sat/simplification.cc  $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/simplification.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssimplification.$O

$(OBJ_DIR)/sat/boolean_problem.$O: $(SRC_DIR)/sat/boolean_problem.cc  $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/boolean_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sboolean_problem.$O

$(GEN_DIR)/sat/boolean_problem.pb.cc: $(SRC_DIR)/sat/boolean_problem.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/sat/boolean_problem.proto

$(GEN_DIR)/sat/boolean_problem.pb.h: $(GEN_DIR)/sat/boolean_problem.pb.cc

$(OBJ_DIR)/sat/boolean_problem.pb.$O: $(GEN_DIR)/sat/boolean_problem.pb.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/sat/boolean_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sboolean_problem.pb.$O

$(OBJ_DIR)/sat/clause.$O: $(SRC_DIR)/sat/clause.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/clause.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sclause.$O

$(OBJ_DIR)/sat/drat.$O: $(SRC_DIR)/sat/drat.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/drat.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sdrat.$O

$(OBJ_DIR)/sat/encoding.$O: $(SRC_DIR)/sat/encoding.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/encoding.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sencoding.$O

$(OBJ_DIR)/sat/integer.$O: $(SRC_DIR)/sat/integer.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/integer.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Sinteger.$O

$(OBJ_DIR)/sat/optimization.$O: $(SRC_DIR)/sat/optimization.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/optimization.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Soptimization.$O

$(OBJ_DIR)/sat/pb_constraint.$O: $(SRC_DIR)/sat/pb_constraint.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/pb_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Spb_constraint.$O

$(OBJ_DIR)/sat/symmetry.$O: $(SRC_DIR)/sat/symmetry.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/symmetry.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssymmetry.$O

$(GEN_DIR)/sat/sat_parameters.pb.cc: $(SRC_DIR)/sat/sat_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/sat/sat_parameters.proto

$(GEN_DIR)/sat/sat_parameters.pb.h: $(GEN_DIR)/sat/sat_parameters.pb.cc

$(OBJ_DIR)/sat/sat_parameters.pb.$O: $(GEN_DIR)/sat/sat_parameters.pb.cc $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/sat/sat_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_parameters.pb.$O


$(OBJ_DIR)/sat/sat_runner.$O: $(EX_DIR)/cpp/sat_runner.cc $(EX_DIR)/cpp/opb_reader.h $(EX_DIR)/cpp/sat_cnf_reader.h $(SAT_DEPS)
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssat_runner.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat$Ssat_runner.$O

$(BIN_DIR)/sat_runner$E: $(OR_TOOLS_LIBS) $(OBJ_DIR)/sat/sat_runner.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Ssat$Ssat_runner.$O $(OR_TOOLS_LNK) $(OR_TOOLS_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssat_runner$E

# Bop solver
BOP_LIB_OBJS = \
	$(OBJ_DIR)/bop/bop_base.$O\
	$(OBJ_DIR)/bop/bop_fs.$O\
	$(OBJ_DIR)/bop/bop_lns.$O\
	$(OBJ_DIR)/bop/bop_ls.$O\
	$(OBJ_DIR)/bop/bop_parameters.pb.$O\
	$(OBJ_DIR)/bop/bop_portfolio.$O\
	$(OBJ_DIR)/bop/bop_solution.$O\
	$(OBJ_DIR)/bop/bop_solver.$O\
	$(OBJ_DIR)/bop/bop_util.$O\
	$(OBJ_DIR)/bop/complete_optimizer.$O\
	$(OBJ_DIR)/bop/integral_solver.$O

$(OBJ_DIR)/bop/bop_base.$O: $(SRC_DIR)/bop/bop_base.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_base.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_base.$O

$(OBJ_DIR)/bop/bop_fs.$O: $(SRC_DIR)/bop/bop_fs.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_fs.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_fs.$O

$(OBJ_DIR)/bop/bop_lns.$O: $(SRC_DIR)/bop/bop_lns.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_lns.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_lns.$O

$(OBJ_DIR)/bop/bop_ls.$O: $(SRC_DIR)/bop/bop_ls.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_ls.$O

$(GEN_DIR)/bop/bop_parameters.pb.cc: $(SRC_DIR)/bop/bop_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/bop/bop_parameters.proto

$(GEN_DIR)/bop/bop_parameters.pb.h: $(GEN_DIR)/bop/bop_parameters.pb.cc

$(OBJ_DIR)/bop/bop_parameters.pb.$O: $(GEN_DIR)/bop/bop_parameters.pb.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/bop/bop_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_parameters.pb.$O

$(OBJ_DIR)/bop/bop_portfolio.$O: $(SRC_DIR)/bop/bop_portfolio.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_portfolio.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_portfolio.$O

$(OBJ_DIR)/bop/bop_solver.$O: $(SRC_DIR)/bop/bop_solver.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_solver.$O

$(OBJ_DIR)/bop/bop_solution.$O: $(SRC_DIR)/bop/bop_solution.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_solution.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_solution.$O

$(OBJ_DIR)/bop/bop_util.$O: $(SRC_DIR)/bop/bop_util.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/bop_util.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sbop_util.$O

$(OBJ_DIR)/bop/complete_optimizer.$O: $(SRC_DIR)/bop/complete_optimizer.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/complete_optimizer.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Scomplete_optimizer.$O

$(OBJ_DIR)/bop/integral_solver.$O: $(SRC_DIR)/bop/integral_solver.cc $(BOP_DEPS)
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/bop/integral_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sbop$Sintegral_solver.$O

# OR Tools unique library.

$(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX): $(CONSTRAINT_SOLVER_LIB_OBJS) $(LINEAR_SOLVER_LIB_OBJS) $(UTIL_LIB_OBJS) $(GRAPH_LIB_OBJS) $(SHORTESTPATHS_LIB_OBJS) $(ROUTING_LIB_OBJS) $(BOP_LIB_OBJS) $(GLOP_LIB_OBJS) $(ALGORITHMS_LIB_OBJS) $(SPLIT_LIB_OBJS) $(SAT_LIB_OBJS) $(BASE_LIB_OBJS)
	$(LINK_CMD) \
	  $(LDOUT)$(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) \
	  $(ALGORITHMS_LIB_OBJS) \
	  $(SPLIT_LIB_OBJS) \
	  $(BASE_LIB_OBJS) \
	  $(CONSTRAINT_SOLVER_LIB_OBJS) \
	  $(GRAPH_LIB_OBJS) \
	  $(LINEAR_SOLVER_LIB_OBJS) \
	  $(BOP_LIB_OBJS) \
	  $(GLOP_LIB_OBJS) \
	  $(ROUTING_LIB_OBJS) \
	  $(SAT_LIB_OBJS) \
	  $(SHORTESTPATHS_LIB_OBJS) \
	  $(UTIL_LIB_OBJS) \
	  $(OR_TOOLS_THIRD_PARTY_DEPS) \
	  $(OR_TOOLS_LD_FLAGS)


# Target for archives

ifeq "$(SYSTEM)" "win"
cc_archive: $(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
	-$(DELREC) temp
	mkdir temp
	mkdir temp\\$(INSTALL_DIR)
	mkdir temp\\$(INSTALL_DIR)\\bin
	mkdir temp\\$(INSTALL_DIR)\\examples
	mkdir temp\\$(INSTALL_DIR)\\examples\\cpp
	mkdir temp\\$(INSTALL_DIR)\\examples\\data
	mkdir temp\\$(INSTALL_DIR)\\include
	mkdir temp\\$(INSTALL_DIR)\\include\\algorithms
	mkdir temp\\$(INSTALL_DIR)\\include\\base
	mkdir temp\\$(INSTALL_DIR)\\include\\constraint_solver
	mkdir temp\\$(INSTALL_DIR)\\include\\gflags
	mkdir temp\\$(INSTALL_DIR)\\include\\glop
	mkdir temp\\$(INSTALL_DIR)\\include\\google
	mkdir temp\\$(INSTALL_DIR)\\include\\graph
	mkdir temp\\$(INSTALL_DIR)\\include\\linear_solver
	mkdir temp\\$(INSTALL_DIR)\\include\\util
	mkdir temp\\$(INSTALL_DIR)\\lib
	mkdir temp\\$(INSTALL_DIR)\\objs
	copy LICENSE-2.0.txt temp\\$(INSTALL_DIR)
	copy tools\\README.cc temp\\$(INSTALL_DIR)\\README
	copy tools\\Makefile.cc temp\\$(INSTALL_DIR)\\Makefile
	copy lib\\ortools.lib temp\\$(INSTALL_DIR)\\lib
	copy examples\\cpp\\*.cc temp\\$(INSTALL_DIR)\\examples\\cpp
	copy examples\\cpp\\*.h temp\\$(INSTALL_DIR)\\examples\\cpp
	cd temp\$(INSTALL_DIR) && ..\..\tools\tar.exe -C ..\.. -c -v --exclude *svn* --exclude *roadef* examples\data | ..\..\tools\tar.exe xvm
	copy src\\algorithms\\*.h temp\\$(INSTALL_DIR)\\include\\algorithms
	copy src\\base\\*.h temp\\$(INSTALL_DIR)\\include\\base
	copy src\\constraint_solver\\*.h temp\\$(INSTALL_DIR)\\include\\constraint_solver
	copy src\\gen\\constraint_solver\\*.pb.h temp\\$(INSTALL_DIR)\\include\\constraint_solver
	copy src\\graph\\*.h temp\\$(INSTALL_DIR)\\include\\graph
	copy src\\bop\\*.h temp\\$(INSTALL_DIR)\\include\\bop
	copy src\\gen\\bop\\*.pb.h temp\\$(INSTALL_DIR)\\include\\bop
	copy src\\glop\\*.h temp\\$(INSTALL_DIR)\\include\\glop
	copy src\\gen\\glop\\*.h temp\\$(INSTALL_DIR)\\include\\glop
	copy src\\linear_solver\\*.h temp\\$(INSTALL_DIR)\\include\\linear_solver
	copy src\\gen\\linear_solver\\*.pb.h temp\\$(INSTALL_DIR)\\include\\linear_solver
	copy src\\sat\\*.h temp\\$(INSTALL_DIR)\\include\\sat
	copy src\\gen\\sat\\*.pb.h temp\\$(INSTALL_DIR)\\include\\sat
	copy src\\util\\*.h temp\\$(INSTALL_DIR)\\include\\util
	cd temp\\$(INSTALL_DIR)\\include && ..\..\..\tools\tar.exe -C ..\\..\\..\\dependencies\\install\\include -c -v gflags | ..\..\..\tools\tar.exe xvm
	cd temp\\$(INSTALL_DIR)\\include && ..\..\..\tools\tar.exe -C ..\\..\\..\\dependencies\\install\\include -c -v google | ..\..\..\tools\tar.exe xvm
	cd temp\\$(INSTALL_DIR)\\include && ..\..\..\tools\tar.exe -C ..\\..\\..\\dependencies\\install\\include -c -v sparsehash | ..\..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.cc.$(INSTALL_PORT)-$(OR_TOOLS_VERSION).zip $(INSTALL_DIR)
	-$(DELREC) temp
else
cc_archive: $(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
	-$(DELREC) temp
	mkdir temp
	mkdir temp/$(INSTALL_DIR)
	mkdir temp/$(INSTALL_DIR)/bin
	mkdir temp/$(INSTALL_DIR)/examples
	mkdir temp/$(INSTALL_DIR)/examples/cpp
	mkdir temp/$(INSTALL_DIR)/examples/data
	mkdir temp/$(INSTALL_DIR)/examples/data/et_jobshop
	mkdir temp/$(INSTALL_DIR)/examples/data/flexible_jobshop
	mkdir temp/$(INSTALL_DIR)/examples/data/jobshop
	mkdir temp/$(INSTALL_DIR)/examples/data/multidim_knapsack
	mkdir temp/$(INSTALL_DIR)/examples/data/cvrptw
	mkdir temp/$(INSTALL_DIR)/examples/data/pdptw
	mkdir temp/$(INSTALL_DIR)/include
	mkdir temp/$(INSTALL_DIR)/include/algorithms
	mkdir temp/$(INSTALL_DIR)/include/base
	mkdir temp/$(INSTALL_DIR)/include/constraint_solver
	mkdir temp/$(INSTALL_DIR)/include/gflags
	mkdir temp/$(INSTALL_DIR)/include/bop
	mkdir temp/$(INSTALL_DIR)/include/glop
	mkdir temp/$(INSTALL_DIR)/include/google
	mkdir temp/$(INSTALL_DIR)/include/graph
	mkdir temp/$(INSTALL_DIR)/include/linear_solver
	mkdir temp/$(INSTALL_DIR)/include/sat
	mkdir temp/$(INSTALL_DIR)/include/util
	mkdir temp/$(INSTALL_DIR)/lib
	mkdir temp/$(INSTALL_DIR)/objs
	cp LICENSE-2.0.txt temp/$(INSTALL_DIR)
	cp tools/README.cc temp/$(INSTALL_DIR)/README
	cp tools/Makefile.cc temp/$(INSTALL_DIR)/Makefile
	cp lib/libortools.$(LIB_SUFFIX) temp/$(INSTALL_DIR)/lib
ifeq ($(PLATFORM),MACOSX)
	cp tools/install_libortools_mac.sh temp/$(INSTALL_DIR)
endif
	cp examples/cpp/*.cc temp/$(INSTALL_DIR)/examples/cpp
	cp examples/cpp/*.h temp/$(INSTALL_DIR)/examples/cpp
	cp -R examples/data/et_jobshop/* temp/$(INSTALL_DIR)/examples/data/et_jobshop
	cp -R examples/data/flexible_jobshop/* temp/$(INSTALL_DIR)/examples/data/flexible_jobshop
	cp -R examples/data/jobshop/* temp/$(INSTALL_DIR)/examples/data/jobshop
	cp -R examples/data/multidim_knapsack/* temp/$(INSTALL_DIR)/examples/data/multidim_knapsack
	cp -R examples/data/cvrptw/* temp/$(INSTALL_DIR)/examples/data/cvrptw
	cp -R examples/data/pdptw/* temp/$(INSTALL_DIR)/examples/data/pdptw
	cp src/algorithms/*.h temp/$(INSTALL_DIR)/include/algorithms
	cp src/base/*.h temp/$(INSTALL_DIR)/include/base
	cp src/constraint_solver/*.h temp/$(INSTALL_DIR)/include/constraint_solver
	cp src/gen/constraint_solver/*.pb.h temp/$(INSTALL_DIR)/include/constraint_solver
	cp src/bop/*.h temp/$(INSTALL_DIR)/include/bop
	cp src/gen/bop/*.pb.h temp/$(INSTALL_DIR)/include/bop
	cp src/glop/*.h temp/$(INSTALL_DIR)/include/glop
	cp src/gen/glop/*.pb.h temp/$(INSTALL_DIR)/include/glop
	cp src/graph/*.h temp/$(INSTALL_DIR)/include/graph
	cp src/linear_solver/*.h temp/$(INSTALL_DIR)/include/linear_solver
	cp src/gen/linear_solver/*.pb.h temp/$(INSTALL_DIR)/include/linear_solver
	cp src/sat/*.h temp/$(INSTALL_DIR)/include/sat
	cp src/gen/sat/*.pb.h temp/$(INSTALL_DIR)/include/sat
	cp src/util/*.h temp/$(INSTALL_DIR)/include/util
	cd temp/$(INSTALL_DIR)/include && tar -C ../../../dependencies/install/include -c -v gflags | tar xvm
	cd temp/$(INSTALL_DIR)/include && tar -C ../../../dependencies/install/include -c -v google | tar xvm
	cd temp/$(INSTALL_DIR)/include && tar -C ../../../dependencies/install/include -c -v sparsehash | tar xvm
	cd temp && tar -c -v -z --no-same-owner -f ../Google.OrTools.cc.$(INSTALL_PORT)-$(OR_TOOLS_VERSION).tar.gz $(INSTALL_DIR)
	-$(DELREC) temp
endif

ifeq "$(SYSTEM)" "win"
fz_archive: fz
	-$(DELREC) temp
	mkdir temp
	mkdir temp\\$(INSTALL_DIR)
	mkdir temp\\$(INSTALL_DIR)\\bin
	mkdir temp\\$(INSTALL_DIR)\\share
	mkdir temp\\$(INSTALL_DIR)\\share\\minizinc
	copy LICENSE-2.0.txt temp\\$(INSTALL_DIR)
	copy bin\\fz.exe temp\\$(INSTALL_DIR)\\bin\\fzn-or-tools.exe
	copy src\\flatzinc\\mznlib\\*.mzn temp\\$(INSTALL_DIR)\\share\\minizinc
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.flatzinc.$(INSTALL_PORT)-$(OR_TOOLS_VERSION).zip $(INSTALL_DIR)
	-$(DELREC) temp
else
fz_archive: $(LIB_DIR)/$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
	-$(DELREC) temp
	mkdir temp
	mkdir temp/$(INSTALL_DIR)
	mkdir temp/$(INSTALL_DIR)/bin
	mkdir temp/$(INSTALL_DIR)/share
	mkdir temp/$(INSTALL_DIR)/share/minizinc
	cp LICENSE-2.0.txt temp/$(INSTALL_DIR)
	cp bin/fz temp/$(INSTALL_DIR)/bin/fzn-or-tools
	cp src/flatzinc/mznlib/* temp/$(INSTALL_DIR)/share/minizinc
	cd temp && tar cvzf ../Google.OrTools.flatzinc.$(INSTALL_PORT)-$(OR_TOOLS_VERSION).tar.gz $(INSTALL_DIR)
	-$(DELREC) temp
endif


# Debug
printdir:
	@echo LIB_DIR = $(LIB_DIR)
	@echo BIN_DIR = $(BIN_DIR)
	@echo GEN_DIR = $(GEN_DIR)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo SRC_DIR = $(SRC_DIR)
	@echo EX_DIR  = $(EX_DIR)
