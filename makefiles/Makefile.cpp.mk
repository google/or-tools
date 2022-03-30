# ---------- C++ support ----------
.PHONY: help_cc # Generate list of C++ targets with descriptions.
help_cc:
	@echo Use one of the following C++ targets:
ifeq ($(PLATFORM),WIN64)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.cpp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.cpp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

# Checks if the user has overwritten default install prefix.
# cf https://www.gnu.org/prep/standards/html_node/Directory-Variables.html#index-prefix
ifeq ($(PLATFORM),WIN64)
  prefix ?= C:\\Program Files\\or-tools
else
  prefix ?= /usr/local
endif

# Checks if the user has overwritten default libraries and binaries.
BUILD_TYPE ?= Release
USE_COINOR ?= ON
USE_SCIP ?= ON
USE_GLPK ?= OFF
USE_CPLEX ?= OFF
USE_XPRESS ?= OFF
PROTOC ?= $(OR_TOOLS_TOP)$Sbin$Sprotoc

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite

GENERATOR ?= $(CMAKE_PLATFORM)

third_party:
	cmake -S . -B $(BUILD_DIR) -DBUILD_DEPS=ON \
 -DBUILD_DOTNET=$(BUILD_DOTNET) \
 -DBUILD_JAVA=$(BUILD_JAVA) \
 -DBUILD_PYTHON=$(BUILD_PYTHON) \
 -DBUILD_EXAMPLES=OFF \
 -DBUILD_SAMPLES=OFF \
 -DUSE_COINOR=$(USE_COINOR) \
 -DUSE_SCIP=$(USE_SCIP) \
 -DUSE_GLPK=$(USE_GLPK) \
 -DUSE_CPLEX=$(USE_CPLEX) \
 -DUSE_XPRESS=$(USE_XPRESS) \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
 $(CMAKE_ARGS) \
 -G $(GENERATOR)

JOBS ?= 4

TEMP_CPP_DIR=temp_cpp

# Main target
.PHONY: cpp # Build C++ OR-Tools library.
.PHONY: check_cpp # Run all C++ OR-Tools test targets.
.PHONY: test_cpp # Run all C++ OR-Tools test targets.
.PHONY: fz # Build Flatzinc.
.PHONY: test_fz # Run all Flatzinc test targets.
.PHONY: package_cc # Create C++ OR-Tools package.

# OR Tools unique library.
cpp:
	$(MAKE) third_party
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v

check_cpp: check_cpp_pimpl

test_cpp: \
 cpp \
 test_cc_tests \
 test_cc_contrib \
 test_cc_cpp

# Now flatzinc is build with cpp
fz: cpp

test_fz: \
 cpp \
 rfz_golomb \
 rfz_alpha

$(TEMP_CPP_DIR):
	$(MKDIR) $(TEMP_CPP_DIR)

.PHONY: detect_cc
detect_cc: detect_cpp
	$(warning $@ is deprecated please use detect_cpp instead.)

.PHONY: cc
cc: cpp
	$(warning $@ is deprecated please use @< instead.)

.PHONY: check_cc
check_cc: check_cpp
	$(warning $@ is deprecated please use $< instead.)

.PHONY: check_cpp_%
check_cc_%: check_cpp_%
	$(warning $@ is deprecated please use @< instead.)

.PHONY: test_cc
test_cc: test_cpp
	$(warning $@ is deprecated please use @< instead.)

.PHONY: test_cc_%
test_cc_%: test_cpp_%
	$(warning $@ is deprecated please use @< instead.)

.PHONY: clean_cc
clean_cc: clean_cpp
	$(warning $@ is deprecated please use $< instead.)
##################
##  C++ SOURCE  ##
##################
ifeq ($(SOURCE_SUFFIX),.cc) # Those rules will be used if SOURCE contain a .cc file

SOURCE_PROJECT_DIR := $(SOURCE)
SOURCE_PROJECT_DIR := $(subst /$(SOURCE_NAME).cc,, $(SOURCE_PROJECT_DIR))
SOURCE_PROJECT_PATH = $(subst /,$S,$(SOURCE_PROJECT_DIR))

$(TEMP_CPP_DIR)/$(SOURCE_NAME): | $(TEMP_CPP_DIR)
	$(MKDIR) $(TEMP_CPP_DIR)$S$(SOURCE_NAME)

$(TEMP_CPP_DIR)/$(SOURCE_NAME)/CMakeLists.txt: ${SRC_DIR}/ortools/cpp/CMakeLists.txt.in | $(TEMP_CPP_DIR)/$(SOURCE_NAME)
	$(SED) -e "s;@CPP_PREFIX_PATH@;$(OR_ROOT_FULL)$S$(INSTALL_DIR);" \
 ortools$Scpp$SCMakeLists.txt.in \
 > $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_NAME@/$(SOURCE_NAME)/' \
 $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_FILE_NAME@/$(SOURCE_NAME).cc/' \
 $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$SCMakeLists.txt
	$(SED) -i -e 's;@TEST_ARGS@;$(ARGS);' \
 $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$SCMakeLists.txt

.PHONY: build # Build a C++ program.
build: cpp \
	$(SOURCE) \
	$(TEMP_CPP_DIR)/$(SOURCE_NAME)/CMakeLists.txt
	-$(DELREC) $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$Sbuild
	$(COPY) $(subst /,$S,$(SOURCE)) $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$S$(SOURCE_NAME).cc
	cd $(TEMP_CPP_DIR)$S$(SOURCE_NAME) &&\
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -G $(GENERATOR) \
 $(CMAKE_ARGS)
ifneq ($(PLATFORM),WIN64)
	cd $(TEMP_CPP_DIR)$S$(SOURCE_NAME) && cmake --build build --config $(BUILD_TYPE) --target all -v
else
	cd $(TEMP_CPP_DIR)$S$(SOURCE_NAME) && cmake --build build --config $(BUILD_TYPE) --target ALL_BUILD -v
endif

.PHONY: run # Run a C++ program.
run: build
ifneq ($(PLATFORM),WIN64)
	cd $(TEMP_CPP_DIR)$S$(SOURCE_NAME) && cmake --build build --config $(BUILD_TYPE) --target test -v
else
	cd $(TEMP_CPP_DIR)$S$(SOURCE_NAME) && cmake --build build --config $(BUILD_TYPE) --target RUN_TESTS -v
endif
endif

##################################
##  CPP Tests/Examples/Samples  ##
##################################

# Samples
define cpp-sample-target =
$(TEMP_CPP_DIR)/$1: | $(TEMP_CPP_DIR)
	-$(MKDIR) $(TEMP_CPP_DIR)$S$1

$(TEMP_CPP_DIR)/$1/%: \
 $(SRC_DIR)/ortools/$1/samples/%.cc \
 | $(TEMP_CPP_DIR)/$1
	-$(MKDIR) $(TEMP_CPP_DIR)$S$1$S$$*

$(TEMP_CPP_DIR)/$1/%/CMakeLists.txt: \
 ${SRC_DIR}/ortools/cpp/CMakeLists.txt.in \
 | $(TEMP_CPP_DIR)/$1/%
	$(SED) -e "s;@CPP_PREFIX_PATH@;$(OR_ROOT_FULL)$S$(INSTALL_DIR);" \
 ortools$Scpp$SCMakeLists.txt.in \
 > $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_NAME@/$$*/' \
 $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_FILE_NAME@/$$*.cc/' \
 $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
	$(SED) -i -e 's/@TEST_ARGS@//' \
 $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt

$(TEMP_CPP_DIR)/$1/%/%.cc: \
 $(SRC_DIR)/ortools/$1/samples/%.cc \
 | $(TEMP_CPP_DIR)/$1/%
	$(MKDIR_P) $(TEMP_CPP_DIR)$S$1$S$$*
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.cc \
 $(TEMP_CPP_DIR)$S$1$S$$*

rcpp_%: \
 cpp \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.txt \
 $(TEMP_CPP_DIR)/$1/%/%.cc \
 FORCE
	cd $(TEMP_CPP_DIR)$S$1$S$$* &&\
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_INSTALL_PREFIX=install \
 $(CMAKE_ARGS) \
 -G $(GENERATOR)
ifneq ($(PLATFORM),WIN64)
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target all -v
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target test -v
else
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target ALL_BUILD -v
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target RUN_TESTS -v
endif
endef

CPP_SAMPLES := algorithms graph glop constraint_solver linear_solver model_builder routing sat
$(foreach sample,$(CPP_SAMPLES),$(eval $(call cpp-sample-target,$(sample))))

# Examples
define cpp-example-target =
$(TEMP_CPP_DIR)/$1: | $(TEMP_CPP_DIR)
	-$(MKDIR) $(TEMP_CPP_DIR)$S$1

$(TEMP_CPP_DIR)/$1/%: $(SRC_DIR)/examples/$1/%.cc | $(TEMP_CPP_DIR)/$1
	-$(MKDIR) $(TEMP_CPP_DIR)$S$1$S$$*

$(TEMP_CPP_DIR)/$1/%/CMakeLists.txt: ${SRC_DIR}/ortools/cpp/CMakeLists.txt.in | $(TEMP_CPP_DIR)/$1/%
	$(SED) -e "s;@CPP_PREFIX_PATH@;$(OR_ROOT_FULL)$S$(INSTALL_DIR);" \
 ortools$Scpp$SCMakeLists.txt.in \
 > $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_NAME@/$$*/' \
 $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_FILE_NAME@/$$*.cc/' \
 $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
	$(SED) -i -e 's/@TEST_ARGS@//' \
 $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt

$(TEMP_CPP_DIR)/$1/%/%.cc: \
 $(SRC_DIR)/examples/$1/%.cc \
 | $(TEMP_CPP_DIR)/$1/%
	$(MKDIR_P) $(TEMP_CPP_DIR)$S$1$S$$*
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.cc \
 $(TEMP_CPP_DIR)$S$1$S$$*

rcpp_%: \
 cpp \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.txt \
 $(TEMP_CPP_DIR)/$1/%/%.cc \
 FORCE
	cd $(TEMP_CPP_DIR)$S$1$S$$* &&\
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_INSTALL_PREFIX=install \
 $(CMAKE_ARGS) \
 -G $(GENERATOR)
ifneq ($(PLATFORM),WIN64)
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target all -v
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target test -v
else
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target ALL_BUILD -v
	cd $(TEMP_CPP_DIR)$S$1$S$$* && cmake --build build --config $(BUILD_TYPE) --target RUN_TESTS -v
endif
endef

CPP_EXAMPLES := contrib cpp
$(foreach example,$(CPP_EXAMPLES),$(eval $(call cpp-example-target,$(example))))

# Tests
CPP_TESTS := tests

$(TEMP_CPP_DIR)/tests: | $(TEMP_CPP_DIR)
	-$(MKDIR) $(TEMP_CPP_DIR)$Stests

$(TEMP_CPP_DIR)/tests/%: \
	$(SRC_DIR)/examples/tests/%.cc \
	| $(TEMP_CPP_DIR)/tests
	-$(MKDIR) $(TEMP_CPP_DIR)$Stests$S$*

$(TEMP_CPP_DIR)/tests/%/CMakeLists.txt: ${SRC_DIR}/ortools/cpp/CMakeLists.txt.in | $(TEMP_CPP_DIR)/tests/%
	$(SED) -e "s;@CPP_PREFIX_PATH@;$(OR_ROOT_FULL)$S$(INSTALL_DIR);" \
 ortools$Scpp$SCMakeLists.txt.in \
 > $(TEMP_CPP_DIR)$Stests$S$*$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_NAME@/$*/' \
 $(TEMP_CPP_DIR)$Stests$S$*$SCMakeLists.txt
	$(SED) -i -e 's/@CPP_FILE_NAME@/$*.cc/' \
 $(TEMP_CPP_DIR)$Stests$S$*$SCMakeLists.txt
	$(SED) -i -e 's/@TEST_ARGS@//' \
 $(TEMP_CPP_DIR)$Stests$S$*$SCMakeLists.txt

$(TEMP_CPP_DIR)/tests/%/%.cc: \
 $(SRC_DIR)/examples/tests/%.cc \
 | $(TEMP_CPP_DIR)/tests/%
	$(MKDIR_P) $(TEMP_CPP_DIR)$Stests$S$*
	$(COPY) $(SRC_DIR)$Sexamples$Stests$S$*.cc \
 $(TEMP_CPP_DIR)$Stests$S$*

rcpp_%: \
 cpp \
 $(TEMP_CPP_DIR)/tests/%/CMakeLists.txt \
 $(TEMP_CPP_DIR)/tests/%/%.cc \
 FORCE
	cd $(TEMP_CPP_DIR)$Stests$S$* && \
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_INSTALL_PREFIX=install \
 $(CMAKE_ARGS) \
 -G $(GENERATOR)
ifneq ($(PLATFORM),WIN64)
	cd $(TEMP_CPP_DIR)$Stests$S$* && cmake --build build --config $(BUILD_TYPE) --target all -v
	cd $(TEMP_CPP_DIR)$Stests$S$* && cmake --build build --config $(BUILD_TYPE) --target test -v
else
	cd $(TEMP_CPP_DIR)$Stests$S$* && cmake --build build --config $(BUILD_TYPE) --target ALL_BUILD -v
	cd $(TEMP_CPP_DIR)$Stests$S$* && cmake --build build --config $(BUILD_TYPE) --target RUN_TESTS -v
endif

##################################
##  Course scheduling example   ##
##################################
# TODO(mizux) Port it to CMake
# examples/cpp/course_scheduling.proto: ;
#
# $(SRC_DIR)/examples/cpp/course_scheduling.pb.cc: \
#  $(SRC_DIR)/examples/cpp/course_scheduling.proto | $(TEMP_CPP_DIR)/examples/cpp
# 	$(PROTOC) \
#  --proto_path=$(INSTALL_DIR)/include \
#  $(PROTOBUF_PROTOC_INC) \
#  --cpp_out=. \
#  $(SRC_DIR)/examples/cpp/course_scheduling.proto
#
# $(SRC_DIR)/examples/cpp/course_scheduling.pb.h: $(SRC_DIR)/examples/cpp/course_scheduling.pb.cc
# 	$(TOUCH) examples$Scpp$Scourse_scheduling.pb.h
#
# bin/course_scheduling$E: \
#  $(SRC_DIR)/examples/cpp/course_scheduling.h \
#  $(SRC_DIR)/examples/cpp/course_scheduling.cc \
#  $(SRC_DIR)/examples/cpp/course_scheduling.pb.h \
#  $(SRC_DIR)/examples/cpp/course_scheduling.pb.cc \
#  $(SRC_DIR)/examples/cpp/course_scheduling_run.cc \
#  cpp \
#  | bin
# 	cmake build to generate bin$Scourse_scheduling$E
#
# rcpp_course_scheduling: bin/course_scheduling$E FORCE
# 	bin$S$*$E $(ARGS)

####################
##  Test targets  ##
####################

.PHONY: test_cc_algorithms_samples # Build and Run all C++ Algorithms Samples (located in ortools/algorithms/samples)
test_cc_algorithms_samples: \
 rcpp_knapsack \
 rcpp_simple_knapsack_program

.PHONY: test_cc_graph_samples # Build and Run all C++ Graph Samples (located in ortools/graph/samples)
test_cc_graph_samples: \
 rcpp_simple_max_flow_program \
 rcpp_simple_min_cost_flow_program

.PHONY: test_cc_constraint_solver_samples # Build and Run all C++ CP Samples (located in ortools/constraint_solver/samples)
test_cc_constraint_solver_samples: \
 rcpp_minimal_jobshop_cp \
 rcpp_nurses_cp \
 rcpp_rabbits_and_pheasants_cp \
 rcpp_simple_ls_program \
 rcpp_simple_cp_program \
 rcpp_simple_routing_program \
 rcpp_tsp \
 rcpp_tsp_cities \
 rcpp_tsp_circuit_board \
 rcpp_tsp_distance_matrix \
 rcpp_vrp \
 rcpp_vrp_breaks \
 rcpp_vrp_capacity \
 rcpp_vrp_drop_nodes \
 rcpp_vrp_global_span \
 rcpp_vrp_initial_routes \
 rcpp_vrp_pickup_delivery \
 rcpp_vrp_pickup_delivery_fifo \
 rcpp_vrp_pickup_delivery_lifo \
 rcpp_vrp_resources \
 rcpp_vrp_starts_ends \
 rcpp_vrp_time_windows \
 rcpp_vrp_with_time_limit

.PHONY: test_cc_linear_solver_samples # Build and Run all C++ LP Samples (located in ortools/linear_solver/samples)
test_cc_linear_solver_samples: \
 rcpp_assignment_mip \
 rcpp_basic_example \
 rcpp_bin_packing_mip \
 rcpp_integer_programming_example \
 rcpp_linear_programming_example \
 rcpp_mip_var_array \
 rcpp_multiple_knapsack_mip \
 rcpp_simple_lp_program \
 rcpp_simple_mip_program \
 rcpp_stigler_diet

.PHONY: test_cc_model_builder_samples # Build and Run all C++ CP Samples (located in ortools/model_builder/samples)
test_cc_model_builder_samples: \


.PHONY: test_cc_sat_samples # Build and Run all C++ Sat Samples (located in ortools/sat/samples)
test_cc_sat_samples: \
 rcpp_assignment_sat \
 rcpp_assumptions_sample_sat \
 rcpp_binpacking_problem_sat \
 rcpp_bool_or_sample_sat \
 rcpp_channeling_sample_sat \
 rcpp_cp_is_fun_sat \
 rcpp_earliness_tardiness_cost_sample_sat \
 rcpp_interval_sample_sat \
 rcpp_literal_sample_sat \
 rcpp_no_overlap_sample_sat \
 rcpp_optional_interval_sample_sat \
 rcpp_rabbits_and_pheasants_sat \
 rcpp_ranking_sample_sat \
 rcpp_reified_sample_sat \
 rcpp_search_for_all_solutions_sample_sat \
 rcpp_simple_sat_program \
 rcpp_solution_hinting_sample_sat \
 rcpp_solve_and_print_intermediate_solutions_sample_sat \
 rcpp_solve_with_time_limit_sample_sat \
 rcpp_step_function_sample_sat \
 rcpp_stop_after_n_solutions_sample_sat

.PHONY: check_cpp_pimpl
check_cpp_pimpl: \
 test_cc_algorithms_samples \
 test_cc_constraint_solver_samples \
 test_cc_graph_samples \
 test_cc_linear_solver_samples \
 test_cc_model_builder_samples \
 test_cc_sat_samples \
 \
 rcpp_linear_programming \
 rcpp_constraint_programming_cp \
 rcpp_integer_programming \
 rcpp_knapsack \
 rcpp_max_flow \
 rcpp_min_cost_flow ;

.PHONY: test_cc_tests # Build and Run all C++ Tests (located in ortools/examples/tests)
test_cc_tests: \
 rcpp_lp_test \
 rcpp_bug_fz1 \
 rcpp_cpp11_test \
 rcpp_forbidden_intervals_test \
 rcpp_issue57 \
 rcpp_min_max_test
#	$(MAKE) rcpp_issue173 # error: too long

.PHONY: test_cc_contrib # Build and Run all C++ Contrib (located in ortools/examples/contrib)
test_cc_contrib: ;

.PHONY: test_cc_cpp # Build and Run all C++ Examples (located in ortools/examples/cpp)
test_cc_cpp: \
 rcpp_costas_array_sat \
 rcpp_cryptarithm_sat \
 rcpp_flow_api \
 rcpp_linear_assignment_api \
 rcpp_linear_solver_protocol_buffers \
 rcpp_magic_sequence_sat \
 rcpp_magic_square_sat \
 rcpp_nqueens \
 rcpp_random_tsp \
 rcpp_slitherlink_sat \
 rcpp_strawberry_fields_with_column_generation \
 rcpp_uncapacitated_facility_location \
# rcpp_weighted_tardiness_sat # need input file
# TODO(mizux) how to build cvrptwlib and depends on it
# rcpp_cvrp_disjoint_tw \
# rcpp_cvrptw \
# rcpp_cvrptw_with_breaks \
# rcpp_cvrptw_with_refueling \
# rcpp_cvrptw_with_resources \
# rcpp_cvrptw_with_stop_times_and_resources
#	$(MAKE) run \
# SOURCE=examples/cpp/dimacs_assignment.cc \
# ARGS=examples/data/dimacs/assignment/small.asn
	$(MAKE) run \
 SOURCE=examples/cpp/dobble_ls.cc \
 ARGS="--time_limit_in_ms=10000"
	$(MAKE) run \
 SOURCE=examples/cpp/golomb_sat.cc \
 ARGS="--size=5"
	$(MAKE) run \
 SOURCE=examples/cpp/jobshop_sat.cc \
 ARGS="--input=examples/data/jobshop/ft06"
	$(MAKE) run \
 SOURCE=examples/cpp/mps_driver.cc \
 ARGS="--input examples/data/tests/test.mps"
	$(MAKE) run \
 SOURCE=examples/cpp/network_routing_sat.cc \
 ARGS="--clients=10 --backbones=5 --demands=10 --traffic_min=5 --traffic_max=10 --min_client_degree=2 --max_client_degree=5 --min_backbone_degree=3 --max_backbone_degree=5 --max_capacity=20 --fixed_charge_cost=10"
	$(MAKE) run \
 SOURCE=examples/cpp/sports_scheduling_sat.cc \
 ARGS="--params max_time_in_seconds:10.0"
#	$(MAKE) run SOURCE=examples/cpp/frequency_assignment_problem.cc  # Need data file
#	$(MAKE) run SOURCE=examples/cpp/pdptw.cc ARGS="--pdp_file $(OR_ROOT_FULL)/examples/data/pdptw/LC1_2_1.txt" # Fails on windows...
	$(MAKE) run \
 SOURCE=examples/cpp/shift_minimization_sat.cc \
 ARGS="--input $(OR_ROOT_FULL)/examples/data/shift_scheduling/minimization/data_1_23_40_66.dat"
	$(MAKE) run \
 SOURCE=examples/cpp/solve.cc \
 ARGS="--input $(OR_ROOT_FULL)/examples/data/tests/test2.mps"

rfz_%: fz
	$(INSTALL_DIR)$Sbin$Sfz$E $(FZ_EX_PATH)$S$*.fzn

###############
##  Archive  ##
###############
.PHONY: archive_cpp # Add C++ OR-Tools to archive.
archive_cpp: $(INSTALL_CPP_NAME)$(ARCHIVE_EXT)

$(INSTALL_CPP_NAME):
	$(MKDIR) $(INSTALL_CPP_NAME)

$(INSTALL_CPP_NAME)/examples: | $(INSTALL_CPP_NAME)
	$(MKDIR) $(INSTALL_CPP_NAME)$Sexamples

define cpp-sample-archive =
$(INSTALL_CPP_NAME)/examples/%/CMakeLists.txt: \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.txt \
 $(SRC_DIR)/ortools/$1/samples/%.cc \
 | $(INSTALL_CPP_NAME)/examples
	-$(MKDIR_P) $(INSTALL_CPP_NAME)$Sexamples$S$$*
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.cc $(INSTALL_CPP_NAME)$Sexamples$S$$*
	$(COPY) $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt $(INSTALL_CPP_NAME)$Sexamples$S$$*
endef

$(foreach sample,$(CPP_SAMPLES),$(eval $(call cpp-sample-archive,$(sample))))

define cpp-example-archive =
$(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/%/CMakeLists.txt: \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.xml \
 $(SRC_DIR)/examples/$1/%.cc \
 | $(INSTALL_CPP_NAME)/examples
	-$(MKDIR_P) $(INSTALL_CPP_NAME)$Sexamples$S$$*
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.cc $(INSTALL_CPP_NAME)$Sexamples$S$$*
	$(COPY) $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt $(INSTALL_CPP_NAME)$Sexamples$S$$*
endef

$(foreach example,$(CPP_EXAMPLES),$(eval $(call cpp-example-archive,$(example))))

SAMPLE_CPP_FILES = \
  $(addsuffix /CMakeLists.txt,$(addprefix $(INSTALL_CPP_NAME)/examples/,$(basename $(notdir $(wildcard ortools/*/samples/*.cc)))))

EXAMPLE_CPP_FILES = \
  $(addsuffix /CMakeLists.txt,$(addprefix $(INSTALL_CPP_NAME)/examples/,$(basename $(notdir $(wildcard examples/contrib/*.cc))))) \
  $(addsuffix /CMakeLists.txt,$(addprefix $(INSTALL_CPP_NAME)/examples/,$(basename $(notdir $(wildcard examples/cpp/*.cc)))))

$(INSTALL_CPP_NAME)$(ARCHIVE_EXT): \
 $(SAMPLE_CPP_FILES) \
 $(EXAMPLE_CPP_FILES)
	$(MAKE) third_party BUILD_PYTHON=OFF BUILD_JAVA=OFF BUILD_DOTNET=OFF INSTALL_DIR=$(OR_ROOT)$(INSTALL_CPP_NAME)
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v
ifeq ($(PLATFORM),WIN64)
	$(ZIP) -r $(INSTALL_CPP_NAME)$(ARCHIVE_EXT) $(INSTALL_CPP_NAME)
else
	$(TAR) --no-same-owner -czvf $(INSTALL_CPP_NAME)$(ARCHIVE_EXT) $(INSTALL_CPP_NAME)
endif






#################
##  Packaging  ##
#################
TEMP_PACKAGE_CC_DIR = temp_package_cc

$(TEMP_PACKAGE_CC_DIR):
	-$(MKDIR_P) $(TEMP_PACKAGE_CC_DIR)

$(TEMP_PACKAGE_CC_DIR)/$(INSTALL_DIR): | $(TEMP_PACKAGE_CC_DIR)
	$(MKDIR) $(TEMP_PACKAGE_CC_DIR)$S$(INSTALL_DIR)

package_cc: cpp | $(TEMP_PACKAGE_CC_DIR)/$(INSTALL_DIR)
ifeq ($(PLATFORM),WIN64)
	cd $(TEMP_PACKAGE_CC_DIR)\$(INSTALL_DIR) && \
		..\..\$(TAR) -C ..\.. -c -v include | ..\..\$(TAR) xvm
	cd $(TEMP_PACKAGE_CC_DIR)\$(INSTALL_DIR) && \
		..\..\$(TAR) -C ..\.. -c -v lib | ..\..\$(TAR) xvm
	cd $(TEMP_PACKAGE_CC_DIR)\$(INSTALL_DIR) && \
		..\..\$(TAR) -C ..\.. -c -v share | ..\..\$(TAR) xvm
else
	cd $(TEMP_PACKAGE_CC_DIR)/$(INSTALL_DIR) && \
		tar -C ../.. -c -v include | tar xvm
	cd $(TEMP_PACKAGE_CC_DIR)/$(INSTALL_DIR) && \
		tar -C ../.. -c -v lib | tar xvm
	cd $(TEMP_PACKAGE_CC_DIR)/$(INSTALL_DIR) && \
		tar -C ../.. -c -v share | tar xvm
endif
ifeq ($(PLATFORM),WIN64)
	cd $(TEMP_PACKAGE_CC_DIR) && ..$S$(ZIP) -r ..$S$(INSTALL_DIR)$(ARCHIVE_EXT) $(INSTALL_DIR)
else
	$(TAR) -C $(TEMP_PACKAGE_CC_DIR) --no-same-owner -czvf $(INSTALL_DIR)$(ARCHIVE_EXT) $(INSTALL_DIR)
endif

###############
##  INSTALL  ##
###############
# ref: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html#index-prefix
# ref: https://www.gnu.org/prep/standards/html_node/DESTDIR.html
.PHONY: install_dirs
install_dirs:
	-$(MKDIR_P) "$(DESTDIR)$(prefix)"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sinclude"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Slib"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sbin"
	-$(MKDIR) "$(DESTDIR)$(prefix)$Sshare"
	-$(MKDIR_P) "$(DESTDIR)$(prefix)$Sshare$Sdocs$Sortools$Ssat"

# Install C++ OR-Tools to $(DESTDIR)$(prefix)
.PHONY: install_cc
install_cc: | install_dirs
	$(COPY) LICENSE "$(DESTDIR)$(prefix)"
ifeq ($(PLATFORM),WIN64)
	$(COPYREC) /E /Y include "$(DESTDIR)$(prefix)"
	$(COPY) "$(INSTALL_DIR)$Slib$S$(LIB_PREFIX)ortools_full.$L" "$(DESTDIR)$(prefix)$Slib$S$(LIB_PREFIX)ortools.$L"
	$(COPYREC) /E /Y share "$(DESTDIR)$(prefix)"
else
	$(COPYREC) include "$(DESTDIR)$(prefix)$S"
	$(COPY) $(INSTALL_DIR)$Slib*$S$(LIB_PREFIX)ortools*.$L* "$(DESTDIR)$(prefix)$Slib$S"
	$(COPYREC) share "$(DESTDIR)$(prefix)$S"
endif
	$(COPY) bin$Sprotoc* "$(DESTDIR)$(prefix)$Sbin"
	$(COPY) ortools$Ssat$Sdocs$S*.md "$(DESTDIR)$(prefix)$Sshare$Sdocs$Sortools$Ssat"

#######################
##  EXAMPLE ARCHIVE  ##
#######################
TEMP_CC_DIR=temp_cpp

$(TEMP_CC_DIR)/ortools_examples: | $(TEMP_CC_DIR)
	$(MKDIR) $(TEMP_CC_DIR)$Sortools_examples

$(TEMP_CC_DIR)/ortools_examples/examples: | $(TEMP_CC_DIR)/ortools_examples
	$(MKDIR) $(TEMP_CC_DIR)$Sortools_examples$Sexamples

$(TEMP_CC_DIR)/ortools_examples/examples/cpp: | $(TEMP_CC_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp

$(TEMP_CC_DIR)/ortools_examples/examples/data: | $(TEMP_CC_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Sdata

.PHONY: cc_examples_archive # Build stand-alone C++ examples archive file for redistribution.
cc_examples_archive: | \
 $(TEMP_CC_DIR)/ortools_examples/examples/cpp \
 $(TEMP_CC_DIR)/ortools_examples/examples/data
	$(COPY) $(CC_EX_PATH)$S*.h $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) $(CC_EX_PATH)$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
#	$(COPY) $(CONTRIB_EX_PATH)$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) ortools$Salgorithms$Ssamples$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) ortools$Sgraph$Ssamples$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) ortools$Slinear_solver$Ssamples$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) ortools$Sconstraint_solver$Ssamples$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) ortools$Ssat$Ssamples$S*.cc $(TEMP_CC_DIR)$Sortools_examples$Sexamples$Scpp
	$(COPY) tools$SREADME.cpp.md $(TEMP_CC_DIR)$Sortools_examples$SREADME.md
	$(COPY) LICENSE $(TEMP_CC_DIR)$Sortools_examples
ifeq ($(PLATFORM),WIN64)
	cd $(TEMP_CC_DIR)\ortools_examples \
 && ..\..\$(TAR) -C ..\.. -c -v \
 --exclude *svn* --exclude *roadef* --exclude *vector_packing* \
 examples\data | ..\..\$(TAR) xvm
	cd $(TEMP_CC_DIR) \
 && ..\$(ZIP) \
 -r ..\or-tools_cpp_examples_v$(OR_TOOLS_VERSION).zip \
 ortools_examples
else
	cd $(TEMP_CC_DIR)/ortools_examples \
 && tar -C ../.. -c -v \
 --exclude *svn* --exclude *roadef* --exclude *vector_packing* \
 examples/data | tar xvm
	cd $(TEMP_CC_DIR) \
 && tar -c -v -z --no-same-owner \
 -f ../or-tools_cpp_examples$(PYPI_OS)_v$(OR_TOOLS_VERSION).tar.gz \
 ortools_examples
endif
	-$(DELREC) $(TEMP_CC_DIR)$Sortools_examples

################
##  Cleaning  ##
################
.PHONY: clean_cpp # Clean C++ output from previous build.
clean_cpp:
	-$(DELREC) $(BUILD_DIR)
	-$(DELREC) $(INSTALL_DIR)
	-$(DELREC) $(TEMP_CPP_DIR)
	-$(DELREC) $(INSTALL_CPP_NAME)

#############
##  DEBUG  ##
#############
.PHONY: detect_cpp # Show variables used to build C++ OR-Tools.
detect_cpp:
	@echo Relevant info for the C++ build:
	@echo SRC_DIR = $(SRC_DIR)
	@echo BUILD_DIR = $(BUILD_DIR)
	@echo INSTALL_DIR = $(INSTALL_DIR)
	@echo prefix = $(prefix)
	@echo BUILD_TYPE = $(BUILD_TYPE)
	@echo USE_GLOP = ON
	@echo USE_PDLP = ON
	@echo USE_COINOR = $(USE_COINOR)
	@echo USE_SCIP = $(USE_SCIP)
	@echo USE_GLPK = $(USE_GLPK)
	@echo USE_CPLEX = $(USE_CPLEX)
	@echo USE_XPRESS = $(USE_XPRESS)
ifdef GLPK_ROOT
	@echo GLPK_ROOT = $(GLPK_ROOT)
endif
ifdef CPLEX_ROOT
	@echo CPLEX_ROOT = $(CPLEX_ROOT)
endif
ifdef XPRESS_ROOT
	@echo XPRESS_ROOT = $(XPRESS_ROOT)
endif
	@echo INSTALL_CPP_NAME = $(INSTALL_CPP_NAME)
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif
