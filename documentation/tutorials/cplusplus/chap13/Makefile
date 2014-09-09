OR_TOOLS_TOP=/home/nikolaj/Documents/WORK/Google/or-tools-read-only
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

TUTORIAL=/home/nikolaj/Documents/WORK/Google/OR_TOOLS_DOC/SOURCES/TUTORIALS/cplusplus

SOURCES= rl_auxiliary_graph

OBJECTS=$(SOURCES:.cc=.o)

EXE=$(SOURCES:.cc=)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all local_clean

all: $(EXE)

rl_auxiliary_graph.o: rl_auxiliary_graph.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h 
	$(CCC) $(CFLAGS) -c rl_auxiliary_graph.cc -o rl_auxiliary_graph.o

rl_auxiliary_graph: $(ROUTING_DEPS) rl_auxiliary_graph.o
	$(CCC) $(CFLAGS) rl_auxiliary_graph.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o rl_auxiliary_graph

local_clean:
	rm $(OBJECTS) $(EXE)

