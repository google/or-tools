# List libraries by module.
BASE_LIBS = \
	$(LIBPREFIX)util.$(LIBSUFFIX)          \
	$(LIBPREFIX)base.$(LIBSUFFIX)

LP_LIBS = \
	$(LIBPREFIX)linear_solver.$(LIBSUFFIX)

ALGORITHMS_LIBS = \
	$(LIBPREFIX)algorithms.$(LIBSUFFIX)

CP_LIBS = \
	$(LIBPREFIX)constraint_solver.$(LIBSUFFIX)

GRAPH_LIBS = \
	$(LIBPREFIX)graph.$(LIBSUFFIX) \
	$(LIBPREFIX)shortestpaths.$(LIBSUFFIX)

ROUTING_LIBS = \
        $(LIBPREFIX)routing.$(LIBSUFFIX)

# Lib dependencies.
BASE_DEPS = $(BASE_LIBS)

LP_DEPS = $(LP_LIBS) $(BASE_LIBS)

ALGORITHMS_DEPS = $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)

CP_DEPS = $(CP_LIBS) $(LP_LIBS) $(BASE_LIBS)

GRAPH_DEPS = $(GRAPH_LIBS) $(BASE_LIBS)

ROUTING_DEPS = $(ROUTING_LIBS) $(CP_LIBS) $(LP_LIBS) $(GRAPH_LIBS) $(BASE_LIBS)

# Create link commands.
BASE_LNK = \
	$(PRE_LIB)util$(POST_LIB) \
	$(PRE_LIB)base$(POST_LIB)

LP_LNK = \
	$(PRE_LIB)linear_solver$(POST_LIB) \
	$(BASE_LNK) \
	$(LDLPDEPS)  # Third party linear solvers.

ALGORITHMS_LNK = \
	$(PRE_LIB)algorithms$(POST_LIB) \
	$(LP_LNK)

CP_LNK = \
	$(PRE_LIB)constraint_solver$(POST_LIB) \
	$(LP_LNK)

ROUTING_LNK = \
	$(PRE_LIB)routing$(POST_LIB) \
	$(PRE_LIB)graph$(POST_LIB) \
	$(PRE_LIB)shortestpaths$(POST_LIB) \
	$(CP_LNK)

GRAPH_LNK = \
	$(PRE_LIB)graph$(POST_LIB) \
	$(PRE_LIB)shortestpaths$(POST_LIB) \
	$(BASE_LNK)

# Binaries

CPBINARIES = \
	$(BINPREFIX)$Scostas_array$E \
	$(BINPREFIX)$Scryptarithm$E \
	$(BINPREFIX)$Scvrptw$E \
	$(BINPREFIX)$Sdobble_ls$E \
	$(BINPREFIX)$Sflow_api$E \
	$(BINPREFIX)$Sgolomb$E \
	$(BINPREFIX)$Sjobshop$E \
	$(BINPREFIX)$Sjobshop_ls$E \
	$(BINPREFIX)$Slinear_assignment_api$E \
	$(BINPREFIX)$Smagic_square$E \
	$(BINPREFIX)$Smodel_util$E \
	$(BINPREFIX)$Smultidim_knapsack$E \
	$(BINPREFIX)$Snetwork_routing$E \
	$(BINPREFIX)$Snqueens$E \
	$(BINPREFIX)$Spdptw$E \
	$(BINPREFIX)$Sdimacs_assignment$E \
	$(BINPREFIX)$Ssports_scheduling$E \
	$(BINPREFIX)$Stsp$E

LPBINARIES = \
	$(BINPREFIX)$Sinteger_programming$E \
	$(BINPREFIX)$Slinear_programming$E \
	$(BINPREFIX)$Slinear_solver_protocol_buffers$E \
	$(BINPREFIX)$Sstrawberry_fields_with_column_generation$E

# Special dimacs example.

DIMACS_LIBS = \
	$(LIBPREFIX)dimacs.$(LIBSUFFIX)

DIMACS_LNK = $(PRE_LIB)dimacs$(POST_LIB)

# Makefile targets.

# Main target
cc: cplibs cpexe algorithmslibs graphlibs lplibs lpexe

# Clean target

clean_cc:
	-$(DEL) $(LIBPREFIX)base.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)util.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)constraint_solver.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)linear_solver.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)graph.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)routing.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)algorithms.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)dimacs.$(LIBSUFFIX)
	-$(DEL) $(LIBPREFIX)shortestpaths.$(LIBSUFFIX)
	-$(DEL) $(OR_ROOT)objs$S*.$O
	-$(DEL) $(CPBINARIES)
	-$(DEL) $(LPBINARIES)
	-$(DEL) $(OR_ROOT)gen$Sconstraint_solver$S*.pb.*
	-$(DEL) $(OR_ROOT)gen$Slinear_solver$S*.pb.*
	-$(DEL) $(OR_ROOT)*.exp

# Individual targets.
algorithmslibs: $(ALGORITHMS_DEPS)

cpexe: $(CPBINARIES)

cplibs: $(CP_DEPS)

lpexe: $(LPBINARIES)

lplibs: $(LP_DEPS)

graphlibs: $(GRAPH_DEPS)

dimacslibs: $(DIMACS_LIBS)

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OS = \
	$(OR_ROOT)objs/alldiff_cst.$O\
	$(OR_ROOT)objs/assignment.$O\
	$(OR_ROOT)objs/assignment.pb.$O\
	$(OR_ROOT)objs/collect_variables.$O\
	$(OR_ROOT)objs/constraint_solver.$O\
	$(OR_ROOT)objs/constraints.$O\
	$(OR_ROOT)objs/count_cst.$O\
	$(OR_ROOT)objs/default_search.$O\
	$(OR_ROOT)objs/demon_profiler.$O\
	$(OR_ROOT)objs/demon_profiler.pb.$O\
	$(OR_ROOT)objs/dependency_graph.$O\
	$(OR_ROOT)objs/deviation.$O\
	$(OR_ROOT)objs/element.$O\
	$(OR_ROOT)objs/expr_array.$O\
	$(OR_ROOT)objs/expr_cst.$O\
	$(OR_ROOT)objs/expressions.$O\
	$(OR_ROOT)objs/hybrid.$O\
	$(OR_ROOT)objs/interval.$O\
	$(OR_ROOT)objs/io.$O\
	$(OR_ROOT)objs/local_search.$O\
	$(OR_ROOT)objs/model.pb.$O\
	$(OR_ROOT)objs/model_cache.$O\
	$(OR_ROOT)objs/nogoods.$O\
	$(OR_ROOT)objs/pack.$O\
	$(OR_ROOT)objs/range_cst.$O\
	$(OR_ROOT)objs/resource.$O\
	$(OR_ROOT)objs/sched_search.$O\
	$(OR_ROOT)objs/search.$O\
	$(OR_ROOT)objs/search_limit.pb.$O\
	$(OR_ROOT)objs/table.$O\
	$(OR_ROOT)objs/timetabling.$O\
	$(OR_ROOT)objs/trace.$O\
	$(OR_ROOT)objs/tree_monitor.$O\
	$(OR_ROOT)objs/utilities.$O \
	$(OR_ROOT)objs/visitor.$O

$(OR_ROOT)objs/alldiff_cst.$O:$(OR_ROOT)constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/alldiff_cst.cc $(OBJOUT)objs/alldiff_cst.$O

$(OR_ROOT)objs/assignment.$O:$(OR_ROOT)constraint_solver/assignment.cc $(OR_ROOT)gen/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/assignment.cc $(OBJOUT)objs/assignment.$O

$(OR_ROOT)objs/assignment.pb.$O:$(OR_ROOT)gen/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/constraint_solver/assignment.pb.cc $(OBJOUT)objs/assignment.pb.$O

$(OR_ROOT)gen/constraint_solver/assignment.pb.cc:$(OR_ROOT)constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(OR_ROOT_INC) --cpp_out=$(OR_ROOT)gen $(OR_ROOT)constraint_solver/assignment.proto

$(OR_ROOT)gen/constraint_solver/assignment.pb.h:$(OR_ROOT)gen/constraint_solver/assignment.pb.cc

$(OR_ROOT)objs/collect_variables.$O:$(OR_ROOT)constraint_solver/collect_variables.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/collect_variables.cc $(OBJOUT)objs/collect_variables.$O

$(OR_ROOT)objs/constraint_solver.$O:$(OR_ROOT)constraint_solver/constraint_solver.cc $(OR_ROOT)gen/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/constraint_solver.cc $(OBJOUT)objs/constraint_solver.$O

$(OR_ROOT)objs/constraints.$O:$(OR_ROOT)constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/constraints.cc $(OBJOUT)objs/constraints.$O

$(OR_ROOT)objs/count_cst.$O:$(OR_ROOT)constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/count_cst.cc $(OBJOUT)objs/count_cst.$O

$(OR_ROOT)objs/default_search.$O:$(OR_ROOT)constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/default_search.cc $(OBJOUT)objs/default_search.$O

$(OR_ROOT)objs/demon_profiler.$O:$(OR_ROOT)constraint_solver/demon_profiler.cc $(OR_ROOT)gen/constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/demon_profiler.cc $(OBJOUT)objs/demon_profiler.$O

$(OR_ROOT)objs/demon_profiler.pb.$O:$(OR_ROOT)gen/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/constraint_solver/demon_profiler.pb.cc $(OBJOUT)objs/demon_profiler.pb.$O

$(OR_ROOT)gen/constraint_solver/demon_profiler.pb.cc:$(OR_ROOT)constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(OR_ROOT_INC) --cpp_out=$(OR_ROOT)gen $(OR_ROOT)constraint_solver/demon_profiler.proto

$(OR_ROOT)gen/constraint_solver/demon_profiler.pb.h:$(OR_ROOT)gen/constraint_solver/demon_profiler.pb.cc

$(OR_ROOT)objs/dependency_graph.$O:$(OR_ROOT)constraint_solver/dependency_graph.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/dependency_graph.cc $(OBJOUT)objs/dependency_graph.$O

$(OR_ROOT)objs/deviation.$O:$(OR_ROOT)constraint_solver/deviation.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/deviation.cc $(OBJOUT)objs/deviation.$O

$(OR_ROOT)objs/element.$O:$(OR_ROOT)constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/element.cc $(OBJOUT)objs/element.$O

$(OR_ROOT)objs/expr_array.$O:$(OR_ROOT)constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/expr_array.cc $(OBJOUT)objs/expr_array.$O

$(OR_ROOT)objs/expr_cst.$O:$(OR_ROOT)constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/expr_cst.cc $(OBJOUT)objs/expr_cst.$O

$(OR_ROOT)objs/expressions.$O:$(OR_ROOT)constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/expressions.cc $(OBJOUT)objs/expressions.$O

$(OR_ROOT)objs/hybrid.$O:$(OR_ROOT)constraint_solver/hybrid.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/hybrid.cc $(OBJOUT)objs/hybrid.$O

$(OR_ROOT)objs/interval.$O:$(OR_ROOT)constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/interval.cc $(OBJOUT)objs/interval.$O

$(OR_ROOT)objs/io.$O:$(OR_ROOT)constraint_solver/io.cc $(OR_ROOT)gen/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/io.cc $(OBJOUT)objs/io.$O

$(OR_ROOT)objs/local_search.$O:$(OR_ROOT)constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/local_search.cc $(OBJOUT)objs/local_search.$O

$(OR_ROOT)objs/model.pb.$O:$(OR_ROOT)gen/constraint_solver/model.pb.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/constraint_solver/model.pb.cc $(OBJOUT)objs/model.pb.$O

$(OR_ROOT)objs/model_cache.$O:$(OR_ROOT)constraint_solver/model_cache.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/model_cache.cc $(OBJOUT)objs/model_cache.$O

$(OR_ROOT)gen/constraint_solver/model.pb.cc:$(OR_ROOT)constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(OR_ROOT_INC) --cpp_out=$(OR_ROOT)gen $(OR_ROOT)constraint_solver/model.proto

$(OR_ROOT)gen/constraint_solver/model.pb.h:$(OR_ROOT)gen/constraint_solver/model.pb.cc $(OR_ROOT)gen/constraint_solver/search_limit.pb.h

$(OR_ROOT)objs/nogoods.$O:$(OR_ROOT)constraint_solver/nogoods.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/nogoods.cc $(OBJOUT)objs/nogoods.$O

$(OR_ROOT)objs/pack.$O:$(OR_ROOT)constraint_solver/pack.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/pack.cc $(OBJOUT)objs/pack.$O

$(OR_ROOT)objs/range_cst.$O:$(OR_ROOT)constraint_solver/range_cst.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/range_cst.cc $(OBJOUT)objs/range_cst.$O

$(OR_ROOT)objs/resource.$O:$(OR_ROOT)constraint_solver/resource.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/resource.cc $(OBJOUT)objs/resource.$O

$(OR_ROOT)objs/sched_search.$O:$(OR_ROOT)constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/sched_search.cc $(OBJOUT)objs/sched_search.$O

$(OR_ROOT)objs/search.$O:$(OR_ROOT)constraint_solver/search.cc $(OR_ROOT)gen/constraint_solver/search_limit.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/search.cc $(OBJOUT)objs/search.$O

$(OR_ROOT)objs/search_limit.pb.$O:$(OR_ROOT)gen/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/constraint_solver/search_limit.pb.cc $(OBJOUT)objs/search_limit.pb.$O

$(OR_ROOT)gen/constraint_solver/search_limit.pb.cc:$(OR_ROOT)constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(OR_ROOT_INC) --cpp_out=$(OR_ROOT)gen $(OR_ROOT)constraint_solver/search_limit.proto

$(OR_ROOT)gen/constraint_solver/search_limit.pb.h:$(OR_ROOT)gen/constraint_solver/search_limit.pb.cc

$(OR_ROOT)objs/table.$O:$(OR_ROOT)constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/table.cc $(OBJOUT)objs/table.$O

$(OR_ROOT)objs/timetabling.$O:$(OR_ROOT)constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/timetabling.cc $(OBJOUT)objs/timetabling.$O

$(OR_ROOT)objs/trace.$O:$(OR_ROOT)constraint_solver/trace.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/trace.cc $(OBJOUT)objs/trace.$O

$(OR_ROOT)objs/tree_monitor.$O:$(OR_ROOT)constraint_solver/tree_monitor.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/tree_monitor.cc $(OBJOUT)objs/tree_monitor.$O

$(OR_ROOT)objs/utilities.$O:$(OR_ROOT)constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/utilities.cc $(OBJOUT)objs/utilities.$O

$(OR_ROOT)objs/visitor.$O:$(OR_ROOT)constraint_solver/visitor.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/visitor.cc $(OBJOUT)objs/visitor.$O

$(LIBPREFIX)constraint_solver.$(LIBSUFFIX): $(CONSTRAINT_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)constraint_solver.$(LIBSUFFIX) $(CONSTRAINT_SOLVER_LIB_OS)

# Linear Solver Library

LINEAR_SOLVER_LIB_OS = \
	$(OR_ROOT)objs/cbc_interface.$O \
	$(OR_ROOT)objs/clp_interface.$O \
	$(OR_ROOT)objs/glpk_interface.$O \
	$(OR_ROOT)objs/linear_solver.$O \
	$(OR_ROOT)objs/linear_solver.pb.$O \
	$(OR_ROOT)objs/scip_interface.$O

$(OR_ROOT)objs/cbc_interface.$O:$(OR_ROOT)linear_solver/cbc_interface.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)linear_solver/cbc_interface.cc $(OBJOUT)objs/cbc_interface.$O

$(OR_ROOT)objs/clp_interface.$O:$(OR_ROOT)linear_solver/clp_interface.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)linear_solver/clp_interface.cc $(OBJOUT)objs/clp_interface.$O

$(OR_ROOT)objs/glpk_interface.$O:$(OR_ROOT)linear_solver/glpk_interface.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)linear_solver/glpk_interface.cc $(OBJOUT)objs/glpk_interface.$O

$(OR_ROOT)objs/linear_solver.$O:$(OR_ROOT)linear_solver/linear_solver.cc $(OR_ROOT)gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)linear_solver/linear_solver.cc $(OBJOUT)objs/linear_solver.$O

$(OR_ROOT)objs/linear_solver.pb.$O:$(OR_ROOT)gen/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)gen/linear_solver/linear_solver.pb.cc $(OBJOUT)objs/linear_solver.pb.$O

$(OR_ROOT)gen/linear_solver/linear_solver.pb.cc:$(OR_ROOT)linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(OR_ROOT_INC) --cpp_out=$(OR_ROOT)gen $(OR_ROOT)linear_solver/linear_solver.proto

$(OR_ROOT)gen/linear_solver/linear_solver.pb.h:$(OR_ROOT)gen/linear_solver/linear_solver.pb.cc

$(OR_ROOT)objs/scip_interface.$O:$(OR_ROOT)linear_solver/scip_interface.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)linear_solver/scip_interface.cc $(OBJOUT)objs/scip_interface.$O

$(LIBPREFIX)linear_solver.$(LIBSUFFIX): $(LINEAR_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)linear_solver.$(LIBSUFFIX) $(LINEAR_SOLVER_LIB_OS) $(SCIP_STATIC_LNK)

# Util library.

UTIL_LIB_OS=\
	$(OR_ROOT)objs/bitset.$O \
	$(OR_ROOT)objs/cached_log.$O \
	$(OR_ROOT)objs/const_int_array.$O \
	$(OR_ROOT)objs/graph_export.$O \
	$(OR_ROOT)objs/xml_helper.$O

$(OR_ROOT)objs/bitset.$O:$(OR_ROOT)util/bitset.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)util/bitset.cc $(OBJOUT)objs/bitset.$O

$(OR_ROOT)objs/cached_log.$O:$(OR_ROOT)util/cached_log.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)util/cached_log.cc $(OBJOUT)objs/cached_log.$O

$(OR_ROOT)objs/const_int_array.$O:$(OR_ROOT)util/const_int_array.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)util/const_int_array.cc $(OBJOUT)objs/const_int_array.$O

$(OR_ROOT)objs/graph_export.$O:$(OR_ROOT)util/graph_export.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)util/graph_export.cc $(OBJOUT)objs/graph_export.$O

$(OR_ROOT)objs/xml_helper.$O:$(OR_ROOT)util/xml_helper.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)util/xml_helper.cc $(OBJOUT)objs/xml_helper.$O

$(LIBPREFIX)util.$(LIBSUFFIX): $(UTIL_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)util.$(LIBSUFFIX) $(UTIL_LIB_OS)

# Graph library.

GRAPH_LIB_OS=\
	$(OR_ROOT)objs/linear_assignment.$O \
	$(OR_ROOT)objs/cliques.$O \
	$(OR_ROOT)objs/connectivity.$O \
	$(OR_ROOT)objs/max_flow.$O \
	$(OR_ROOT)objs/min_cost_flow.$O

$(OR_ROOT)objs/linear_assignment.$O:$(OR_ROOT)graph/linear_assignment.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/linear_assignment.cc $(OBJOUT)objs/linear_assignment.$O

$(OR_ROOT)objs/cliques.$O:$(OR_ROOT)graph/cliques.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/cliques.cc $(OBJOUT)objs/cliques.$O

$(OR_ROOT)objs/connectivity.$O:$(OR_ROOT)graph/connectivity.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/connectivity.cc $(OBJOUT)objs/connectivity.$O

$(OR_ROOT)objs/max_flow.$O:$(OR_ROOT)graph/max_flow.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/max_flow.cc $(OBJOUT)objs/max_flow.$O

$(OR_ROOT)objs/min_cost_flow.$O:$(OR_ROOT)graph/min_cost_flow.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/min_cost_flow.cc $(OBJOUT)objs/min_cost_flow.$O

$(LIBPREFIX)graph.$(LIBSUFFIX): $(GRAPH_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)graph.$(LIBSUFFIX) $(GRAPH_LIB_OS)

# Shortestpaths library.

SHORTESTPATHS_LIB_OS=\
	$(OR_ROOT)objs/bellman_ford.$O \
	$(OR_ROOT)objs/dijkstra.$O \
	$(OR_ROOT)objs/shortestpaths.$O

$(OR_ROOT)objs/bellman_ford.$O:$(OR_ROOT)graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/bellman_ford.cc $(OBJOUT)objs/bellman_ford.$O

$(OR_ROOT)objs/dijkstra.$O:$(OR_ROOT)graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/dijkstra.cc $(OBJOUT)objs/dijkstra.$O

$(OR_ROOT)objs/shortestpaths.$O:$(OR_ROOT)graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)graph/shortestpaths.cc $(OBJOUT)objs/shortestpaths.$O

$(LIBPREFIX)shortestpaths.$(LIBSUFFIX): $(SHORTESTPATHS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)shortestpaths.$(LIBSUFFIX) $(SHORTESTPATHS_LIB_OS)

# Routing library.

ROUTING_LIB_OS=\
	$(OR_ROOT)objs/routing.$O

$(OR_ROOT)objs/routing.$O:$(OR_ROOT)constraint_solver/routing.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)constraint_solver/routing.cc $(OBJOUT)objs/routing.$O

$(LIBPREFIX)routing.$(LIBSUFFIX): $(ROUTING_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)routing.$(LIBSUFFIX) $(ROUTING_LIB_OS)

# Algorithms library.

ALGORITHMS_LIB_OS=\
	$(OR_ROOT)objs/hungarian.$O \
	$(OR_ROOT)objs/knapsack_solver.$O

$(OR_ROOT)objs/hungarian.$O:$(OR_ROOT)algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)algorithms/hungarian.cc $(OBJOUT)objs/hungarian.$O

$(OR_ROOT)objs/knapsack_solver.$O:$(OR_ROOT)algorithms/knapsack_solver.cc $(OR_ROOT)gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)algorithms/knapsack_solver.cc $(OBJOUT)objs/knapsack_solver.$O

$(LIBPREFIX)algorithms.$(LIBSUFFIX): $(ALGORITHMS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)algorithms.$(LIBSUFFIX) $(ALGORITHMS_LIB_OS)

# Base library.

BASE_LIB_OS=\
	$(OR_ROOT)objs/bitmap.$O\
	$(OR_ROOT)objs/callback.$O\
	$(OR_ROOT)objs/file.$O\
	$(OR_ROOT)objs/filelinereader.$O\
	$(OR_ROOT)objs/join.$O\
	$(OR_ROOT)objs/logging.$O\
	$(OR_ROOT)objs/random.$O\
	$(OR_ROOT)objs/recordio.$O\
	$(OR_ROOT)objs/split.$O\
	$(OR_ROOT)objs/stringpiece.$O\
	$(OR_ROOT)objs/stringprintf.$O\
	$(OR_ROOT)objs/sysinfo.$O\
	$(OR_ROOT)objs/timer.$O

$(OR_ROOT)objs/bitmap.$O:$(OR_ROOT)base/bitmap.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/bitmap.cc $(OBJOUT)objs/bitmap.$O
$(OR_ROOT)objs/callback.$O:$(OR_ROOT)base/callback.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/callback.cc $(OBJOUT)objs/callback.$O
$(OR_ROOT)objs/file.$O:$(OR_ROOT)base/file.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/file.cc $(OBJOUT)objs/file.$O
$(OR_ROOT)objs/filelinereader.$O:$(OR_ROOT)base/filelinereader.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/filelinereader.cc $(OBJOUT)objs/filelinereader.$O
$(OR_ROOT)objs/logging.$O:$(OR_ROOT)base/logging.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/logging.cc $(OBJOUT)objs/logging.$O
$(OR_ROOT)objs/join.$O:$(OR_ROOT)base/join.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/join.cc $(OBJOUT)objs/join.$O
$(OR_ROOT)objs/random.$O:$(OR_ROOT)base/random.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/random.cc $(OBJOUT)objs/random.$O
$(OR_ROOT)objs/recordio.$O:$(OR_ROOT)base/recordio.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/recordio.cc $(OBJOUT)objs/recordio.$O
$(OR_ROOT)objs/split.$O:$(OR_ROOT)base/split.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/split.cc $(OBJOUT)objs/split.$O
$(OR_ROOT)objs/stringpiece.$O:$(OR_ROOT)base/stringpiece.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/stringpiece.cc $(OBJOUT)objs/stringpiece.$O
$(OR_ROOT)objs/stringprintf.$O:$(OR_ROOT)base/stringprintf.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/stringprintf.cc $(OBJOUT)objs/stringprintf.$O
$(OR_ROOT)objs/sysinfo.$O:$(OR_ROOT)base/sysinfo.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/sysinfo.cc $(OBJOUT)objs/sysinfo.$O
$(OR_ROOT)objs/timer.$O:$(OR_ROOT)base/timer.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)base/timer.cc $(OBJOUT)objs/timer.$O

$(LIBPREFIX)base.$(LIBSUFFIX): $(BASE_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)base.$(LIBSUFFIX) $(BASE_LIB_OS)

# DIMACS challenge problem format library

DIMACS_LIB_OS=\
	$(OR_ROOT)objs/parse_dimacs_assignment.$O\
	$(OR_ROOT)objs/print_dimacs_assignment.$O

$(OR_ROOT)objs/parse_dimacs_assignment.$O:$(OR_ROOT)examples/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/parse_dimacs_assignment.cc $(OBJOUT)objs/parse_dimacs_assignment.$O
$(OR_ROOT)objs/print_dimacs_assignment.$O:$(OR_ROOT)examples/print_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/print_dimacs_assignment.cc $(OBJOUT)objs/print_dimacs_assignment.$O

$(LIBPREFIX)dimacs.$(LIBSUFFIX): $(DIMACS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)dimacs.$(LIBSUFFIX) $(DIMACS_LIB_OS)

# Flow and linear assignment examples

$(OR_ROOT)objs/linear_assignment_api.$O:$(OR_ROOT)examples/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/linear_assignment_api.cc $(OBJOUT)objs/linear_assignment_api.$O

$(BINPREFIX)/linear_assignment_api$E: $(GRAPH_DEPS) $(OR_ROOT)objs/linear_assignment_api.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/linear_assignment_api.$O $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)linear_assignment_api$E

$(OR_ROOT)objs/flow_api.$O:$(OR_ROOT)examples/flow_api.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/flow_api.cc $(OBJOUT)objs/flow_api.$O

$(BINPREFIX)/flow_api$E: $(GRAPH_DEPS) $(OR_ROOT)objs/flow_api.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/flow_api.$O $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)flow_api$E

$(OR_ROOT)objs/dimacs_assignment.$O:$(OR_ROOT)examples/dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/dimacs_assignment.cc $(OBJOUT)objs/dimacs_assignment.$O

$(BINPREFIX)/dimacs_assignment$E: $(ALGORITHMS_DEPS) $(GRAPH_DEPS) $(DIMACS_LIBS) $(OR_ROOT)objs/dimacs_assignment.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/dimacs_assignment.$O $(DIMACS_LNK) $(ALGORITHMS_LNK) $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)dimacs_assignment$E

# Pure CP and Routing Examples

$(OR_ROOT)objs/costas_array.$O: $(OR_ROOT)examples/costas_array.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/costas_array.cc $(OBJOUT)objs/costas_array.$O

$(BINPREFIX)/costas_array$E: $(CP_DEPS) $(OR_ROOT)objs/costas_array.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/costas_array.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)costas_array$E

$(OR_ROOT)objs/cryptarithm.$O:$(OR_ROOT)examples/cryptarithm.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/cryptarithm.cc $(OBJOUT)objs/cryptarithm.$O

$(BINPREFIX)/cryptarithm$E: $(CP_DEPS) $(OR_ROOT)objs/cryptarithm.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/cryptarithm.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)cryptarithm$E

$(OR_ROOT)objs/cvrptw.$O: $(OR_ROOT)examples/cvrptw.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/cvrptw.cc $(OBJOUT)objs/cvrptw.$O

$(BINPREFIX)/cvrptw$E: $(ROUTING_DEPS) $(OR_ROOT)objs/cvrptw.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/cvrptw.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)cvrptw$E

$(OR_ROOT)objs/dobble_ls.$O:$(OR_ROOT)examples/dobble_ls.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/dobble_ls.cc $(OBJOUT)objs/dobble_ls.$O

$(BINPREFIX)/dobble_ls$E: $(CP_DEPS) $(OR_ROOT)objs/dobble_ls.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/dobble_ls.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)dobble_ls$E

$(OR_ROOT)objs/golomb.$O:$(OR_ROOT)examples/golomb.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/golomb.cc $(OBJOUT)objs/golomb.$O

$(BINPREFIX)/golomb$E: $(CP_DEPS) $(OR_ROOT)objs/golomb.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/golomb.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)golomb$E

$(OR_ROOT)objs/jobshop.$O:$(OR_ROOT)examples/jobshop.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/jobshop.cc $(OBJOUT)objs/jobshop.$O

$(BINPREFIX)/jobshop$E: $(CP_DEPS) $(OR_ROOT)objs/jobshop.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/jobshop.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)jobshop$E

$(OR_ROOT)objs/jobshop_ls.$O:$(OR_ROOT)examples/jobshop_ls.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/jobshop_ls.cc $(OBJOUT)objs/jobshop_ls.$O

$(BINPREFIX)/jobshop_ls$E: $(CP_DEPS) $(OR_ROOT)objs/jobshop_ls.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/jobshop_ls.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)jobshop_ls$E

$(OR_ROOT)objs/magic_square.$O:$(OR_ROOT)examples/magic_square.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/magic_square.cc $(OBJOUT)objs/magic_square.$O

$(BINPREFIX)/magic_square$E: $(CP_DEPS) $(OR_ROOT)objs/magic_square.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/magic_square.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)magic_square$E

$(OR_ROOT)objs/model_util.$O:$(OR_ROOT)examples/model_util.cc $(OR_ROOT)gen/constraint_solver/model.pb.h $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/model_util.cc $(OBJOUT)objs/model_util.$O

$(BINPREFIX)/model_util$E: $(CP_DEPS) $(OR_ROOT)objs/model_util.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/model_util.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)model_util$E

$(OR_ROOT)objs/multidim_knapsack.$O:$(OR_ROOT)examples/multidim_knapsack.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/multidim_knapsack.cc $(OBJOUT)objs/multidim_knapsack.$O

$(BINPREFIX)/multidim_knapsack$E: $(CP_DEPS) $(OR_ROOT)objs/multidim_knapsack.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/multidim_knapsack.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)multidim_knapsack$E

$(OR_ROOT)objs/network_routing.$O:$(OR_ROOT)examples/network_routing.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/network_routing.cc $(OBJOUT)objs/network_routing.$O

$(BINPREFIX)/network_routing$E: $(CP_DEPS) $(GRAPH_DEPS) $(OR_ROOT)objs/network_routing.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/network_routing.$O $(CP_LNK) $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)network_routing$E

$(OR_ROOT)objs/nqueens.$O: $(OR_ROOT)examples/nqueens.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/nqueens.cc $(OBJOUT)objs/nqueens.$O

$(BINPREFIX)/nqueens$E: $(CP_DEPS) $(OR_ROOT)objs/nqueens.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/nqueens.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)nqueens$E

$(OR_ROOT)objs/pdptw.$O: $(OR_ROOT)examples/pdptw.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/pdptw.cc $(OBJOUT)objs/pdptw.$O

$(BINPREFIX)/pdptw$E: $(ROUTING_DEPS) $(OR_ROOT)objs/pdptw.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/pdptw.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)pdptw$E

$(OR_ROOT)objs/sports_scheduling.$O:$(OR_ROOT)examples/sports_scheduling.cc $(OR_ROOT)constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/sports_scheduling.cc $(OBJOUT)objs/sports_scheduling.$O

$(BINPREFIX)/sports_scheduling$E: $(CP_DEPS) $(OR_ROOT)objs/sports_scheduling.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/sports_scheduling.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)sports_scheduling$E

$(OR_ROOT)objs/tsp.$O: $(OR_ROOT)examples/tsp.cc $(OR_ROOT)constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/tsp.cc $(OBJOUT)objs/tsp.$O

$(BINPREFIX)/tsp$E: $(ROUTING_DEPS) $(OR_ROOT)objs/tsp.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/tsp.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)tsp$E

# Linear Programming Examples

$(OR_ROOT)objs/strawberry_fields_with_column_generation.$O: $(OR_ROOT)examples/strawberry_fields_with_column_generation.cc $(OR_ROOT)linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/strawberry_fields_with_column_generation.cc $(OBJOUT)objs/strawberry_fields_with_column_generation.$O

$(BINPREFIX)/strawberry_fields_with_column_generation$E: $(LP_DEPS) $(OR_ROOT)objs/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/strawberry_fields_with_column_generation.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)strawberry_fields_with_column_generation$E

$(OR_ROOT)objs/linear_programming.$O: $(OR_ROOT)examples/linear_programming.cc $(OR_ROOT)linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/linear_programming.cc $(OBJOUT)objs/linear_programming.$O

$(BINPREFIX)/linear_programming$E: $(LP_DEPS) $(OR_ROOT)objs/linear_programming.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/linear_programming.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)linear_programming$E

$(OR_ROOT)objs/linear_solver_protocol_buffers.$O: $(OR_ROOT)examples/linear_solver_protocol_buffers.cc $(OR_ROOT)linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/linear_solver_protocol_buffers.cc $(OBJOUT)objs/linear_solver_protocol_buffers.$O

$(BINPREFIX)/linear_solver_protocol_buffers$E: $(LP_DEPS) $(OR_ROOT)objs/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/linear_solver_protocol_buffers.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)linear_solver_protocol_buffers$E

$(OR_ROOT)objs/integer_programming.$O: $(OR_ROOT)examples/integer_programming.cc $(OR_ROOT)linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(OR_ROOT)examples/integer_programming.cc $(OBJOUT)objs/integer_programming.$O

$(BINPREFIX)/integer_programming$E: $(LP_DEPS) $(OR_ROOT)objs/integer_programming.$O
	$(CCC) $(CFLAGS) $(OR_ROOT)objs/integer_programming.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)integer_programming$E
