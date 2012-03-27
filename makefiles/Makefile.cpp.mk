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
	-$(DEL) $(TOP)$Sobjs$S*.$O
	-$(DEL) $(CPBINARIES)
	-$(DEL) $(LPBINARIES)
	-$(DEL) $(TOP)$Sgen$Sconstraint_solver$S*.pb.*
	-$(DEL) $(TOP)$Sgen$Slinear_solver$S*.pb.*
	-$(DEL) $(TOP)$S*.exp

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
	$(TOP)/objs/alldiff_cst.$O\
	$(TOP)/objs/assignment.$O\
	$(TOP)/objs/assignment.pb.$O\
	$(TOP)/objs/collect_variables.$O\
	$(TOP)/objs/constraint_solver.$O\
	$(TOP)/objs/constraints.$O\
	$(TOP)/objs/count_cst.$O\
	$(TOP)/objs/default_search.$O\
	$(TOP)/objs/demon_profiler.$O\
	$(TOP)/objs/demon_profiler.pb.$O\
	$(TOP)/objs/dependency_graph.$O\
	$(TOP)/objs/deviation.$O\
	$(TOP)/objs/element.$O\
	$(TOP)/objs/expr_array.$O\
	$(TOP)/objs/expr_cst.$O\
	$(TOP)/objs/expressions.$O\
	$(TOP)/objs/hybrid.$O\
	$(TOP)/objs/interval.$O\
	$(TOP)/objs/io.$O\
	$(TOP)/objs/local_search.$O\
	$(TOP)/objs/model.pb.$O\
	$(TOP)/objs/model_cache.$O\
	$(TOP)/objs/nogoods.$O\
	$(TOP)/objs/pack.$O\
	$(TOP)/objs/range_cst.$O\
	$(TOP)/objs/resource.$O\
	$(TOP)/objs/sched_search.$O\
	$(TOP)/objs/search.$O\
	$(TOP)/objs/search_limit.pb.$O\
	$(TOP)/objs/table.$O\
	$(TOP)/objs/timetabling.$O\
	$(TOP)/objs/trace.$O\
	$(TOP)/objs/tree_monitor.$O\
	$(TOP)/objs/utilities.$O \
	$(TOP)/objs/visitor.$O

$(TOP)/objs/alldiff_cst.$O:$(TOP)/constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/alldiff_cst.cc $(OBJOUT)objs/alldiff_cst.$O

$(TOP)/objs/assignment.$O:$(TOP)/constraint_solver/assignment.cc $(TOP)/gen/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/assignment.cc $(OBJOUT)objs/assignment.$O

$(TOP)/objs/assignment.pb.$O:$(TOP)/gen/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/constraint_solver/assignment.pb.cc $(OBJOUT)objs/assignment.pb.$O

$(TOP)/gen/constraint_solver/assignment.pb.cc:$(TOP)/constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(TOP) --cpp_out=$(TOP)/gen $(TOP)/constraint_solver/assignment.proto

$(TOP)/gen/constraint_solver/assignment.pb.h:$(TOP)/gen/constraint_solver/assignment.pb.cc

$(TOP)/objs/collect_variables.$O:$(TOP)/constraint_solver/collect_variables.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/collect_variables.cc $(OBJOUT)objs/collect_variables.$O

$(TOP)/objs/constraint_solver.$O:$(TOP)/constraint_solver/constraint_solver.cc $(TOP)/gen/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/constraint_solver.cc $(OBJOUT)objs/constraint_solver.$O

$(TOP)/objs/constraints.$O:$(TOP)/constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/constraints.cc $(OBJOUT)objs/constraints.$O

$(TOP)/objs/count_cst.$O:$(TOP)/constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/count_cst.cc $(OBJOUT)objs/count_cst.$O

$(TOP)/objs/default_search.$O:$(TOP)/constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/default_search.cc $(OBJOUT)objs/default_search.$O

$(TOP)/objs/demon_profiler.$O:$(TOP)/constraint_solver/demon_profiler.cc $(TOP)/gen/constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/demon_profiler.cc $(OBJOUT)objs/demon_profiler.$O

$(TOP)/objs/demon_profiler.pb.$O:$(TOP)/gen/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/constraint_solver/demon_profiler.pb.cc $(OBJOUT)objs/demon_profiler.pb.$O

$(TOP)/gen/constraint_solver/demon_profiler.pb.cc:$(TOP)/constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(TOP) --cpp_out=$(TOP)/gen $(TOP)/constraint_solver/demon_profiler.proto

$(TOP)/gen/constraint_solver/demon_profiler.pb.h:$(TOP)/gen/constraint_solver/demon_profiler.pb.cc

$(TOP)/objs/dependency_graph.$O:$(TOP)/constraint_solver/dependency_graph.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/dependency_graph.cc $(OBJOUT)objs/dependency_graph.$O

$(TOP)/objs/deviation.$O:$(TOP)/constraint_solver/deviation.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/deviation.cc $(OBJOUT)objs/deviation.$O

$(TOP)/objs/element.$O:$(TOP)/constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/element.cc $(OBJOUT)objs/element.$O

$(TOP)/objs/expr_array.$O:$(TOP)/constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/expr_array.cc $(OBJOUT)objs/expr_array.$O

$(TOP)/objs/expr_cst.$O:$(TOP)/constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/expr_cst.cc $(OBJOUT)objs/expr_cst.$O

$(TOP)/objs/expressions.$O:$(TOP)/constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/expressions.cc $(OBJOUT)objs/expressions.$O

$(TOP)/objs/hybrid.$O:$(TOP)/constraint_solver/hybrid.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/hybrid.cc $(OBJOUT)objs/hybrid.$O

$(TOP)/objs/interval.$O:$(TOP)/constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/interval.cc $(OBJOUT)objs/interval.$O

$(TOP)/objs/io.$O:$(TOP)/constraint_solver/io.cc $(TOP)/gen/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/io.cc $(OBJOUT)objs/io.$O

$(TOP)/objs/local_search.$O:$(TOP)/constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/local_search.cc $(OBJOUT)objs/local_search.$O

$(TOP)/objs/model.pb.$O:$(TOP)/gen/constraint_solver/model.pb.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/constraint_solver/model.pb.cc $(OBJOUT)objs/model.pb.$O

$(TOP)/objs/model_cache.$O:$(TOP)/constraint_solver/model_cache.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/model_cache.cc $(OBJOUT)objs/model_cache.$O

$(TOP)/gen/constraint_solver/model.pb.cc:$(TOP)/constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(TOP) --cpp_out=$(TOP)/gen $(TOP)/constraint_solver/model.proto

$(TOP)/gen/constraint_solver/model.pb.h:$(TOP)/gen/constraint_solver/model.pb.cc $(TOP)/gen/constraint_solver/search_limit.pb.h

$(TOP)/objs/nogoods.$O:$(TOP)/constraint_solver/nogoods.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/nogoods.cc $(OBJOUT)objs/nogoods.$O

$(TOP)/objs/pack.$O:$(TOP)/constraint_solver/pack.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/pack.cc $(OBJOUT)objs/pack.$O

$(TOP)/objs/range_cst.$O:$(TOP)/constraint_solver/range_cst.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/range_cst.cc $(OBJOUT)objs/range_cst.$O

$(TOP)/objs/resource.$O:$(TOP)/constraint_solver/resource.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/resource.cc $(OBJOUT)objs/resource.$O

$(TOP)/objs/sched_search.$O:$(TOP)/constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/sched_search.cc $(OBJOUT)objs/sched_search.$O

$(TOP)/objs/search.$O:$(TOP)/constraint_solver/search.cc $(TOP)/gen/constraint_solver/search_limit.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/search.cc $(OBJOUT)objs/search.$O

$(TOP)/objs/search_limit.pb.$O:$(TOP)/gen/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/constraint_solver/search_limit.pb.cc $(OBJOUT)objs/search_limit.pb.$O

$(TOP)/gen/constraint_solver/search_limit.pb.cc:$(TOP)/constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(TOP) --cpp_out=$(TOP)/gen $(TOP)/constraint_solver/search_limit.proto

$(TOP)/gen/constraint_solver/search_limit.pb.h:$(TOP)/gen/constraint_solver/search_limit.pb.cc

$(TOP)/objs/table.$O:$(TOP)/constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/table.cc $(OBJOUT)objs/table.$O

$(TOP)/objs/timetabling.$O:$(TOP)/constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/timetabling.cc $(OBJOUT)objs/timetabling.$O

$(TOP)/objs/trace.$O:$(TOP)/constraint_solver/trace.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/trace.cc $(OBJOUT)objs/trace.$O

$(TOP)/objs/tree_monitor.$O:$(TOP)/constraint_solver/tree_monitor.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/tree_monitor.cc $(OBJOUT)objs/tree_monitor.$O

$(TOP)/objs/utilities.$O:$(TOP)/constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/utilities.cc $(OBJOUT)objs/utilities.$O

$(TOP)/objs/visitor.$O:$(TOP)/constraint_solver/visitor.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/visitor.cc $(OBJOUT)objs/visitor.$O

$(LIBPREFIX)constraint_solver.$(LIBSUFFIX): $(CONSTRAINT_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)constraint_solver.$(LIBSUFFIX) $(CONSTRAINT_SOLVER_LIB_OS)

# Linear Solver Library

LINEAR_SOLVER_LIB_OS = \
	$(TOP)/objs/cbc_interface.$O \
	$(TOP)/objs/clp_interface.$O \
	$(TOP)/objs/glpk_interface.$O \
	$(TOP)/objs/linear_solver.$O \
	$(TOP)/objs/linear_solver.pb.$O \
	$(TOP)/objs/scip_interface.$O

$(TOP)/objs/cbc_interface.$O:$(TOP)/linear_solver/cbc_interface.cc
	$(CCC) $(CFLAGS) -c $(TOP)/linear_solver/cbc_interface.cc $(OBJOUT)objs/cbc_interface.$O

$(TOP)/objs/clp_interface.$O:$(TOP)/linear_solver/clp_interface.cc
	$(CCC) $(CFLAGS) -c $(TOP)/linear_solver/clp_interface.cc $(OBJOUT)objs/clp_interface.$O

$(TOP)/objs/glpk_interface.$O:$(TOP)/linear_solver/glpk_interface.cc
	$(CCC) $(CFLAGS) -c $(TOP)/linear_solver/glpk_interface.cc $(OBJOUT)objs/glpk_interface.$O

$(TOP)/objs/linear_solver.$O:$(TOP)/linear_solver/linear_solver.cc $(TOP)/gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/linear_solver/linear_solver.cc $(OBJOUT)objs/linear_solver.$O

$(TOP)/objs/linear_solver.pb.$O:$(TOP)/gen/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c $(TOP)/gen/linear_solver/linear_solver.pb.cc $(OBJOUT)objs/linear_solver.pb.$O

$(TOP)/gen/linear_solver/linear_solver.pb.cc:$(TOP)/linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=$(TOP) --cpp_out=$(TOP)/gen $(TOP)/linear_solver/linear_solver.proto

$(TOP)/gen/linear_solver/linear_solver.pb.h:$(TOP)/gen/linear_solver/linear_solver.pb.cc

$(TOP)/objs/scip_interface.$O:$(TOP)/linear_solver/scip_interface.cc
	$(CCC) $(CFLAGS) -c $(TOP)/linear_solver/scip_interface.cc $(OBJOUT)objs/scip_interface.$O

$(LIBPREFIX)linear_solver.$(LIBSUFFIX): $(LINEAR_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)linear_solver.$(LIBSUFFIX) $(LINEAR_SOLVER_LIB_OS) $(SCIP_STATIC_LNK)

# Util library.

UTIL_LIB_OS=\
	$(TOP)/objs/bitset.$O \
	$(TOP)/objs/cached_log.$O \
	$(TOP)/objs/const_int_array.$O \
	$(TOP)/objs/graph_export.$O \
	$(TOP)/objs/xml_helper.$O

$(TOP)/objs/bitset.$O:$(TOP)/util/bitset.cc
	$(CCC) $(CFLAGS) -c $(TOP)/util/bitset.cc $(OBJOUT)objs/bitset.$O

$(TOP)/objs/cached_log.$O:$(TOP)/util/cached_log.cc
	$(CCC) $(CFLAGS) -c $(TOP)/util/cached_log.cc $(OBJOUT)objs/cached_log.$O

$(TOP)/objs/const_int_array.$O:$(TOP)/util/const_int_array.cc
	$(CCC) $(CFLAGS) -c $(TOP)/util/const_int_array.cc $(OBJOUT)objs/const_int_array.$O

$(TOP)/objs/graph_export.$O:$(TOP)/util/graph_export.cc
	$(CCC) $(CFLAGS) -c $(TOP)/util/graph_export.cc $(OBJOUT)objs/graph_export.$O

$(TOP)/objs/xml_helper.$O:$(TOP)/util/xml_helper.cc
	$(CCC) $(CFLAGS) -c $(TOP)/util/xml_helper.cc $(OBJOUT)objs/xml_helper.$O

$(LIBPREFIX)util.$(LIBSUFFIX): $(UTIL_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)util.$(LIBSUFFIX) $(UTIL_LIB_OS)

# Graph library.

GRAPH_LIB_OS=\
	$(TOP)/objs/linear_assignment.$O \
	$(TOP)/objs/cliques.$O \
	$(TOP)/objs/connectivity.$O \
	$(TOP)/objs/max_flow.$O \
	$(TOP)/objs/min_cost_flow.$O

$(TOP)/objs/linear_assignment.$O:$(TOP)/graph/linear_assignment.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/linear_assignment.cc $(OBJOUT)objs/linear_assignment.$O

$(TOP)/objs/cliques.$O:$(TOP)/graph/cliques.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/cliques.cc $(OBJOUT)objs/cliques.$O

$(TOP)/objs/connectivity.$O:$(TOP)/graph/connectivity.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/connectivity.cc $(OBJOUT)objs/connectivity.$O

$(TOP)/objs/max_flow.$O:$(TOP)/graph/max_flow.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/max_flow.cc $(OBJOUT)objs/max_flow.$O

$(TOP)/objs/min_cost_flow.$O:$(TOP)/graph/min_cost_flow.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/min_cost_flow.cc $(OBJOUT)objs/min_cost_flow.$O

$(LIBPREFIX)graph.$(LIBSUFFIX): $(GRAPH_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)graph.$(LIBSUFFIX) $(GRAPH_LIB_OS)

# Shortestpaths library.

SHORTESTPATHS_LIB_OS=\
	$(TOP)/objs/bellman_ford.$O \
	$(TOP)/objs/dijkstra.$O \
	$(TOP)/objs/shortestpaths.$O

$(TOP)/objs/bellman_ford.$O:$(TOP)/graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/bellman_ford.cc $(OBJOUT)objs/bellman_ford.$O

$(TOP)/objs/dijkstra.$O:$(TOP)/graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/dijkstra.cc $(OBJOUT)objs/dijkstra.$O

$(TOP)/objs/shortestpaths.$O:$(TOP)/graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c $(TOP)/graph/shortestpaths.cc $(OBJOUT)objs/shortestpaths.$O

$(LIBPREFIX)shortestpaths.$(LIBSUFFIX): $(SHORTESTPATHS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)shortestpaths.$(LIBSUFFIX) $(SHORTESTPATHS_LIB_OS)

# Routing library.

ROUTING_LIB_OS=\
	$(TOP)/objs/routing.$O

$(TOP)/objs/routing.$O:$(TOP)/constraint_solver/routing.cc
	$(CCC) $(CFLAGS) -c $(TOP)/constraint_solver/routing.cc $(OBJOUT)objs/routing.$O

$(LIBPREFIX)routing.$(LIBSUFFIX): $(ROUTING_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)routing.$(LIBSUFFIX) $(ROUTING_LIB_OS)

# Algorithms library.

ALGORITHMS_LIB_OS=\
	$(TOP)/objs/hungarian.$O \
	$(TOP)/objs/knapsack_solver.$O

$(TOP)/objs/hungarian.$O:$(TOP)/algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c $(TOP)/algorithms/hungarian.cc $(OBJOUT)objs/hungarian.$O

$(TOP)/objs/knapsack_solver.$O:$(TOP)/algorithms/knapsack_solver.cc $(TOP)/gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c $(TOP)/algorithms/knapsack_solver.cc $(OBJOUT)objs/knapsack_solver.$O

$(LIBPREFIX)algorithms.$(LIBSUFFIX): $(ALGORITHMS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)algorithms.$(LIBSUFFIX) $(ALGORITHMS_LIB_OS)

# Base library.

BASE_LIB_OS=\
	$(TOP)/objs/bitmap.$O\
	$(TOP)/objs/callback.$O\
	$(TOP)/objs/file.$O\
	$(TOP)/objs/filelinereader.$O\
	$(TOP)/objs/join.$O\
	$(TOP)/objs/logging.$O\
	$(TOP)/objs/random.$O\
	$(TOP)/objs/recordio.$O\
	$(TOP)/objs/split.$O\
	$(TOP)/objs/stringpiece.$O\
	$(TOP)/objs/stringprintf.$O\
	$(TOP)/objs/sysinfo.$O\
	$(TOP)/objs/timer.$O

$(TOP)/objs/bitmap.$O:$(TOP)/base/bitmap.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/bitmap.cc $(OBJOUT)objs/bitmap.$O
$(TOP)/objs/callback.$O:$(TOP)/base/callback.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/callback.cc $(OBJOUT)objs/callback.$O
$(TOP)/objs/file.$O:$(TOP)/base/file.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/file.cc $(OBJOUT)objs/file.$O
$(TOP)/objs/filelinereader.$O:$(TOP)/base/filelinereader.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/filelinereader.cc $(OBJOUT)objs/filelinereader.$O
$(TOP)/objs/logging.$O:$(TOP)/base/logging.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/logging.cc $(OBJOUT)objs/logging.$O
$(TOP)/objs/join.$O:$(TOP)/base/join.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/join.cc $(OBJOUT)objs/join.$O
$(TOP)/objs/random.$O:$(TOP)/base/random.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/random.cc $(OBJOUT)objs/random.$O
$(TOP)/objs/recordio.$O:$(TOP)/base/recordio.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/recordio.cc $(OBJOUT)objs/recordio.$O
$(TOP)/objs/split.$O:$(TOP)/base/split.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/split.cc $(OBJOUT)objs/split.$O
$(TOP)/objs/stringpiece.$O:$(TOP)/base/stringpiece.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/stringpiece.cc $(OBJOUT)objs/stringpiece.$O
$(TOP)/objs/stringprintf.$O:$(TOP)/base/stringprintf.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/stringprintf.cc $(OBJOUT)objs/stringprintf.$O
$(TOP)/objs/sysinfo.$O:$(TOP)/base/sysinfo.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/sysinfo.cc $(OBJOUT)objs/sysinfo.$O
$(TOP)/objs/timer.$O:$(TOP)/base/timer.cc
	$(CCC) $(CFLAGS) -c $(TOP)/base/timer.cc $(OBJOUT)objs/timer.$O

$(LIBPREFIX)base.$(LIBSUFFIX): $(BASE_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)base.$(LIBSUFFIX) $(BASE_LIB_OS)

# DIMACS challenge problem format library

DIMACS_LIB_OS=\
	$(TOP)/objs/parse_dimacs_assignment.$O\
	$(TOP)/objs/print_dimacs_assignment.$O

$(TOP)/objs/parse_dimacs_assignment.$O:$(TOP)/examples/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(TOP)/examples/parse_dimacs_assignment.cc $(OBJOUT)objs/parse_dimacs_assignment.$O
$(TOP)/objs/print_dimacs_assignment.$O:$(TOP)/examples/print_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(TOP)/examples/print_dimacs_assignment.cc $(OBJOUT)objs/print_dimacs_assignment.$O

$(LIBPREFIX)dimacs.$(LIBSUFFIX): $(DIMACS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)dimacs.$(LIBSUFFIX) $(DIMACS_LIB_OS)

# Flow and linear assignment examples

$(TOP)/objs/linear_assignment_api.$O:$(TOP)/examples/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c $(TOP)/examples/linear_assignment_api.cc $(OBJOUT)objs/linear_assignment_api.$O

$(BINPREFIX)/linear_assignment_api$E: $(GRAPH_DEPS) $(TOP)/objs/linear_assignment_api.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/linear_assignment_api.$O $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)linear_assignment_api$E

$(TOP)/objs/flow_api.$O:$(TOP)/examples/flow_api.cc
	$(CCC) $(CFLAGS) -c $(TOP)/examples/flow_api.cc $(OBJOUT)objs/flow_api.$O

$(BINPREFIX)/flow_api$E: $(GRAPH_DEPS) $(TOP)/objs/flow_api.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/flow_api.$O $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)flow_api$E

$(TOP)/objs/dimacs_assignment.$O:$(TOP)/examples/dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c $(TOP)/examples/dimacs_assignment.cc $(OBJOUT)objs/dimacs_assignment.$O

$(BINPREFIX)/dimacs_assignment$E: $(ALGORITHMS_DEPS) $(GRAPH_DEPS) $(DIMACS_LIBS) $(TOP)/objs/dimacs_assignment.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/dimacs_assignment.$O $(DIMACS_LNK) $(ALGORITHMS_LNK) $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)dimacs_assignment$E

# Pure CP and Routing Examples

$(TOP)/objs/costas_array.$O: $(TOP)/examples/costas_array.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/costas_array.cc $(OBJOUT)objs/costas_array.$O

$(BINPREFIX)/costas_array$E: $(CP_DEPS) $(TOP)/objs/costas_array.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/costas_array.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)costas_array$E

$(TOP)/objs/cryptarithm.$O:$(TOP)/examples/cryptarithm.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/cryptarithm.cc $(OBJOUT)objs/cryptarithm.$O

$(BINPREFIX)/cryptarithm$E: $(CP_DEPS) $(TOP)/objs/cryptarithm.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/cryptarithm.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)cryptarithm$E

$(TOP)/objs/cvrptw.$O: $(TOP)/examples/cvrptw.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/cvrptw.cc $(OBJOUT)objs/cvrptw.$O

$(BINPREFIX)/cvrptw$E: $(ROUTING_DEPS) $(TOP)/objs/cvrptw.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/cvrptw.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)cvrptw$E

$(TOP)/objs/dobble_ls.$O:$(TOP)/examples/dobble_ls.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/dobble_ls.cc $(OBJOUT)objs/dobble_ls.$O

$(BINPREFIX)/dobble_ls$E: $(CP_DEPS) $(TOP)/objs/dobble_ls.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/dobble_ls.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)dobble_ls$E

$(TOP)/objs/golomb.$O:$(TOP)/examples/golomb.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/golomb.cc $(OBJOUT)objs/golomb.$O

$(BINPREFIX)/golomb$E: $(CP_DEPS) $(TOP)/objs/golomb.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/golomb.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)golomb$E

$(TOP)/objs/jobshop.$O:$(TOP)/examples/jobshop.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/jobshop.cc $(OBJOUT)objs/jobshop.$O

$(BINPREFIX)/jobshop$E: $(CP_DEPS) $(TOP)/objs/jobshop.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/jobshop.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)jobshop$E

$(TOP)/objs/jobshop_ls.$O:$(TOP)/examples/jobshop_ls.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/jobshop_ls.cc $(OBJOUT)objs/jobshop_ls.$O

$(BINPREFIX)/jobshop_ls$E: $(CP_DEPS) $(TOP)/objs/jobshop_ls.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/jobshop_ls.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)jobshop_ls$E

$(TOP)/objs/magic_square.$O:$(TOP)/examples/magic_square.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/magic_square.cc $(OBJOUT)objs/magic_square.$O

$(BINPREFIX)/magic_square$E: $(CP_DEPS) $(TOP)/objs/magic_square.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/magic_square.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)magic_square$E

$(TOP)/objs/model_util.$O:$(TOP)/examples/model_util.cc $(TOP)/gen/constraint_solver/model.pb.h $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/model_util.cc $(OBJOUT)objs/model_util.$O

$(BINPREFIX)/model_util$E: $(CP_DEPS) $(TOP)/objs/model_util.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/model_util.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)model_util$E

$(TOP)/objs/multidim_knapsack.$O:$(TOP)/examples/multidim_knapsack.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/multidim_knapsack.cc $(OBJOUT)objs/multidim_knapsack.$O

$(BINPREFIX)/multidim_knapsack$E: $(CP_DEPS) $(TOP)/objs/multidim_knapsack.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/multidim_knapsack.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)multidim_knapsack$E

$(TOP)/objs/network_routing.$O:$(TOP)/examples/network_routing.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/network_routing.cc $(OBJOUT)objs/network_routing.$O

$(BINPREFIX)/network_routing$E: $(CP_DEPS) $(GRAPH_DEPS) $(TOP)/objs/network_routing.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/network_routing.$O $(CP_LNK) $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)network_routing$E

$(TOP)/objs/nqueens.$O: $(TOP)/examples/nqueens.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/nqueens.cc $(OBJOUT)objs/nqueens.$O

$(BINPREFIX)/nqueens$E: $(CP_DEPS) $(TOP)/objs/nqueens.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/nqueens.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)nqueens$E

$(TOP)/objs/pdptw.$O: $(TOP)/examples/pdptw.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/pdptw.cc $(OBJOUT)objs/pdptw.$O

$(BINPREFIX)/pdptw$E: $(ROUTING_DEPS) $(TOP)/objs/pdptw.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/pdptw.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)pdptw$E

$(TOP)/objs/sports_scheduling.$O:$(TOP)/examples/sports_scheduling.cc $(TOP)/constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/sports_scheduling.cc $(OBJOUT)objs/sports_scheduling.$O

$(BINPREFIX)/sports_scheduling$E: $(CP_DEPS) $(TOP)/objs/sports_scheduling.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/sports_scheduling.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)sports_scheduling$E

$(TOP)/objs/tsp.$O: $(TOP)/examples/tsp.cc $(TOP)/constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/tsp.cc $(OBJOUT)objs/tsp.$O

$(BINPREFIX)/tsp$E: $(ROUTING_DEPS) $(TOP)/objs/tsp.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/tsp.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)tsp$E

# Linear Programming Examples

$(TOP)/objs/strawberry_fields_with_column_generation.$O: $(TOP)/examples/strawberry_fields_with_column_generation.cc $(TOP)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/strawberry_fields_with_column_generation.cc $(OBJOUT)objs/strawberry_fields_with_column_generation.$O

$(BINPREFIX)/strawberry_fields_with_column_generation$E: $(LP_DEPS) $(TOP)/objs/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/strawberry_fields_with_column_generation.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)strawberry_fields_with_column_generation$E

$(TOP)/objs/linear_programming.$O: $(TOP)/examples/linear_programming.cc $(TOP)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/linear_programming.cc $(OBJOUT)objs/linear_programming.$O

$(BINPREFIX)/linear_programming$E: $(LP_DEPS) $(TOP)/objs/linear_programming.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/linear_programming.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)linear_programming$E

$(TOP)/objs/linear_solver_protocol_buffers.$O: $(TOP)/examples/linear_solver_protocol_buffers.cc $(TOP)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/linear_solver_protocol_buffers.cc $(OBJOUT)objs/linear_solver_protocol_buffers.$O

$(BINPREFIX)/linear_solver_protocol_buffers$E: $(LP_DEPS) $(TOP)/objs/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/linear_solver_protocol_buffers.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)linear_solver_protocol_buffers$E

$(TOP)/objs/integer_programming.$O: $(TOP)/examples/integer_programming.cc $(TOP)/linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c $(TOP)/examples/integer_programming.cc $(OBJOUT)objs/integer_programming.$O

$(BINPREFIX)/integer_programming$E: $(LP_DEPS) $(TOP)/objs/integer_programming.$O
	$(CCC) $(CFLAGS) $(TOP)/objs/integer_programming.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)integer_programming$E
