OR_TOOLS_TOP=
OR_TOOLS_SOURCES=$(OR_TOOLS_TOP)/src

TUTORIAL=

SOURCES= vrp_partial_routes.cc rl_auxiliary_graph.cc cvrp_data_generator.cc check_vrp_solution.cc check_cvrp_solution.cc \
	 cvrp_solution_to_epix.cc vrp.cc vrp_solution_to_epix.cc cvrp_basic.cc

OBJECTS=$(SOURCES:.cc=.$O)

EXE=$(SOURCES:.cc=)

include $(OR_TOOLS_TOP)/Makefile

.PHONY: all tutorials local_clean

tutorials: $(EXE)

vrp_partial_routes.o: vrp_partial_routes.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h 
	$(CCC) $(CFLAGS) -c vrp_partial_routes.cc -o vrp_partial_routes.o

vrp_partial_routes: $(ROUTING_DEPS) vrp_partial_routes.o
	$(CCC) $(CFLAGS) vrp_partial_routes.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o vrp_partial_routes

rl_auxiliary_graph.o: rl_auxiliary_graph.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h 
	$(CCC) $(CFLAGS) -c rl_auxiliary_graph.cc -o rl_auxiliary_graph.o

rl_auxiliary_graph: $(ROUTING_DEPS) rl_auxiliary_graph.o
	$(CCC) $(CFLAGS) rl_auxiliary_graph.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o rl_auxiliary_graph

cvrp_data_generator.o: cvrp_data_generator.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h \
	$(TUTORIAL)/routing_common/routing_common.h $(TUTORIAL)/routing_common/routing_data_generator.h cvrp_data_generator.h
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c cvrp_data_generator.cc -o cvrp_data_generator.o

cvrp_data_generator: $(ROUTING_DEPS) cvrp_data_generator.o
	$(CCC) $(CFLAGS) cvrp_data_generator.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o cvrp_data_generator

check_vrp_solution.o: check_vrp_solution.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h \
	$(TUTORIAL)/routing_common/routing_common.h $(TUTORIAL)/routing_common/routing_data_generator.h cvrp_solution.h
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c check_vrp_solution.cc -o check_vrp_solution.o

check_vrp_solution: $(ROUTING_DEPS) check_vrp_solution.o
	$(CCC) $(CFLAGS) check_vrp_solution.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o check_vrp_solution

check_cvrp_solution.o: check_cvrp_solution.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h \
	$(TUTORIAL)/routing_common/routing_common.h $(TUTORIAL)/routing_common/routing_data_generator.h cvrp_solution.h
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c check_cvrp_solution.cc -o check_cvrp_solution.o

check_cvrp_solution: $(ROUTING_DEPS) check_cvrp_solution.o
	$(CCC) $(CFLAGS) check_cvrp_solution.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o check_cvrp_solution
	
cvrp_solution_to_epix.o: cvrp_solution_to_epix.cc $(TUTORIAL)/routing_common/routing_common.h $(TUTORIAL)/routing_common/routing_data.h \
	$(TUTORIAL)/routing_common/routing_solution.h $(TUTORIAL)/routing_common/routing_epix_helper.h cvrp_data.h cvrp_solution.h \
	cvrp_epix_data.h
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c cvrp_solution_to_epix.cc -o cvrp_solution_to_epix.o

cvrp_solution_to_epix: $(ROUTING_DEPS) cvrp_solution_to_epix.o
	$(CCC) $(CFLAGS) cvrp_solution_to_epix.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o cvrp_solution_to_epix

vrp.o: vrp.cc $(TUTORIAL)/routing_common/routing_common.h $(TUTORIAL)/routing_common/routing_data.h \
	$(TUTORIAL)/routing_common/routing_solution.h $(TUTORIAL)/routing_common/routing_epix_helper.h cvrp_data.h cvrp_solution.h
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c vrp.cc -o vrp.o

vrp: $(ROUTING_DEPS) vrp.o
	$(CCC) $(CFLAGS) vrp.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o vrp
	
vrp_solution_to_epix.o: vrp_solution_to_epix.cc $(TUTORIAL)/routing_common/routing_common.h $(TUTORIAL)/routing_common/routing_data.h \
	$(TUTORIAL)/routing_common/routing_solution.h $(TUTORIAL)/routing_common/routing_epix_helper.h cvrp_data.h cvrp_solution.h \
	cvrp_epix_data.h
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c vrp_solution_to_epix.cc -o vrp_solution_to_epix.o

vrp_solution_to_epix: $(ROUTING_DEPS) vrp_solution_to_epix.o
	$(CCC) $(CFLAGS) vrp_solution_to_epix.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o vrp_solution_to_epix

cvrp_basic.o: cvrp_basic.cc $(OR_TOOLS_SOURCES)/constraint_solver/routing.h \
	$(TUTORIAL)/routing_common/tsplib_reader.h 
	$(CCC) $(CFLAGS) -I $(TUTORIAL) -c cvrp_basic.cc -o cvrp_basic.o

cvrp_basic: $(ROUTING_DEPS) cvrp_basic.o
	$(CCC) $(CFLAGS) cvrp_basic.o $(DYNAMIC_ROUTING_LNK) $(DYNAMIC_LD_FLAGS) -o cvrp_basic
	
local_clean:
	rm $(OBJECTS) $(EXE)

