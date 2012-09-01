#### DYNAMIC link and libs ####

# List libraries by module.
DYNAMIC_BASE_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX)          \
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

# Lib dependencies.
DYNAMIC_BASE_DEPS = $(DYNAMIC_BASE_LIBS)

DYNAMIC_LP_DEPS = $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_ALGORITHMS_DEPS = $(DYNAMIC_ALGORITHMS_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_CP_DEPS = $(DYNAMIC_CP_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_GRAPH_DEPS = $(DYNAMIC_GRAPH_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_ROUTING_DEPS = $(DYNAMIC_ROUTING_LIBS) $(DYNAMIC_CP_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_GRAPH_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_FLATZINC_DEPS = $(DYNAMIC_FLATZINC_LIBS) $(DYNAMIC_CP_LIBS) $(DYNAMIC_LP_LIBS) $(DYNAMIC_BASE_LIBS)

DYNAMIC_DIMACS_DEPS = $(DYNAMIC_DIMACS_LIBS) $(DYNAMIC_GRAPH_LIBS) $(DYNAMIC_ALGORITHMS_LIBS) $(DYNAMIC_BASE_LIBS)


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

#### STATIC link and libs ####

# List libraries by module.
STATIC_BASE_LIBS = \
	$(LIB_DIR)/$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX)          \
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

# Lib dependencies.
STATIC_BASE_DEPS = $(STATIC_BASE_LIBS)

STATIC_LP_DEPS = $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_ALGORITHMS_DEPS = $(STATIC_ALGORITHMS_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_CP_DEPS = $(STATIC_CP_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)

STATIC_GRAPH_DEPS = $(STATIC_GRAPH_LIBS) $(STATIC_BASE_LIBS)

STATIC_ROUTING_DEPS = $(STATIC_ROUTING_LIBS) $(STATIC_CP_LIBS) $(STATIC_LP_LIBS) $(STATIC_GRAPH_LIBS) $(STATIC_BASE_LIBS)

STATIC_FLATZINC_DEPS = $(STATIC_FLATZINC_LIBS) $(STATIC_CP_LIBS) $(STATIC_LP_LIBS) $(STATIC_BASE_LIBS)


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
cc: cplibs cpexe algorithmslibs graphlibs lplibs lpexe

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
	-$(DEL) $(OR_ROOT)*.exp

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

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OBJS = \
	$(OBJ_DIR)/alldiff_cst.$O\
	$(OBJ_DIR)/assignment.$O\
	$(OBJ_DIR)/assignment.pb.$O\
	$(OBJ_DIR)/ac4r_table.$O\
	$(OBJ_DIR)/booleans.$O\
	$(OBJ_DIR)/collect_variables.$O\
	$(OBJ_DIR)/constraint_solver.$O\
	$(OBJ_DIR)/constraints.$O\
	$(OBJ_DIR)/count_cst.$O\
	$(OBJ_DIR)/default_search.$O\
	$(OBJ_DIR)/demon_profiler.$O\
	$(OBJ_DIR)/demon_profiler.pb.$O\
	$(OBJ_DIR)/dependency_graph.$O\
	$(OBJ_DIR)/deviation.$O\
	$(OBJ_DIR)/element.$O\
	$(OBJ_DIR)/expr_array.$O\
	$(OBJ_DIR)/expr_cst.$O\
	$(OBJ_DIR)/expressions.$O\
	$(OBJ_DIR)/gcc.$O\
	$(OBJ_DIR)/hybrid.$O\
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
	$(OBJ_DIR)/sched_expr.$O\
	$(OBJ_DIR)/sched_search.$O\
	$(OBJ_DIR)/search.$O\
	$(OBJ_DIR)/search_limit.pb.$O\
	$(OBJ_DIR)/set_var.$O\
	$(OBJ_DIR)/softgcc.$O\
	$(OBJ_DIR)/table.$O\
	$(OBJ_DIR)/timetabling.$O\
	$(OBJ_DIR)/trace.$O\
	$(OBJ_DIR)/tree_monitor.$O\
	$(OBJ_DIR)/utilities.$O \
	$(OBJ_DIR)/visitor.$O

$(OBJ_DIR)/alldiff_cst.$O:$(SRC_DIR)/constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/alldiff_cst.cc $(OBJ_OUT)alldiff_cst.$O

$(OBJ_DIR)/assignment.$O:$(SRC_DIR)/constraint_solver/assignment.cc $(GEN_DIR)/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/assignment.cc $(OBJ_OUT)assignment.$O

$(OBJ_DIR)/assignment.pb.$O:$(GEN_DIR)/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/assignment.pb.cc $(OBJ_OUT)assignment.pb.$O

$(OBJ_DIR)/ac4r_table.$O:$(SRC_DIR)/constraint_solver/ac4r_table.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/ac4r_table.cc $(OBJ_OUT)ac4r_table.$O

$(OBJ_DIR)/booleans.$O:$(SRC_DIR)/constraint_solver/booleans.cc $(OR_ROOT)dependencies/sources/Minisat/core/Solver.h $(OR_ROOT)dependencies/sources/Minisat/core/Solver.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/booleans.cc $(OBJ_OUT)booleans.$O

$(GEN_DIR)/constraint_solver/assignment.pb.cc:$(SRC_DIR)/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/assignment.proto

$(GEN_DIR)/constraint_solver/assignment.pb.h:$(GEN_DIR)/constraint_solver/assignment.pb.cc

$(OBJ_DIR)/collect_variables.$O:$(SRC_DIR)/constraint_solver/collect_variables.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/collect_variables.cc $(OBJ_OUT)collect_variables.$O

$(OBJ_DIR)/constraint_solver.$O:$(SRC_DIR)/constraint_solver/constraint_solver.cc $(GEN_DIR)/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraint_solver.cc $(OBJ_OUT)constraint_solver.$O

$(OBJ_DIR)/constraints.$O:$(SRC_DIR)/constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/constraints.cc $(OBJ_OUT)constraints.$O

$(OBJ_DIR)/count_cst.$O:$(SRC_DIR)/constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/count_cst.cc $(OBJ_OUT)count_cst.$O

$(OBJ_DIR)/default_search.$O:$(SRC_DIR)/constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/default_search.cc $(OBJ_OUT)default_search.$O

$(OBJ_DIR)/demon_profiler.$O:$(SRC_DIR)/constraint_solver/demon_profiler.cc $(GEN_DIR)/constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/demon_profiler.cc $(OBJ_OUT)demon_profiler.$O

$(OBJ_DIR)/demon_profiler.pb.$O:$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/demon_profiler.pb.cc $(OBJ_OUT)demon_profiler.pb.$O

$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc:$(SRC_DIR)/constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/demon_profiler.proto

$(GEN_DIR)/constraint_solver/demon_profiler.pb.h:$(GEN_DIR)/constraint_solver/demon_profiler.pb.cc

$(OBJ_DIR)/dependency_graph.$O:$(SRC_DIR)/constraint_solver/dependency_graph.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/dependency_graph.cc $(OBJ_OUT)dependency_graph.$O

$(OBJ_DIR)/deviation.$O:$(SRC_DIR)/constraint_solver/deviation.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/deviation.cc $(OBJ_OUT)deviation.$O

$(OBJ_DIR)/element.$O:$(SRC_DIR)/constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/element.cc $(OBJ_OUT)element.$O

$(OBJ_DIR)/expr_array.$O:$(SRC_DIR)/constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_array.cc $(OBJ_OUT)expr_array.$O

$(OBJ_DIR)/expr_cst.$O:$(SRC_DIR)/constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expr_cst.cc $(OBJ_OUT)expr_cst.$O

$(OBJ_DIR)/expressions.$O:$(SRC_DIR)/constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/expressions.cc $(OBJ_OUT)expressions.$O

$(OBJ_DIR)/gcc.$O:$(SRC_DIR)/constraint_solver/gcc.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/gcc.cc $(OBJ_OUT)gcc.$O

$(OBJ_DIR)/hybrid.$O:$(SRC_DIR)/constraint_solver/hybrid.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/hybrid.cc $(OBJ_OUT)hybrid.$O

$(OBJ_DIR)/interval.$O:$(SRC_DIR)/constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/interval.cc $(OBJ_OUT)interval.$O

$(OBJ_DIR)/io.$O:$(SRC_DIR)/constraint_solver/io.cc $(GEN_DIR)/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/io.cc $(OBJ_OUT)io.$O

$(OBJ_DIR)/local_search.$O:$(SRC_DIR)/constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/local_search.cc $(OBJ_OUT)local_search.$O

$(OBJ_DIR)/model.pb.$O:$(GEN_DIR)/constraint_solver/model.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/model.pb.cc $(OBJ_OUT)model.pb.$O

$(OBJ_DIR)/model_cache.$O:$(SRC_DIR)/constraint_solver/model_cache.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/model_cache.cc $(OBJ_OUT)model_cache.$O

$(GEN_DIR)/constraint_solver/model.pb.cc:$(SRC_DIR)/constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/model.proto

$(GEN_DIR)/constraint_solver/model.pb.h:$(GEN_DIR)/constraint_solver/model.pb.cc $(GEN_DIR)/constraint_solver/search_limit.pb.h

$(OBJ_DIR)/nogoods.$O:$(SRC_DIR)/constraint_solver/nogoods.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/nogoods.cc $(OBJ_OUT)nogoods.$O

$(OBJ_DIR)/mtsearch.$O:$(SRC_DIR)/constraint_solver/mtsearch.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/mtsearch.cc $(OBJ_OUT)mtsearch.$O

$(OBJ_DIR)/pack.$O:$(SRC_DIR)/constraint_solver/pack.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/pack.cc $(OBJ_OUT)pack.$O

$(OBJ_DIR)/range_cst.$O:$(SRC_DIR)/constraint_solver/range_cst.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/range_cst.cc $(OBJ_OUT)range_cst.$O

$(OBJ_DIR)/resource.$O:$(SRC_DIR)/constraint_solver/resource.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/resource.cc $(OBJ_OUT)resource.$O

$(OBJ_DIR)/sched_expr.$O:$(SRC_DIR)/constraint_solver/sched_expr.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_expr.cc $(OBJ_OUT)sched_expr.$O

$(OBJ_DIR)/sched_search.$O:$(SRC_DIR)/constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/sched_search.cc $(OBJ_OUT)sched_search.$O

$(OBJ_DIR)/search.$O:$(SRC_DIR)/constraint_solver/search.cc $(GEN_DIR)/constraint_solver/search_limit.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/search.cc $(OBJ_OUT)search.$O

$(OBJ_DIR)/search_limit.pb.$O:$(GEN_DIR)/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/constraint_solver/search_limit.pb.cc $(OBJ_OUT)search_limit.pb.$O

$(GEN_DIR)/constraint_solver/search_limit.pb.cc:$(SRC_DIR)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/constraint_solver/search_limit.proto

$(GEN_DIR)/constraint_solver/search_limit.pb.h:$(GEN_DIR)/constraint_solver/search_limit.pb.cc

$(OBJ_DIR)/set_var.$O:$(SRC_DIR)/constraint_solver/set_var.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/set_var.cc $(OBJ_OUT)set_var.$O

$(OBJ_DIR)/softgcc.$O:$(SRC_DIR)/constraint_solver/softgcc.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/softgcc.cc $(OBJ_OUT)softgcc.$O

$(OBJ_DIR)/table.$O:$(SRC_DIR)/constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/table.cc $(OBJ_OUT)table.$O

$(OBJ_DIR)/timetabling.$O:$(SRC_DIR)/constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/timetabling.cc $(OBJ_OUT)timetabling.$O

$(OBJ_DIR)/trace.$O:$(SRC_DIR)/constraint_solver/trace.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/trace.cc $(OBJ_OUT)trace.$O

$(OBJ_DIR)/tree_monitor.$O:$(SRC_DIR)/constraint_solver/tree_monitor.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/tree_monitor.cc $(OBJ_OUT)tree_monitor.$O

$(OBJ_DIR)/utilities.$O:$(SRC_DIR)/constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/utilities.cc $(OBJ_OUT)utilities.$O

$(OBJ_DIR)/visitor.$O:$(SRC_DIR)/constraint_solver/visitor.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/visitor.cc $(OBJ_OUT)visitor.$O

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
	$(OBJ_DIR)/linear_solver.$O \
	$(OBJ_DIR)/linear_solver.pb.$O \
	$(OBJ_DIR)/scip_interface.$O \
	$(OBJ_DIR)/sulum_interface.$O


$(OBJ_DIR)/cbc_interface.$O:$(SRC_DIR)/linear_solver/cbc_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/cbc_interface.cc $(OBJ_OUT)cbc_interface.$O

$(OBJ_DIR)/clp_interface.$O:$(SRC_DIR)/linear_solver/clp_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/clp_interface.cc $(OBJ_OUT)clp_interface.$O

$(OBJ_DIR)/glpk_interface.$O:$(SRC_DIR)/linear_solver/glpk_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/glpk_interface.cc $(OBJ_OUT)glpk_interface.$O

$(OBJ_DIR)/linear_solver.$O:$(SRC_DIR)/linear_solver/linear_solver.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/linear_solver.cc $(OBJ_OUT)linear_solver.$O

$(OBJ_DIR)/linear_solver.pb.$O:$(GEN_DIR)/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c $(GEN_DIR)/linear_solver/linear_solver.pb.cc $(OBJ_OUT)linear_solver.pb.$O

$(GEN_DIR)/linear_solver/linear_solver.pb.cc:$(SRC_DIR)/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(INC_DIR) --cpp_out=$(GEN_DIR) $(SRC_DIR)/linear_solver/linear_solver.proto

$(GEN_DIR)/linear_solver/linear_solver.pb.h:$(GEN_DIR)/linear_solver/linear_solver.pb.cc

$(OBJ_DIR)/scip_interface.$O:$(SRC_DIR)/linear_solver/scip_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/scip_interface.cc $(OBJ_OUT)scip_interface.$O

$(OBJ_DIR)/sulum_interface.$O:$(SRC_DIR)/linear_solver/sulum_interface.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/linear_solver/sulum_interface.cc $(OBJ_OUT)sulum_interface.$O

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
	$(OBJ_DIR)/const_int_array.$O \
	$(OBJ_DIR)/graph_export.$O \
	$(OBJ_DIR)/xml_helper.$O

$(OBJ_DIR)/bitset.$O:$(SRC_DIR)/util/bitset.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/bitset.cc $(OBJ_OUT)bitset.$O

$(OBJ_DIR)/cached_log.$O:$(SRC_DIR)/util/cached_log.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/cached_log.cc $(OBJ_OUT)cached_log.$O

$(OBJ_DIR)/const_int_array.$O:$(SRC_DIR)/util/const_int_array.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/const_int_array.cc $(OBJ_OUT)const_int_array.$O

$(OBJ_DIR)/graph_export.$O:$(SRC_DIR)/util/graph_export.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/graph_export.cc $(OBJ_OUT)graph_export.$O

$(OBJ_DIR)/xml_helper.$O:$(SRC_DIR)/util/xml_helper.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/util/xml_helper.cc $(OBJ_OUT)xml_helper.$O

$(LIB_DIR)/$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX): $(UTIL_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)util.$(DYNAMIC_LIB_SUFFIX) $(UTIL_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX): $(UTIL_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)util.$(STATIC_LIB_SUFFIX) $(UTIL_LIB_OBJS)
endif

# Graph library.

GRAPH_LIB_OBJS=\
	$(OBJ_DIR)/linear_assignment.$O \
	$(OBJ_DIR)/cliques.$O \
	$(OBJ_DIR)/connectivity.$O \
	$(OBJ_DIR)/max_flow.$O \
	$(OBJ_DIR)/min_cost_flow.$O

$(OBJ_DIR)/linear_assignment.$O:$(SRC_DIR)/graph/linear_assignment.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/linear_assignment.cc $(OBJ_OUT)linear_assignment.$O

$(OBJ_DIR)/cliques.$O:$(SRC_DIR)/graph/cliques.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/cliques.cc $(OBJ_OUT)cliques.$O

$(OBJ_DIR)/connectivity.$O:$(SRC_DIR)/graph/connectivity.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/connectivity.cc $(OBJ_OUT)connectivity.$O

$(OBJ_DIR)/max_flow.$O:$(SRC_DIR)/graph/max_flow.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/max_flow.cc $(OBJ_OUT)max_flow.$O

$(OBJ_DIR)/min_cost_flow.$O:$(SRC_DIR)/graph/min_cost_flow.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/min_cost_flow.cc $(OBJ_OUT)min_cost_flow.$O

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
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/bellman_ford.cc $(OBJ_OUT)bellman_ford.$O

$(OBJ_DIR)/dijkstra.$O:$(SRC_DIR)/graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/dijkstra.cc $(OBJ_OUT)dijkstra.$O

$(OBJ_DIR)/shortestpaths.$O:$(SRC_DIR)/graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/graph/shortestpaths.cc $(OBJ_OUT)shortestpaths.$O

$(LIB_DIR)/$(LIBPREFIX)shortestpaths.$(DYNAMIC_LIB_SUFFIX): $(SHORTESTPATHS_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)shortestpaths.$(DYNAMIC_LIB_SUFFIX) $(SHORTESTPATHS_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)shortestpaths.$(STATIC_LIB_SUFFIX): $(SHORTESTPATHS_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)shortestpaths.$(STATIC_LIB_SUFFIX) $(SHORTESTPATHS_LIB_OBJS)
endif

# Routing library.

ROUTING_LIB_OBJS=\
	$(OBJ_DIR)/routing.$O

$(OBJ_DIR)/routing.$O:$(SRC_DIR)/constraint_solver/routing.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/constraint_solver/routing.cc $(OBJ_OUT)routing.$O

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
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/hungarian.cc $(OBJ_OUT)hungarian.$O

$(OBJ_DIR)/knapsack_solver.$O:$(SRC_DIR)/algorithms/knapsack_solver.cc $(GEN_DIR)/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/algorithms/knapsack_solver.cc $(OBJ_OUT)knapsack_solver.$O

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
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/bitmap.cc $(OBJ_OUT)bitmap.$O
$(OBJ_DIR)/callback.$O:$(SRC_DIR)/base/callback.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/callback.cc $(OBJ_OUT)callback.$O
$(OBJ_DIR)/file.$O:$(SRC_DIR)/base/file.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/file.cc $(OBJ_OUT)file.$O
$(OBJ_DIR)/filelinereader.$O:$(SRC_DIR)/base/filelinereader.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/filelinereader.cc $(OBJ_OUT)filelinereader.$O
$(OBJ_DIR)/logging.$O:$(SRC_DIR)/base/logging.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/logging.cc $(OBJ_OUT)logging.$O
$(OBJ_DIR)/mutex.$O:$(SRC_DIR)/base/mutex.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/mutex.cc $(OBJ_OUT)mutex.$O
$(OBJ_DIR)/join.$O:$(SRC_DIR)/base/join.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/join.cc $(OBJ_OUT)join.$O
$(OBJ_DIR)/random.$O:$(SRC_DIR)/base/random.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/random.cc $(OBJ_OUT)random.$O
$(OBJ_DIR)/recordio.$O:$(SRC_DIR)/base/recordio.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/recordio.cc $(OBJ_OUT)recordio.$O
$(OBJ_DIR)/threadpool.$O:$(SRC_DIR)/base/threadpool.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/threadpool.cc $(OBJ_OUT)threadpool.$O
$(OBJ_DIR)/split.$O:$(SRC_DIR)/base/split.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/split.cc $(OBJ_OUT)split.$O
$(OBJ_DIR)/stringpiece.$O:$(SRC_DIR)/base/stringpiece.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringpiece.cc $(OBJ_OUT)stringpiece.$O
$(OBJ_DIR)/stringprintf.$O:$(SRC_DIR)/base/stringprintf.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/stringprintf.cc $(OBJ_OUT)stringprintf.$O
$(OBJ_DIR)/sysinfo.$O:$(SRC_DIR)/base/sysinfo.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/sysinfo.cc $(OBJ_OUT)sysinfo.$O
$(OBJ_DIR)/timer.$O:$(SRC_DIR)/base/timer.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)/base/timer.cc $(OBJ_OUT)timer.$O

$(LIB_DIR)/$(LIBPREFIX)base.$(DYNAMIC_LIB_SUFFIX): $(BASE_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)base.$(DYNAMIC_LIB_SUFFIX) $(BASE_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)base.$(STATIC_LIB_SUFFIX): $(BASE_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)base.$(STATIC_LIB_SUFFIX) $(BASE_LIB_OBJS)
endif

# DIMACS challenge problem format library

DIMACS_LIB_OBJS=\
	$(OBJ_DIR)/parse_dimacs_assignment.$O\
	$(OBJ_DIR)/print_dimacs_assignment.$O

$(OBJ_DIR)/parse_dimacs_assignment.$O:$(EX_DIR)/cpp/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/parse_dimacs_assignment.cc $(OBJ_OUT)parse_dimacs_assignment.$O
$(OBJ_DIR)/print_dimacs_assignment.$O:$(EX_DIR)/cpp/print_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/print_dimacs_assignment.cc $(OBJ_OUT)print_dimacs_assignment.$O

$(LIB_DIR)/$(LIBPREFIX)dimacs.$(DYNAMIC_LIB_SUFFIX): $(DIMACS_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)dimacs.$(DYNAMIC_LIB_SUFFIX) $(DIMACS_LIB_OBJS)

FLATZINC_LIB_OBJS=\
	$(OBJ_DIR)/flatzinc.$O\
	$(OBJ_DIR)/fz_search.$O\
	$(OBJ_DIR)/lexer.yy.$O\
	$(OBJ_DIR)/parser.tab.$O\
	$(OBJ_DIR)/parser.$O\
	$(OBJ_DIR)/registry.$O

ifeq ($(SYSTEM),win)
$(SRC_DIR)/flatzinc/lexer.yy.cc: $(SRC_DIR)/flatzinc/lexer.win.cc $(SRC_DIR)/flatzinc/parser.tab.h
	copy $(SRC_DIR)\\flatzinc\\lexer.win.cc $(SRC_DIR)\\flatzinc\\lexer.yy.cc

$(SRC_DIR)/flatzinc/parser.tab.cc: $(SRC_DIR)/flatzinc/parser.win.cc
	copy $(SRC_DIR)\\flatzinc\\parser.win.cc $(SRC_DIR)\\flatzinc\\parser.tab.cc

$(SRC_DIR)/flatzinc/parser.tab.h: $(SRC_DIR)/flatzinc/parser.win.hh
	copy $(SRC_DIR)\\flatzinc\\parser.win.hh $(SRC_DIR)\\flatzinc\\parser.tab.h

else
$(SRC_DIR)/flatzinc/lexer.yy.cc: $(SRC_DIR)/flatzinc/lexer.lxx $(SRC_DIR)/flatzinc/parser.tab.h $(SRC_DIR)/flatzinc/parser.h $(SRC_DIR)/flatzinc/flatzinc.h
	flex -o$(SRC_DIR)/flatzinc/lexer.yy.cc $(SRC_DIR)/flatzinc/lexer.lxx

$(SRC_DIR)/flatzinc/parser.tab.cc: $(SRC_DIR)/flatzinc/parser.yxx $(SRC_DIR)/flatzinc/parser.h $(SRC_DIR)/flatzinc/flatzinc.h
	bison -t -o $(SRC_DIR)/flatzinc/parser.tab.cc -d $<
	mv $(SRC_DIR)/flatzinc/parser.tab.hh $(SRC_DIR)/flatzinc/parser.tab.h

$(SRC_DIR)/flatzinc/parser.tab.h: $(SRC_DIR)/flatzinc/parser.tab.cc

win_parser: $(SRC_DIR)/flatzinc/lexer.win.cc $(SRC_DIR)/flatzinc/parser.win.cc

$(SRC_DIR)/flatzinc/lexer.win.cc: $(SRC_DIR)/flatzinc/lexer.lxx $(SRC_DIR)/flatzinc/parser.tab.h $(SRC_DIR)/flatzinc/parser.h $(SRC_DIR)/flatzinc/flatzinc.h
	flex -nounistd -o$(SRC_DIR)/flatzinc/lexer.win.cc $(SRC_DIR)/flatzinc/lexer.lxx

$(SRC_DIR)/flatzinc/parser.win.cc: $(SRC_DIR)/flatzinc/parser.yxx $(SRC_DIR)/flatzinc/parser.h $(SRC_DIR)/flatzinc/flatzinc.h
	bison -t -o $(SRC_DIR)/flatzinc/parser.win.cc -d $<

endif

$(OBJ_DIR)/flatzinc.$O:$(SRC_DIR)/flatzinc/flatzinc.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sflatzinc.cc $(OBJ_OUT)flatzinc.$O
$(OBJ_DIR)/fz_search.$O:$(SRC_DIR)/flatzinc/fz_search.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sfz_search.cc $(OBJ_OUT)fz_search.$O
 $(OBJ_DIR)/lexer.yy.$O:$(SRC_DIR)/flatzinc/lexer.yy.cc
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Slexer.yy.cc $(OBJ_OUT)lexer.yy.$O
$(OBJ_DIR)/parser.$O:$(SRC_DIR)/flatzinc/parser.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparser.cc $(OBJ_OUT)parser.$O
$(OBJ_DIR)/parser.tab.$O:$(SRC_DIR)/flatzinc/parser.tab.cc $(SRC_DIR)/flatzinc/flatzinc.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sparser.tab.cc $(OBJ_OUT)parser.tab.$O
$(OBJ_DIR)/registry.$O:$(SRC_DIR)/flatzinc/registry.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sregistry.cc $(OBJ_OUT)registry.$O

$(LIB_DIR)/$(LIBPREFIX)fz.$(DYNAMIC_LIB_SUFFIX): $(FLATZINC_LIB_OBJS)
	$(DYNAMIC_LINK_CMD) $(DYNAMIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)fz.$(DYNAMIC_LIB_SUFFIX) $(FLATZINC_LIB_OBJS)

ifneq ($(SYSTEM),win)
$(LIB_DIR)/$(LIBPREFIX)fz.$(STATIC_LIB_SUFFIX): $(FLATZINC_LIB_OBJS)
	$(STATIC_LINK_CMD) $(STATIC_LINK_PREFIX)$(LIB_DIR)$S$(LIBPREFIX)fz.$(STATIC_LIB_SUFFIX) $(FLATZINC_LIB_OBJS)
endif

$(OBJ_DIR)/fz.$O:$(SRC_DIR)/flatzinc/fz.cc $(SRC_DIR)/flatzinc/flatzinc.h $(SRC_DIR)/flatzinc/parser.h
	$(CCC) $(CFLAGS) -c $(SRC_DIR)$Sflatzinc$Sfz.cc $(OBJ_OUT)fz.$O

fz: $(BIN_DIR)/fz$E

$(BIN_DIR)/fz$E: $(STATIC_FLATZINC_DEPS) $(OBJ_DIR)/fz.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/fz.$O $(STATIC_FLATZINC_LNK) $(FZ_STATIC) $(STATIC_LD_FLAGS) $(EXEOUT)fz$E

# Flow and linear assignment cpp

$(OBJ_DIR)/linear_assignment_api.$O:$(EX_DIR)/cpp/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_assignment_api.cc $(OBJ_OUT)linear_assignment_api.$O

$(BIN_DIR)/linear_assignment_api$E: $(DYNAMIC_GRAPH_DEPS) $(OBJ_DIR)/linear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_assignment_api.$O $(DYNAMIC_GRAPH_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)linear_assignment_api$E

$(OBJ_DIR)/flow_api.$O:$(EX_DIR)/cpp/flow_api.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/flow_api.cc $(OBJ_OUT)flow_api.$O

$(BIN_DIR)/flow_api$E: $(DYNAMIC_GRAPH_DEPS) $(OBJ_DIR)/flow_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/flow_api.$O $(DYNAMIC_GRAPH_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)flow_api$E

$(OBJ_DIR)/dimacs_assignment.$O:$(EX_DIR)/cpp/dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dimacs_assignment.cc $(OBJ_OUT)dimacs_assignment.$O

$(BIN_DIR)/dimacs_assignment$E: $(DYNAMIC_DIMACS_DEPS) $(OBJ_DIR)/dimacs_assignment.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dimacs_assignment.$O $(DYNAMIC_DIMACS_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)dimacs_assignment$E

# Pure CP and Routing Examples

$(OBJ_DIR)/costas_array.$O: $(EX_DIR)/cpp/costas_array.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/costas_array.cc $(OBJ_OUT)costas_array.$O

$(BIN_DIR)/costas_array$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/costas_array.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/costas_array.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)costas_array$E

$(OBJ_DIR)/cryptarithm.$O:$(EX_DIR)/cpp/cryptarithm.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cryptarithm.cc $(OBJ_OUT)cryptarithm.$O

$(BIN_DIR)/cryptarithm$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/cryptarithm.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cryptarithm.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)cryptarithm$E

$(OBJ_DIR)/cvrptw.$O: $(EX_DIR)/cpp/cvrptw.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/cvrptw.cc $(OBJ_OUT)cvrptw.$O

$(BIN_DIR)/cvrptw$E: $(DYNAMIC_ROUTING_DEPS) $(OBJ_DIR)/cvrptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/cvrptw.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)cvrptw$E

$(OBJ_DIR)/dobble_ls.$O:$(EX_DIR)/cpp/dobble_ls.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/dobble_ls.cc $(OBJ_OUT)dobble_ls.$O

$(BIN_DIR)/dobble_ls$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/dobble_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/dobble_ls.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)dobble_ls$E

$(OBJ_DIR)/golomb.$O:$(EX_DIR)/cpp/golomb.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/golomb.cc $(OBJ_OUT)golomb.$O

$(BIN_DIR)/golomb$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/golomb.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/golomb.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)golomb$E

$(OBJ_DIR)/jobshop.$O:$(EX_DIR)/cpp/jobshop.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop.cc $(OBJ_OUT)jobshop.$O

$(BIN_DIR)/jobshop$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/jobshop.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)jobshop$E

$(OBJ_DIR)/jobshop_ls.$O:$(EX_DIR)/cpp/jobshop_ls.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_ls.cc $(OBJ_OUT)jobshop_ls.$O

$(BIN_DIR)/jobshop_ls$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/jobshop_ls.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_ls.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)jobshop_ls$E

$(OBJ_DIR)/jobshop_earlytardy.$O:$(EX_DIR)/cpp/jobshop_earlytardy.cc $(SRC_DIR)/constraint_solver/constraint_solver.h $(EX_DIR)/cpp/jobshop_earlytardy.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/jobshop_earlytardy.cc $(OBJ_OUT)jobshop_earlytardy.$O

$(BIN_DIR)/jobshop_earlytardy$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/jobshop_earlytardy.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/jobshop_earlytardy.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)jobshop_earlytardy$E

$(OBJ_DIR)/magic_square.$O:$(EX_DIR)/cpp/magic_square.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/magic_square.cc $(OBJ_OUT)magic_square.$O

$(BIN_DIR)/magic_square$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/magic_square.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/magic_square.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)magic_square$E

$(OBJ_DIR)/model_util.$O:$(EX_DIR)/cpp/model_util.cc $(GEN_DIR)/constraint_solver/model.pb.h $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/model_util.cc $(OBJ_OUT)model_util.$O

$(BIN_DIR)/model_util$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/model_util.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/model_util.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)model_util$E

$(OBJ_DIR)/multidim_knapsack.$O:$(EX_DIR)/cpp/multidim_knapsack.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/multidim_knapsack.cc $(OBJ_OUT)multidim_knapsack.$O

$(BIN_DIR)/multidim_knapsack$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/multidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/multidim_knapsack.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)multidim_knapsack$E

$(OBJ_DIR)/network_routing.$O:$(EX_DIR)/cpp/network_routing.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/network_routing.cc $(OBJ_OUT)network_routing.$O

$(BIN_DIR)/network_routing$E: $(DYNAMIC_CP_DEPS) $(DYNAMIC_GRAPH_DEPS) $(OBJ_DIR)/network_routing.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/network_routing.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_GRAPH_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)network_routing$E

$(OBJ_DIR)/nqueens.$O: $(EX_DIR)/cpp/nqueens.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/nqueens.cc $(OBJ_OUT)nqueens.$O

$(BIN_DIR)/nqueens$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/nqueens.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/nqueens.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)nqueens$E

$(OBJ_DIR)/pdptw.$O: $(EX_DIR)/cpp/pdptw.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/pdptw.cc $(OBJ_OUT)pdptw.$O

$(BIN_DIR)/pdptw$E: $(DYNAMIC_ROUTING_DEPS) $(OBJ_DIR)/pdptw.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/pdptw.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)pdptw$E

$(OBJ_DIR)/sports_scheduling.$O:$(EX_DIR)/cpp/sports_scheduling.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/sports_scheduling.cc $(OBJ_OUT)sports_scheduling.$O

$(BIN_DIR)/sports_scheduling$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/sports_scheduling.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/sports_scheduling.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)sports_scheduling$E

$(OBJ_DIR)/tsp.$O: $(EX_DIR)/cpp/tsp.cc $(SRC_DIR)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/tsp.cc $(OBJ_OUT)tsp.$O

$(BIN_DIR)/tsp$E: $(DYNAMIC_ROUTING_DEPS) $(OBJ_DIR)/tsp.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/tsp.$O $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)tsp$E

# CP tests.

$(OBJ_DIR)/bug_pack.$O:$(EX_DIR)/tests/bug_pack.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/bug_pack.cc $(OBJ_OUT)bug_pack.$O

$(BIN_DIR)/bug_pack$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/bug_pack.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/bug_pack.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)bug_pack$E

$(OBJ_DIR)/mtsearch_test.$O:$(EX_DIR)/tests/mtsearch_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/mtsearch_test.cc $(OBJ_OUT)mtsearch_test.$O

$(BIN_DIR)/mtsearch_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/mtsearch_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/mtsearch_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)mtsearch_test$E

$(OBJ_DIR)/ac4r_table_test.$O:$(EX_DIR)/tests/ac4r_table_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/ac4r_table_test.cc $(OBJ_OUT)ac4r_table_test.$O

$(BIN_DIR)/ac4r_table_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/ac4r_table_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ac4r_table_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)ac4r_table_test$E

$(OBJ_DIR)/gcc_test.$O:$(EX_DIR)/tests/gcc_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/gcc_test.cc $(OBJ_OUT)gcc_test.$O

$(BIN_DIR)/gcc_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/gcc_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/gcc_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)gcc_test$E

$(OBJ_DIR)/min_max_test.$O:$(EX_DIR)/tests/min_max_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/min_max_test.cc $(OBJ_OUT)min_max_test.$O

$(BIN_DIR)/min_max_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/min_max_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/min_max_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)min_max_test$E

$(OBJ_DIR)/visitor_test.$O:$(EX_DIR)/tests/visitor_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/visitor_test.cc $(OBJ_OUT)visitor_test.$O

$(BIN_DIR)/visitor_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/visitor_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/visitor_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)visitor_test$E

$(OBJ_DIR)/boolean_test.$O:$(EX_DIR)/tests/boolean_test.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Stests/boolean_test.cc $(OBJ_OUT)boolean_test.$O

$(BIN_DIR)/boolean_test$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/boolean_test.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/boolean_test.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)boolean_test$E

$(OBJ_DIR)/ls_api.$O:$(EX_DIR)/cpp/ls_api.cc $(SRC_DIR)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/ls_api.cc $(OBJ_OUT)ls_api.$O

$(BIN_DIR)/ls_api$E: $(DYNAMIC_CP_DEPS) $(OBJ_DIR)/ls_api.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/ls_api.$O $(DYNAMIC_CP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)ls_api$E

# Linear Programming Examples

$(OBJ_DIR)/strawberry_fields_with_column_generation.$O: $(EX_DIR)/cpp/strawberry_fields_with_column_generation.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/strawberry_fields_with_column_generation.cc $(OBJ_OUT)strawberry_fields_with_column_generation.$O

$(BIN_DIR)/strawberry_fields_with_column_generation$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/strawberry_fields_with_column_generation.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)strawberry_fields_with_column_generation$E

$(OBJ_DIR)/linear_programming.$O: $(EX_DIR)/cpp/linear_programming.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_programming.cc $(OBJ_OUT)linear_programming.$O

$(BIN_DIR)/linear_programming$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/linear_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_programming.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)linear_programming$E

$(OBJ_DIR)/linear_solver_protocol_buffers.$O: $(EX_DIR)/cpp/linear_solver_protocol_buffers.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/linear_solver_protocol_buffers.cc $(OBJ_OUT)linear_solver_protocol_buffers.$O

$(BIN_DIR)/linear_solver_protocol_buffers$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/linear_solver_protocol_buffers.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)linear_solver_protocol_buffers$E

$(OBJ_DIR)/integer_programming.$O: $(EX_DIR)/cpp/integer_programming.cc $(SRC_DIR)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(EX_DIR)$Scpp/integer_programming.cc $(OBJ_OUT)integer_programming.$O

$(BIN_DIR)/integer_programming$E: $(DYNAMIC_LP_DEPS) $(OBJ_DIR)/integer_programming.$O
	$(CCC) $(CFLAGS) $(OBJ_DIR)/integer_programming.$O $(DYNAMIC_LP_LNK) $(DYNAMIC_LD_FLAGS) $(EXEOUT)integer_programming$E

printdir:
	@echo LIB_DIR = $(LIB_DIR)
	@echo BIN_DIR = $(BIN_DIR)
	@echo GEN_DIR = $(GEN_DIR)
	@echo OBJ_DIR = $(OBJ_DIR)
	@echo SRC_DIR = $(SRC_DIR)
	@echo EX_DIR  = $(EX_DIR)
