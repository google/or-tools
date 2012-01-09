# List libraries by module.

BASE_LIBS = \
	$(LIBPREFIX)util.$(LIBSUFFIX)          \
	$(LIBPREFIX)base.$(LIBSUFFIX)

LP_LIBS = \
	$(LIBPREFIX)linear_solver.$(LIBSUFFIX)

ALGORITHMS_LIBS = \
	$(LIBPREFIX)algorithms.$(LIBSUFFIX) \

CP_LIBS = \
	$(LIBPREFIX)constraint_solver.$(LIBSUFFIX)

GRAPH_LIBS = \
	$(LIBPREFIX)graph.$(LIBSUFFIX) \
	$(LIBPREFIX)shortestpaths.$(LIBSUFFIX)

ROUTING_LIBS = \
        $(LIBPREFIX)routing.$(LIBSUFFIX) \
        $(LIBPREFIX)graph.$(LIBSUFFIX) \
        $(LIBPREFIX)constraint_solver.$(LIBSUFFIX) \

# Lib dependencies.
BASE_DEPS = $(BASE_LIBS)

LP_DEPS = $(LP_LIBS) $(BASE_LIBS)

ALGORITHMS_DEPS = $(ALGORITHMS_LIBS) $(LP_LIBS) $(BASE_LIBS)

CP_DEPS = $(CP_LIBS) $(LP_LIBS) $(BASE_LIBS)

GRAPH_DEPS = $(GRAPH_LIBS) $(BASE_LIBS)

ROUTING_DEPS = $(ROUTING_LIBS) $(CP_LIBS) $(GRAPH_LIBS) $(BASE_LIBS)

# Create link commands.
BASE_LNK = \
	$(PRE_LIB)util$(POST_LIB) \
	$(PRE_LIB)base$(POST_LIB)

LP_LNK = \
	$(PRE_LIB)linear_solver$(POST_LIB) \
	$(BASE_LNK) \
	$(LDLPDEPS)

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

graphlibs: $(GRAPH_DEPS)

cplibs: $(CP_DEPS)

lplibs: $(LP_DEPS)

CPBINARIES = \
	costas_array$E \
	cryptarithm$E \
        cvrptw$E \
	flow_api$E \
	golomb$E \
	jobshop$E \
	jobshop_ls$E \
	linear_assignment_api$E \
	magic_square$E \
	model_util$E \
	multidim_knapsack$E \
	network_routing$E \
	nqueens$E \
	pdptw$E \
	dimacs_assignment$E \
	sports_scheduling$E \
	tsp$E

cpexe: $(CPBINARIES)

LPBINARIES = \
	integer_programming$E \
	linear_programming$E \
	linear_solver_protocol_buffers$E \
	strawberry_fields_with_column_generation$E


lpexe: $(LPBINARIES)

algorithmslibs: $(ALGORITHMS_DEPS)

DIMACS_LIBS = \
	$(LIBPREFIX)dimacs.$(LIBSUFFIX)

DIMACS_LNK = $(PRE_LIB)dimacs

dimacslibs: $(DIMACS_LIBS)

clean:
	$(DEL) *.$(LIBSUFFIX)
	$(DEL) objs$S*.$O
	$(DEL) $(CPBINARIES)
	$(DEL) $(LPBINARIES)
	$(DEL) gen$Salgorithms$S*wrap*
	$(DEL) gen$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java
	$(DEL) gen$Scom$Sgoogle$Sortools$Sflow$S*.java
	$(DEL) gen$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.java
	$(DEL) gen$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	$(DEL) gen$Sconstraint_solver$S*.pb.*
	$(DEL) gen$Sconstraint_solver$S*wrap*
	$(DEL) gen$Sgraph$S*wrap*
	$(DEL) gen$Slinear_solver$S*.pb.*
	$(DEL) gen$Slinear_solver$S*wrap*
	$(DEL) objs$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.class
	$(DEL) objs$Scom$Sgoogle$Sortools$Sflow$S*.class
	$(DEL) objs$Scom$Sgoogle$Sortools$Sknapsacksolver$S*.class
	$(DEL) objs$Scom$Sgoogle$Sortools$Slinearsolver$S*.class
	$(DEL) *.$(SHAREDLIBEXT)
	$(DEL) *.$(JNILIBEXT)
	$(DEL) *.jar
	$(DEL) *.pdb
	$(DEL) *.exp
	$(DEL) cs*.exe
	$(DEL) Google.*.netmodule
	$(DEL) Google.*.lib

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OS = \
	objs/alldiff_cst.$O\
	objs/assignment.$O\
	objs/assignment.pb.$O\
	objs/collect_variables.$O\
	objs/constraint_solver.$O\
	objs/constraints.$O\
	objs/count_cst.$O\
	objs/default_search.$O\
	objs/demon_profiler.$O\
	objs/demon_profiler.pb.$O\
	objs/dependency_graph.$O\
	objs/deviation.$O\
	objs/element.$O\
	objs/expr_array.$O\
	objs/expr_cst.$O\
	objs/expressions.$O\
	objs/hybrid.$O\
	objs/interval.$O\
	objs/io.$O\
	objs/local_search.$O\
	objs/model.pb.$O\
	objs/model_cache.$O\
	objs/nogoods.$O\
	objs/pack.$O\
	objs/range_cst.$O\
	objs/resource.$O\
	objs/sched_search.$O\
	objs/search.$O\
	objs/search_limit.pb.$O\
	objs/table.$O\
	objs/timetabling.$O\
	objs/trace.$O\
	objs/tree_monitor.$O\
	objs/utilities.$O

objs/alldiff_cst.$O:constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/alldiff_cst.cc $(OBJOUT)objs/alldiff_cst.$O

objs/assignment.$O:constraint_solver/assignment.cc gen/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/assignment.cc $(OBJOUT)objs/assignment.$O

objs/assignment.pb.$O:gen/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/assignment.pb.cc $(OBJOUT)objs/assignment.pb.$O

gen/constraint_solver/assignment.pb.cc:constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=. --cpp_out=gen constraint_solver/assignment.proto

gen/constraint_solver/assignment.pb.h:gen/constraint_solver/assignment.pb.cc

objs/collect_variables.$O:constraint_solver/collect_variables.cc
	$(CCC) $(CFLAGS) -c constraint_solver/collect_variables.cc $(OBJOUT)objs/collect_variables.$O

objs/constraint_solver.$O:constraint_solver/constraint_solver.cc gen/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/constraint_solver.cc $(OBJOUT)objs/constraint_solver.$O

objs/constraints.$O:constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraints.cc $(OBJOUT)objs/constraints.$O

objs/count_cst.$O:constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/count_cst.cc $(OBJOUT)objs/count_cst.$O

objs/default_search.$O:constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/default_search.cc $(OBJOUT)objs/default_search.$O

objs/demon_profiler.$O:constraint_solver/demon_profiler.cc gen/constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/demon_profiler.cc $(OBJOUT)objs/demon_profiler.$O

objs/demon_profiler.pb.$O:gen/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/demon_profiler.pb.cc $(OBJOUT)objs/demon_profiler.pb.$O

gen/constraint_solver/demon_profiler.pb.cc:constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=. --cpp_out=gen constraint_solver/demon_profiler.proto

gen/constraint_solver/demon_profiler.pb.h:gen/constraint_solver/demon_profiler.pb.cc

objs/dependency_graph.$O:constraint_solver/dependency_graph.cc
	$(CCC) $(CFLAGS) -c constraint_solver/dependency_graph.cc $(OBJOUT)objs/dependency_graph.$O

objs/deviation.$O:constraint_solver/deviation.cc
	$(CCC) $(CFLAGS) -c constraint_solver/deviation.cc $(OBJOUT)objs/deviation.$O

objs/element.$O:constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c constraint_solver/element.cc $(OBJOUT)objs/element.$O

objs/expr_array.$O:constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expr_array.cc $(OBJOUT)objs/expr_array.$O

objs/expr_cst.$O:constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expr_cst.cc $(OBJOUT)objs/expr_cst.$O

objs/expressions.$O:constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expressions.cc $(OBJOUT)objs/expressions.$O

objs/hybrid.$O:constraint_solver/hybrid.cc
	$(CCC) $(CFLAGS) -c constraint_solver/hybrid.cc $(OBJOUT)objs/hybrid.$O

objs/interval.$O:constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c constraint_solver/interval.cc $(OBJOUT)objs/interval.$O

objs/io.$O:constraint_solver/io.cc gen/constraint_solver/model.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/io.cc $(OBJOUT)objs/io.$O

objs/local_search.$O:constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/local_search.cc $(OBJOUT)objs/local_search.$O

objs/model.pb.$O:gen/constraint_solver/model.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/model.pb.cc $(OBJOUT)objs/model.pb.$O

objs/model_cache.$O:constraint_solver/model_cache.cc
	$(CCC) $(CFLAGS) -c constraint_solver/model_cache.cc $(OBJOUT)objs/model_cache.$O

gen/constraint_solver/model.pb.cc:constraint_solver/model.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=. --cpp_out=gen constraint_solver/model.proto

gen/constraint_solver/model.pb.h:gen/constraint_solver/model.pb.cc gen/constraint_solver/search_limit.pb.h

objs/nogoods.$O:constraint_solver/nogoods.cc
	$(CCC) $(CFLAGS) -c constraint_solver/nogoods.cc $(OBJOUT)objs/nogoods.$O

objs/pack.$O:constraint_solver/pack.cc
	$(CCC) $(CFLAGS) -c constraint_solver/pack.cc $(OBJOUT)objs/pack.$O

objs/range_cst.$O:constraint_solver/range_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/range_cst.cc $(OBJOUT)objs/range_cst.$O

objs/resource.$O:constraint_solver/resource.cc
	$(CCC) $(CFLAGS) -c constraint_solver/resource.cc $(OBJOUT)objs/resource.$O

objs/sched_search.$O:constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/sched_search.cc $(OBJOUT)objs/sched_search.$O

objs/search.$O:constraint_solver/search.cc gen/constraint_solver/search_limit.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/search.cc $(OBJOUT)objs/search.$O

objs/search_limit.pb.$O:gen/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/search_limit.pb.cc $(OBJOUT)objs/search_limit.pb.$O

gen/constraint_solver/search_limit.pb.cc:constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=. --cpp_out=gen constraint_solver/search_limit.proto

gen/constraint_solver/search_limit.pb.h:gen/constraint_solver/search_limit.pb.cc

objs/table.$O:constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c constraint_solver/table.cc $(OBJOUT)objs/table.$O

objs/timetabling.$O:constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c constraint_solver/timetabling.cc $(OBJOUT)objs/timetabling.$O

objs/trace.$O:constraint_solver/trace.cc
	$(CCC) $(CFLAGS) -c constraint_solver/trace.cc $(OBJOUT)objs/trace.$O

objs/tree_monitor.$O:constraint_solver/tree_monitor.cc
	$(CCC) $(CFLAGS) -c constraint_solver/tree_monitor.cc $(OBJOUT)objs/tree_monitor.$O

objs/utilities.$O:constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c constraint_solver/utilities.cc $(OBJOUT)objs/utilities.$O

$(LIBPREFIX)constraint_solver.$(LIBSUFFIX): $(CONSTRAINT_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)constraint_solver.$(LIBSUFFIX) $(CONSTRAINT_SOLVER_LIB_OS)

# Linear Solver Library

LINEAR_SOLVER_LIB_OS = \
	objs/cbc_interface.$O \
	objs/clp_interface.$O \
	objs/glpk_interface.$O \
	objs/linear_solver.$O \
	objs/linear_solver.pb.$O \
	objs/scip_interface.$O

objs/cbc_interface.$O:linear_solver/cbc_interface.cc
	$(CCC) $(CFLAGS) -c linear_solver/cbc_interface.cc $(OBJOUT)objs/cbc_interface.$O

objs/clp_interface.$O:linear_solver/clp_interface.cc
	$(CCC) $(CFLAGS) -c linear_solver/clp_interface.cc $(OBJOUT)objs/clp_interface.$O

objs/glpk_interface.$O:linear_solver/glpk_interface.cc
	$(CCC) $(CFLAGS) -c linear_solver/glpk_interface.cc $(OBJOUT)objs/glpk_interface.$O

objs/linear_solver.$O:linear_solver/linear_solver.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/linear_solver.cc $(OBJOUT)objs/linear_solver.$O

objs/linear_solver.pb.$O:gen/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c gen/linear_solver/linear_solver.pb.cc $(OBJOUT)objs/linear_solver.pb.$O

gen/linear_solver/linear_solver.pb.cc:linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=. --cpp_out=gen linear_solver/linear_solver.proto

gen/linear_solver/linear_solver.pb.h:gen/linear_solver/linear_solver.pb.cc

objs/scip_interface.$O:linear_solver/scip_interface.cc
	$(CCC) $(CFLAGS) -c linear_solver/scip_interface.cc $(OBJOUT)objs/scip_interface.$O

$(LIBPREFIX)linear_solver.$(LIBSUFFIX): $(LINEAR_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)linear_solver.$(LIBSUFFIX) $(LINEAR_SOLVER_LIB_OS)

# Util library.

UTIL_LIB_OS=\
	objs/bitset.$O \
	objs/cached_log.$O \
	objs/const_int_array.$O \
	objs/graph_export.$O \
	objs/xml_helper.$O

objs/bitset.$O:util/bitset.cc
	$(CCC) $(CFLAGS) -c util/bitset.cc $(OBJOUT)objs/bitset.$O

objs/cached_log.$O:util/cached_log.cc
	$(CCC) $(CFLAGS) -c util/cached_log.cc $(OBJOUT)objs/cached_log.$O

objs/const_int_array.$O:util/const_int_array.cc
	$(CCC) $(CFLAGS) -c util/const_int_array.cc $(OBJOUT)objs/const_int_array.$O

objs/graph_export.$O:util/graph_export.cc
	$(CCC) $(CFLAGS) -c util/graph_export.cc $(OBJOUT)objs/graph_export.$O

objs/xml_helper.$O:util/xml_helper.cc
	$(CCC) $(CFLAGS) -c util/xml_helper.cc $(OBJOUT)objs/xml_helper.$O

$(LIBPREFIX)util.$(LIBSUFFIX): $(UTIL_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)util.$(LIBSUFFIX) $(UTIL_LIB_OS)

# Graph library.

GRAPH_LIB_OS=\
	objs/linear_assignment.$O \
	objs/cliques.$O \
	objs/connectivity.$O \
	objs/max_flow.$O \
	objs/min_cost_flow.$O

objs/linear_assignment.$O:graph/linear_assignment.cc
	$(CCC) $(CFLAGS) -c graph/linear_assignment.cc $(OBJOUT)objs/linear_assignment.$O

objs/cliques.$O:graph/cliques.cc
	$(CCC) $(CFLAGS) -c graph/cliques.cc $(OBJOUT)objs/cliques.$O

objs/connectivity.$O:graph/connectivity.cc
	$(CCC) $(CFLAGS) -c graph/connectivity.cc $(OBJOUT)objs/connectivity.$O

objs/max_flow.$O:graph/max_flow.cc
	$(CCC) $(CFLAGS) -c graph/max_flow.cc $(OBJOUT)objs/max_flow.$O

objs/min_cost_flow.$O:graph/min_cost_flow.cc
	$(CCC) $(CFLAGS) -c graph/min_cost_flow.cc $(OBJOUT)objs/min_cost_flow.$O

$(LIBPREFIX)graph.$(LIBSUFFIX): $(GRAPH_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)graph.$(LIBSUFFIX) $(GRAPH_LIB_OS)

# Shortestpaths library.

SHORTESTPATHS_LIB_OS=\
	objs/bellman_ford.$O \
	objs/dijkstra.$O \
	objs/shortestpaths.$O

objs/bellman_ford.$O:graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c graph/bellman_ford.cc $(OBJOUT)objs/bellman_ford.$O

objs/dijkstra.$O:graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c graph/dijkstra.cc $(OBJOUT)objs/dijkstra.$O

objs/shortestpaths.$O:graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c graph/shortestpaths.cc $(OBJOUT)objs/shortestpaths.$O

$(LIBPREFIX)shortestpaths.$(LIBSUFFIX): $(SHORTESTPATHS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)shortestpaths.$(LIBSUFFIX) $(SHORTESTPATHS_LIB_OS)

# Routing library.

ROUTING_LIB_OS=\
	objs/routing.$O

objs/routing.$O:constraint_solver/routing.cc
	$(CCC) $(CFLAGS) -c constraint_solver/routing.cc $(OBJOUT)objs/routing.$O

$(LIBPREFIX)routing.$(LIBSUFFIX): $(ROUTING_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)routing.$(LIBSUFFIX) $(ROUTING_LIB_OS)

# Algorithms library.

ALGORITHMS_LIB_OS=\
	objs/hungarian.$O \
	objs/knapsack_solver.$O

objs/hungarian.$O:algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c algorithms/hungarian.cc $(OBJOUT)objs/hungarian.$O

objs/knapsack_solver.$O:algorithms/knapsack_solver.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c algorithms/knapsack_solver.cc $(OBJOUT)objs/knapsack_solver.$O

$(LIBPREFIX)algorithms.$(LIBSUFFIX): $(ALGORITHMS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)algorithms.$(LIBSUFFIX) $(ALGORITHMS_LIB_OS)

# Base library.

BASE_LIB_OS=\
	objs/bitmap.$O\
	objs/callback.$O\
	objs/file.$O\
	objs/filelinereader.$O\
	objs/join.$O\
	objs/logging.$O\
	objs/random.$O\
	objs/recordio.$O\
	objs/split.$O\
	objs/stringpiece.$O\
	objs/stringprintf.$O\
	objs/sysinfo.$O\
	objs/timer.$O

objs/bitmap.$O:base/bitmap.cc
	$(CCC) $(CFLAGS) -c base/bitmap.cc $(OBJOUT)objs/bitmap.$O
objs/callback.$O:base/callback.cc
	$(CCC) $(CFLAGS) -c base/callback.cc $(OBJOUT)objs/callback.$O
objs/file.$O:base/file.cc
	$(CCC) $(CFLAGS) -c base/file.cc $(OBJOUT)objs/file.$O
objs/filelinereader.$O:base/filelinereader.cc
	$(CCC) $(CFLAGS) -c base/filelinereader.cc $(OBJOUT)objs/filelinereader.$O
objs/logging.$O:base/logging.cc
	$(CCC) $(CFLAGS) -c base/logging.cc $(OBJOUT)objs/logging.$O
objs/join.$O:base/join.cc
	$(CCC) $(CFLAGS) -c base/join.cc $(OBJOUT)objs/join.$O
objs/random.$O:base/random.cc
	$(CCC) $(CFLAGS) -c base/random.cc $(OBJOUT)objs/random.$O
objs/recordio.$O:base/recordio.cc
	$(CCC) $(CFLAGS) -c base/recordio.cc $(OBJOUT)objs/recordio.$O
objs/split.$O:base/split.cc
	$(CCC) $(CFLAGS) -c base/split.cc $(OBJOUT)objs/split.$O
objs/stringpiece.$O:base/stringpiece.cc
	$(CCC) $(CFLAGS) -c base/stringpiece.cc $(OBJOUT)objs/stringpiece.$O
objs/stringprintf.$O:base/stringprintf.cc
	$(CCC) $(CFLAGS) -c base/stringprintf.cc $(OBJOUT)objs/stringprintf.$O
objs/sysinfo.$O:base/sysinfo.cc
	$(CCC) $(CFLAGS) -c base/sysinfo.cc $(OBJOUT)objs/sysinfo.$O
objs/timer.$O:base/timer.cc
	$(CCC) $(CFLAGS) -c base/timer.cc $(OBJOUT)objs/timer.$O

$(LIBPREFIX)base.$(LIBSUFFIX): $(BASE_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)base.$(LIBSUFFIX) $(BASE_LIB_OS)

# DIMACS challenge problem format library

DIMACS_LIB_OS=\
	objs/parse_dimacs_assignment.$O\
	objs/print_dimacs_assignment.$O

objs/parse_dimacs_assignment.$O:examples/parse_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c examples/parse_dimacs_assignment.cc $(OBJOUT)objs/parse_dimacs_assignment.$O
objs/print_dimacs_assignment.$O:examples/print_dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c examples/print_dimacs_assignment.cc $(OBJOUT)objs/print_dimacs_assignment.$O

$(LIBPREFIX)dimacs.$(LIBSUFFIX): $(DIMACS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)dimacs.$(LIBSUFFIX) $(DIMACS_LIB_OS)

# Flow and linear assignment examples

objs/linear_assignment_api.$O:examples/linear_assignment_api.cc
	$(CCC) $(CFLAGS) -c examples/linear_assignment_api.cc $(OBJOUT)objs/linear_assignment_api.$O

linear_assignment_api$E: $(GRAPH_DEPS) objs/linear_assignment_api.$O
	$(CCC) $(CFLAGS) objs/linear_assignment_api.$O $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)linear_assignment_api$E

objs/flow_api.$O:examples/flow_api.cc
	$(CCC) $(CFLAGS) -c examples/flow_api.cc $(OBJOUT)objs/flow_api.$O

flow_api$E: $(GRAPH_DEPS) objs/flow_api.$O
	$(CCC) $(CFLAGS) objs/flow_api.$O $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)flow_api$E

objs/dimacs_assignment.$O:examples/dimacs_assignment.cc
	$(CCC) $(CFLAGS) -c examples/dimacs_assignment.cc $(OBJOUT)objs/dimacs_assignment.$O

dimacs_assignment$E: $(ALGORITHMS_DEPS) $(GRAPH_DEPS) $(DIMACS_LIBS) objs/dimacs_assignment.$O
	$(CCC) $(CFLAGS) objs/dimacs_assignment.$O $(DIMACS_LNK) $(ALGORITHMS_LNK) $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)dimacs_assignment$E

# Pure CP and Routing Examples

objs/costas_array.$O: examples/costas_array.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/costas_array.cc $(OBJOUT)objs/costas_array.$O

costas_array$E: $(CP_DEPS) objs/costas_array.$O
	$(CCC) $(CFLAGS) objs/costas_array.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)costas_array$E

objs/cryptarithm.$O:examples/cryptarithm.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/cryptarithm.cc $(OBJOUT)objs/cryptarithm.$O

cryptarithm$E: $(CP_DEPS) objs/cryptarithm.$O
	$(CCC) $(CFLAGS) objs/cryptarithm.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)cryptarithm$E

objs/cvrptw.$O: examples/cvrptw.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/cvrptw.cc $(OBJOUT)objs/cvrptw.$O

cvrptw$E: $(ROUTING_DEPS) objs/cvrptw.$O
	$(CCC) $(CFLAGS) objs/cvrptw.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)cvrptw$E

objs/dobble_ls.$O:examples/dobble_ls.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/dobble_ls.cc $(OBJOUT)objs/dobble_ls.$O

dobble_ls$E: $(CP_DEPS) objs/dobble_ls.$O
	$(CCC) $(CFLAGS) objs/dobble_ls.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)dobble_ls$E

objs/golomb.$O:examples/golomb.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/golomb.cc $(OBJOUT)objs/golomb.$O

golomb$E: $(CP_DEPS) objs/golomb.$O
	$(CCC) $(CFLAGS) objs/golomb.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)golomb$E

objs/jobshop.$O:examples/jobshop.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/jobshop.cc $(OBJOUT)objs/jobshop.$O

jobshop$E: $(CP_DEPS) objs/jobshop.$O
	$(CCC) $(CFLAGS) objs/jobshop.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)jobshop$E

objs/jobshop_ls.$O:examples/jobshop_ls.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/jobshop_ls.cc $(OBJOUT)objs/jobshop_ls.$O

jobshop_ls$E: $(CP_DEPS) objs/jobshop_ls.$O
	$(CCC) $(CFLAGS) objs/jobshop_ls.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)jobshop_ls$E

objs/magic_square.$O:examples/magic_square.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/magic_square.cc $(OBJOUT)objs/magic_square.$O

magic_square$E: $(CP_DEPS) objs/magic_square.$O
	$(CCC) $(CFLAGS) objs/magic_square.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)magic_square$E

objs/model_util.$O:examples/model_util.cc gen/constraint_solver/model.pb.h constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/model_util.cc $(OBJOUT)objs/model_util.$O

model_util$E: $(CP_DEPS) objs/model_util.$O
	$(CCC) $(CFLAGS) objs/model_util.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)model_util$E

objs/multidim_knapsack.$O:examples/multidim_knapsack.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/multidim_knapsack.cc $(OBJOUT)objs/multidim_knapsack.$O

multidim_knapsack$E: $(CP_DEPS) objs/multidim_knapsack.$O
	$(CCC) $(CFLAGS) objs/multidim_knapsack.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)multidim_knapsack$E

objs/network_routing.$O:examples/network_routing.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/network_routing.cc $(OBJOUT)objs/network_routing.$O

network_routing$E: $(CP_DEPS) $(GRAPH_DEPS) objs/network_routing.$O
	$(CCC) $(CFLAGS) objs/network_routing.$O $(CP_LNK) $(GRAPH_LNK) $(LDFLAGS) $(EXEOUT)network_routing$E

objs/nqueens.$O: examples/nqueens.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/nqueens.cc $(OBJOUT)objs/nqueens.$O

nqueens$E: $(CP_DEPS) objs/nqueens.$O
	$(CCC) $(CFLAGS) objs/nqueens.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)nqueens$E

objs/pdptw.$O: examples/pdptw.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/pdptw.cc $(OBJOUT)objs/pdptw.$O

pdptw$E: $(ROUTING_DEPS) objs/pdptw.$O
	$(CCC) $(CFLAGS) objs/pdptw.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)pdptw$E

objs/sports_scheduling.$O:examples/sports_scheduling.cc constraint_solver/constraint_solver.h
	$(CCC) $(CFLAGS) -c examples/sports_scheduling.cc $(OBJOUT)objs/sports_scheduling.$O

sports_scheduling$E: $(CP_DEPS) objs/sports_scheduling.$O
	$(CCC) $(CFLAGS) objs/sports_scheduling.$O $(CP_LNK) $(LDFLAGS) $(EXEOUT)sports_scheduling$E

objs/tsp.$O: examples/tsp.cc constraint_solver/routing.h
	$(CCC) $(CFLAGS) -c examples/tsp.cc $(OBJOUT)objs/tsp.$O

tsp$E: $(ROUTING_DEPS) objs/tsp.$O
	$(CCC) $(CFLAGS) objs/tsp.$O $(ROUTING_LNK) $(LDFLAGS) $(EXEOUT)tsp$E

# Linear Programming Examples

objs/strawberry_fields_with_column_generation.$O: examples/strawberry_fields_with_column_generation.cc linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c examples/strawberry_fields_with_column_generation.cc $(OBJOUT)objs/strawberry_fields_with_column_generation.$O

strawberry_fields_with_column_generation$E: $(LP_DEPS) objs/strawberry_fields_with_column_generation.$O
	$(CCC) $(CFLAGS) objs/strawberry_fields_with_column_generation.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)strawberry_fields_with_column_generation$E

objs/linear_programming.$O: examples/linear_programming.cc linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c examples/linear_programming.cc $(OBJOUT)objs/linear_programming.$O

linear_programming$E: $(LP_DEPS) objs/linear_programming.$O
	$(CCC) $(CFLAGS) objs/linear_programming.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)linear_programming$E

objs/linear_solver_protocol_buffers.$O: examples/linear_solver_protocol_buffers.cc linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c examples/linear_solver_protocol_buffers.cc $(OBJOUT)objs/linear_solver_protocol_buffers.$O

linear_solver_protocol_buffers$E: $(LP_DEPS) objs/linear_solver_protocol_buffers.$O
	$(CCC) $(CFLAGS) objs/linear_solver_protocol_buffers.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)linear_solver_protocol_buffers$E

objs/integer_programming.$O: examples/integer_programming.cc linear_solver/linear_solver.h
	$(CCC) $(CFLAGS) -c examples/integer_programming.cc $(OBJOUT)objs/integer_programming.$O

integer_programming$E: $(LP_DEPS) objs/integer_programming.$O
	$(CCC) $(CFLAGS) objs/integer_programming.$O $(LP_LNK) $(LDFLAGS) $(EXEOUT)integer_programming$E
