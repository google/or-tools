# Please edit the following:
PYTHON_VER=2.6
GFLAGS_DIR=../gflags-1.4
SWIG_BINARY=swig
PROTOBUF_DIR=../protobuf-2.3.0

#  ----- You should not need to modify the following, unless the -----
#  ----- configuration is not standard ------

# This is needed to find python.h
PYTHON_INC=-I/usr/include/python$(PYTHON_VER) -I/usr/lib/python$(PYTHON_VER)
# This is needed to find gflags/gflags.h
GFLAGS_INC = -I$(GFLAGS_DIR)/include
# This is needed to find protocol buffers.
PROTOBUF_INC = -I$(PROTOBUF_DIR)/include

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
endif

CFLAGS= $(SYSCFLAGS) $(DEBUG) -I. $(GFLAGS_INC) $(ARCH) \
        -Wno-deprecated $(PROTOBUF_INC)
JNIFLAGS= $(SYSCFLAGS) $(JNIDEBUG) -I. $(GFLAGS_INC) $(ARCH) \
        -Wno-deprecated $(PROTOBUF_INC) $(CBC_INC) $(CLP_INC) $(GLPK_INC)
LDFLAGS=$(GFLAGS_LNK) $(ZLIB_LNK) $(PROTOBUF_LNK) $(SYS_LNK)

# Real targets

all:
	@echo Please define target:
	@echo "  - constraint programming: cplibs, cpexe, pycp, javacp"
	@echo "  - algorithms: algoritmlibs, pyalgorithms"
	@echo "  - graph: graphlibs, pygraph"
	@echo "  - misc: clean"

CPLIBS = \
	librouting.a       \
	libconstraint_solver.a

GRAPH_LIBS = \
	libgraph.a \
	libshortestpaths.a

graphlibs: $(GRAPH_LIBS) $(BASE_LIBS)

BASE_LIBS = \
	libutil.a          \
	libbase.a

cplibs: $(CPLIBS) $(BASE_LIBS)

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

ALGORITHM_LIBS = \
	libalgorithms.a

algorithm_libs: $(ALGORITHM_LIBS)

clean:
	rm -f *.a
	rm -f objs/*.o
	rm -f $(CPBINARIES)
	rm -f constraint_solver/*wrap*
	rm -f constraint_solver/assignment.pb.*
	rm -f *.so

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
	objs/table.o\
	objs/timetabling.o\
	objs/tree_monitor.o\
	objs/utilities.o

objs/alldiff_cst.o:constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/alldiff_cst.cc -o objs/alldiff_cst.o

objs/assignment.o:constraint_solver/assignment.cc constraint_solver/assignment.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/assignment.cc -o objs/assignment.o

objs/assignment.pb.o:constraint_solver/assignment.pb.cc
	$(CCC) $(CFLAGS) -c constraint_solver/assignment.pb.cc -o objs/assignment.pb.o

constraint_solver/assignment.pb.cc:constraint_solver/assignment.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=constraint_solver constraint_solver/assignment.proto

constraint_solver/assignment.pb.h:constraint_solver/assignment.pb.cc

objs/constraint_solver.o:constraint_solver/constraint_solver.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraint_solver.cc -o objs/constraint_solver.o

objs/constraints.o:constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraints.cc -o objs/constraints.o

objs/count_cst.o:constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/count_cst.cc -o objs/count_cst.o

objs/default_search.o:constraint_solver/default_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/default_search.cc -o objs/default_search.o

objs/demon_profiler.o:constraint_solver/demon_profiler.cc constraint_solver/demon_profiler.pb.h
	$(CCC) $(CFLAGS) -c constraint_solver/demon_profiler.cc -o objs/demon_profiler.o

objs/demon_profiler.pb.o:constraint_solver/demon_profiler.pb.cc
	$(CCC) $(CFLAGS) -c constraint_solver/demon_profiler.pb.cc -o objs/demon_profiler.pb.o

constraint_solver/demon_profiler.pb.cc:constraint_solver/demon_profiler.proto
	$(PROTOBUF_DIR)/bin/protoc --proto_path=constraint_solver --cpp_out=constraint_solver constraint_solver/demon_profiler.proto

constraint_solver/demon_profiler.pb.h:constraint_solver/demon_profiler.pb.cc

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

objs/search.o:constraint_solver/search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/search.cc -o objs/search.o

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

costas_array: $(CPLIBS) $(BASE_LIBS) objs/costas_array.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/costas_array.o $(CPLIBS) $(BASE_LIBS) -o costas_array

objs/cryptarithm.o:examples/cryptarithm.cc
	$(CCC) $(CFLAGS) -c examples/cryptarithm.cc -o objs/cryptarithm.o

cryptarithm: $(CPLIBS) $(BASE_LIBS) objs/cryptarithm.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cryptarithm.o $(CPLIBS) $(BASE_LIBS) -o cryptarithm

objs/cvrptw.o: examples/cvrptw.cc
	$(CCC) $(CFLAGS) -c examples/cvrptw.cc -o objs/cvrptw.o

cvrptw: $(CPLIBS) $(BASE_LIBS) objs/cvrptw.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cvrptw.o $(CPLIBS) $(BASE_LIBS) -o cvrptw

objs/flow_example.o:examples/flow_example.cc
	$(CCC) $(CFLAGS) -c examples/flow_example.cc -o objs/flow_example.o

flow_example: $(GRAPH_LIBS) $(BASE_LIBS) objs/flow_example.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/flow_example.o $(GRAPH_LIBS) $(BASE_LIBS) -o flow_example

objs/golomb.o:examples/golomb.cc
	$(CCC) $(CFLAGS) -c examples/golomb.cc -o objs/golomb.o

golomb: $(CPLIBS) $(BASE_LIBS) objs/golomb.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/golomb.o $(CPLIBS) $(BASE_LIBS) -o golomb

objs/magic_square.o:examples/magic_square.cc
	$(CCC) $(CFLAGS) -c examples/magic_square.cc -o objs/magic_square.o

magic_square: $(CPLIBS) $(BASE_LIBS) objs/magic_square.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/magic_square.o $(CPLIBS) $(BASE_LIBS) -o magic_square

objs/network_routing.o:examples/network_routing.cc
	$(CCC) $(CFLAGS) -c examples/network_routing.cc -o objs/network_routing.o

network_routing: $(CPLIBS) $(BASE_LIBS) $(GRAPH_LIBS) objs/network_routing.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/network_routing.o $(CPLIBS) $(GRAPH_LIBS) $(BASE_LIBS) -o network_routing

objs/nqueens.o: examples/nqueens.cc
	$(CCC) $(CFLAGS) -c examples/nqueens.cc -o objs/nqueens.o

nqueens: $(CPLIBS) $(BASE_LIBS) objs/nqueens.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/nqueens.o $(CPLIBS) $(BASE_LIBS) -o nqueens

objs/tricks.o: examples/tricks.cc
	$(CCC) $(CFLAGS) -c examples/tricks.cc -o objs/tricks.o

objs/global_arith.o: examples/global_arith.cc
	$(CCC) $(CFLAGS) -c examples/global_arith.cc -o objs/global_arith.o

tricks: $(CPLIBS) $(BASE_LIBS) objs/tricks.o objs/global_arith.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/tricks.o objs/global_arith.o $(CPLIBS) $(BASE_LIBS) -o tricks

# Routing Examples

objs/tsp.o: examples/tsp.cc
	$(CCC) $(CFLAGS) -c examples/tsp.cc -o objs/tsp.o

tsp: $(CPLIBS) $(BASE_LIBS) objs/tsp.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/tsp.o $(CPLIBS) $(BASE_LIBS) -o tsp

# SWIG

# pywrapknapsack_solver

pyalgorithms: _pywrapknapsack_solver.so algorithms/pywrapknapsack_solver.py $(ALGORITHM_LIBS) $(BASE_LIBS)

algorithms/pywrapknapsack_solver.py: algorithms/knapsack_solver.swig algorithms/knapsack_solver.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o algorithms/knapsack_solver_wrap.cc -module pywrapknapsack_solver algorithms/knapsack_solver.swig

algorithms/knapsack_solver_wrap.cc: algorithms/pywrapknapsack_solver.py

objs/knapsack_solver_wrap.o: algorithms/knapsack_solver_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c algorithms/knapsack_solver_wrap.cc -o objs/knapsack_solver_wrap.o

_pywrapknapsack_solver.so: objs/knapsack_solver_wrap.o $(ALGORITHM_LIBS) $(BASE_LIBS)
	$(LD) -o _pywrapknapsack_solver.so objs/knapsack_solver_wrap.o $(ALGORITHM_LIBS) $(BASE_LIBS) $(LDFLAGS)

# pywrapflow

pygraph: _pywrapflow.so graph/pywrapflow.py $(GRAPH_LIBS) $(BASE_LIBS)

graph/pywrapflow.py: graph/flow.swig graph/min_cost_flow.h graph/max_flow.h graph/ebert_graph.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o graph/pywrapflow_wrap.cc -module pywrapflow graph/flow.swig

graph/pywrapflow_wrap.cc: graph/pywrapflow.py

objs/pywrapflow_wrap.o: graph/pywrapflow_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c graph/pywrapflow_wrap.cc -o objs/pywrapflow_wrap.o

_pywrapflow.so: objs/pywrapflow_wrap.o $(GRAPH_LIBS) $(BASE_LIBS)
	$(LD) -o _pywrapflow.so objs/pywrapflow_wrap.o $(GRAPH_LIBS) $(BASE_LIBS) $(LDFLAGS)

# pywrapcp

pycp: _pywrapcp.so constraint_solver/pywrapcp.py _pywraprouting.so constraint_solver/pywraprouting.py $(CPLIBS) $(BASE_LIBS)

constraint_solver/pywrapcp.py: constraint_solver/constraint_solver.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o constraint_solver/constraint_solver_wrap.cc -module pywrapcp constraint_solver/constraint_solver.swig

constraint_solver/constraint_solver_wrap.cc: constraint_solver/pywrapcp.py

objs/constraint_solver_wrap.o: constraint_solver/constraint_solver_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c constraint_solver/constraint_solver_wrap.cc -o objs/constraint_solver_wrap.o

_pywrapcp.so: objs/constraint_solver_wrap.o $(CPLIBS) $(BASE_LIBS)
	$(LD) -o _pywrapcp.so objs/constraint_solver_wrap.o $(CPLIBS) $(BASE_LIBS) $(LDFLAGS)

# pywraprouting

constraint_solver/pywraprouting.py: constraint_solver/routing.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h constraint_solver/routing.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o constraint_solver/routing_wrap.cc -module pywraprouting constraint_solver/routing.swig

constraint_solver/routing_wrap.cc: constraint_solver/pywraprouting.py

objs/routing_wrap.o: constraint_solver/routing_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c constraint_solver/routing_wrap.cc -o objs/routing_wrap.o

_pywraprouting.so: objs/routing_wrap.o $(CPLIBS) $(BASE_LIBS)
	$(LD) -o _pywraprouting.so objs/routing_wrap.o $(CPLIBS) $(BASE_LIBS) $(LDFLAGS)

# ---------- Java Support ----------

# javawrapcp

javacp: com.google.ortools.constraintsolver.jar libjniconstraintsolver.$(JNILIBEXT)
constraint_solver/constraint_solver_java_wrap.cc: constraint_solver/constraint_solver.swig base/base.swig util/data.swig constraint_solver/constraint_solver.h
	$(SWIG_BINARY) -c++ -java -o constraint_solver/constraint_solver_java_wrap.cc -package com.google.ortools.constraintsolver -outdir com/google/ortools/constraintsolver constraint_solver/constraint_solver.swig
	sed -i -e 's/Tlong/T_long/g' com/google/ortools/constraintsolver/Solver.java

objs/constraint_solver_java_wrap.o: constraint_solver/constraint_solver_java_wrap.cc
	$(CCC) $(JNIFLAGS) $(JAVA_INC) -c constraint_solver/constraint_solver_java_wrap.cc -o objs/constraint_solver_java_wrap.o

com.google.ortools.constraintsolver.jar: constraint_solver/constraint_solver_java_wrap.cc
	$(JAVAC_BIN) com/google/ortools/constraintsolver/*.java
	jar cf com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/*.class

libjniconstraintsolver.$(JNILIBEXT): objs/constraint_solver_java_wrap.o $(CPLIBS) $(BASE_LIBS)
	$(LD) -o libjniconstraintsolver.$(JNILIBEXT) objs/constraint_solver_java_wrap.o $(CPLIBS) $(BASE_LIBS) $(LDFLAGS)

# Java CP Examples

compile_RabbitsPheasants: com/google/ortools/constraintsolver/samples/RabbitsPheasants.class

com/google/ortools/constraintsolver/samples/RabbitsPheasants.class: javacp com/google/ortools/constraintsolver/samples/RabbitsPheasants.java
	$(JAVAC_BIN) -cp com.google.ortools.constraintsolver.jar com/google/ortools/constraintsolver/samples/RabbitsPheasants.java

run_RabbitsPheasants: compile_RabbitsPheasants
	$(JAVA_BIN) -Djava.library.path=`pwd` -cp .:com.google.ortools.constraintsolver.jar com.google.ortools.constraintsolver.samples.RabbitsPheasants






