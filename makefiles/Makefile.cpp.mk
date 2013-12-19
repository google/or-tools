#### DYNAMIC link and libs ####

# List libraries by module.
DYNAMIC_BASE_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX) \
	$(LIB_DIR)/$(LIBPREFIX)base.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_LP_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)linear_solver.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_ALGORITHMS_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)algorithms.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_CP_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)constraint_solver.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_GRAPH_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)graph.$(DYNAMIC_LIB_SUFFIX) \
	$(LIB_DIR)/$(LIBPREFIX)shortestpaths.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_ROUTING_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)routing.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_FLATZINC_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)fz.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_DIMACS_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)dimacs.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_FAP_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)fap.$(DYNAMIC_LIB_SUFFIX)

DYNAMIC_SAT_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)sat.$(DYNAMIC_LIB_SUFFIX)

# Lib dependencies.
DYNAMIC_BASE_DEPS = $(DYNAMIC_BASE_LIBS)

DYNAMIC_LP_DEPS = $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_ALGORITHMS_DEPS = $(DYNAMIC_ALGORITHMS_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_CP_DEPS = $(DYNAMIC_CP_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_GRAPH_DEPS = $(DYNAMIC_GRAPH_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_ROUTING_DEPS = $(DYNAMIC_ROUTING_LIBS) $(DYNAMIC_CP_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_GRAPH_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_FLATZINC_DEPS = $(DYNAMIC_FLATZINC_LIBS) $(DYNAMIC_CP_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_DIMACS_DEPS = $(DYNAMIC_DIMACS_LIBS) $(DYNAMIC_GRAPH_LIBS) $(DYNAMIC_ALGORITHMS_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_FAP_DEPS = \
	$(DYNAMIC_FAP_LIBS) \
	$(DYNAMIC_CP_LIBS) \
	$(DYNAMIC_LP_LIBS) \
	$(DYNAMIC_BASE_LIBS)

DYNAMIC_SAT_DEPS = $(DYNAMIC_SAT_LIBS) $(DYNAMIC_BASE_LIBS)

# Create link commands.
DYNAMIC_BASE_LNK = \
	$(DYNAMIC_PRE_LIB)util$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_PRE_LIB)base$(DYNAMIC_POST_LIB)

DYNAMIC_LP_LNK = \
	$(DYNAMIC_PRE_LIB)linear_solver$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_BASE_LNK) \
	$(DYNAMIC_LD_LP_DEPS)  # Third party linear solvers.

DYNAMIC_ALGORITHMS_LNK = \
	$(DYNAMIC_PRE_LIB)algorithms$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_LP_LNK)

DYNAMIC_CP_LNK = \
	$(DYNAMIC_PRE_LIB)constraint_solver$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_LP_LNK)

DYNAMIC_ROUTING_LNK = \
	$(DYNAMIC_PRE_LIB)routing$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_PRE_LIB)graph$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_PRE_LIB)shortestpaths$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_CP_LNK)

DYNAMIC_GRAPH_LNK = \
	$(DYNAMIC_PRE_LIB)graph$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_PRE_LIB)shortestpaths$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_BASE_LNK)

DYNAMIC_FLATZINC_LNK = \
	$(DYNAMIC_PRE_LIB)fz$(DYNAMIC_POST_LIB)\
	$(DYNAMIC_CP_LNK)

DYNAMIC_DIMACS_LNK = \
	$(DYNAMIC_PRE_LIB)graph$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_PRE_LIB)shortestpaths$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_PRE_LIB)dimacs$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_ALGORITHMS_LNK)

DYNAMIC_FAP_LNK = \
	$(DYNAMIC_PRE_LIB)fap$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_CP_LNK)

DYNAMIC_SAT_LNK = \
	$(DYNAMIC_PRE_LIB)sat$(DYNAMIC_POST_LIB) \
	$(DYNAMIC_BASE_LNK)

#### STATIC link and libs ####

# List libraries by module.
STATIC_BASE_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX) \
	$(LIB_DIR)/$(LIBPREFIX)base.$(STATIC_LIB_SUFFIX)

STATIC_LP_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)linear_solver.$(STATIC_LIB_SUFFIX)

STATIC_ALGORITHMS_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)algorithms.$(STATIC_LIB_SUFFIX)

STATIC_CP_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)constraint_solver.$(STATIC_LIB_SUFFIX)

STATIC_GRAPH_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)graph.$(STATIC_LIB_SUFFIX) \
	$(LIB_DIR)/$(LIBPREFIX)shortestpaths.$(STATIC_LIB_SUFFIX)

STATIC_ROUTING_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)routing.$(STATIC_LIB_SUFFIX)

STATIC_FLATZINC_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)fz.$(STATIC_LIB_SUFFIX)

STATIC_SAT_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)sat.$(STATIC_LIB_SUFFIX)

# Lib dependencies.
STATIC_BASE_DEPS = $(STATIC_BASE_LIBS)

STATIC_LP_DEPS = $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_ALGORITHMS_DEPS = $(STATIC_ALGORITHMS_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_CP_DEPS = $(STATIC_CP_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_GRAPH_DEPS = $(STATIC_GRAPH_LIBS) $(STATIC_BASE_LIBS)

STATIC_ROUTING_DEPS = $(STATIC_ROUTING_LIBS) $(STATIC_CP_LIBS) $(STATIC_LP_LIBS) $(STATIC_GRAPH_LIBS) $(STATIC_BASE_LIBS)

STATIC_FLATZINC_DEPS = $(STATIC_FLATZINC_LIBS) $(STATIC_CP_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_SAT_DEPS = $(STATIC_SAT_LIBS) $(STATIC_BASE_LIBS)

# Create link commands.
STATIC_BASE_LNK = \
	$(STATIC_PRE_LIB)util$(STATIC_POST_LIB) \
	$(STATIC_PRE_LIB)base$(STATIC_POST_LIB)

STATIC_LP_LNK = \
	$(STATIC_PRE_LIB)linear_solver$(STATIC_POST_LIB) \
	$(STATIC_BASE_LNK) \
	$(STATIC_LD_LP_DEPS)  # Third party linear solvers.

STATIC_ALGORITHMS_LNK = \
	$(STATIC_PRE_LIB)algorithms$(STATIC_POST_LIB) \
	$(STATIC_LP_LNK)

STATIC_CP_LNK = \
	$(STATIC_PRE_LIB)constraint_solver$(STATIC_POST_LIB) \
	$(STATIC_LP_LNK)

STATIC_ROUTING_LNK = \
	$(STATIC_PRE_LIB)routing$(STATIC_POST_LIB) \
	$(STATIC_PRE_LIB)graph$(STATIC_POST_LIB) \
	$(STATIC_PRE_LIB)shortestpaths$(STATIC_POST_LIB) \
	$(STATIC_CP_LNK)

STATIC_GRAPH_LNK = \
	$(STATIC_PRE_LIB)graph$(STATIC_POST_LIB) \
	$(STATIC_PRE_LIB)shortestpaths$(STATIC_POST_LIB) \
	$(STATIC_BASE_LNK)

STATIC_FLATZINC_LNK = \
	$(STATIC_PRE_LIB)fz$(STATIC_POST_LIB)\
	$(STATIC_CP_LNK)

STATIC_SAT_LNK = \
	$(STATIC_PRE_LIB)sat$(STATIC_POST_LIB) \
	$(STATIC_BASE_LNK)

# Binaries

CPBINARIES = \
	$(BIN_DIR)/costas_array$E \
	$(BIN_DIR)/cryptarithm$E \
	$(BIN_DIR)/cvrptw$E \
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
	$(BIN_DIR)/strawberry_fields_with_column_generation$E

# Special dimacs example.

# Makefile targets.

# Main target
cc: cplibs cpexe algorithmslibs graphlibs lplibs lpexe sat

# Clean target

clean_cc:
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)base.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)constraint_solver.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)linear_solver.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)graph.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)routing.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)algorithms.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)dimacs.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)fz.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)shortestpaths.$(DYNAMIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)base.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)constraint_solver.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)linear_solver.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)graph.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)routing.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)algorithms.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(LIB_DIR)$S$(LIBPREFIX)shortestpaths.$(STATIC_LIB_SUFFIX)
	-$(DEL) $(OBJ_DIR)$S*.$O
	-$(DEL) $(CPBINARIES)
	-$(DEL) $(BIN_DIR)$Sfz$E
	-$(DEL) $(BIN_DIR)$Smtsearch_test$E
	-$(DEL) $(LPBINARIES)
	-$(DEL) $(GEN_DIR)$Sconstraint_solver$S*.pb.*
	-$(DEL) $(GEN_DIR)$Slinear_solver$S*.pb.*
	-$(DEL) $(GEN_DIR)$Sflatzinc$S*
	-$(DEL) $(BIN_DIR)$S*.exp
	-$(DEL) $(BIN_DIR)$S*.lib
	-$(DEL) $(SRC_DIR)$Sflatzinc$Slexer*
	-$(DEL) $(SRC_DIR)$Sflatzinc$Sparser.tab.*

clean_compat:
	-$(DELREC) $(OR_ROOT)constraint_solver
	-$(DELREC) $(OR_ROOT)linear_solver
	-$(DELREC) $(OR_ROOT)algorithms
	-$(DELREC) $(OR_ROOT)graph
	-$(DELREC) $(OR_ROOT)gen


# Individual targets.
algorithmslibs: $(DYNAMIC_ALGORITHMS_DEPS) $(STATIC_ALGORITHMS_DEPS)

cpexe: $(CPBINARIES)

cplibs: $(DYNAMIC_CP_DEPS) $(STATIC_CP_DEPS)

lpexe: $(LPBINARIES)

lplibs: $(DYNAMIC_LP_DEPS) $(STATIC_LP_DEPS)

graphlibs: $(DYNAMIC_GRAPH_DEPS) $(STATIC_GRAPH_DEPS)

dimacslibs: $(DYNAMIC_DIMACS_LIBS)

faplibs: $(DYNAMIC_FAP_LIBS)

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OBJS = \
	$(OBJ_DIR)/alldiff_cst.$O\
	$(OBJ_DIR)/assignment.$O\
	$(OBJ_DIR)/assignment.pb.$O\
	$(OBJ_DIR)/ac4r_table.$O\
	$(OBJ_DIR)/collect_variables.$O\
	$(OBJ_DIR)/constraint_solver.$O\
	$(OBJ_DIR)/constraints.$O\
	$(OBJ_DIR)/count_cst.$O\
	$(OBJ_DIR)/default_search.$O\
	$(OBJ_DIR)/demon_profiler.$O\
	$(OBJ_DIR)/demon_profiler.pb.$O\
	$(OBJ_DIR)/dependency_graph.$O\
	$(OBJ_DIR)/deviation.$O\
	$(OBJ_DIR)/diffn.$O\
	$(OBJ_DIR)/element.$O\
	$(OBJ_DIR)/expr_array.$O\
	$(OBJ_DIR)/expr_cst.$O\
	$(OBJ_DIR)/expressions.$O\
	$(OBJ_DIR)/gcc.$O\
	$(OBJ_DIR)/hybrid.$O\
    $(OBJ_DIR)/graph_constraints.$O\
	$(OBJ_DIR)/interval.$O\
	$(OBJ_DIR)/io.$O\
	$(OBJ_DIR)/local_search.$O\
	$(OBJ_DIR)/model.pb.$O\
	$(OBJ_DIR)/model_cache.$O\
	$(OBJ_DIR)/mtsearch.$O\
	$(OBJ_DIR)/nogoods.$O\
	$(OBJ_DIR)/pack.$O\
	$(OBJ_DIR)/range_cst.$O\
	$(OBJ_DIR)/resource.$O\
	$(OBJ_DIR)/sched_constraints.$O\
	$(OBJ_DIR)/sched_expr.$O\
	$(OBJ_DIR)/sched_search.$O\
	$(OBJ_DIR)/search.$O\
	$(OBJ_DIR)/search_limit.pb.$O\
	$(OBJ_DIR)/softgcc.$O\
	$(OBJ_DIR)/table.$O\
	$(OBJ_DIR)/timetabling.$O\
	$(OBJ_DIR)/trace.$O\
	$(OBJ_DIR)/tree_monitor.$O\
	$(OBJ_DIR)/utilities.$O \
	$(OBJ_DIR)/visitor.$O

$(OBJ_DIR)/alldiff_cst.$O:$(SRC_DIR)/constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/alldiff_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Salldiff_cst.$O

$(OBJ_DIR)/assignment.$O:$(SRC_DIR)/constraint_solver/assignment.cc $(GEN_DIR)/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sassignment.$O

$(OBJ_DIR)/assignment.pb.$O:$(GEN_DIR)/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/assignment.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sassignment.pb.$O

$(OBJ_DIR)/ac4r_table.$O:$(SRC_DIR)/constraint_solver/ac4r_table.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/ac4r_table.cc $(OBJ_OUT)$(OBJ_DIR)$Sac4r_table.$O

$(GEN_DIR)/constraint_solver/assignment.pb.cc:$(SRC_DIR)/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/assignment.proto

$(GEN_DIR)/constraint_solver/assignment.pb.h:$(GEN_DIR)/constraint_solver/assignment.pb.cc

$(OBJ_DIR)/collect_variables.$O:$(SRC_DIR)/constraint_solver/collect_variables.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/collect_variables.cc $(OBJ_OUT)$(OBJ_DIR)$Scollect_variables.$O

$(OBJ_DIR)/constraint_solver.$O:$(SRC_DIR)/constraint_solver/constraint_solver.cc $(GEN_DIR)/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraint_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraint_solver.$O

$(OBJ_DIR)/constraints.$O:$(SRC_DIR)/constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sconstraints.$O

$(OBJ_DIR)/count_cst.$O:$(SRC_DIR)/constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/count_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Scount_cst.$O

$(OBJ_DIR)/default_search.$O:$(SRC_DIR)/constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/default_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sdefault_search.$O

$(OBJ_DIR)/demon_profiler.$O:$(SRC_DIR)/constraint_solver/demon_profiler.cc $(GEN_DIR)/constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/demon_profiler.cc $(OBJ_OUT)$(OBJ_DIR)$Sdemon_profiler.$O

$(OBJ_DIR)/demon_profiler.pb.$O:$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sdemon_profiler.pb.$O

$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc:$(SRC_DIR)/constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/demon_profiler.proto

$(GEN_DIR)/constraint_solver/demon_profiler.pb.h:$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc

$(OBJ_DIR)/dependency_graph.$O:$(SRC_DIR)/constraint_solver/dependency_graph.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/dependency_graph.cc $(OBJ_OUT)$(OBJ_DIR)$Sdependency_graph.$O

$(OBJ_DIR)/deviation.$O:$(SRC_DIR)/constraint_solver/deviation.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/deviation.cc $(OBJ_OUT)$(OBJ_DIR)$Sdeviation.$O

$(OBJ_DIR)/diffn.$O:$(SRC_DIR)/constraint_solver/diffn.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/diffn.cc $(OBJ_OUT)$(OBJ_DIR)$Sdiffn.$O

$(OBJ_DIR)/element.$O:$(SRC_DIR)/constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/element.cc $(OBJ_OUT)$(OBJ_DIR)$Selement.$O

$(OBJ_DIR)/expr_array.$O:$(SRC_DIR)/constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_array.cc $(OBJ_OUT)$(OBJ_DIR)$Sexpr_array.$O

$(OBJ_DIR)/expr_cst.$O:$(SRC_DIR)/constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Sexpr_cst.$O

$(OBJ_DIR)/expressions.$O:$(SRC_DIR)/constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expressions.cc $(OBJ_OUT)$(OBJ_DIR)$Sexpressions.$O

$(OBJ_DIR)/gcc.$O:$(SRC_DIR)/constraint_solver/gcc.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/gcc.cc $(OBJ_OUT)$(OBJ_DIR)$Sgcc.$O

$(OBJ_DIR)/graph_constraints.$O:$(SRC_DIR)/constraint_solver/graph_constraints.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/graph_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph_constraints.$O

$(OBJ_DIR)/hybrid.$O:$(SRC_DIR)/constraint_solver/hybrid.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/hybrid.cc $(OBJ_OUT)$(OBJ_DIR)$Shybrid.$O

$(OBJ_DIR)/interval.$O:$(SRC_DIR)/constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/interval.cc $(OBJ_OUT)$(OBJ_DIR)$Sinterval.$O

$(OBJ_DIR)/io.$O:$(SRC_DIR)/constraint_solver/io.cc $(GEN_DIR)/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/io.cc $(OBJ_OUT)$(OBJ_DIR)$Sio.$O

$(OBJ_DIR)/local_search.$O:$(SRC_DIR)/constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/local_search.cc $(OBJ_OUT)$(OBJ_DIR)$Slocal_search.$O

$(OBJ_DIR)/model.pb.$O:$(GEN_DIR)/constraint_solver/model.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/model.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel.pb.$O

$(OBJ_DIR)/model_cache.$O:$(SRC_DIR)/constraint_solver/model_cache.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/model_cache.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_cache.$O

$(GEN_DIR)/constraint_solver/model.pb.cc:$(SRC_DIR)/constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/model.proto

$(GEN_DIR)/constraint_solver/model.pb.h:$(GEN_DIR)/constraint_solver/model.pb.cc $(GEN_DIR)/constraint_solver/search_limit.pb.h

$(OBJ_DIR)/nogoods.$O:$(SRC_DIR)/constraint_solver/nogoods.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/nogoods.cc $(OBJ_OUT)$(OBJ_DIR)$Snogoods.$O

$(OBJ_DIR)/mtsearch.$O:$(SRC_DIR)/constraint_solver/mtsearch.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/mtsearch.cc $(OBJ_OUT)$(OBJ_DIR)$Smtsearch.$O

$(OBJ_DIR)/pack.$O:$(SRC_DIR)/constraint_solver/pack.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/pack.cc $(OBJ_OUT)$(OBJ_DIR)$Spack.$O

$(OBJ_DIR)/range_cst.$O:$(SRC_DIR)/constraint_solver/range_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/range_cst.cc $(OBJ_OUT)$(OBJ_DIR)$Srange_cst.$O

$(OBJ_DIR)/resource.$O:$(SRC_DIR)/constraint_solver/resource.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/resource.cc $(OBJ_OUT)$(OBJ_DIR)$Sresource.$O

$(OBJ_DIR)/sched_constraints.$O:$(SRC_DIR)/constraint_solver/sched_constraints.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Ssched_constraints.$O

$(OBJ_DIR)/sched_expr.$O:$(SRC_DIR)/constraint_solver/sched_expr.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_expr.cc $(OBJ_OUT)$(OBJ_DIR)$Ssched_expr.$O

$(OBJ_DIR)/sched_search.$O:$(SRC_DIR)/constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_search.cc $(OBJ_OUT)$(OBJ_DIR)$Ssched_search.$O

$(OBJ_DIR)/search.$O:$(SRC_DIR)/constraint_solver/search.cc $(GEN_DIR)/constraint_solver/search_limit.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/search.cc $(OBJ_OUT)$(OBJ_DIR)$Ssearch.$O

$(OBJ_DIR)/search_limit.pb.$O:$(GEN_DIR)/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/search_limit.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssearch_limit.pb.$O

$(GEN_DIR)/constraint_solver/search_limit.pb.cc:$(SRC_DIR)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/search_limit.proto

$(GEN_DIR)/constraint_solver/search_limit.pb.h:$(GEN_DIR)/constraint_solver/search_limit.pb.cc

$(OBJ_DIR)/softgcc.$O:$(SRC_DIR)/constraint_solver/softgcc.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/softgcc.cc $(OBJ_OUT)$(OBJ_DIR)$Ssoftgcc.$O

$(OBJ_DIR)/table.$O:$(SRC_DIR)/constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/table.cc $(OBJ_OUT)$(OBJ_DIR)$Stable.$O

$(OBJ_DIR)/timetabling.$O:$(SRC_DIR)/constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/timetabling.cc $(OBJ_OUT)$(OBJ_DIR)$Stimetabling.$O

$(OBJ_DIR)/trace.$O:$(SRC_DIR)/constraint_solver/trace.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/trace.cc $(OBJ_OUT)$(OBJ_DIR)$Strace.$O

$(OBJ_DIR)/tree_monitor.$O:$(SRC_DIR)/constraint_solver/tree_monitor.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/tree_monitor.cc $(OBJ_OUT)$(OBJ_DIR)$Stree_monitor.$O

$(OBJ_DIR)/utilities.$O:$(SRC_DIR)/constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sutilities.$O

$(OBJ_DIR)/visitor.$O:$(SRC_DIR)/constraint_solver/visitor.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/visitor.cc $(OBJ_OUT)$(OBJ_DIR)$Svisitor.$O

$(LIB_DIR)/$(LIBPREFIX)constraint_solver.$(DYNAMIC_LIB_SUFFIX): $(CONSTRAINT_SOLVER_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)constraint_solver.$(DYNAMIC_LIB_SUFFIX) $(CONSTRAINT_SOLVER_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)constraint_solver.$(STATIC_LIB_SUFFIX): $(CONSTRAINT_SOLVER_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)constraint_solver.$(STATIC_LIB_SUFFIX) $(CONSTRAINT_SOLVER_LIB_OBJS)
endif

# Linear Solver Library

LINEAR_SOLVER_LIB_OBJS = \
	$(OBJ_DIR)/cbc_interface.$O \
	$(OBJ_DIR)/clp_interface.$O \
	$(OBJ_DIR)/glpk_interface.$O \
	$(OBJ_DIR)/gurobi_interface.$O \
	$(OBJ_DIR)/linear_solver.$O \
	$(OBJ_DIR)/linear_solver.pb.$O \
	$(OBJ_DIR)/linear_solver2.pb.$O \
	$(OBJ_DIR)/model_exporter.$O \
        $(OBJ_DIR)/proto_tools.$O \
	$(OBJ_DIR)/scip_interface.$O \
	$(OBJ_DIR)/sulum_interface.$O


$(OBJ_DIR)/cbc_interface.$O:$(SRC_DIR)/linear_solver/cbc_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/cbc_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Scbc_interface.$O

$(OBJ_DIR)/clp_interface.$O:$(SRC_DIR)/linear_solver/clp_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/clp_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Sclp_interface.$O

$(OBJ_DIR)/glpk_interface.$O:$(SRC_DIR)/linear_solver/glpk_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/glpk_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Sglpk_interface.$O

$(OBJ_DIR)/gurobi_interface.$O:$(SRC_DIR)/linear_solver/gurobi_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/gurobi_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Sgurobi_interface.$O

$(OBJ_DIR)/linear_solver.$O:$(SRC_DIR)/linear_solver/linear_solver.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h $(GEN_DIR)/linear_solver/linear_solver2.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/linear_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver.$O

$(OBJ_DIR)/linear_solver.pb.$O:$(GEN_DIR)/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver.pb.$O

$(GEN_DIR)/linear_solver/linear_solver.pb.cc:$(SRC_DIR)/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/linear_solver/linear_solver.proto

$(GEN_DIR)/linear_solver/linear_solver.pb.h:$(GEN_DIR)/linear_solver/linear_solver.pb.cc

$(OBJ_DIR)/linear_solver2.pb.$O:$(GEN_DIR)/linear_solver/linear_solver2.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver2.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver2.pb.$O

$(GEN_DIR)/linear_solver/linear_solver2.pb.cc:$(SRC_DIR)/linear_solver/linear_solver2.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/linear_solver/linear_solver2.proto

$(GEN_DIR)/linear_solver/linear_solver2.pb.h:$(GEN_DIR)/linear_solver/linear_solver2.pb.cc

$(OBJ_DIR)/model_exporter.$O:$(SRC_DIR)/linear_solver/model_exporter.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/model_exporter.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_exporter.$O

$(OBJ_DIR)/proto_tools.$O:$(SRC_DIR)/linear_solver/proto_tools.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h $(GEN_DIR)/linear_solver/linear_solver2.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/proto_tools.cc $(OBJ_OUT)$(OBJ_DIR)$Sproto_tools.$O

$(OBJ_DIR)/scip_interface.$O:$(SRC_DIR)/linear_solver/scip_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/scip_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Sscip_interface.$O

$(OBJ_DIR)/sulum_interface.$O:$(SRC_DIR)/linear_solver/sulum_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/sulum_interface.cc $(OBJ_OUT)$(OBJ_DIR)$Ssulum_interface.$O

$(LIB_DIR)/$(LIBPREFIX)linear_solver.$(DYNAMIC_LIB_SUFFIX): $(LINEAR_SOLVER_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)linear_solver.$(DYNAMIC_LIB_SUFFIX) $(LINEAR_SOLVER_LIB_OBJS) $(STATIC_SCIP_LNK)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)linear_solver.$(STATIC_LIB_SUFFIX): $(LINEAR_SOLVER_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)linear_solver.$(STATIC_LIB_SUFFIX) $(LINEAR_SOLVER_LIB_OBJS)
endif

# Util library.

UTIL_LIB_OBJS=\
	$(OBJ_DIR)/bitset.$O \
	$(OBJ_DIR)/cached_log.$O \
	$(OBJ_DIR)/graph_export.$O \
	$(OBJ_DIR)/piecewise_linear_function.$O \
	$(OBJ_DIR)/saturated_arithmetic.$O \
	$(OBJ_DIR)/stats.$O \
	$(OBJ_DIR)/xml_helper.$O

$(OBJ_DIR)/bitset.$O:$(SRC_DIR)/util/bitset.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/bitset.cc $(OBJ_OUT)$(OBJ_DIR)$Sbitset.$O

$(OBJ_DIR)/cached_log.$O:$(SRC_DIR)/util/cached_log.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/cached_log.cc $(OBJ_OUT)$(OBJ_DIR)$Scached_log.$O

$(OBJ_DIR)/graph_export.$O:$(SRC_DIR)/util/graph_export.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/graph_export.cc $(OBJ_OUT)$(OBJ_DIR)$Sgraph_export.$O

$(OBJ_DIR)/piecewise_linear_function.$O:$(SRC_DIR)/util/piecewise_linear_function.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/piecewise_linear_function.cc $(OBJ_OUT)$(OBJ_DIR)$Spiecewise_linear_function.$O

$(OBJ_DIR)/saturated_arithmetic.$O:$(SRC_DIR)/util/saturated_arithmetic.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/saturated_arithmetic.cc $(OBJ_OUT)$(OBJ_DIR)$Ssaturated_arithmetic.$O

$(OBJ_DIR)/stats.$O:$(SRC_DIR)/util/stats.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/stats.cc $(OBJ_OUT)$(OBJ_DIR)$Sstats.$O

$(OBJ_DIR)/xml_helper.$O:$(SRC_DIR)/util/xml_helper.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/xml_helper.cc $(OBJ_OUT)$(OBJ_DIR)$Sxml_helper.$O

$(LIB_DIR)/$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX): $(UTIL_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX) $(UTIL_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX): $(UTIL_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX) $(UTIL_LIB_OBJS)
endif

# Graph library.

GRAPH_LIB_OBJS=\
	$(OBJ_DIR)/simple_assignment.$O \
	$(OBJ_DIR)/linear_assignment.$O \
	$(OBJ_DIR)/cliques.$O \
	$(OBJ_DIR)/connectivity.$O \
	$(OBJ_DIR)/max_flow.$O \
	$(OBJ_DIR)/min_cost_flow.$O

$(OBJ_DIR)/linear_assignment.$O:$(SRC_DIR)/graph/linear_assignment.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/linear_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment.$O

$(OBJ_DIR)/simple_assignment.$O:$(SRC_DIR)/graph/assignment.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Ssimple_assignment.$O

$(OBJ_DIR)/cliques.$O:$(SRC_DIR)/graph/cliques.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/cliques.cc $(OBJ_OUT)$(OBJ_DIR)$Scliques.$O

$(OBJ_DIR)/connectivity.$O:$(SRC_DIR)/graph/connectivity.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/connectivity.cc $(OBJ_OUT)$(OBJ_DIR)$Sconnectivity.$O

$(OBJ_DIR)/max_flow.$O:$(SRC_DIR)/graph/max_flow.cc $(SRC_DIR)/util/stats.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/max_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Smax_flow.$O

$(OBJ_DIR)/min_cost_flow.$O:$(SRC_DIR)/graph/min_cost_flow.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/min_cost_flow.cc $(OBJ_OUT)$(OBJ_DIR)$Smin_cost_flow.$O

$(LIB_DIR)/$(LIBPREFIX)graph.$(DYNAMIC_LIB_SUFFIX): $(GRAPH_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)graph.$(DYNAMIC_LIB_SUFFIX) $(GRAPH_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)graph.$(STATIC_LIB_SUFFIX): $(GRAPH_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)graph.$(STATIC_LIB_SUFFIX) $(GRAPH_LIB_OBJS)
endif

# Shortestpaths library.

SHORTESTPATHS_LIB_OBJS=\
	$(OBJ_DIR)/bellman_ford.$O \
	$(OBJ_DIR)/dijkstra.$O \
	$(OBJ_DIR)/shortestpaths.$O

$(OBJ_DIR)/bellman_ford.$O:$(SRC_DIR)/graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/bellman_ford.cc $(OBJ_OUT)$(OBJ_DIR)$Sbellman_ford.$O

$(OBJ_DIR)/dijkstra.$O:$(SRC_DIR)/graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/dijkstra.cc $(OBJ_OUT)$(OBJ_DIR)$Sdijkstra.$O

$(OBJ_DIR)/shortestpaths.$O:$(SRC_DIR)/graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/shortestpaths.cc $(OBJ_OUT)$(OBJ_DIR)$Sshortestpaths.$O

$(LIB_DIR)/$(LIBPREFIX)shortestpaths.$(DYNAMIC_LIB_SUFFIX): $(SHORTESTPATHS_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)shortestpaths.$(DYNAMIC_LIB_SUFFIX) $(SHORTESTPATHS_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)shortestpaths.$(STATIC_LIB_SUFFIX): $(SHORTESTPATHS_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)shortestpaths.$(STATIC_LIB_SUFFIX) $(SHORTESTPATHS_LIB_OBJS)
endif

# Routing library.

ROUTING_LIB_OBJS=\
	$(OBJ_DIR)/routing.$O \
	$(OBJ_DIR)/routing_search.$O

$(OBJ_DIR)/routing.$O:$(SRC_DIR)/constraint_solver/routing.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing.cc $(OBJ_OUT)$(OBJ_DIR)$Srouting.$O

$(OBJ_DIR)/routing_search.$O:$(SRC_DIR)/constraint_solver/routing_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing_search.cc $(OBJ_OUT)$(OBJ_DIR)$Srouting_search.$O

$(LIB_DIR)/$(LIBPREFIX)routing.$(DYNAMIC_LIB_SUFFIX): $(ROUTING_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)routing.$(DYNAMIC_LIB_SUFFIX) $(ROUTING_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)routing.$(STATIC_LIB_SUFFIX): $(ROUTING_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)routing.$(STATIC_LIB_SUFFIX) $(ROUTING_LIB_OBJS)
endif

# Algorithms library.

ALGORITHMS_LIB_OBJS=\
	$(OBJ_DIR)/hungarian.$O \
	$(OBJ_DIR)/knapsack_solver.$O

$(OBJ_DIR)/hungarian.$O:$(SRC_DIR)/algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/hungarian.cc $(OBJ_OUT)$(OBJ_DIR)$Shungarian.$O

$(OBJ_DIR)/knapsack_solver.$O:$(SRC_DIR)/algorithms/knapsack_solver.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/knapsack_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Sknapsack_solver.$O

$(LIB_DIR)/$(LIBPREFIX)algorithms.$(DYNAMIC_LIB_SUFFIX): $(ALGORITHMS_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)algorithms.$(DYNAMIC_LIB_SUFFIX) $(ALGORITHMS_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)algorithms.$(STATIC_LIB_SUFFIX): $(ALGORITHMS_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)algorithms.$(STATIC_LIB_SUFFIX) $(ALGORITHMS_LIB_OBJS)
endif

# Base library.

BASE_LIB_OBJS=\
	$(OBJ_DIR)/bitmap.$O\
	$(OBJ_DIR)/callback.$O\
	$(OBJ_DIR)/file.$O\
	$(OBJ_DIR)/filelinereader.$O\
	$(OBJ_DIR)/join.$O\
	$(OBJ_DIR)/logging.$O\
	$(OBJ_DIR)/mutex.$O\
	$(OBJ_DIR)/random.$O\
	$(OBJ_DIR)/recordio.$O\
	$(OBJ_DIR)/threadpool.$O\
	$(OBJ_DIR)/split.$O\
	$(OBJ_DIR)/stringpiece.$O\
	$(OBJ_DIR)/stringprintf.$O\
	$(OBJ_DIR)/sysinfo.$O\
	$(OBJ_DIR)/timer.$O

$(OBJ_DIR)/bitmap.$O:$(SRC_DIR)/base/bitmap.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/bitmap.cc $(OBJ_OUT)$(OBJ_DIR)$Sbitmap.$O
$(OBJ_DIR)/callback.$O:$(SRC_DIR)/base/callback.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/callback.cc $(OBJ_OUT)$(OBJ_DIR)$Scallback.$O
$(OBJ_DIR)/file.$O:$(SRC_DIR)/base/file.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/file.cc $(OBJ_OUT)$(OBJ_DIR)$Sfile.$O
$(OBJ_DIR)/filelinereader.$O:$(SRC_DIR)/base/filelinereader.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/filelinereader.cc $(OBJ_OUT)$(OBJ_DIR)$Sfilelinereader.$O
$(OBJ_DIR)/logging.$O:$(SRC_DIR)/base/logging.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/logging.cc $(OBJ_OUT)$(OBJ_DIR)$Slogging.$O
$(OBJ_DIR)/mutex.$O:$(SRC_DIR)/base/mutex.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/mutex.cc $(OBJ_OUT)$(OBJ_DIR)$Smutex.$O
$(OBJ_DIR)/join.$O:$(SRC_DIR)/base/join.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/join.cc $(OBJ_OUT)$(OBJ_DIR)$Sjoin.$O
$(OBJ_DIR)/random.$O:$(SRC_DIR)/base/random.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/random.cc $(OBJ_OUT)$(OBJ_DIR)$Srandom.$O
$(OBJ_DIR)/recordio.$O:$(SRC_DIR)/base/recordio.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/recordio.cc $(OBJ_OUT)$(OBJ_DIR)$Srecordio.$O
$(OBJ_DIR)/threadpool.$O:$(SRC_DIR)/base/threadpool.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/threadpool.cc $(OBJ_OUT)$(OBJ_DIR)$Sthreadpool.$O
$(OBJ_DIR)/split.$O:$(SRC_DIR)/base/split.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/split.cc $(OBJ_OUT)$(OBJ_DIR)$Ssplit.$O
$(OBJ_DIR)/stringpiece.$O:$(SRC_DIR)/base/stringpiece.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringpiece.cc $(OBJ_OUT)$(OBJ_DIR)$Sstringpiece.$O
$(OBJ_DIR)/stringprintf.$O:$(SRC_DIR)/base/stringprintf.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringprintf.cc $(OBJ_OUT)$(OBJ_DIR)$Sstringprintf.$O
$(OBJ_DIR)/sysinfo.$O:$(SRC_DIR)/base/sysinfo.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/sysinfo.cc $(OBJ_OUT)$(OBJ_DIR)$Ssysinfo.$O
$(OBJ_DIR)/timer.$O:$(SRC_DIR)/base/timer.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/timer.cc $(OBJ_OUT)$(OBJ_DIR)$Stimer.$O

$(LIB_DIR)/$(LIBPREFIX)base.$(DYNAMIC_LIB_SUFFIX): $(BASE_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)base.$(DYNAMIC_LIB_SUFFIX) $(BASE_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)base.$(STATIC_LIB_SUFFIX): $(BASE_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)base.$(STATIC_LIB_SUFFIX) $(BASE_LIB_OBJS)
endif

# DIMACS challenge problem format library

DIMACS_LIB_OBJS=\
	$(OBJ_DIR)/parse_dimacs_assignment.$O

$(OBJ_DIR)/parse_dimacs_assignment.$O:$(EX_DIR)/cpp/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/parse_dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sparse_dimacs_assignment.$O

$(LIB_DIR)/$(LIBPREFIX)dimacs.$(DYNAMIC_LIB_SUFFIX): $(DIMACS_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)dimacs.$(DYNAMIC_LIB_SUFFIX) $(DIMACS_LIB_OBJS)

# FAP challenge problem format library

FAP_LIB_OBJS=\
	$(OBJ_DIR)/fap_model_printer.$O\
	$(OBJ_DIR)/fap_parser.$O\
	$(OBJ_DIR)/fap_utilities.$O

$(OBJ_DIR)/fap_model_printer.$O:$(EX_DIR)/cpp/fap_model_printer.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/fap_model_printer.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_model_printer.$O
$(OBJ_DIR)/fap_parser.$O:$(EX_DIR)/cpp/fap_parser.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/fap_parser.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_parser.$O
$(OBJ_DIR)/fap_utilities.$O:$(EX_DIR)/cpp/fap_utilities.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/fap_utilities.cc $(OBJ_OUT)$(OBJ_DIR)$Sfap_utilities.$O

$(LIB_DIR)/$(LIBPREFIX)fap.$(DYNAMIC_LIB_SUFFIX): $(FAP_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)fap.$(DYNAMIC_LIB_SUFFIX) $(FAP_LIB_OBJS)

# Flatzinc Support

FLATZINC_LIB_OBJS=\
	$(OBJ_DIR)/booleans.$O\
	$(OBJ_DIR)/flatzinc.$O\
	$(OBJ_DIR)/flatzinc_constraints.$O\
	$(OBJ_DIR)/fz_search.$O\
	$(OBJ_DIR)/flatzinc.yy.$O\
	$(OBJ_DIR)/flatzinc.tab.$O\
	$(OBJ_DIR)/parser.$O\
	$(OBJ_DIR)/registry.$O

ifeq ($(SYSTEM),win)
$(GEN_DIR)/flatzinc/flatzinc.yy.cc: $(SRC_DIR)/flatzinc/win/flatzinc.yy.cc
	copy $(SRC_DIR)\\flatzinc\\win\\flatzinc.yy.cc $(GEN_DIR)\\flatzinc\\flatzinc.yy.cc

$(GEN_DIR)/flatzinc/flatzinc.tab.cc: $(SRC_DIR)/flatzinc/win/flatzinc.tab.cc
	copy $(SRC_DIR)\\flatzinc\\win\\flatzinc.tab.cc $(GEN_DIR)\\flatzinc\\flatzinc.tab.cc

$(GEN_DIR)/flatzinc/flatzinc.tab.hh: $(SRC_DIR)/flatzinc/win/flatzinc.tab.hh
	copy $(SRC_DIR)\\flatzinc\\win\\flatzinc.tab.hh $(GEN_DIR)\\flatzinc\\flatzinc.tab.hh

else
$(GEN_DIR)/flatzinc/flatzinc.yy.cc: $(SRC_DIR)/flatzinc/flatzinc.lex
	flex -o$(GEN_DIR)/flatzinc/flatzinc.yy.cc $(SRC_DIR)/flatzinc/flatzinc.lex

$(GEN_DIR)/flatzinc/flatzinc.tab.cc: $(SRC_DIR)/flatzinc/flatzinc.yy
	bison -t -o $(GEN_DIR)/flatzinc/flatzinc.tab.cc -d $<

$(GEN_DIR)/flatzinc/flatzinc.tab.hh: $(GEN_DIR)/flatzinc/flatzinc.tab.cc

win_parser: $(SRC_DIR)/flatzinc/win/flatzinc.yy.cc $(SRC_DIR)/flatzinc/win/flatzinc.tab.cc $(SRC_DIR)/flatzinc/win/flatzinc.tab.hh

$(SRC_DIR)/flatzinc/win/flatzinc.yy.cc: $(SRC_DIR)/flatzinc/flatzinc.lex
	flex -nounistd -o$(SRC_DIR)/flatzinc/win/flatzinc.yy.cc $(SRC_DIR)/flatzinc/flatzinc.lex

$(SRC_DIR)/flatzinc/win/flatzinc.tab.cc: $(SRC_DIR)/flatzinc/flatzinc.yy
	bison -t -o $(SRC_DIR)/flatzinc/win/flatzinc.tab.cc -d $<

$(SRC_DIR)/flatzinc/win/flatzinc.tab.hh: $(SRC_DIR)/flatzinc/win/flatzinc.tab.cc
endif

$(OBJ_DIR)/booleans.$O:$(SRC_DIR)/flatzinc/booleans.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sbooleans.cc $(OBJ_OUT)$(OBJ_DIR)$Sbooleans.$O
$(OBJ_DIR)/flatzinc.$O:$(SRC_DIR)/flatzinc/flatzinc.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sflatzinc.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc.$O
$(OBJ_DIR)/flatzinc_constraints.$O:$(SRC_DIR)/flatzinc/flatzinc_constraints.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sflatzinc_constraints.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc_constraints.$O
$(OBJ_DIR)/fz_search.$O:$(SRC_DIR)/flatzinc/fz_search.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sfz_search.cc $(OBJ_OUT)$(OBJ_DIR)$Sfz_search.$O
$(OBJ_DIR)/flatzinc.yy.$O:$(GEN_DIR)/flatzinc/flatzinc.yy.cc $(GEN_DIR)/flatzinc/flatzinc.tab.hh $(SRC_DIR)/flatzinc/parser.h $(SRC_DIR)/flatzinc/flatzinc.h
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sflatzinc$Sflatzinc.yy.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc.yy.$O
$(OBJ_DIR)/parser.$O:$(SRC_DIR)/flatzinc/parser.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparser.cc $(OBJ_OUT)$(OBJ_DIR)$Sparser.$O
$(OBJ_DIR)/flatzinc.tab.$O:$(GEN_DIR)/flatzinc/flatzinc.tab.cc $(SRC_DIR)/flatzinc/flatzinc.h
	$(CCC) $(CFLAGS) -c $(GEN_DIR)$Sflatzinc$Sflatzinc.tab.cc $(OBJ_OUT)$(OBJ_DIR)$Sflatzinc.tab.$O
$(OBJ_DIR)/registry.$O:$(SRC_DIR)/flatzinc/registry.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sregistry.cc $(OBJ_OUT)$(OBJ_DIR)$Sregistry.$O

$(LIB_DIR)/$(LIBPREFIX)fz.$(DYNAMIC_LIB_SUFFIX): $(FLATZINC_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)fz.$(DYNAMIC_LIB_SUFFIX) $(FLATZINC_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)fz.$(STATIC_LIB_SUFFIX): $(FLATZINC_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)fz.$(STATIC_LIB_SUFFIX) $(FLATZINC_LIB_OBJS)
endif

$(OBJ_DIR)/fz.$O:$(SRC_DIR)/flatzinc/fz.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sfz.cc $(OBJ_OUT)$(OBJ_DIR)$Sfz.$O

fz: $(BIN_DIR)/fz$E

$(BIN_DIR)/fz$E: $(OBJ_DIR)/fz.$O $(STATIC_FLATZINC_DEPS)
	$(CCC) $(CFLAGS) $(OBJ_DIR)/fz.$O $(STATIC_FZ) $(STATIC_FLATZINC_LNK) $(FZ_STATIC) $(STATIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sfz$E

ifeq ($(PLATFORM),LINUX)
$(BIN_DIR)/fzn_or-tools: $(OBJ_DIR)/fz.$O $(STATIC_FLATZINC_DEPS)
	$(CCC) -static -static-libgcc $(CFLAGS) $(OBJ_DIR)/fz.$O $(STATIC_FZ) $(STATIC_FLATZINC_LNK) $(FZ_STATIC) $(STATIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sfzn_or-tools
endif


# Flow and linear assignment cpp

$(OBJ_DIR)/linear_assignment_api.$O:$(EX_DIR)/cpp/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_assignment_api.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_assignment_api.$O

$(BIN_DIR)/linear_assignment_api$E: $(DYNAMIC_GRAPH_DEPS) $(OBJ_DIR)/linear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_assignment_api.$O $(DYNAMIC_GRAPH_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_assignment_api$E

$(OBJ_DIR)/flow_api.$O:$(EX_DIR)/cpp/flow_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flow_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sflow_api.$O

$(BIN_DIR)/flow_api$E: $(DYNAMIC_GRAPH_DEPS) $(OBJ_DIR)/flow_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flow_api.$O $(DYNAMIC_GRAPH_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflow_api$E

$(OBJ_DIR)/dimacs_assignment.$O:$(EX_DIR)/cpp/dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dimacs_assignment.cc $(OBJ_OUT)$(OBJ_DIR)$Sdimacs_assignment.$O

$(BIN_DIR)/dimacs_assignment$E: $(DYNAMIC_DIMACS_DEPS) $(OBJ_DIR)/dimacs_assignment.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dimacs_assignment.$O $(DYNAMIC_DIMACS_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdimacs_assignment$E

# Pure CP and Routing Examples

$(OBJ_DIR)/costas_array.$O: $(EX_DIR)/cpp/costas_array.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/costas_array.cc $(OBJ_OUT)$(OBJ_DIR)$Scostas_array.$O

$(BIN_DIR)/costas_array$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/costas_array.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/costas_array.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scostas_array$E

$(OBJ_DIR)/cryptarithm.$O:$(EX_DIR)/cpp/cryptarithm.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cryptarithm.cc $(OBJ_OUT)$(OBJ_DIR)$Scryptarithm.$O

$(BIN_DIR)/cryptarithm$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/cryptarithm.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cryptarithm.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scryptarithm$E

$(OBJ_DIR)/cvrptw.$O: $(EX_DIR)/cpp/cvrptw.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw.cc $(OBJ_OUT)$(OBJ_DIR)$Scvrptw.$O

$(BIN_DIR)/cvrptw$E: $(DYNAMIC_ROUTING_DEPS) $(OBJ_DIR)/cvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Scvrptw$E

$(OBJ_DIR)/dobble_ls.$O:$(EX_DIR)/cpp/dobble_ls.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dobble_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sdobble_ls.$O

$(BIN_DIR)/dobble_ls$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/dobble_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dobble_ls.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sdobble_ls$E

$(OBJ_DIR)/flexible_jobshop.$O:$(EX_DIR)/cpp/flexible_jobshop.cc $(SRC_DIR)/constraint_solver/constraint_solver.h $(EX_DIR)/cpp/flexible_jobshop.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flexible_jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sflexible_jobshop.$O

$(BIN_DIR)/flexible_jobshop$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/flexible_jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flexible_jobshop.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sflexible_jobshop$E

$(OBJ_DIR)/golomb.$O:$(EX_DIR)/cpp/golomb.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/golomb.cc $(OBJ_OUT)$(OBJ_DIR)$Sgolomb.$O

$(BIN_DIR)/golomb$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/golomb.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/golomb.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sgolomb$E

$(OBJ_DIR)/jobshop.$O:$(EX_DIR)/cpp/jobshop.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop.$O

$(BIN_DIR)/jobshop$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop$E

$(OBJ_DIR)/jobshop_ls.$O:$(EX_DIR)/cpp/jobshop_ls.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_ls.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_ls.$O

$(BIN_DIR)/jobshop_ls$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/jobshop_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_ls.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_ls$E

$(OBJ_DIR)/jobshop_earlytardy.$O:$(EX_DIR)/cpp/jobshop_earlytardy.cc $(SRC_DIR)/constraint_solver/constraint_solver.h $(EX_DIR)/cpp/jobshop_earlytardy.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_earlytardy.cc $(OBJ_OUT)$(OBJ_DIR)$Sjobshop_earlytardy.$O

$(BIN_DIR)/jobshop_earlytardy$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/jobshop_earlytardy.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_earlytardy.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sjobshop_earlytardy$E

$(OBJ_DIR)/magic_square.$O:$(EX_DIR)/cpp/magic_square.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/magic_square.cc $(OBJ_OUT)$(OBJ_DIR)$Smagic_square.$O

$(BIN_DIR)/magic_square$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/magic_square.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/magic_square.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smagic_square$E

$(OBJ_DIR)/model_util.$O:$(EX_DIR)/cpp/model_util.cc $(GEN_DIR)/constraint_solver/model.pb.h $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/model_util.cc $(OBJ_OUT)$(OBJ_DIR)$Smodel_util.$O

$(BIN_DIR)/model_util$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/model_util.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/model_util.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smodel_util$E

$(OBJ_DIR)/multidim_knapsack.$O:$(EX_DIR)/cpp/multidim_knapsack.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/multidim_knapsack.cc $(OBJ_OUT)$(OBJ_DIR)$Smultidim_knapsack.$O

$(BIN_DIR)/multidim_knapsack$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/multidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/multidim_knapsack.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smultidim_knapsack$E

$(OBJ_DIR)/network_routing.$O:$(EX_DIR)/cpp/network_routing.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/network_routing.cc $(OBJ_OUT)$(OBJ_DIR)$Snetwork_routing.$O

$(BIN_DIR)/network_routing$E: $(DYNAMIC_CP_DEPS) $(DYNAMIC_GRAPH_DEPS) $(OBJ_DIR)/network_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/network_routing.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_GRAPH_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snetwork_routing$E

$(OBJ_DIR)/nqueens.$O: $(EX_DIR)/cpp/nqueens.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/nqueens.cc $(OBJ_OUT)$(OBJ_DIR)$Snqueens.$O

$(BIN_DIR)/nqueens$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/nqueens.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Snqueens$E

$(OBJ_DIR)/pdptw.$O: $(EX_DIR)/cpp/pdptw.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/pdptw.cc $(OBJ_OUT)$(OBJ_DIR)$Spdptw.$O

$(BIN_DIR)/pdptw$E: $(DYNAMIC_ROUTING_DEPS) $(OBJ_DIR)/pdptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/pdptw.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Spdptw$E

$(OBJ_DIR)/sports_scheduling.$O:$(EX_DIR)/cpp/sports_scheduling.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/sports_scheduling.cc $(OBJ_OUT)$(OBJ_DIR)$Ssports_scheduling.$O

$(BIN_DIR)/sports_scheduling$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/sports_scheduling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/sports_scheduling.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssports_scheduling$E

$(OBJ_DIR)/tsp.$O: $(EX_DIR)/cpp/tsp.cc $(SRC_DIR)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/tsp.cc $(OBJ_OUT)$(OBJ_DIR)$Stsp.$O

$(BIN_DIR)/tsp$E: $(DYNAMIC_ROUTING_DEPS) $(OBJ_DIR)/tsp.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/tsp.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Stsp$E

# CP tests.

$(OBJ_DIR)/bug_pack.$O:$(EX_DIR)/tests/bug_pack.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_pack.cc $(OBJ_OUT)$(OBJ_DIR)$Sbug_pack.$O

$(BIN_DIR)/bug_pack$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/bug_pack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_pack.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sbug_pack$E

$(OBJ_DIR)/bug_fz1.$O:$(EX_DIR)/tests/bug_fz1.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_fz1.cc $(OBJ_OUT)$(OBJ_DIR)$Sbug_fz1.$O

$(BIN_DIR)/bug_fz1$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/bug_fz1.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_fz1.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sbug_fz1$E

$(OBJ_DIR)/mtsearch_test.$O:$(EX_DIR)/tests/mtsearch_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/mtsearch_test.cc $(OBJ_OUT)$(OBJ_DIR)$Smtsearch_test.$O

$(BIN_DIR)/mtsearch_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/mtsearch_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/mtsearch_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smtsearch_test$E

$(OBJ_DIR)/ac4r_table_test.$O:$(EX_DIR)/tests/ac4r_table_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/ac4r_table_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sac4r_table_test.$O

$(BIN_DIR)/ac4r_table_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/ac4r_table_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ac4r_table_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sac4r_table_test$E

$(OBJ_DIR)/gcc_test.$O:$(EX_DIR)/tests/gcc_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/gcc_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sgcc_test.$O

$(BIN_DIR)/gcc_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/gcc_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/gcc_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sgcc_test$E

$(OBJ_DIR)/min_max_test.$O:$(EX_DIR)/tests/min_max_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/min_max_test.cc $(OBJ_OUT)$(OBJ_DIR)$Smin_max_test.$O

$(BIN_DIR)/min_max_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/min_max_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/min_max_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Smin_max_test$E

$(OBJ_DIR)/visitor_test.$O:$(EX_DIR)/tests/visitor_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/visitor_test.cc $(OBJ_OUT)$(OBJ_DIR)$Svisitor_test.$O

$(BIN_DIR)/visitor_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/visitor_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/visitor_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Svisitor_test$E

$(OBJ_DIR)/boolean_test.$O:$(EX_DIR)/tests/boolean_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/boolean_test.cc $(OBJ_OUT)$(OBJ_DIR)$Sboolean_test.$O

$(BIN_DIR)/boolean_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/boolean_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/boolean_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sboolean_test$E

$(OBJ_DIR)/ls_api.$O:$(EX_DIR)/cpp/ls_api.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/ls_api.cc $(OBJ_OUT)$(OBJ_DIR)$Sls_api.$O

$(BIN_DIR)/ls_api$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/ls_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ls_api.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sls_api$E

$(OBJ_DIR)/cpp11_test.$O:$(EX_DIR)/tests/cpp11_test.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/cpp11_test.cc $(OBJ_OUT)$(OBJ_DIR)$Scpp11_test.$O

$(BIN_DIR)/cpp11_test$E: $(OBJ_DIR)/cpp11_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cpp11_test.$O $(EXE_OUT)$(BIN_DIR)$Scpp11_test$E

# Frequency Assignment Problem

$(OBJ_DIR)/frequency_assignment_problem.$O:$(EX_DIR)/cpp/frequency_assignment_problem.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/frequency_assignment_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Sfrequency_assignment_problem.$O

$(BIN_DIR)/frequency_assignment_problem$E: $(DYNAMIC_FAP_DEPS) $(OBJ_DIR)/frequency_assignment_problem.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/frequency_assignment_problem.$O $(DYNAMIC_FAP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sfrequency_assignment_problem$E

# Linear Programming Examples

$(OBJ_DIR)/strawberry_fields_with_column_generation.$O: $(EX_DIR)/cpp/strawberry_fields_with_column_generation.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/strawberry_fields_with_column_generation.cc $(OBJ_OUT)$(OBJ_DIR)$Sstrawberry_fields_with_column_generation.$O

$(BIN_DIR)/strawberry_fields_with_column_generation$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sstrawberry_fields_with_column_generation$E

$(OBJ_DIR)/linear_programming.$O: $(EX_DIR)/cpp/linear_programming.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_programming.$O

$(BIN_DIR)/linear_programming$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/linear_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_programming.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_programming$E

$(OBJ_DIR)/linear_solver_protocol_buffers.$O: $(EX_DIR)/cpp/linear_solver_protocol_buffers.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_solver_protocol_buffers.cc $(OBJ_OUT)$(OBJ_DIR)$Slinear_solver_protocol_buffers.$O

$(BIN_DIR)/linear_solver_protocol_buffers$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Slinear_solver_protocol_buffers$E

$(OBJ_DIR)/integer_programming.$O: $(EX_DIR)/cpp/integer_programming.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/integer_programming.cc $(OBJ_OUT)$(OBJ_DIR)$Sinteger_programming.$O

$(BIN_DIR)/integer_programming$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/integer_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/integer_programming.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Sinteger_programming$E

# Sat solver

SAT_LIB_OBJS = \
        $(OBJ_DIR)/boolean_problem.pb.$O\
	$(OBJ_DIR)/boolean_problem.$O\
	$(OBJ_DIR)/pb_constraint.$O\
	$(OBJ_DIR)/sat_conflict.$O\
	$(OBJ_DIR)/sat_parameters.pb.$O\
	$(OBJ_DIR)/sat_solver.$O\

sat: bin/sat_runner$E

satlibs: $(DYNAMIC_SAT_DEPS) $(STATIC_SAT_DEPS)

$(OBJ_DIR)/sat_solver.$O: $(SRC_DIR)/sat/sat_solver.cc $(SRC_DIR)/sat/sat_solver.h  $(SRC_DIR)/sat/sat_base.h $(GEN_DIR)/sat/sat_parameters.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/sat_solver.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat_solver.$O

$(OBJ_DIR)/sat_conflict.$O: $(SRC_DIR)/sat/sat_conflict.cc $(SRC_DIR)/sat/sat_solver.h  $(SRC_DIR)/sat/sat_base.h $(GEN_DIR)/sat/sat_parameters.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/sat_conflict.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat_conflict.$O

$(OBJ_DIR)/boolean_problem.$O: $(SRC_DIR)/sat/boolean_problem.cc  $(SRC_DIR)/sat/boolean_problem.h $(GEN_DIR)/sat/boolean_problem.pb.h  $(SRC_DIR)/sat/sat_solver.h  $(SRC_DIR)/sat/sat_base.h $(GEN_DIR)/sat/sat_parameters.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/boolean_problem.cc $(OBJ_OUT)$(OBJ_DIR)$Sboolean_problem.$O

$(GEN_DIR)/sat/boolean_problem.pb.cc: $(SRC_DIR)/sat/boolean_problem.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/sat/boolean_problem.proto

$(GEN_DIR)/sat/boolean_problem.pb.h: $(GEN_DIR)/sat/boolean_problem.pb.cc

$(OBJ_DIR)/boolean_problem.pb.$O: $(GEN_DIR)/sat/boolean_problem.pb.cc $(GEN_DIR)/sat/boolean_problem.pb.h
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/sat/boolean_problem.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Sboolean_problem.pb.$O

$(OBJ_DIR)/pb_constraint.$O: $(SRC_DIR)/sat/pb_constraint.cc $(SRC_DIR)/sat/sat_base.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/sat/pb_constraint.cc $(OBJ_OUT)$(OBJ_DIR)$Spb_constraint.$O

$(GEN_DIR)/sat/sat_parameters.pb.cc: $(SRC_DIR)/sat/sat_parameters.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/sat/sat_parameters.proto

$(GEN_DIR)/sat/sat_parameters.pb.h: $(GEN_DIR)/sat/sat_parameters.pb.cc

$(OBJ_DIR)/sat_parameters.pb.$O: $(GEN_DIR)/sat/sat_parameters.pb.cc $(GEN_DIR)/sat/sat_parameters.pb.h
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/sat/sat_parameters.pb.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat_parameters.pb.$O

$(LIB_DIR)/$(LIBPREFIX)sat.$(DYNAMIC_LIB_SUFFIX): $(SAT_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)sat.$(DYNAMIC_LIB_SUFFIX) $(SAT_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)sat.$(STATIC_LIB_SUFFIX): $(SAT_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)sat.$(STATIC_LIB_SUFFIX) $(SAT_LIB_OBJS)
endif

$(OBJ_DIR)/sat_runner.$O:$(EX_DIR)/cpp/sat_runner.cc $(SRC_DIR)/sat/sat_solver.h $(EX_DIR)/cpp/opb_reader.h $(EX_DIR)/cpp/sat_cnf_reader.h $(GEN_DIR)/sat/sat_parameters.pb.h  $(GEN_DIR)/sat/boolean_problem.pb.h  $(SRC_DIR)/sat/boolean_problem.h  $(SRC_DIR)/sat/sat_base.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp$Ssat_runner.cc $(OBJ_OUT)$(OBJ_DIR)$Ssat_runner.$O

$(BIN_DIR)/sat_runner$E: $(DYNAMIC_SAT_DEPS) $(OBJ_DIR)/sat_runner.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)$Ssat_runner.$O $(DYNAMIC_SAT_LNK) $(DYNAMIC_LD_FLAGS) $(EXE_OUT)$(BIN_DIR)$Ssat_runner$E

# Target for archives

ifeq "$(SYSTEM)" "win"
$(LIB_DIR)/ortools.lib: $(STATIC_ROUTING_DEPS)
	lib /out:$(LIB_DIR)/ortools.lib  $(STATIC_ROUTING_LNK) $(STATIC_LD_FLAGS)


cc_archive: $(LIB_DIR)/$(LIBPREFIX)ortools.$(DYNAMIC_LIB_SUFFIX)
	-$(DELREC) temp
	mkdir temp
	mkdir temp\\or-tools.$(PORT)
	mkdir temp\\or-tools.$(PORT)\\bin
	mkdir temp\\or-tools.$(PORT)\\examples
	mkdir temp\\or-tools.$(PORT)\\include
	mkdir temp\\or-tools.$(PORT)\\include\\algorithms
	mkdir temp\\or-tools.$(PORT)\\include\\base
	mkdir temp\\or-tools.$(PORT)\\include\\constraint_solver
	mkdir temp\\or-tools.$(PORT)\\include\\gflags
	mkdir temp\\or-tools.$(PORT)\\include\\gflags
	mkdir temp\\or-tools.$(PORT)\\include\\google
	mkdir temp\\or-tools.$(PORT)\\include\\linear_solver
	mkdir temp\\or-tools.$(PORT)\\include\\util
	mkdir temp\\or-tools.$(PORT)\\lib
	mkdir temp\\or-tools.$(PORT)\\objs
	copy LICENSE-2.0.txt temp\\or-tools.$(PORT)
	copy tools\\README.cc temp\\or-tools.$(PORT)\\README
	copy tools\\Makefile.cc temp\\or-tools.$(PORT)\\Makefile
	copy lib\\ortools.lib temp\\or-tools.$(PORT)\\lib
	copy examples\\cpp\\*.cc temp\\or-tools.$(PORT)\\examples
	copy examples\\cpp\\*.h temp\\or-tools.$(PORT)\\examples
	copy src\\algorithms\\*.h temp\\or-tools.$(PORT)\\include\\algorithms
	copy src\\base\\*.h temp\\or-tools.$(PORT)\\include\\base
	copy src\\constraint_solver\\*.h temp\\or-tools.$(PORT)\\include\\constraint_solver
	copy src\\gen\\constraint_solver\\*.pb.h temp\\or-tools.$(PORT)\\include\\constraint_solver
	copy src\\graph\\*.h temp\\or-tools.$(PORT)\\include\\graph
	copy src\\linear_solver\\*.h temp\\or-tools.$(PORT)\\include\\linear_solver
	copy src\\gen\\linear_solver\\*.pb.h temp\\or-tools.$(PORT)\\include\\linear_solver
	copy src\\util\\*.h temp\\or-tools.$(PORT)\\include\\util
	cd temp\\or-tools.$(PORT)\\include && ..\..\..\tools\tar.exe -C ..\\..\\..\\dependencies\\install\\include -c -v gflags | ..\..\..\tools\tar.exe xvm
	cd temp\\or-tools.$(PORT)\\include && ..\..\..\tools\tar.exe -C ..\\..\\..\\dependencies\\install\\include -c -v google | ..\..\..\tools\tar.exe xvm
	cd temp\\or-tools.$(PORT)\\include && ..\..\..\tools\tar.exe -C ..\\..\\..\\dependencies\\install\\include -c -v sparsehash | ..\..\..\tools\tar.exe xvm
	cd temp && ..\tools\zip.exe -r ..\Google.OrTools.cc.$(PORT).$(SVNVERSION).zip or-tools.$(PORT)
	-$(DELREC) temp
else
$(LIB_DIR)/$(LIBPREFIX)ortools.$(DYNAMIC_LIB_SUFFIX): $(CONSTRAINT_SOLVER_LIB_OBJS) $(LINEAR_SOLVER_LIB_OBJS) $(UTIL_LIB_OBJS) $(GRAPH_LIB_OBJS) $(SHORTESTPATHS_LIB_OBJS) $(ROUTING_LIB_OBJS) $(ALGORITHMS_LIB_OBJS) $(BASE_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) \
	  $(LDOUT)$(LIB_DIR)$S$(LIBPREFIX)ortools.$(DYNAMIC_LIB_SUFFIX) \
	  $(ALGORITHMS_LIB_OBJS) \
	  $(BASE_LIB_OBJS) \
	  $(CONSTRAINT_SOLVER_LIB_OBJS) \
	  $(GRAPH_LIB_OBJS) \
	  $(LINEAR_SOLVER_LIB_OBJS) \
	  $(ROUTING_LIB_OBJS) \
	  $(SHORTESTPATHS_LIB_OBJS) \
	  $(UTIL_LIB_OBJS) \
	  $(STATIC_LD_LP_DEPS) \
	  $(STATIC_LD_FLAGS)

cc_archive: $(LIB_DIR)/$(LIBPREFIX)ortools.$(DYNAMIC_LIB_SUFFIX)
	-$(DELREC) temp
	mkdir temp
	mkdir temp/or-tools.$(PORT)
	mkdir temp/or-tools.$(PORT)/bin
	mkdir temp/or-tools.$(PORT)/examples
	mkdir temp/or-tools.$(PORT)/include
	mkdir temp/or-tools.$(PORT)/include/algorithms
	mkdir temp/or-tools.$(PORT)/include/base
	mkdir temp/or-tools.$(PORT)/include/constraint_solver
	mkdir temp/or-tools.$(PORT)/include/gflags
	mkdir temp/or-tools.$(PORT)/include/graph
	mkdir temp/or-tools.$(PORT)/include/linear_solver
	mkdir temp/or-tools.$(PORT)/include/util
	mkdir temp/or-tools.$(PORT)/lib
	mkdir temp/or-tools.$(PORT)/objs
	cp LICENSE-2.0.txt temp/or-tools.$(PORT)
	cp tools/README.cc temp/or-tools.$(PORT)/README
	cp tools/Makefile.cc temp/or-tools.$(PORT)/Makefile
	cp lib/libortools.$(DYNAMIC_LIB_SUFFIX) temp/or-tools.$(PORT)/lib
	cp examples/cpp/*.cc temp/or-tools.$(PORT)/examples
	cp examples/cpp/*.h temp/or-tools.$(PORT)/examples
	cp src/algorithms/*.h temp/or-tools.$(PORT)/include/algorithms
	cp src/base/*.h temp/or-tools.$(PORT)/include/base
	cp src/constraint_solver/*.h temp/or-tools.$(PORT)/include/constraint_solver
	cp src/gen/constraint_solver/*.pb.h temp/or-tools.$(PORT)/include/constraint_solver
	cp src/graph/*.h temp/or-tools.$(PORT)/include/graph
	cp src/linear_solver/*.h temp/or-tools.$(PORT)/include/linear_solver
	cp src/gen/linear_solver/*.pb.h temp/or-tools.$(PORT)/include/linear_solver
	cp src/util/*.h temp/or-tools.$(PORT)/include/util
	cd temp/or-tools.$(PORT)/include && tar -C ../../../dependencies/install/include -c -v gflags | tar xvm
	cd temp/or-tools.$(PORT)/include && tar -C ../../../dependencies/install/include -c -v google | tar xvm
	cd temp/or-tools.$(PORT)/include && tar -C ../../../dependencies/install/include -c -v sparsehash | tar xvm
	cd temp && tar cvzf ../Google.OrTools.cc.$(PORT).$(SVNVERSION).tar.gz or-tools.$(PORT)
	-$(DELREC) temp
endif

printdir:
	@echo LIB_DIR = $(LIB_DIR)
	@echo BIN_DIR = $(BIN_DIR)
	@echo GEN_DIR = $(GEN_DIR)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo SRC_DIR = $(SRC_DIR)
	@echo EX_DIR  = $(EX_DIR)
