# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# ---------- C++ support ----------
.PHONY: help_cpp # Generate list of C++ targets with descriptions.
help_cpp:
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
USE_GLPK ?= OFF
USE_HIGHS ?= OFF
USE_PDLP := ON # OFF not supported
USE_SCIP ?= ON
USE_CPLEX ?= OFF

USE_DOTNET_CORE_31 ?= OFF
USE_DOTNET_8 ?= ON
BUILD_VENV ?= ON
JOBS ?= 4

# Main targets.
.PHONY: detect_cpp # Show variables used to build C++ OR-Tools.
.PHONY: third_party # Build OR-Tools Prerequisite
.PHONY: cpp # Build C++ OR-Tools library.
.PHONY: check_cpp # Run all C++ OR-Tools samples and examples targets.
.PHONY: test_cpp # Run all C++ OR-Tools test targets.
.PHONY: test_fz # Run all Flatzinc test targets.
.PHONY: archive_cpp # Add C++ OR-Tools to archive.
.PHONY: clean_cpp # Clean C++ output from previous build.

GENERATOR ?= $(CMAKE_PLATFORM)

third_party:
	cmake -S . -B $(BUILD_DIR) -DBUILD_DEPS=ON \
 -DBUILD_DOTNET=$(BUILD_DOTNET) \
 -DBUILD_JAVA=$(BUILD_JAVA) \
 -DBUILD_PYTHON=$(BUILD_PYTHON) \
 -DBUILD_EXAMPLES=OFF \
 -DBUILD_SAMPLES=OFF \
 -DBUILD_TESTING=OFF \
 -DUSE_COINOR=$(USE_COINOR) \
 -DUSE_GLPK=$(USE_GLPK) \
 -DUSE_HIGHS=$(USE_HIGHS) \
 -DUSE_PDLP=$(USE_PDLP) \
 -DUSE_SCIP=$(USE_SCIP) \
 -DUSE_CPLEX=$(USE_CPLEX) \
 -DUSE_DOTNET_CORE_31=$(USE_DOTNET_CORE_31) \
 -DUSE_DOTNET_8=$(USE_DOTNET_8) \
 -DBUILD_VENV=$(BUILD_VENV) \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
 $(CMAKE_ARGS) \
 -G $(GENERATOR)

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

test_fz: \
 cpp \
 rfz_golomb \
 rfz_alpha

TEMP_CPP_DIR := temp_cpp

$(TEMP_CPP_DIR):
	$(MKDIR) $(TEMP_CPP_DIR)

# Deprecated alias
.PHONY: help_cc
help_cc: help_cpp
	$(warning $@ is deprecated please use $< instead.)

.PHONY: detect_cc
detect_cc: detect_cpp
	$(warning $@ is deprecated please use $< instead.)

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

.PHONY: package_cpp
package_cpp: archive_cpp

.PHONY: package_cc
package_cc: package_cpp
	$(warning $@ is deprecated please use @< instead.)

.PHONY: clean_cc
clean_cc: clean_cpp
	$(warning $@ is deprecated please use $< instead.)

# Now flatzinc is build with cpp
fz: cpp
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
	$(COPY) ortools$Scpp$SCMakeLists.txt.in $(TEMP_CPP_DIR)$S$(SOURCE_NAME)$SCMakeLists.txt
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
 -DCMAKE_PREFIX_PATH=$(OR_ROOT_FULL)/$(INSTALL_DIR) \
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
 $(SRC_DIR)/ortools/$1/samples/%.cc \
 ${SRC_DIR}/ortools/cpp/CMakeLists.txt.in \
 | $(TEMP_CPP_DIR)/$1/%
	$(COPY) ortools$Scpp$SCMakeLists.txt.in $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
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
 $(SRC_DIR)/ortools/$1/samples/%.cc \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.txt \
 $(TEMP_CPP_DIR)/$1/%/%.cc \
 FORCE
	cd $(TEMP_CPP_DIR)$S$1$S$$* &&\
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_PREFIX_PATH=$(OR_ROOT_FULL)/$(INSTALL_DIR) \
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

CPP_SAMPLES := algorithms graph glop constraint_solver linear_solver math_opt model_builder pdlp routing sat set_cover
$(foreach sample,$(CPP_SAMPLES),$(eval $(call cpp-sample-target,$(sample))))

# Examples
define cpp-example-target =
$(TEMP_CPP_DIR)/$1: | $(TEMP_CPP_DIR)
	-$(MKDIR) $(TEMP_CPP_DIR)$S$1

$(TEMP_CPP_DIR)/$1/%: \
 $(SRC_DIR)/examples/$1/%.cc \
 | $(TEMP_CPP_DIR)/$1
	-$(MKDIR) $(TEMP_CPP_DIR)$S$1$S$$*

$(TEMP_CPP_DIR)/$1/%/CMakeLists.txt: \
 $(SRC_DIR)/examples/$1/%.cc \
 ${SRC_DIR}/ortools/cpp/CMakeLists.txt.in \
 | $(TEMP_CPP_DIR)/$1/%
	$(COPY) ortools$Scpp$SCMakeLists.txt.in $(TEMP_CPP_DIR)$S$1$S$$*$SCMakeLists.txt
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
 $(SRC_DIR)/examples/$1/%.cc \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.txt \
 $(TEMP_CPP_DIR)/$1/%/%.cc \
 FORCE
	cd $(TEMP_CPP_DIR)$S$1$S$$* &&\
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_PREFIX_PATH=$(OR_ROOT_FULL)/$(INSTALL_DIR) \
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
	$(COPY) ortools$Scpp$SCMakeLists.txt.in $(TEMP_CPP_DIR)$Stests$S$*$SCMakeLists.txt
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
 $(SRC_DIR)/examples/tests/%.cc \
 $(TEMP_CPP_DIR)/tests/%/CMakeLists.txt \
 $(TEMP_CPP_DIR)/tests/%/%.cc \
 FORCE
	cd $(TEMP_CPP_DIR)$Stests$S$* && \
 cmake -S. -Bbuild \
 -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
 -DCMAKE_PREFIX_PATH=$(OR_ROOT_FULL)/$(INSTALL_DIR) \
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
# TODO(user) Port it to CMake
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

.PHONY: test_cpp_algorithms_samples # Build and Run all C++ Algorithms Samples (located in ortools/algorithms/samples)
test_cpp_algorithms_samples: \
 rcpp_knapsack \
 rcpp_simple_knapsack_program

.PHONY: test_cpp_graph_samples # Build and Run all C++ Graph Samples (located in ortools/graph/samples)
test_cpp_graph_samples: \
 rcpp_simple_max_flow_program \
 rcpp_simple_min_cost_flow_program

.PHONY: test_cpp_constraint_solver_samples # Build and Run all C++ CP Samples (located in ortools/constraint_solver/samples)
test_cpp_constraint_solver_samples: \
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
 rcpp_vrp_solution_callback \
 rcpp_vrp_starts_ends \
 rcpp_vrp_time_windows \
 rcpp_vrp_with_time_limit

.PHONY: test_cpp_linear_solver_samples # Build and Run all C++ LP Samples (located in ortools/linear_solver/samples)
test_cpp_linear_solver_samples: \
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

.PHONY: test_cpp_model_builder_samples # Build and Run all C++ CP Samples (located in ortools/model_builder/samples)
test_cpp_model_builder_samples: \

.PHONY: test_cpp_pdlp_samples # Build and Run all C++ CP Samples (located in ortools/model_builder/samples)
test_cpp_pdlp_samples: \
 rcpp_simple_pdlp_program

.PHONY: test_cpp_sat_samples # Build and Run all C++ Sat Samples (located in ortools/sat/samples)
test_cpp_sat_samples: \
 rcpp_assignment_groups_sat \
 rcpp_assignment_sat \
 rcpp_assignment_task_sizes_sat \
 rcpp_assignment_teams_sat \
 rcpp_assumptions_sample_sat \
 rcpp_binpacking_problem_sat \
 rcpp_bool_or_sample_sat \
 rcpp_channeling_sample_sat \
 rcpp_copy_model_sample_sat \
 rcpp_cp_is_fun_sat \
 rcpp_cp_sat_example \
 rcpp_earliness_tardiness_cost_sample_sat \
 rcpp_interval_sample_sat \
 rcpp_literal_sample_sat \
 rcpp_minimal_jobshop_sat \
 rcpp_multiple_knapsack_sat \
 rcpp_non_linear_sat \
 rcpp_no_overlap_sample_sat \
 rcpp_nqueens_sat \
 rcpp_nurses_sat \
 rcpp_optional_interval_sample_sat \
 rcpp_rabbits_and_pheasants_sat \
 rcpp_ranking_sample_sat \
 rcpp_reified_sample_sat \
 rcpp_schedule_requests_sat \
 rcpp_search_for_all_solutions_sample_sat \
 rcpp_simple_sat_program \
 rcpp_solution_hinting_sample_sat \
 rcpp_solve_and_print_intermediate_solutions_sample_sat \
 rcpp_solve_with_time_limit_sample_sat \
 rcpp_step_function_sample_sat \
 rcpp_stop_after_n_solutions_sample_sat

.PHONY: check_cpp_pimpl
check_cpp_pimpl: \
 test_cpp_algorithms_samples \
 test_cpp_graph_samples \
 test_cpp_constraint_solver_samples \
 test_cpp_linear_solver_samples \
 test_cpp_model_builder_samples \
 test_cpp_pdlp_samples \
 test_cpp_sat_samples \
 \
 rcpp_linear_programming \
 rcpp_constraint_programming_cp \
 rcpp_integer_programming \
 rcpp_knapsack \
 rcpp_max_flow \
 rcpp_min_cost_flow

.PHONY: test_cc_tests # Build and Run all C++ Tests (located in examples/tests)
test_cc_tests: \
 rcpp_lp_test \
 rcpp_bug_fz1 \
 rcpp_cpp11_test \
 rcpp_forbidden_intervals_test \
 rcpp_issue57 \
 rcpp_min_max_test
#	$(MAKE) rcpp_issue173 # error: too long

.PHONY: test_cc_contrib # Build and Run all C++ Contrib (located in examples/contrib)
test_cc_contrib:

.PHONY: test_cc_cpp # Build and Run all C++ Examples (located in examples/cpp)
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
# TODO(user) how to build cvrptwlib and depends on it
# rcpp_cvrp_disjoint_tw \
# rcpp_cvrptw \
# rcpp_cvrptw_with_breaks \
# rcpp_cvrptw_with_refueling \
# rcpp_cvrptw_with_resources \
# rcpp_cvrptw_with_stop_times_and_resources
#	$(MAKE) run \
 SOURCE=examples/cpp/dimacs_assignment.cc \
 ARGS=examples/cpp/dimacs.asn
	$(MAKE) run \
 SOURCE=examples/cpp/dobble_ls.cc \
 ARGS="--time_limit_in_ms=10000"
	$(MAKE) run \
 SOURCE=examples/cpp/golomb_sat.cc \
 ARGS="--size=5"
	$(MAKE) run \
 SOURCE=examples/cpp/jobshop_sat.cc \
 ARGS="--input=$(subst $S,/,$(OR_ROOT_FULL))/examples/cpp/jobshop"
	$(MAKE) run \
 SOURCE=examples/cpp/mps_driver.cc \
 ARGS="--input=$(subst $S,/,$(OR_ROOT_FULL))/examples/cpp/test.mps"
	$(MAKE) run \
 SOURCE=examples/cpp/network_routing_sat.cc \
 ARGS="--clients=10 --backbones=5 --demands=10 --traffic_min=5 --traffic_max=10 --min_client_degree=2 --max_client_degree=5 --min_backbone_degree=3 --max_backbone_degree=5 --max_capacity=20 --fixed_charge_cost=10"
	$(MAKE) run \
 SOURCE=examples/cpp/sports_scheduling_sat.cc \
 ARGS="--params max_time_in_seconds:10.0"
#	$(MAKE) run SOURCE=examples/cpp/frequency_assignment_problem.cc  # Need data file
#	$(MAKE) run SOURCE=examples/cpp/pdptw.cc \
 ARGS="--pdp_file examples/cpp/pdptw.txt" # Fails on windows...
#	$(MAKE) run \
 SOURCE=examples/cpp/shift_minimization_sat.cc \
 ARGS="--input=$(subst $S,/,$(OR_ROOT_FULL))/examples/cpp/shift_minimization.dat"

rfz_%: cpp
	$(INSTALL_DIR)$Sbin$Sfzn-ortools$E $(OR_ROOT_FULL)/examples/flatzinc/$*.fzn

###############
##  Archive  ##
###############
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
$(INSTALL_CPP_NAME)/examples/%/CMakeLists.txt: \
 $(TEMP_CPP_DIR)/$1/%/CMakeLists.txt \
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
 $(EXAMPLE_CPP_FILES) \
 tools/README.cpp.md tools/Makefile.cpp.mk
	$(MAKE) third_party BUILD_DOTNET=OFF BUILD_JAVA=OFF BUILD_PYTHON=OFF INSTALL_DIR=$(OR_ROOT)$(INSTALL_CPP_NAME)
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v
	$(COPY) tools$SREADME.cpp.md $(INSTALL_CPP_NAME)$SREADME.md
	$(COPY) tools$SMakefile.cpp.mk $(INSTALL_CPP_NAME)$SMakefile
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' $(INSTALL_CPP_NAME)$SMakefile
ifeq ($(PLATFORM),WIN64)
	$(ZIP) -r $(INSTALL_CPP_NAME)$(ARCHIVE_EXT) $(INSTALL_CPP_NAME)
else
	$(TAR) --no-same-owner -czvf $(INSTALL_CPP_NAME)$(ARCHIVE_EXT) $(INSTALL_CPP_NAME)
endif

# Test archive
TEMP_CPP_TEST_DIR := temp_cpp_test
.PHONY: test_archive_cpp # Test C++ OR-Tools archive is OK.
test_archive_cpp: $(INSTALL_CPP_NAME)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_CPP_TEST_DIR)
	-$(MKDIR) $(TEMP_CPP_TEST_DIR)
ifeq ($(PLATFORM),WIN64)
	$(UNZIP) $< -d $(TEMP_CPP_TEST_DIR)
else
	$(TAR) -xvf $< -C $(TEMP_CPP_TEST_DIR)
endif
	cd $(TEMP_CPP_TEST_DIR)$S$(INSTALL_CPP_NAME) \
 && $(MAKE) MAKEFLAGS= \
 && $(MAKE) MAKEFLAGS= test

################
##  Cleaning  ##
################
clean_cpp:
	-$(DELREC) $(BUILD_DIR)
	-$(DELREC) $(INSTALL_DIR)
	-$(DELREC) $(TEMP_CPP_DIR)
	-$(DELREC) $(INSTALL_CPP_NAME)
	-$(DELREC) or-tools_cpp_*
	-$(DELREC) $(TEMP_CPP_TEST_DIR)
	-$(DEL) *.deb

#############
##  DEBUG  ##
#############
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
