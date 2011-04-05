# Please edit the following:
PYTHON_VER=2.6
GFLAGS_DIR=../gflags-1.4
SWIG_BINARY=swig
PROTOBUF_DIR=../protobuf-2.3.0
# This is the root directory of the CLP installation. Please undefine if CLP is
# not installed. If you have installed CBC, CLP_DIR can have the same value as
# CBC_DIR.
CLP_DIR=../cbc-2.6.2
# This is the root directory of the CBC installation. Please undefine if CBC is
# not installed.
CBC_DIR=../cbc-2.6.2
# This is the root directory of glpk installation. Please undefine if GLPK is
# not installed.
GLPK_DIR=../glpk-4.45

#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard ------

# This is needed to find python.h
PYTHON_INC=-I/usr/include/python$(PYTHON_VER) -I/usr/lib/python$(PYTHON_VER)
# This is needed to find gflags/gflags.h
GFLAGS_INC = -I$(GFLAGS_DIR)/include
# This is needed to find protocol buffers.
PROTOBUF_INC = -I$(PROTOBUF_DIR)/include

# Define CLP_DIR if unset and if CBC_DIR is set.
ifdef CBC_DIR
ifndef CLP_DIR
CLP_DIR=$(CBC_DIR)
endif
endif
# This is needed to find Coin LP include files.
ifdef CLP_DIR
CLP_INC = -I$(CLP_DIR)/include -DUSE_CLP
endif
# This is needed to find Coin Branch and Cut include files.
ifdef CBC_DIR
CBC_INC = -I$(CBC_DIR)/include -DUSE_CBC
endif
# This is needed to find GLPK include files.
ifdef GLPK_DIR
GLPK_INC = -I$(GLPK_DIR)/include -DUSE_GLPK
endif

# Compilation flags
DEBUG=-O3 -DNDEBUG
JNIDEBUG=-O1 -DNDEBUG
SYSCFLAGS=-fPIC
CCC=g++

# ----- OS Dependent -----
OS=$(shell uname -s)

ifeq ($(OS),Linux)
LD = gcc -shared
# This is needed to find libgflags.a
GFLAGS_LNK = -Wl,-rpath $(GFLAGS_DIR)/lib -L$(GFLAGS_DIR)/lib -lgflags
# This is needed to find libz.a
ZLIB_LNK = -lz
# This is needed to find libprotobuf.a
PROTOBUF_LNK = -Wl,-rpath $(PROTOBUF_DIR)/lib -L$(PROTOBUF_DIR)/lib -lprotobuf -lpthread
ifdef GLPK_DIR
GLPK_LNK = -Wl,-rpath $(GLPK_DIR)/lib -L$(GLPK_DIR)/lib -lglpk
endif
ifdef CLP_DIR
CLP_LNK = -Wl,-rpath $(CLP_DIR)/lib/coin -L$(CLP_DIR)/lib/coin -lClp -lCoinUtils
endif
ifdef CBC_DIR
CBC_LNK = -Wl,-rpath $(CBC_DIR)/lib/coin -L$(CBC_DIR)/lib/coin -lCbcSolver -lCbc -lCgl -lOsi -lOsiCbc -lOsiClp -lOsiVol -lVol
endif
# Detect 32 bit or 64 bit OS and define ARCH flags correctly.
LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
   JDK_EXT=64
   ARCH=-DARCH_K8
else
   JDK_EXT=32
   ARCH=
endif
SYS_LNK=-lrt
JAVA_INC=-I/usr/local/buildtools/java/jdk-$(JDK_EXT)/include -I/usr/local/buildtools/java/jdk-$(JDK_EXT)/include/linux
JAVAC_BIN=/usr/local/buildtools/java/jdk-$(JDK_EXT)/bin/javac
JAVA_BIN=/usr/local/buildtools/java/jdk-$(JDK_EXT)/bin/java
JNILIBEXT=so
FIX_SWIG=sed -i -e 's/Tlong/T_long/g' gen/com/google/ortools/constraintsolver/Solver.java
endif
ifeq ($(OS),Darwin) # Assume Mac Os X
LD = ld -arch x86_64 -bundle -flat_namespace -undefined suppress
GFLAGS_LNK = -L$(GFLAGS_DIR)/lib -lgflags
ZLIB_LNK = -lz
PROTOBUF_LNK = -L$(PROTOBUF_DIR)/lib -lprotobuf
ARCH=-DARCH_K8
SYS_LNK=
JAVA_INC=-I/System/Library/Java/JavaVirtualMachines/1.6.0.jdk/Contents/Home/bundle/Headers
JAVAC_BIN=javac
JAVA_BIN=java
JNILIBEXT=jnilib
FIX_SWIG=

ifdef GLPK_DIR
GLPK_LNK = -L$(GLPK_DIR)/lib -lglpk
endif
ifdef CLP_DIR
CLP_LNK = -L$(CLP_DIR)/lib/coin -lClp -lCoinUtils
endif
ifdef CBC_DIR
CBC_LNK = -L$(CBC_DIR)/lib/coin -lCbcSolver -lCbc -lCgl -lOsi -lOsiCbc -lOsiClp
endif
endif

CFLAGS= $(SYSCFLAGS) $(DEBUG) -I. -Igen $(GFLAGS_INC) $(ARCH) \
        -Wno-deprecated $(PROTOBUF_INC) $(CBC_INC) $(CLP_INC) $(GLPK_INC)
JNIFLAGS= $(SYSCFLAGS) $(JNIDEBUG) -I. -Igen $(GFLAGS_INC) $(ARCH) \
        -Wno-deprecated $(PROTOBUF_INC) $(CBC_INC) $(CLP_INC) $(GLPK_INC)
LDFLAGS=$(GFLAGS_LNK) $(ZLIB_LNK) $(PROTOBUF_LNK) $(SYS_LNK)
LDLPDEPS= $(GLPK_LNK) $(CBC_LNK) $(CLP_LNK)

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
	librouting.a       \
	libconstraint_solver.a

GRAPH_LIBS = \
	libgraph.a \
	libshortestpaths.a

graphlibs: $(GRAPH_LIBS) $(BASE_LIBS)

BASE_LIBS = \
	libutil.a          \
	libbase.a

cplibs: $(CP_LIBS) $(BASE_LIBS)

LP_LIBS = \
	liblinear_solver.a

lplibs: $(LP_LIBS) $(BASE_LIBS)

CPBINARIES = \
	costas_array \
	cryptarithm \
        cvrptw \
	flow_example \
	golomb \
	magic_square \
	network_routing \
	nqueens \
	tsp

cpexe: $(CPBINARIES)

LPBINARIES = \
	integer_solver_example \
	linear_solver_example

lpexe: $(LPBINARIES)

ALGORITHMS_LIBS = \
	libalgorithms.a

algorithmslibs: $(ALGORITHMS_LIBS)

clean:
	rm -f *.a
	rm -f objs/*.o
	rm -f $(CPBINARIES)
	rm -f $(LPBINARIES)
	rm -f gen/*/*wrap*
	rm -f gen/*/*.pb.*
	rm -f objs/com/google/ortools/*/*.class
	rm -f gen/com/google/ortools/*/*.java
	rm -f *.so
	rm -f *.jar

# Constraint Solver Lib.

CONSTRAINT_SOLVER_LIB_OBJS = \
	objs/alldiff_cst.o\
	objs/assignment.o\
	objs/assignment.pb.o\
	objs/constraint_solver.o\
	objs/constraints.o\
	objs/count_cst.o\
	objs/default_search.o\
	objs/demon_profiler.o\
	objs/demon_profiler.pb.o\
	objs/element.o\
	objs/expr_array.o\
	objs/expr_cst.o\
	objs/expressions.o\
	objs/interval.o\
	objs/local_search.o\
	objs/pack.o\
	objs/range_cst.o\
	objs/resource.o\
	objs/sched_search.o\
	objs/search.o\
	objs/search_limit.pb.o\
	objs/table.o\
	objs/timetabling.o\
	objs/tree_monitor.o\
	objs/utilities.o

objs/alldiff_cst.o:constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/alldiff_cst.cc -o objs/alldiff_cst.o

objs/assignment.o:constraint_solver/assignment.cc gen/constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/assignment.cc -o objs/assignment.o

objs/assignment.pb.o:gen/constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/assignment.pb.cc -o objs/assignment.pb.o

gen/constraint_solver/assignment.pb.cc:constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=gen/constraint_solver constraint_solver/assignment.proto

gen/constraint_solver/assignment.pb.h:gen/constraint_solver/assignment.pb.cc

objs/constraint_solver.o:constraint_solver/constraint_solver.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraint_solver.cc -o objs/constraint_solver.o

objs/constraints.o:constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraints.cc -o objs/constraints.o

objs/count_cst.o:constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/count_cst.cc -o objs/count_cst.o

objs/default_search.o:constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/default_search.cc -o objs/default_search.o

objs/demon_profiler.o:constraint_solver/demon_profiler.cc gen/constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/demon_profiler.cc -o objs/demon_profiler.o

objs/demon_profiler.pb.o:gen/constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/demon_profiler.pb.cc -o objs/demon_profiler.pb.o

gen/constraint_solver/demon_profiler.pb.cc:constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=gen/constraint_solver constraint_solver/demon_profiler.proto

gen/constraint_solver/demon_profiler.pb.h:gen/constraint_solver/demon_profiler.pb.cc

objs/element.o:constraint_solver/element.cc
	$(CCC) $(CFLAGS) -c constraint_solver/element.cc -o objs/element.o

objs/expr_array.o:constraint_solver/expr_array.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expr_array.cc -o objs/expr_array.o

objs/expr_cst.o:constraint_solver/expr_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expr_cst.cc -o objs/expr_cst.o

objs/expressions.o:constraint_solver/expressions.cc
	$(CCC) $(CFLAGS) -c constraint_solver/expressions.cc -o objs/expressions.o

objs/interval.o:constraint_solver/interval.cc
	$(CCC) $(CFLAGS) -c constraint_solver/interval.cc -o objs/interval.o

objs/local_search.o:constraint_solver/local_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/local_search.cc -o objs/local_search.o

objs/pack.o:constraint_solver/pack.cc
	$(CCC) $(CFLAGS) -c constraint_solver/pack.cc -o objs/pack.o

objs/range_cst.o:constraint_solver/range_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/range_cst.cc -o objs/range_cst.o

objs/resource.o:constraint_solver/resource.cc
	$(CCC) $(CFLAGS) -c constraint_solver/resource.cc -o objs/resource.o

objs/sched_search.o:constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/sched_search.cc -o objs/sched_search.o

objs/search.o:constraint_solver/search.cc gen/constraint_solver/search_limit.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/search.cc -o objs/search.o

objs/search_limit.pb.o:gen/constraint_solver/search_limit.pb.cc
	$(CCC) $(CFLAGS) -c gen/constraint_solver/search_limit.pb.cc -o objs/search_limit.pb.o

gen/constraint_solver/search_limit.pb.cc:constraint_solver/search_limit.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=gen/constraint_solver constraint_solver/search_limit.proto

gen/constraint_solver/search_limit.pb.h:gen/constraint_solver/search_limit.pb.cc

objs/table.o:constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c constraint_solver/table.cc -o objs/table.o

objs/timetabling.o:constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c constraint_solver/timetabling.cc -o objs/timetabling.o

objs/tree_monitor.o:constraint_solver/tree_monitor.cc
	$(CCC) $(CFLAGS) -c constraint_solver/tree_monitor.cc -o objs/tree_monitor.o

objs/utilities.o:constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c constraint_solver/utilities.cc -o objs/utilities.o

libconstraint_solver.a: $(CONSTRAINT_SOLVER_LIB_OBJS)
	ar rv libconstraint_solver.a $(CONSTRAINT_SOLVER_LIB_OBJS)

# Linear Solver Library

LINEAR_SOLVER_LIB_OBJS = \
	objs/cbc_interface.o \
	objs/clp_interface.o \
	objs/glpk_interface.o \
	objs/linear_solver.o \
	objs/linear_solver.pb.o

objs/cbc_interface.o:linear_solver/cbc_interface.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/cbc_interface.cc -o objs/cbc_interface.o

objs/clp_interface.o:linear_solver/clp_interface.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/clp_interface.cc -o objs/clp_interface.o

objs/glpk_interface.o:linear_solver/glpk_interface.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/glpk_interface.cc -o objs/glpk_interface.o

objs/linear_solver.o:linear_solver/linear_solver.cc gen/linear_solver/linear_solver.pb.h
	$(CCC) $(CFLAGS) -c linear_solver/linear_solver.cc -o objs/linear_solver.o

objs/linear_solver.pb.o:gen/linear_solver/linear_solver.pb.cc
	$(CCC) $(CFLAGS) -c gen/linear_solver/linear_solver.pb.cc -o objs/linear_solver.pb.o

gen/linear_solver/linear_solver.pb.cc:linear_solver/linear_solver.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=linear_solver --cpp_out=gen/linear_solver linear_solver/linear_solver.proto

gen/linear_solver/linear_solver.pb.h:gen/linear_solver/linear_solver.pb.cc

liblinear_solver.a: $(LINEAR_SOLVER_LIB_OBJS)
	ar rv liblinear_solver.a $(LINEAR_SOLVER_LIB_OBJS)

# Util library.

UTIL_LIB_OBJS=\
	objs/bitset.o \
	objs/cached_log.o \
	objs/xml_helper.o

objs/bitset.o:util/bitset.cc
	$(CCC) $(CFLAGS) -c util/bitset.cc -o objs/bitset.o

objs/cached_log.o:util/cached_log.cc
	$(CCC) $(CFLAGS) -c util/cached_log.cc -o objs/cached_log.o

objs/xml_helper.o:util/xml_helper.cc
	$(CCC) $(CFLAGS) -c util/xml_helper.cc -o objs/xml_helper.o

libutil.a: $(UTIL_LIB_OBJS)
	ar rv libutil.a $(UTIL_LIB_OBJS)

# Graph library.

GRAPH_LIB_OBJS=\
	objs/bron_kerbosch.o \
	objs/max_flow.o \
	objs/min_cost_flow.o

objs/bron_kerbosch.o:graph/bron_kerbosch.cc
	$(CCC) $(CFLAGS) -c graph/bron_kerbosch.cc -o objs/bron_kerbosch.o

objs/max_flow.o:graph/max_flow.cc
	$(CCC) $(CFLAGS) -c graph/max_flow.cc -o objs/max_flow.o

objs/min_cost_flow.o:graph/min_cost_flow.cc
	$(CCC) $(CFLAGS) -c graph/min_cost_flow.cc -o objs/min_cost_flow.o

libgraph.a: $(GRAPH_LIB_OBJS)
	ar rv libgraph.a $(GRAPH_LIB_OBJS)

# Shortestpaths library.

SHORTESTPATHS_LIB_OBJS=\
	objs/bellman_ford.o \
	objs/dijkstra.o \
	objs/shortestpaths.o

objs/bellman_ford.o:graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c graph/bellman_ford.cc -o objs/bellman_ford.o

objs/dijkstra.o:graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c graph/dijkstra.cc -o objs/dijkstra.o

objs/shortestpaths.o:graph/shortestpaths.cc
	$(CCC) $(CFLAGS) -c graph/shortestpaths.cc -o objs/shortestpaths.o

libshortestpaths.a: $(SHORTESTPATHS_LIB_OBJS)
	ar rv libshortestpaths.a $(SHORTESTPATHS_LIB_OBJS)

# Routing library.

ROUTING_LIB_OBJS=\
	objs/routing.o

objs/routing.o:constraint_solver/routing.cc
	$(CCC) $(CFLAGS) -c constraint_solver/routing.cc -o objs/routing.o

librouting.a: $(ROUTING_LIB_OBJS)
	ar rv librouting.a $(ROUTING_LIB_OBJS)

# Algorithms library.

ALGORITHMS_LIB_OBJS=\
	objs/hungarian.o \
	objs/knapsack_solver.o

objs/hungarian.o:algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c algorithms/hungarian.cc -o objs/hungarian.o

objs/knapsack_solver.o:algorithms/knapsack_solver.cc
	$(CCC) $(CFLAGS) -c algorithms/knapsack_solver.cc -o objs/knapsack_solver.o

libalgorithms.a: $(ALGORITHMS_LIB_OBJS)
	ar rv libalgorithms.a $(ALGORITHMS_LIB_OBJS)

# Base library.

BASE_LIB_OBJS=\
	objs/bitmap.o\
	objs/callback.o\
	objs/join.o\
	objs/logging.o\
	objs/random.o\
	objs/stringpiece.o\
	objs/stringprintf.o\
	objs/sysinfo.o\
	objs/timer.o


objs/bitmap.o:base/bitmap.cc
	$(CCC) $(CFLAGS) -c base/bitmap.cc -o objs/bitmap.o
objs/callback.o:base/callback.cc
	$(CCC) $(CFLAGS) -c base/callback.cc -o objs/callback.o
objs/logging.o:base/logging.cc
	$(CCC) $(CFLAGS) -c base/logging.cc -o objs/logging.o
objs/join.o:base/join.cc
	$(CCC) $(CFLAGS) -c base/join.cc -o objs/join.o
objs/random.o:base/random.cc
	$(CCC) $(CFLAGS) -c base/random.cc -o objs/random.o
objs/stringpiece.o:base/stringpiece.cc
	$(CCC) $(CFLAGS) -c base/stringpiece.cc -o objs/stringpiece.o
objs/stringprintf.o:base/stringprintf.cc
	$(CCC) $(CFLAGS) -c base/stringprintf.cc -o objs/stringprintf.o
objs/sysinfo.o:base/sysinfo.cc
	$(CCC) $(CFLAGS) -c base/sysinfo.cc -o objs/sysinfo.o
objs/timer.o:base/timer.cc
	$(CCC) $(CFLAGS) -c base/timer.cc -o objs/timer.o

libbase.a: $(BASE_LIB_OBJS)
	ar rv libbase.a $(BASE_LIB_OBJS)

# Pure CP Examples

objs/costas_array.o: examples/costas_array.cc
	$(CCC) $(CFLAGS) -c examples/costas_array.cc -o objs/costas_array.o

costas_array: $(CP_LIBS) $(BASE_LIBS) objs/costas_array.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/costas_array.o $(CP_LIBS) $(BASE_LIBS) -o costas_array

objs/cryptarithm.o:examples/cryptarithm.cc
	$(CCC) $(CFLAGS) -c examples/cryptarithm.cc -o objs/cryptarithm.o

cryptarithm: $(CP_LIBS) $(BASE_LIBS) objs/cryptarithm.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cryptarithm.o $(CP_LIBS) $(BASE_LIBS) -o cryptarithm

objs/cvrptw.o: examples/cvrptw.cc
	$(CCC) $(CFLAGS) -c examples/cvrptw.cc -o objs/cvrptw.o

cvrptw: $(CP_LIBS) $(BASE_LIBS) objs/cvrptw.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cvrptw.o $(CP_LIBS) $(BASE_LIBS) -o cvrptw

objs/dobble_ls.o:examples/dobble_ls.cc
	$(CCC) $(CFLAGS) -c examples/dobble_ls.cc -o objs/dobble_ls.o

dobble_ls: $(CP_LIBS) $(BASE_LIBS) objs/dobble_ls.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/dobble_ls.o $(CP_LIBS) $(BASE_LIBS) -o dobble_ls

objs/flow_example.o:examples/flow_example.cc
	$(CCC) $(CFLAGS) -c examples/flow_example.cc -o objs/flow_example.o

flow_example: $(GRAPH_LIBS) $(BASE_LIBS) objs/flow_example.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/flow_example.o $(GRAPH_LIBS) $(BASE_LIBS) -o flow_example

objs/golomb.o:examples/golomb.cc
	$(CCC) $(CFLAGS) -c examples/golomb.cc -o objs/golomb.o

golomb: $(CP_LIBS) $(BASE_LIBS) objs/golomb.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/golomb.o $(CP_LIBS) $(BASE_LIBS) -o golomb

objs/magic_square.o:examples/magic_square.cc
	$(CCC) $(CFLAGS) -c examples/magic_square.cc -o objs/magic_square.o

magic_square: $(CP_LIBS) $(BASE_LIBS) objs/magic_square.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/magic_square.o $(CP_LIBS) $(BASE_LIBS) -o magic_square

objs/network_routing.o:examples/network_routing.cc
	$(CCC) $(CFLAGS) -c examples/network_routing.cc -o objs/network_routing.o

network_routing: $(CP_LIBS) $(BASE_LIBS) $(GRAPH_LIBS) objs/network_routing.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/network_routing.o $(CP_LIBS) $(GRAPH_LIBS) $(BASE_LIBS) -o network_routing

objs/nqueens.o: examples/nqueens.cc
	$(CCC) $(CFLAGS) -c examples/nqueens.cc -o objs/nqueens.o

nqueens: $(CP_LIBS) $(BASE_LIBS) objs/nqueens.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/nqueens.o $(CP_LIBS) $(BASE_LIBS) -o nqueens

objs/tricks.o: examples/tricks.cc
	$(CCC) $(CFLAGS) -c examples/tricks.cc -o objs/tricks.o

objs/global_arith.o: examples/global_arith.cc
	$(CCC) $(CFLAGS) -c examples/global_arith.cc -o objs/global_arith.o

tricks: $(CPLIBS) $(BASE_LIBS) objs/tricks.o objs/global_arith.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/tricks.o objs/global_arith.o $(CPLIBS) $(BASE_LIBS) -o tricks

# Routing Examples

objs/tsp.o: examples/tsp.cc
	$(CCC) $(CFLAGS) -c examples/tsp.cc -o objs/tsp.o

tsp: $(CP_LIBS) $(BASE_LIBS) objs/tsp.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/tsp.o $(CP_LIBS) $(BASE_LIBS) -o tsp

# Linear Programming Examples

objs/linear_solver_example.o: examples/linear_solver_example.cc
	$(CCC) $(CFLAGS) -c examples/linear_solver_example.cc -o objs/linear_solver_example.o

linear_solver_example: $(LP_LIBS) $(BASE_LIBS) objs/linear_solver_example.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/linear_solver_example.o $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) -o linear_solver_example

objs/integer_solver_example.o: examples/integer_solver_example.cc
	$(CCC) $(CFLAGS) -c examples/integer_solver_example.cc -o objs/integer_solver_example.o

integer_solver_example: $(LP_LIBS) $(BASE_LIBS) objs/integer_solver_example.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/integer_solver_example.o $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) -o integer_solver_example

# SWIG

# pywrapknapsack_solver

pyalgorithms: _pywrapknapsack_solver.so algorithms/pywrapknapsack_solver.py $(ALGORITHMS_LIBS) $(BASE_LIBS)

algorithms/pywrapknapsack_solver.py: algorithms/knapsack_solver.swig algorithms/knapsack_solver.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/algorithms/knapsack_solver_wrap.cc -module pywrapknapsack_solver algorithms/knapsack_solver.swig

gen/algorithms/knapsack_solver_wrap.cc: algorithms/pywrapknapsack_solver.py

objs/knapsack_solver_wrap.o: gen/algorithms/knapsack_solver_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/algorithms/knapsack_solver_wrap.cc -o objs/knapsack_solver_wrap.o

_pywrapknapsack_solver.so: objs/knapsack_solver_wrap.o $(ALGORITHMS_LIBS) $(BASE_LIBS)
	$(LD) -o _pywrapknapsack_solver.so objs/knapsack_solver_wrap.o $(ALGORITHMS_LIBS) $(BASE_LIBS) $(LDFLAGS)

# pywrapflow

pygraph: _pywrapflow.so graph/pywrapflow.py $(GRAPH_LIBS) $(BASE_LIBS)

graph/pywrapflow.py: graph/flow.swig graph/min_cost_flow.h graph/max_flow.h graph/ebert_graph.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/graph/pywrapflow_wrap.cc -module pywrapflow graph/flow.swig

gen/graph/pywrapflow_wrap.cc: graph/pywrapflow.py

objs/pywrapflow_wrap.o: gen/graph/pywrapflow_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/graph/pywrapflow_wrap.cc -o objs/pywrapflow_wrap.o

_pywrapflow.so: objs/pywrapflow_wrap.o $(GRAPH_LIBS) $(BASE_LIBS)
	$(LD) -o _pywrapflow.so objs/pywrapflow_wrap.o $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS)

# pywrapcp

pycp: _pywrapcp.so constraint_solver/pywrapcp.py _pywraprouting.so constraint_solver/pywraprouting.py $(CP_LIBS) $(BASE_LIBS)

constraint_solver/pywrapcp.py: constraint_solver/constraint_solver.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/constraint_solver/constraint_solver_wrap.cc -module pywrapcp constraint_solver/constraint_solver.swig

gen/constraint_solver/constraint_solver_wrap.cc: constraint_solver/pywrapcp.py

objs/constraint_solver_wrap.o: gen/constraint_solver/constraint_solver_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/constraint_solver/constraint_solver_wrap.cc -o objs/constraint_solver_wrap.o

_pywrapcp.so: objs/constraint_solver_wrap.o $(CP_LIBS) $(BASE_LIBS)
	$(LD) -o _pywrapcp.so objs/constraint_solver_wrap.o $(CP_LIBS) $(BASE_LIBS) $(LDFLAGS)

# pywraprouting

constraint_solver/pywraprouting.py: constraint_solver/routing.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h constraint_solver/routing.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o gen/constraint_solver/routing_wrap.cc -module pywraprouting constraint_solver/routing.swig

gen/constraint_solver/routing_wrap.cc: constraint_solver/pywraprouting.py

objs/routing_wrap.o: gen/constraint_solver/routing_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/constraint_solver/routing_wrap.cc -o objs/routing_wrap.o

_pywraprouting.so: objs/routing_wrap.o $(CP_LIBS) $(BASE_LIBS)
	$(LD) -o _pywraprouting.so objs/routing_wrap.o $(CP_LIBS) $(BASE_LIBS) $(LDFLAGS)

# pywraplp

pylp: _pywraplp.so linear_solver/pywraplp.py $(LP_LIBS) $(BASE_LIBS)

linear_solver/pywraplp.py: linear_solver/linear_solver.swig linear_solver/linear_solver.h gen/linear_solver/linear_solver.pb.h base/base.swig
	$(SWIG_BINARY) $(CLP_INC) $(CBC_INC) $(GLPK_INC) -c++ -python -o gen/linear_solver/linear_solver_wrap.cc -module pywraplp linear_solver/linear_solver.swig

gen/linear_solver/linear_solver_wrap.cc: linear_solver/pywraplp.py

objs/linear_solver_wrap.o: gen/linear_solver/linear_solver_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c gen/linear_solver/linear_solver_wrap.cc -o objs/linear_solver_wrap.o

_pywraplp.so: objs/linear_solver_wrap.o $(LP_LIBS) $(BASE_LIBS)
	$(LD) -o _pywraplp.so objs/linear_solver_wrap.o $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# ---------- Java Support ----------

# javacp

javacp: com.google.ortools.constraintsolver.jar libjniconstraintsolver.$(JNILIBEXT)
gen/constraint_solver/constraint_solver_java_wrap.cc: constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) -c++ -java -o gen/constraint_solver/constraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -outdir gen/com/google/ortools/constraintsolver constraint_solver/constraint_solver.swig
	$(FIX_SWIG)

objs/constraint_solver_java_wrap.o: gen/constraint_solver/constraint_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/constraint_solver/constraint_solver_java_wrap.cc -o objs/constraint_solver_java_wrap.o

com.google.ortools.constraintsolver.jar: gen/constraint_solver/constraint_solver_java_wrap.cc
	$(JAVAC_BIN) -d objs com/google/ortools/constraintsolver/*.java gen/com/google/ortools/constraintsolver/*.java
	jar cf com.google.ortools.constraintsolver.jar -C objs com/google/ortools/constraintsolver

libjniconstraintsolver.$(JNILIBEXT): objs/constraint_solver_java_wrap.o $(CP_LIBS) $(BASE_LIBS)
	$(LD) -o libjniconstraintsolver.$(JNILIBEXT) objs/constraint_solver_java_wrap.o $(CP_LIBS) $(BASE_LIBS) $(LDFLAGS)

# Java CP Examples

compile_RabbitsPheasants: objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class

objs/com/google/ortools/constraintsolver/samples/RabbitsPheasants.class: javacp com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/RabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.RabbitsPheasants

compile_GolombRuler: objs/com/google/ortools/constraintsolver/samples/GolombRuler.class

objs/com/google/ortools/constraintsolver/samples/GolombRuler.class: javacp com/google/ortools/constraintsolver/samples/GolombRuler.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/GolombRuler.java

run_GolombRuler: compile_GolombRuler
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.GolombRuler

compile_Partition: objs/com/google/ortools/constraintsolver/samples/Partition.class

objs/com/google/ortools/constraintsolver/samples/Partition.class: javacp com/google/ortools/constraintsolver/samples/Partition.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Partition.java

run_Partition: compile_Partition
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Partition

compile_SendMoreMoney: objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class

objs/com/google/ortools/constraintsolver/samples/SendMoreMoney.class: javacp com/google/ortools/constraintsolver/samples/SendMoreMoney.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SendMoreMoney.java

run_SendMoreMoney: compile_SendMoreMoney
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney

compile_SendMoreMoney2: objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class

objs/com/google/ortools/constraintsolver/samples/SendMoreMoney2.class: javacp com/google/ortools/constraintsolver/samples/SendMoreMoney2.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SendMoreMoney2.java

run_SendMoreMoney2: compile_SendMoreMoney2
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMoreMoney2

compile_LeastDiff: objs/com/google/ortools/constraintsolver/samples/LeastDiff.class

objs/com/google/ortools/constraintsolver/samples/LeastDiff.class: javacp com/google/ortools/constraintsolver/samples/LeastDiff.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/LeastDiff.java

run_LeastDiff: compile_LeastDiff
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.LeastDiff

compile_MagicSquare: objs/com/google/ortools/constraintsolver/samples/MagicSquare.class

objs/com/google/ortools/constraintsolver/samples/MagicSquare.class: javacp com/google/ortools/constraintsolver/samples/MagicSquare.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/MagicSquare.java

run_MagicSquare: compile_MagicSquare
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.MagicSquare

compile_NQueens: objs/com/google/ortools/constraintsolver/samples/NQueens.class

objs/com/google/ortools/constraintsolver/samples/NQueens.class: javacp com/google/ortools/constraintsolver/samples/NQueens.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/NQueens.java

run_NQueens: compile_NQueens
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens

compile_NQueens2: objs/com/google/ortools/constraintsolver/samples/NQueens2.class

objs/com/google/ortools/constraintsolver/samples/NQueens2.class: javacp com/google/ortools/constraintsolver/samples/NQueens2.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/NQueens2.java

run_NQueens2: compile_NQueens2
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.NQueens2


compile_AllDifferentExcept0: objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class

objs/com/google/ortools/constraintsolver/samples/AllDifferentExcept0.class: javacp com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/AllDifferentExcept0.java

run_AllDifferentExcept0: compile_AllDifferentExcept0
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.AllDifferentExcept0


compile_Diet: objs/com/google/ortools/constraintsolver/samples/Diet.class

objs/com/google/ortools/constraintsolver/samples/Diet.class: javacp com/google/ortools/constraintsolver/samples/Diet.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Diet.java

run_Diet: compile_Diet
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Diet


compile_Map: objs/com/google/ortools/constraintsolver/samples/Map.class

objs/com/google/ortools/constraintsolver/samples/Map.class: javacp com/google/ortools/constraintsolver/samples/Map.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Map.java

run_Map: compile_Map
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map


compile_Map2: objs/com/google/ortools/constraintsolver/samples/Map2.class

objs/com/google/ortools/constraintsolver/samples/Map2.class: javacp com/google/ortools/constraintsolver/samples/Map2.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Map2.java

run_Map2: compile_Map2
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Map2


compile_Minesweeper: objs/com/google/ortools/constraintsolver/samples/Minesweeper.class

objs/com/google/ortools/constraintsolver/samples/Minesweeper.class: javacp com/google/ortools/constraintsolver/samples/Minesweeper.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Minesweeper.java

run_Minesweeper: compile_Minesweeper
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Minesweeper


compile_QuasigroupCompletion: objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class

objs/com/google/ortools/constraintsolver/samples/QuasigroupCompletion.class: javacp com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/QuasigroupCompletion.java

run_QuasigroupCompletion: compile_QuasigroupCompletion
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.QuasigroupCompletion


compile_SendMostMoney: objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class

objs/com/google/ortools/constraintsolver/samples/SendMostMoney.class: javacp com/google/ortools/constraintsolver/samples/SendMostMoney.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SendMostMoney.java

run_SendMostMoney: compile_SendMostMoney
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SendMostMoney


compile_Seseman: objs/com/google/ortools/constraintsolver/samples/Seseman.class

objs/com/google/ortools/constraintsolver/samples/Seseman.class: javacp com/google/ortools/constraintsolver/samples/Seseman.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Seseman.java

run_Seseman: compile_Seseman
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Seseman


compile_Sudoku: objs/com/google/ortools/constraintsolver/samples/Sudoku.class

objs/com/google/ortools/constraintsolver/samples/Sudoku.class: javacp com/google/ortools/constraintsolver/samples/Sudoku.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Sudoku.java

run_Sudoku: compile_Sudoku
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Sudoku


compile_Xkcd: objs/com/google/ortools/constraintsolver/samples/Xkcd.class

objs/com/google/ortools/constraintsolver/samples/Xkcd.class: javacp com/google/ortools/constraintsolver/samples/Xkcd.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Xkcd.java

run_Xkcd: compile_Xkcd
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Xkcd


compile_SurvoPuzzle: objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class

objs/com/google/ortools/constraintsolver/samples/SurvoPuzzle.class: javacp com/google/ortools/constraintsolver/samples/SurvoPuzzle.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/SurvoPuzzle.java

run_SurvoPuzzle: compile_SurvoPuzzle
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.SurvoPuzzle


compile_Circuit: objs/com/google/ortools/constraintsolver/samples/Circuit.class

objs/com/google/ortools/constraintsolver/samples/Circuit.class: javacp com/google/ortools/constraintsolver/samples/Circuit.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/Circuit.java

run_Circuit: compile_Circuit
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.Circuit


compile_CoinsGrid: objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class

objs/com/google/ortools/constraintsolver/samples/CoinsGrid.class: javacp com/google/ortools/constraintsolver/samples/CoinsGrid.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/CoinsGrid.java

run_CoinsGrid: compile_CoinsGrid
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.CoinsGrid

# javaalgorithms

javaalgorithms: com.google.ortools.knapsacksolver.jar libjniknapsacksolver.$(JNILIBEXT)
gen/algorithms/knapsack_solver_java_wrap.cc: algorithms/knapsack_solver.swig base/base.swig util/data.swig algorithms/knapsack_solver.h
	$(SWIG_BINARY) -c++ -java -o gen/algorithms/knapsack_solver_java_wrap.cc -package com.google.ortools.knapsacksolver -outdir gen/com/google/ortools/knapsacksolver algorithms/knapsack_solver.swig

objs/knapsack_solver_java_wrap.o: gen/algorithms/knapsack_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/algorithms/knapsack_solver_java_wrap.cc -o objs/knapsack_solver_java_wrap.o

com.google.ortools.knapsacksolver.jar: gen/algorithms/knapsack_solver_java_wrap.cc
	$(JAVAC_BIN) -d objs gen/com/google/ortools/knapsacksolver/*.java
	jar cf com.google.ortools.knapsacksolver.jar -C objs com/google/ortools/knapsacksolver

libjniknapsacksolver.$(JNILIBEXT): objs/knapsack_solver_java_wrap.o $(ALGORITHMS_LIBS) $(BASE_LIBS)
	$(LD) -o libjniknapsacksolver.$(JNILIBEXT) objs/knapsack_solver_java_wrap.o $(ALGORITHMS_LIBS) $(BASE_LIBS) $(LDFLAGS)

# Java Algorithms Examples

compile_Knapsack: objs/com/google/ortools/knapsacksolver/samples/Knapsack.class

objs/com/google/ortools/knapsacksolver/samples/Knapsack.class: javacp com/google/ortools/knapsacksolver/samples/Knapsack.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.knapsacksolver.jar com/google/ortools/knapsacksolver/samples/Knapsack.java

run_Knapsack: compile_Knapsack
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.knapsacksolver.jar com.google.ortools.knapsacksolver.samples.Knapsack

# javagraph

javagraph: com.google.ortools.flow.jar libjniflow.$(JNILIBEXT)
gen/graph/flow_java_wrap.cc: graph/flow.swig base/base.swig util/data.swig graph/max_flow.h graph/min_cost_flow.h
	$(SWIG_BINARY) -c++ -java -o gen/graph/flow_java_wrap.cc -package com.google.ortools.flow -outdir gen/com/google/ortools/flow graph/flow.swig

objs/flow_java_wrap.o: gen/graph/flow_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/graph/flow_java_wrap.cc -o objs/flow_java_wrap.o

com.google.ortools.flow.jar: gen/graph/flow_java_wrap.cc
	$(JAVAC_BIN) -d objs gen/com/google/ortools/flow/*.java
	jar cf com.google.ortools.flow.jar -C objs com/google/ortools/flow

libjniflow.$(JNILIBEXT): objs/flow_java_wrap.o $(GRAPH_LIBS) $(BASE_LIBS)
	$(LD) -o libjniflow.$(JNILIBEXT) objs/flow_java_wrap.o $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS)

# Java Algorithms Examples

compile_FlowExample: objs/com/google/ortools/flow/samples/FlowExample.class

objs/com/google/ortools/flow/samples/FlowExample.class: javacp com/google/ortools/flow/samples/FlowExample.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.flow.jar com/google/ortools/flow/samples/FlowExample.java

run_FlowExample: compile_FlowExample
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.flow.jar com.google.ortools.flow.samples.FlowExample

# javalp

javalp: com.google.ortools.linearsolver.jar libjnilinearsolver.$(JNILIBEXT)
gen/linear_solver/linear_solver_java_wrap.cc: linear_solver/linear_solver.swig base/base.swig util/data.swig linear_solver/linear_solver.h
	$(SWIG_BINARY) $(CLP_INC) $(CBC_INC) $(GLPK_INC) -c++ -java -o gen/linear_solver/linear_solver_java_wrap.cc -package com.google.ortools.linearsolver -outdir gen/com/google/ortools/linearsolver linear_solver/linear_solver.swig

objs/linear_solver_java_wrap.o: gen/linear_solver/linear_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c gen/linear_solver/linear_solver_java_wrap.cc -o objs/linear_solver_java_wrap.o

com.google.ortools.linearsolver.jar: gen/linear_solver/linear_solver_java_wrap.cc
	$(JAVAC_BIN) -d objs gen/com/google/ortools/linearsolver/*.java
	jar cf com.google.ortools.linearsolver.jar -C objs com/google/ortools/linearsolver

libjnilinearsolver.$(JNILIBEXT): objs/linear_solver_java_wrap.o $(LP_LIBS) $(BASE_LIBS)
	$(LD) -o libjnilinearsolver.$(JNILIBEXT) objs/linear_solver_java_wrap.o $(LP_LIBS) $(BASE_LIBS) $(LDLPDEPS) $(LDFLAGS)

# Java Algorithms Examples

compile_LinearSolverExample: objs/com/google/ortools/linearsolver/samples/LinearSolverExample.class

objs/com/google/ortools/linearsolver/samples/LinearSolverExample.class: javacp com/google/ortools/linearsolver/samples/LinearSolverExample.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.linearsolver.jar com/google/ortools/linearsolver/samples/LinearSolverExample.java

run_LinearSolverExample: compile_LinearSolverExample
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.LinearSolverExample

compile_IntegerSolverExample: objs/com/google/ortools/linearsolver/samples/IntegerSolverExample.class

objs/com/google/ortools/linearsolver/samples/IntegerSolverExample.class: javacp com/google/ortools/linearsolver/samples/IntegerSolverExample.java
	$(JAVAC_BIN) -d objs -cp com.google.ortools.linearsolver.jar com/google/ortools/linearsolver/samples/IntegerSolverExample.java

run_IntegerSolverExample: compile_IntegerSolverExample
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp objs:com.google.ortools.linearsolver.jar com.google.ortools.linearsolver.samples.IntegerSolverExample
