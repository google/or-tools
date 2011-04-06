# Real targets

help:
	@echo Please define target:
	@echo "  - constraint programming: cplibs cpexe pycp javacp"
	@echo "  - mathematical programming: lplibs lpexe pylp javalp"
	@echo "  - algorithms: algorithmslibs pyalgorithms javaalgorithms"
	@echo "  - graph: graphlibs pygraph javagraph"
	@echo "  - misc: clean"

all: cplibs cpexe pycp javacp algorithmslibs pyalgorithms javaalgorithms graphlibs pygraph javagraph lplibs lpexe pylp javalp

CP_LIBS = \
	$(LIBPREFIX)routing.$(LIBSUFFIX)       \
	$(LIBPREFIX)constraint_solver.$(LIBSUFFIX)

GRAPH_LIBS = \
	$(LIBPREFIX)graph.$(LIBSUFFIX) \
	$(LIBPREFIX)shortestpaths.$(LIBSUFFIX)

graphlibs: $(GRAPH_LIBS) $(BASE_LIBS)

BASE_LIBS = \
	$(LIBPREFIX)util.$(LIBSUFFIX)          \
	$(LIBPREFIX)base.$(LIBSUFFIX)

cplibs: $(CP_LIBS) $(BASE_LIBS)

LP_LIBS = \
	$(LIBPREFIX)linear_solver.$(LIBSUFFIX)

lplibs: $(LP_LIBS) $(BASE_LIBS)

CPBINARIES = \
	costas_array$E \
	cryptarithm$E \
        cvrptw$E \
	flow_example$E \
	golomb$E \
	magic_square$E \
	network_routing$E \
	nqueens$E \
	tsp$E

cpexe: $(CPBINARIES)

LPBINARIES = \
	integer_solver_example$E \
	linear_solver_example$E

lpexe: $(LPBINARIES)

ALGORITHMS_LIBS = \
	$(LIBPREFIX)algorithms.$(LIBSUFFIX)

algorithmslibs: $(ALGORITHMS_LIBS)

clean:
	rm -f *.$(LIBSUFFIX)
	rm -f objs/*.$O
	rm -f $(CPBINARIES)
	rm -f $(LPBINARIES)
	rm -f gen/*/*wrap*
	rm -f gen/*/*.pb.*
	rm -f objs/com/google/ortools/*/*.class
	rm -f gen/com/google/ortools/*/*.java
	rm -f *.so
	rm -f *.jar

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OS = \
	objs/alldiff_cst.$O\
	objs/assignment.$O\
	objs/assignment.pb.$O\
	objs/constraint_solver.$O\
	objs/constraints.$O\
	objs/count_cst.$O\
	objs/default_search.$O\
	objs/demon_profiler.$O\
	objs/demon_profiler.pb.$O\
	objs/element.$O\
	objs/expr_array.$O\
	objs/expr_cst.$O\
	objs/expressions.$O\
	objs/interval.$O\
	objs/local_search.$O\
	objs/pack.$O\
	objs/range_cst.$O\
	objs/resource.$O\
	objs/sched_search.$O\
	objs/search.$O\
	objs/search_limit.pb.$O\
	objs/table.$O\
	objs/timetabling.$O\
	objs/tree_monitor.$O\
	objs/utilities.$O

objs/alldiff_cst.$O:constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/alldiff_cst.cc $(OBJOUT)objs/alldiff_cst.$O

objs/assignment.$O:constraint_solver/assignment.cc gen/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/assignment.cc $(OBJOUT)objs/assignment.$O

objs/assignment.pb.$O:gen/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/assignment.pb.cc $(OBJOUT)objs/assignment.pb.$O

gen/constraint_solver/assignment.pb.cc:constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=gen/constraint_solver constraint_solver/assignment.proto

gen/constraint_solver/assignment.pb.h:gen/constraint_solver/assignment.pb.cc

objs/constraint_solver.$O:constraint_solver/constraint_solver.cc
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
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=gen/constraint_solver constraint_solver/demon_profiler.proto

gen/constraint_solver/demon_profiler.pb.h:gen/constraint_solver/demon_profiler.pb.cc

objs/element.$O:constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c constraint_solver/element.cc $(OBJOUT)objs/element.$O

objs/expr_array.$O:constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expr_array.cc $(OBJOUT)objs/expr_array.$O

objs/expr_cst.$O:constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expr_cst.cc $(OBJOUT)objs/expr_cst.$O

objs/expressions.$O:constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expressions.cc $(OBJOUT)objs/expressions.$O

objs/interval.$O:constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c constraint_solver/interval.cc $(OBJOUT)objs/interval.$O

objs/local_search.$O:constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/local_search.cc $(OBJOUT)objs/local_search.$O

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
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=gen/constraint_solver constraint_solver/search_limit.proto

gen/constraint_solver/search_limit.pb.h:gen/constraint_solver/search_limit.pb.cc

objs/table.$O:constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c constraint_solver/table.cc $(OBJOUT)objs/table.$O

objs/timetabling.$O:constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c constraint_solver/timetabling.cc $(OBJOUT)objs/timetabling.$O

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
	objs/linear_solver.pb.$O

objs/cbc_interface.$O:linear_solver/cbc_interface.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/cbc_interface.cc $(OBJOUT)objs/cbc_interface.$O

objs/clp_interface.$O:linear_solver/clp_interface.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/clp_interface.cc $(OBJOUT)objs/clp_interface.$O

objs/glpk_interface.$O:linear_solver/glpk_interface.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/glpk_interface.cc $(OBJOUT)objs/glpk_interface.$O

objs/linear_solver.$O:linear_solver/linear_solver.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/linear_solver.cc $(OBJOUT)objs/linear_solver.$O

objs/linear_solver.pb.$O:gen/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c gen/linear_solver/linear_solver.pb.cc $(OBJOUT)objs/linear_solver.pb.$O

gen/linear_solver/linear_solver.pb.cc:linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=linear_solver --cpp_out=gen/linear_solver linear_solver/linear_solver.proto

gen/linear_solver/linear_solver.pb.h:gen/linear_solver/linear_solver.pb.cc

$(LIBPREFIX)linear_solver.$(LIBSUFFIX): $(LINEAR_SOLVER_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)liblinear_solver.$(LIBSUFFIX) $(LINEAR_SOLVER_LIB_OS)

# Util library.

UTIL_LIB_OS=\
	objs/bitset.$O \
	objs/cached_log.$O \
	objs/xml_helper.$O

objs/bitset.$O:util/bitset.cc
	$(CCC) $(CFLAGS) -c util/bitset.cc $(OBJOUT)objs/bitset.$O

objs/cached_log.$O:util/cached_log.cc
	$(CCC) $(CFLAGS) -c util/cached_log.cc $(OBJOUT)objs/cached_log.$O

objs/xml_helper.$O:util/xml_helper.cc
	$(CCC) $(CFLAGS) -c util/xml_helper.cc $(OBJOUT)objs/xml_helper.$O

$(LIBPREFIX)util.$(LIBSUFFIX): $(UTIL_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)util.$(LIBSUFFIX) $(UTIL_LIB_OS)

# Graph library.

GRAPH_LIB_OS=\
	objs/bron_kerbosch.$O \
	objs/max_flow.$O \
	objs/min_cost_flow.$O

objs/bron_kerbosch.$O:graph/bron_kerbosch.cc
	$(CCC) $(CFLAGS) -c graph/bron_kerbosch.cc $(OBJOUT)objs/bron_kerbosch.$O

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

objs/knapsack_solver.$O:algorithms/knapsack_solver.cc
	$(CCC) $(CFLAGS) -c algorithms/knapsack_solver.cc $(OBJOUT)objs/knapsack_solver.$O

$(LIBPREFIX)algorithms.$(LIBSUFFIX): $(ALGORITHMS_LIB_OS)
	$(LINKCMD) $(LINKPREFIX)$(LIBPREFIX)algorithms.$(LIBSUFFIX) $(ALGORITHMS_LIB_OS)

# Base library.

BASE_LIB_OS=\
	objs/bitmap.$O\
	objs/callback.$O\
	objs/join.$O\
	objs/logging.$O\
	objs/random.$O\
	objs/stringpiece.$O\
	objs/stringprintf.$O\
	objs/sysinfo.$O\
	objs/timer.$O


objs/bitmap.$O:base/bitmap.cc
	$(CCC) $(CFLAGS) -c base/bitmap.cc $(OBJOUT)objs/bitmap.$O
objs/callback.$O:base/callback.cc
	$(CCC) $(CFLAGS) -c base/callback.cc $(OBJOUT)objs/callback.$O
objs/logging.$O:base/logging.cc
	$(CCC) $(CFLAGS) -c base/logging.cc $(OBJOUT)objs/logging.$O
objs/join.$O:base/join.cc
	$(CCC) $(CFLAGS) -c base/join.cc $(OBJOUT)objs/join.$O
objs/random.$O:base/random.cc
	$(CCC) $(CFLAGS) -c base/random.cc $(OBJOUT)objs/random.$O
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

# Pure CP and Routing Examples

objs/costas_array.$O: examples/costas_array.cc
	$(CCC) $(CFLAGS) -c examples/costas_array.cc $(OBJOUT)objs/costas_array.$O

costas_array$E: $(CP_LIBS) $(BASE_LIBS) objs/costas_array.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/costas_array.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) costas_array$E

objs/cryptarithm.$O:examples/cryptarithm.cc
	$(CCC) $(CFLAGS) -c examples/cryptarithm.cc $(OBJOUT)objs/cryptarithm.$O

cryptarithm$E: $(CP_LIBS) $(BASE_LIBS) objs/cryptarithm.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cryptarithm.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) cryptarithm$E

objs/cvrptw.$O: examples/cvrptw.cc
	$(CCC) $(CFLAGS) -c examples/cvrptw.cc $(OBJOUT)objs/cvrptw.$O

cvrptw$E: $(CP_LIBS) $(BASE_LIBS) objs/cvrptw.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cvrptw.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) cvrptw$E

objs/dobble_ls.$O:examples/dobble_ls.cc
	$(CCC) $(CFLAGS) -c examples/dobble_ls.cc $(OBJOUT)objs/dobble_ls.$O

dobble_ls$E: $(CP_LIBS) $(BASE_LIBS) objs/dobble_ls.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/dobble_ls.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) dobble_ls$E

objs/flow_example.$O:examples/flow_example.cc
	$(CCC) $(CFLAGS) -c examples/flow_example.cc $(OBJOUT)objs/flow_example.$O

flow_example$E: $(GRAPH_LIBS) $(BASE_LIBS) objs/flow_example.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/flow_example.$O $(GRAPH_LIBS) $(BASE_LIBS) $(EXEOUT) flow_example$E

objs/golomb.$O:examples/golomb.cc
	$(CCC) $(CFLAGS) -c examples/golomb.cc $(OBJOUT)objs/golomb.$O

golomb$E: $(CP_LIBS) $(BASE_LIBS) objs/golomb.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/golomb.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) golomb$E

objs/magic_square.$O:examples/magic_square.cc
	$(CCC) $(CFLAGS) -c examples/magic_square.cc $(OBJOUT)objs/magic_square.$O

magic_square$E: $(CP_LIBS) $(BASE_LIBS) objs/magic_square.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/magic_square.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) magic_square$E

objs/network_routing.$O:examples/network_routing.cc
	$(CCC) $(CFLAGS) -c examples/network_routing.cc $(OBJOUT)objs/network_routing.$O

network_routing$E: $(CP_LIBS) $(BASE_LIBS) $(GRAPH_LIBS) objs/network_routing.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/network_routing.$O $(CP_LIBS) $(GRAPH_LIBS) $(BASE_LIBS) $(EXEOUT) network_routing$E

objs/nqueens.$O: examples/nqueens.cc
	$(CCC) $(CFLAGS) -c examples/nqueens.cc $(OBJOUT)objs/nqueens.$O

nqueens$E: $(CP_LIBS) $(BASE_LIBS) objs/nqueens.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/nqueens.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) nqueens$E

objs/tricks.$O: examples/tricks.cc
	$(CCC) $(CFLAGS) -c examples/tricks.cc $(OBJOUT)objs/tricks.$O

objs/global_arith.$O: examples/global_arith.cc
	$(CCC) $(CFLAGS) -c examples/global_arith.cc $(OBJOUT)objs/global_arith.$O

tricks$E: $(CPLIBS) $(BASE_LIBS) objs/tricks.$O objs/global_arith.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/tricks.$O objs/global_arith.$O $(CPLIBS) $(BASE_LIBS) $(EXEOUT) tricks$E

objs/tsp.$O: examples/tsp.cc
	$(CCC) $(CFLAGS) -c examples/tsp.cc $(OBJOUT)objs/tsp.$O

tsp$E: $(CP_LIBS) $(BASE_LIBS) objs/tsp.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/tsp.$O $(CP_LIBS) $(BASE_LIBS) $(EXEOUT) tsp$E

# Linear Programming Examples

objs/linear_solver_example.$O: examples/linear_solver_example.cc
	$(CCC) $(CFLAGS) -c examples/linear_solver_example.cc $(OBJOUT)objs/linear_solver_example.$O

linear_solver_example$E: $(LP_LIBS) $(BASE_LIBS) objs/linear_solver_example.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/linear_solver_example.$O $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(EXEOUT) linear_solver_example$E

objs/integer_solver_example.$O: examples/integer_solver_example.cc
	$(CCC) $(CFLAGS) -c examples/integer_solver_example.cc $(OBJOUT)objs/integer_solver_example.$O

integer_solver_example$E: $(LP_LIBS) $(BASE_LIBS) objs/integer_solver_example.$O
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/integer_solver_example.$O $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(EXEOUT) integer_solver_example$E

