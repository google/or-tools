# Please edit the following:
PYTHON_VER=2.6
GFLAGS_DIR=../gflags-1.3
SWIG_BINARY=swig
ZLIB_DIR=../zlib-1.2.5

#  ----- No more editing below -----

PYTHON_INC=-I/usr/include/python$(PYTHON_VER) -I/usr/lib/python$(PYTHON_VER)

DEBUG=-O -DNDEBUG
SYSCFLAGS=-fPIC
CCC=g++

GFLAGS_INC = -I$(GFLAGS_DIR)/include
ZLIB_INC = -I$(ZLIB_DIR)/include
CFLAGS= $(SYSCFLAGS) $(DEBUG) -I. -DARCH_K8 $(GFLAGS_INC) $(ZLIB_INC) \
        -Wno-deprecated

# ----- OS Dependent -----
OS=$(shell uname -s)

ifeq ($(OS),Linux)
LD = gcc -shared
GFLAGS_LNK = -Wl,-rpath $(GFLAGS_DIR)/lib -L$(GFLAGS_DIR)/lib -lgflags
ZLIB_LNK = -Wl,-rpath $(ZLIB_DIR)/lib -L$(ZLIB_DIR)/lib -lz
endif
ifeq ($(OS),Darwin) # Assume Mac Os X
LD = ld -arch x86_64 -bundle -flat_namespace -undefined suppress
GFLAGS_LNK = -L$(GFLAGS_DIR)/lib -lgflags
ZLIB_LNK = -L$(ZLIB_DIR)/lib -lz
endif

LDFLAGS=$(GFLAGS_LNK) $(ZLIB_LNK)

# Real targets

BINARIES=nqueens golomb magic_square cryptarithm

all: libs $(BINARIES) pylib

LIBS = \
	libconstraint_solver.a            \
	libutil.a          \
	libbase.a          \
	libalgorithms.a    \
	libgraph.a         \
	libshortestpaths.a

libs: $(LIBS)

clean:
	rm -f *.a
	rm -f objs/*.o
	rm -f $(BINARIES)
	rm constraint_solver/*wrap*
	rm -f *.so

# Constraint Solver Lib.

CP_LIB_OBJS = \
	objs/alldiff_cst.o\
	objs/assignment.o\
	objs/constraint_solver.o\
	objs/constraints.o\
	objs/count_cst.o\
	objs/element.o\
	objs/expr_array.o\
	objs/expr_cst.o\
	objs/expressions.o\
	objs/interval.o\
	objs/local_search.o\
	objs/pack.o\
	objs/range_cst.o\
	objs/sched_search.o\
	objs/search.o\
	objs/table.o\
	objs/timetabling.o\
	objs/utilities.o

objs/alldiff_cst.o:constraint_solver/alldiff_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/alldiff_cst.cc -o objs/alldiff_cst.o

objs/assignment.o:constraint_solver/assignment.cc
	$(CCC) $(CFLAGS) -c constraint_solver/assignment.cc -o objs/assignment.o

objs/constraint_solver.o:constraint_solver/constraint_solver.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraint_solver.cc -o objs/constraint_solver.o

objs/constraints.o:constraint_solver/constraints.cc
	$(CCC) $(CFLAGS) -c constraint_solver/constraints.cc -o objs/constraints.o

objs/count_cst.o:constraint_solver/count_cst.cc
	$(CCC) $(CFLAGS) -c constraint_solver/count_cst.cc -o objs/count_cst.o

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

objs/sched_search.o:constraint_solver/sched_search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/sched_search.cc -o objs/sched_search.o

objs/search.o:constraint_solver/search.cc
	$(CCC) $(CFLAGS) -c constraint_solver/search.cc -o objs/search.o

objs/table.o:constraint_solver/table.cc
	$(CCC) $(CFLAGS) -c constraint_solver/table.cc -o objs/table.o

objs/timetabling.o:constraint_solver/timetabling.cc
	$(CCC) $(CFLAGS) -c constraint_solver/timetabling.cc -o objs/timetabling.o

objs/utilities.o:constraint_solver/utilities.cc
	$(CCC) $(CFLAGS) -c constraint_solver/utilities.cc -o objs/utilities.o

libconstraint_solver.a: $(CP_LIB_OBJS)
	ar rv libconstraint_solver.a $(CP_LIB_OBJS)

# Util library.

UTIL_LIB_OBJS=\
	objs/bitset.o

objs/bitset.o:util/bitset.cc
	$(CCC) $(CFLAGS) -c util/bitset.cc -o objs/bitset.o

libutil.a: $(UTIL_LIB_OBJS)
	ar rv libutil.a $(UTIL_LIB_OBJS)

# Graph library.

GRAPH_LIB_OBJS=\
	objs/bron_kerbosch.o

objs/bron_kerbosch.o:graph/bron_kerbosch.cc
	$(CCC) $(CFLAGS) -c graph/bron_kerbosch.cc -o objs/bron_kerbosch.o

libgraph.a: $(GRAPH_LIB_OBJS)
	ar rv libgraph.a $(GRAPH_LIB_OBJS)

# Shortestpaths library.

SHORTESTPATHS_LIB_OBJS=\
	objs/bellman_ford.o \
	objs/dijkstra.o

objs/bellman_ford.o:graph/bellman_ford.cc
	$(CCC) $(CFLAGS) -c graph/bellman_ford.cc -o objs/bellman_ford.o

objs/dijkstra.o:graph/dijkstra.cc
	$(CCC) $(CFLAGS) -c graph/dijkstra.cc -o objs/dijkstra.o

libshortestpaths.a: $(SHORTESTPATHS_LIB_OBJS)
	ar rv libshortestpaths.a $(SHORTESTPATHS_LIB_OBJS)

# Algorithms library.

ALGORITHMS_LIB_OBJS=\
	objs/hungarian.o

objs/hungarian.o:algorithms/hungarian.cc
	$(CCC) $(CFLAGS) -c algorithms/hungarian.cc -o objs/hungarian.o

libalgorithms.a: $(ALGORITHMS_LIB_OBJS)
	ar rv libalgorithms.a $(ALGORITHMS_LIB_OBJS)

# Base library.

BASE_LIB_OBJS=\
	objs/bitmap.o\
	objs/callback.o\
	objs/logging.o\
	objs/random.o\
	objs/stringpiece.o\
	objs/stringprintf.o\
	objs/util.o

objs/bitmap.o:base/bitmap.cc
	$(CCC) $(CFLAGS) -c base/bitmap.cc -o objs/bitmap.o
objs/callback.o:base/callback.cc
	$(CCC) $(CFLAGS) -c base/callback.cc -o objs/callback.o
objs/logging.o:base/logging.cc
	$(CCC) $(CFLAGS) -c base/logging.cc -o objs/logging.o
objs/random.o:base/random.cc
	$(CCC) $(CFLAGS) -c base/random.cc -o objs/random.o
objs/stringpiece.o:base/stringpiece.cc
	$(CCC) $(CFLAGS) -c base/stringpiece.cc -o objs/stringpiece.o
objs/stringprintf.o:base/stringprintf.cc
	$(CCC) $(CFLAGS) -c base/stringprintf.cc -o objs/stringprintf.o
objs/util.o:base/util.cc
	$(CCC) $(CFLAGS) -c base/util.cc -o objs/util.o

libbase.a: $(BASE_LIB_OBJS)
	ar rv libbase.a $(BASE_LIB_OBJS)

# Examples

objs/cryptarithm.o:examples/cryptarithm.cc
	$(CCC) $(CFLAGS) -c examples/cryptarithm.cc -o objs/cryptarithm.o

cryptarithm: $(LIBS) objs/cryptarithm.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/cryptarithm.o $(LIBS) -o cryptarithm

objs/golomb.o:examples/golomb.cc
	$(CCC) $(CFLAGS) -c examples/golomb.cc -o objs/golomb.o

golomb: $(LIBS) objs/golomb.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/golomb.o $(LIBS) -o golomb

objs/magic_square.o:examples/magic_square.cc
	$(CCC) $(CFLAGS) -c examples/magic_square.cc -o objs/magic_square.o

magic_square: $(LIBS) objs/magic_square.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/magic_square.o $(LIBS) -o magic_square

objs/nqueens.o: examples/nqueens.cc
	$(CCC) $(CFLAGS) -c examples/nqueens.cc -o objs/nqueens.o

nqueens: $(LIBS) objs/nqueens.o
	$(CCC) $(CFLAGS) $(LDFLAGS) objs/nqueens.o $(LIBS) -o nqueens

# SWIG

pylib: _pywrapcp.so constraint_solver/pywrapcp.py $(LIBS)

constraint_solver/pywrapcp.py: constraint_solver/constraint_solver.swig constraint_solver/constraint_solver.h constraint_solver/constraint_solveri.h base/base.swig
	$(SWIG_BINARY) -c++ -python -o constraint_solver/constraint_solver_wrap.cc -module pywrapcp constraint_solver/constraint_solver.swig

constraint_solver/constraint_solver_wrap.cc: constraint_solver/pywrapcp.py

objs/constraint_solver_wrap.o: constraint_solver/constraint_solver_wrap.cc
	$(CCC) $(CFLAGS) $(PYTHON_INC) -c constraint_solver/constraint_solver_wrap.cc -o objs/constraint_solver_wrap.o

_pywrapcp.so: objs/constraint_solver_wrap.o $(LIBS)
	$(LD) -o _pywrapcp.so objs/constraint_solver_wrap.o $(LIBS) $(GFLAGS_LNK)

