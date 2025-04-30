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

# ---------- .Net support ----------
.PHONY: help_dotnet # Generate list of dotnet targets with descriptions.
help_dotnet:
	@echo Use one of the following dotnet targets:
ifeq ($(PLATFORM),WIN64)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.dotnet.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.dotnet.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

# Main targets.
.PHONY: detect_dotnet # Show variables used to build dotnet OR-Tools.
.PHONY: dotnet # Build .Net OR-Tools library.
.PHONY: check_dotnet # Quick check only running .Net OR-Tools samples.
.PHONY: test_dotnet # Run all .Net OR-Tools test targets.
.PHONY: archive_dotnet # Add C++ OR-Tools to archive.
.PHONY: clean_dotnet # Clean files

ifeq ($(HAS_DOTNET),OFF)
dotnet:
	$(warning Either .NET support was turned of, of the dotnet binary was not found.)

check_dotnet: dotnet
test_dotnet: dotnet
package_dotnet: dotnet

else # HAS_DOTNET=ON

DOTNET_BUILD_PATH := $(BUILD_DIR)$Sdotnet

# All libraries and dependencies
DOTNET_PACKAGE_DIR := temp_dotnet/packages
DOTNET_PACKAGE_PATH := $(subst /,$S,$(DOTNET_PACKAGE_DIR))
DOTNET_ORTOOLS_ASSEMBLY_NAME := Google.OrTools
TEMP_DOTNET_DIR := temp_dotnet

# OR Tools unique library.
dotnet:
	$(MAKE) third_party BUILD_DOTNET=ON
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v

.PHONY: package_dotnet
package_dotnet: dotnet
	-$(DEL) $.*pkg
	$(COPY) $(DOTNET_BUILD_PATH)$Spackages$S*.*pkg .

$(TEMP_DOTNET_DIR):
	$(MKDIR) $(TEMP_DOTNET_DIR)

###################
##  .NET SOURCE  ##
###################
# .Net C#
ifeq ($(SOURCE_SUFFIX),.cs) # Those rules will be used if SOURCE contain a .cs file

SOURCE_PROJECT_DIR := $(SOURCE)
SOURCE_PROJECT_DIR := $(subst /$(SOURCE_NAME).cs,, $(SOURCE_PROJECT_DIR))
SOURCE_PROJECT_PATH = $(subst /,$S,$(SOURCE_PROJECT_DIR))

.PHONY: build # Build a .Net C# program.
build: $(SOURCE) $(SOURCE)proj dotnet
	cd $(SOURCE_PROJECT_PATH) && "$(DOTNET_BIN)" build -c Release $(ARGS)
	cd $(SOURCE_PROJECT_PATH) && "$(DOTNET_BIN)" pack -c Release

.PHONY: run # Run a .Net C# program (using 'dotnet run').
run: build
	cd $(SOURCE_PROJECT_PATH) && "$(DOTNET_BIN)" run --no-build -c Release $(ARGS)
	cd $(SOURCE_PROJECT_PATH) && "$(DOTNET_BIN)" clean -c Release -v minimal

.PHONY: run_test # Run a .Net C# program (using 'dotnet test').
run_test: build
	cd $(SOURCE_PROJECT_PATH) && "$(DOTNET_BIN)" test --no-build -c Release $(ARGS)
	cd $(SOURCE_PROJECT_PATH) && "$(DOTNET_BIN)" clean -c Release -v minimal
endif

###################################
##  .NET Samples/Examples/Tests  ##
###################################
# Samples
define dotnet-sample-target =
$(TEMP_DOTNET_DIR)/$1: | $(TEMP_DOTNET_DIR)
	-$(MKDIR) $(TEMP_DOTNET_DIR)$S$1

$(TEMP_DOTNET_DIR)/$1/%: \
 $(SRC_DIR)/ortools/$1/samples/%.cs \
 | $(TEMP_DOTNET_DIR)/$1
	-$(MKDIR) $(TEMP_DOTNET_DIR)$S$1$S$$*

$(TEMP_DOTNET_DIR)/$1/%/%.csproj: \
 $(SRC_DIR)/ortools/$1/samples/%.cs \
 ${SRC_DIR}/ortools/dotnet/Sample.csproj.in \
 | $(TEMP_DOTNET_DIR)/$1/%
	$(SED) -e "s/@DOTNET_PACKAGES_DIR@/..\/..\/..\/$(BUILD_DIR)\/dotnet\/packages/" \
 ortools$Sdotnet$SSample.csproj.in \
 > $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_SAMPLE_LANG@/9.0/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
ifeq ($(USE_DOTNET_8)_$(USE_DOTNET_CORE_31),ON_ON)
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFrameworks>netcoreapp3.1;net8.0<\/TargetFrameworks>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
else
 ifeq ($(USE_DOTNET_8),ON)
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFramework>net8.0<\/TargetFramework>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
 else
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFramework>netcoreapp3.1<\/TargetFramework>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
 endif
endif
	$(SED) -i -e 's/@DOTNET_PROJECT@/$(DOTNET_ORTOOLS_ASSEMBLY_NAME)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@SAMPLE_NAME@/$$*/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@COMPONENT_NAME@/$1/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@SAMPLE_FILE_NAME@/$$*.cs/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj

$(TEMP_DOTNET_DIR)/$1/%/%.cs: \
 $(SRC_DIR)/ortools/$1/samples/%.cs \
 | $(TEMP_DOTNET_DIR)/$1/%
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.cs \
 $(TEMP_DOTNET_DIR)$S$1$S$$*

rdotnet_%: \
 dotnet \
 $(SRC_DIR)/ortools/$1/samples/%.cs \
 $(TEMP_DOTNET_DIR)/$1/%/%.csproj \
 $(TEMP_DOTNET_DIR)/$1/%/%.cs \
 FORCE
ifeq ($(USE_DOTNET_8),ON)
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" build --framework net8.0 -c Release
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" run --no-build --framework net8.0 -c Release $(ARGS)
endif
ifeq ($(USE_DOTNET_CORE_31),ON)
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" build --framework netcoreapp3.1 -c Release
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" run --no-build --framework netcoreapp3.1 -c Release $(ARGS)
endif
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" clean -c Release -v minimal
endef

DOTNET_SAMPLES := init algorithms graph constraint_solver linear_solver sat util
$(foreach sample,$(DOTNET_SAMPLES),$(eval $(call dotnet-sample-target,$(sample))))

# Examples
define dotnet-example-target =
$(TEMP_DOTNET_DIR)/$1: | $(TEMP_DOTNET_DIR)
	-$(MKDIR) $(TEMP_DOTNET_DIR)$S$1

$(TEMP_DOTNET_DIR)/$1/%: \
 $(SRC_DIR)/examples/$1/%.cs \
 | $(TEMP_DOTNET_DIR)/$1
	-$(MKDIR) $(TEMP_DOTNET_DIR)$S$1$S$$*

$(TEMP_DOTNET_DIR)/$1/%/%.csproj: \
 $(SRC_DIR)/examples/$1/%.cs \
 ${SRC_DIR}/ortools/dotnet/Example.csproj.in \
 | $(TEMP_DOTNET_DIR)/$1/%
	$(SED) -e "s/@DOTNET_PACKAGES_DIR@/..\/..\/..\/$(BUILD_DIR)\/dotnet\/packages/" \
 ortools$Sdotnet$SExample.csproj.in \
 > $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_SAMPLE_LANG@/9.0/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
ifeq ($(USE_DOTNET_8)_$(USE_DOTNET_CORE_31),ON_ON)
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFrameworks>netcoreapp3.1;net8.0<\/TargetFrameworks>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
else
 ifeq ($(USE_DOTNET_8),ON)
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFramework>net8.0<\/TargetFramework>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
 else
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFramework>netcoreapp3.1<\/TargetFramework>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
 endif
endif
	$(SED) -i -e 's/@DOTNET_PROJECT@/$(DOTNET_ORTOOLS_ASSEMBLY_NAME)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@EXAMPLE_NAME@/$$*/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@COMPONENT_NAME@/$1/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@EXAMPLE_FILE_NAME@/$$*.cs/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj

$(TEMP_DOTNET_DIR)/$1/%/%.cs: \
 $(SRC_DIR)/examples/$1/%.cs \
 | $(TEMP_DOTNET_DIR)/$1/%
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.cs \
 $(TEMP_DOTNET_DIR)$S$1$S$$*

rdotnet_%: \
 dotnet \
 $(SRC_DIR)/examples/$1/%.cs \
 $(TEMP_DOTNET_DIR)/$1/%/%.csproj \
 $(TEMP_DOTNET_DIR)/$1/%/%.cs \
 FORCE
ifeq ($(USE_DOTNET_8),ON)
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" build --framework net8.0 -c Release
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" run --no-build --framework net8.0 -c Release $(ARGS)
endif
ifeq ($(USE_DOTNET_CORE_31),ON)
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" build --framework netcoreapp3.1 -c Release
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" run --no-build --framework netcoreapp3.1 -c Release $(ARGS)
endif
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" clean -c Release -v minimal
endef

DOTNET_EXAMPLES := contrib dotnet tests
$(foreach example,$(DOTNET_EXAMPLES),$(eval $(call dotnet-example-target,$(example))))

# Tests
define dotnet-test-target =
$(TEMP_DOTNET_DIR)/$1/%: \
 $(SRC_DIR)/ortools/$1/csharp/%.cs \
 | $(TEMP_DOTNET_DIR)/$1
	-$(MKDIR) $(TEMP_DOTNET_DIR)$S$1$S$$*

$(TEMP_DOTNET_DIR)/$1/%/%.csproj: \
 $(SRC_DIR)/ortools/$1/csharp/%.cs \
 ${SRC_DIR}/ortools/dotnet/Test.csproj.in \
 | $(TEMP_DOTNET_DIR)/$1/%
	$(SED) -e "s/@DOTNET_PACKAGES_DIR@/..\/..\/..\/$(BUILD_DIR)\/dotnet\/packages/" \
 ortools$Sdotnet$STest.csproj.in \
 > $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_LANG@/9.0/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
ifeq ($(USE_DOTNET_8)_$(USE_DOTNET_CORE_31),ON_ON)
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFrameworks>netcoreapp3.1;net8.0<\/TargetFrameworks>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
else
 ifeq ($(USE_DOTNET_8),ON)
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFramework>net8.0<\/TargetFramework>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
 else
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFramework>netcoreapp3.1<\/TargetFramework>/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
 endif
endif
	$(SED) -i -e 's/@DOTNET_PROJECT@/$(DOTNET_ORTOOLS_ASSEMBLY_NAME)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@TEST_NAME@/$$*/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_PROJECT@/$(DOTNET_ORTOOLS_PROJECT)/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj
	$(SED) -i -e 's/@FILE_NAME@/$$*.cs/' \
 $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj

$(TEMP_DOTNET_DIR)/$1/%/%.cs: \
 $(SRC_DIR)/ortools/$1/csharp/%.cs \
 | $(TEMP_DOTNET_DIR)/$1/%
	$(COPY) $(SRC_DIR)$Sortools$S$1$Scsharp$S$$*.cs \
 $(TEMP_DOTNET_DIR)$S$1$S$$*

rdotnet_%: \
 dotnet \
 $(SRC_DIR)/ortools/$1/csharp/%.cs \
 $(TEMP_DOTNET_DIR)/$1/%/%.csproj \
 $(TEMP_DOTNET_DIR)/$1/%/%.cs \
 FORCE
ifeq ($(USE_DOTNET_8),ON)
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" build --framework net8.0 -c Release
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" test --no-build --framework net8.0 -c Release $(ARGS)
endif
ifeq ($(USE_DOTNET_CORE_31),ON)
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" build --framework netcoreapp3.1 -c Release
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" test --no-build --framework netcoreapp3.1 -c Release $(ARGS)
endif
	cd $(TEMP_DOTNET_DIR)$S$1$S$$* && "$(DOTNET_BIN)" clean -c Release -v minimal
endef

DOTNET_TESTS := init algorithms graph constraint_solver linear_solver sat util
$(foreach test,$(DOTNET_TESTS),$(eval $(call dotnet-test-target,$(test))))

####################
##  Test targets  ##
####################
.PHONY: test_dotnet_algorithms_samples # Build and Run all .Net LP Samples (located in ortools/algorithms/samples)
test_dotnet_algorithms_samples: \
	rdotnet_Knapsack

.PHONY: test_dotnet_constraint_solver_samples # Build and Run all .Net CP Samples (located in ortools/constraint_solver/samples)
test_dotnet_constraint_solver_samples: \
 rdotnet_SimpleCpProgram \
 rdotnet_SimpleRoutingProgram \
 rdotnet_Tsp \
 rdotnet_TspCities \
 rdotnet_TspCircuitBoard \
 rdotnet_TspDistanceMatrix \
 rdotnet_Vrp \
 rdotnet_VrpBreaks \
 rdotnet_VrpCapacity \
 rdotnet_VrpDropNodes \
 rdotnet_VrpGlobalSpan \
 rdotnet_VrpInitialRoutes \
 rdotnet_VrpPickupDelivery \
 rdotnet_VrpPickupDeliveryFifo \
 rdotnet_VrpPickupDeliveryLifo \
 rdotnet_VrpResources \
 rdotnet_VrpSolutionCallback \
 rdotnet_VrpStartsEnds \
 rdotnet_VrpTimeWindows \
 rdotnet_VrpWithTimeLimit

.PHONY: test_dotnet_graph_samples # Build and Run all .Net LP Samples (located in ortools/graph/samples)
test_dotnet_graph_samples: \
 rdotnet_AssignmentLinearSumAssignment \
 rdotnet_AssignmentMinFlow \
 rdotnet_BalanceMinFlow \
 rdotnet_SimpleMaxFlowProgram \
 rdotnet_SimpleMinCostFlowProgram \

.PHONY: test_dotnet_linear_solver_samples # Build and Run all .Net LP Samples (located in ortools/linear_solver/samples)
test_dotnet_linear_solver_samples: \
 rdotnet_AssignmentMip \
 rdotnet_BasicExample \
 rdotnet_BinPackingMip \
 rdotnet_LinearProgrammingExample \
 rdotnet_MipVarArray \
 rdotnet_MultipleKnapsackMip \
 rdotnet_SimpleLpProgram \
 rdotnet_SimpleMipProgram \
 rdotnet_StiglerDiet

.PHONY: test_dotnet_sat_samples # Build and Run all .Net SAT Samples (located in ortools/sat/samples)
test_dotnet_sat_samples: \
 rdotnet_AssignmentGroupsSat \
 rdotnet_AssignmentSat \
 rdotnet_AssignmentTaskSizesSat \
 rdotnet_AssignmentTeamsSat \
 rdotnet_AssumptionsSampleSat \
 rdotnet_BinPackingProblemSat \
 rdotnet_BoolOrSampleSat \
 rdotnet_ChannelingSampleSat \
 rdotnet_CpIsFunSat \
 rdotnet_CpSatExample \
 rdotnet_EarlinessTardinessCostSampleSat \
 rdotnet_IntervalSampleSat \
 rdotnet_LiteralSampleSat \
 rdotnet_MinimalJobshopSat \
 rdotnet_MultipleKnapsackSat \
 rdotnet_NQueensSat \
 rdotnet_NoOverlapSampleSat \
 rdotnet_NonLinearSat \
 rdotnet_NursesSat \
 rdotnet_OptionalIntervalSampleSat \
 rdotnet_RabbitsAndPheasantsSat \
 rdotnet_RankingSampleSat \
 rdotnet_ReifiedSampleSat \
 rdotnet_ScheduleRequestsSat \
 rdotnet_SearchForAllSolutionsSampleSat \
 rdotnet_SimpleSatProgram \
 rdotnet_SolutionHintingSampleSat \
 rdotnet_SolveAndPrintIntermediateSolutionsSampleSat \
 rdotnet_SolveWithTimeLimitSampleSat \
 rdotnet_StepFunctionSampleSat \
 rdotnet_StopAfterNSolutionsSampleSat

.PHONY: check_dotnet
check_dotnet: \
 test_dotnet_algorithms_samples \
 test_dotnet_graph_samples \
 test_dotnet_constraint_solver_samples \
 test_dotnet_linear_solver_samples \
 test_dotnet_sat_samples

.PHONY: test_dotnet_tests # Build and Run all .Net Tests (located in examples/test and ortools/<cmp>/csharp)
test_dotnet_tests: \
 rdotnet_InitTests \
 rdotnet_KnapsackSolverTests \
 rdotnet_FlowTests \
 rdotnet_LinearSolverTests \
 rdotnet_ConstraintSolverTests \
 rdotnet_RoutingSolverTests \
 rdotnet_SatSolverTests \
 rdotnet_issue18 \
 rdotnet_issue22 \
 rdotnet_issue33

.PHONY: test_dotnet_contrib # Build and Run all .Net Examples (located in examples/contrib)
test_dotnet_contrib: \
 rdotnet_3_jugs_regular \
 rdotnet_a_puzzle \
 rdotnet_a_round_of_golf \
 rdotnet_all_interval \
 rdotnet_alldifferent_except_0 \
 rdotnet_assignment \
 rdotnet_broken_weights \
 rdotnet_bus_schedule \
 rdotnet_circuit \
 rdotnet_circuit2 \
 rdotnet_coins3 \
 rdotnet_combinatorial_auction2 \
 rdotnet_contiguity_regular \
 rdotnet_contiguity_transition \
 rdotnet_costas_array \
 rdotnet_covering_opl \
 rdotnet_crew \
 rdotnet_crossword \
 rdotnet_crypta \
 rdotnet_crypto \
 rdotnet_csdiet \
 rdotnet_curious_set_of_integers \
 rdotnet_debruijn \
 rdotnet_discrete_tomography \
 rdotnet_divisible_by_9_through_1 \
 rdotnet_dudeney \
 rdotnet_einav_puzzle2 \
 rdotnet_eq10 \
 rdotnet_eq20 \
 rdotnet_fill_a_pix \
 rdotnet_furniture_moving \
 rdotnet_futoshiki \
 rdotnet_golomb_ruler \
 rdotnet_grocery \
 rdotnet_hidato_table \
 rdotnet_just_forgotten \
 rdotnet_kakuro \
 rdotnet_kenken2 \
 rdotnet_killer_sudoku \
 rdotnet_labeled_dice \
 rdotnet_langford \
 rdotnet_least_diff \
 rdotnet_lectures \
 rdotnet_magic_sequence \
 rdotnet_magic_square \
 rdotnet_magic_square_and_cards \
 rdotnet_map \
 rdotnet_map2 \
 rdotnet_marathon2 \
 rdotnet_max_flow_taha \
 rdotnet_max_flow_winston1 \
 rdotnet_minesweeper \
 rdotnet_mr_smith \
 rdotnet_nqueens \
 rdotnet_nurse_rostering_regular \
 rdotnet_nurse_rostering_transition \
 rdotnet_olympic \
 rdotnet_organize_day \
 rdotnet_p_median \
 rdotnet_pandigital_numbers \
 rdotnet_perfect_square_sequence \
 rdotnet_photo_problem \
 rdotnet_place_number_puzzle \
 rdotnet_post_office_problem2 \
 rdotnet_quasigroup_completion \
 rdotnet_regex \
 rdotnet_rogo2 \
 rdotnet_scheduling_speakers \
 rdotnet_secret_santa2 \
 rdotnet_send_more_money \
 rdotnet_send_more_money2 \
 rdotnet_send_most_money \
 rdotnet_seseman \
 rdotnet_set_covering \
 rdotnet_set_covering2 \
 rdotnet_set_covering3 \
 rdotnet_set_covering4 \
 rdotnet_set_covering_deployment \
 rdotnet_set_covering_skiena \
 rdotnet_set_partition \
 rdotnet_sicherman_dice \
 rdotnet_ski_assignment \
 rdotnet_stable_marriage \
 rdotnet_strimko2 \
 rdotnet_subset_sum \
 rdotnet_sudoku \
 rdotnet_survo_puzzle \
 rdotnet_to_num \
 rdotnet_traffic_lights \
 rdotnet_volsay \
 rdotnet_volsay2 \
 rdotnet_volsay3 \
 rdotnet_wedding_optimal_chart \
 rdotnet_who_killed_agatha \
 rdotnet_xkcd \
 rdotnet_young_tableaux \
 rdotnet_zebra
#	rdotnet_coins_grid ARGS="5 2" \
#	rdotnet_nontransitive_dice \ # too long
#	rdotnet_partition \ # too long
#	rdotnet_secret_santa \ # too long
#	rdotnet_word_square \ # depends on /usr/share/dict/words

.PHONY: test_dotnet_dotnet # Build and Run all .Net Examples (located in examples/dotnet)
test_dotnet_dotnet: \
 rdotnet_BalanceGroupSat \
 rdotnet_cscvrptw \
 rdotnet_csflow \
 rdotnet_csintegerprogramming \
 rdotnet_csknapsack \
 rdotnet_cslinearprogramming \
 rdotnet_csls_api \
 rdotnet_csrabbitspheasants \
 rdotnet_cstsp \
 rdotnet_furniture_moving_intervals \
 rdotnet_organize_day_intervals \
 rdotnet_techtalk_scheduling \
 rdotnet_GateSchedulingSat \
 rdotnet_JobshopFt06Sat \
 rdotnet_JobshopSat \
 rdotnet_NursesSat \
 rdotnet_ShiftSchedulingSat \
 rdotnet_SpeakerSchedulingSat \
 rdotnet_TaskSchedulingSat
#	rdotnet_NetworkRoutingSat \
 ARGS="--clients=10 --backbones=5 --demands=10 --trafficMin=5 --trafficMax=10 --minClientDegree=2 --maxClientDegree=5 --minBackboneDegree=3 --maxBackboneDegree=5 --maxCapacity=20 --fixedChargeCost=10" \

.PHONY: test_dotnet
test_dotnet: \
 check_dotnet \
 test_dotnet_tests \
 test_dotnet_contrib \
 test_dotnet_dotnet

###############
##  Archive  ##
###############
archive_dotnet: $(INSTALL_DOTNET_NAME)$(ARCHIVE_EXT)

$(INSTALL_DOTNET_NAME):
	$(MKDIR) $(INSTALL_DOTNET_NAME)

$(INSTALL_DOTNET_NAME)/examples: | $(INSTALL_DOTNET_NAME)
	$(MKDIR) $(INSTALL_DOTNET_NAME)$Sexamples

define dotnet-sample-archive =
$(INSTALL_DOTNET_NAME)/examples/%/project.csproj: \
 $(TEMP_DOTNET_DIR)/$1/%/%.csproj \
 $(SRC_DIR)/ortools/$1/samples/%.cs \
 | $(INSTALL_DOTNET_NAME)/examples
	-$(MKDIR_P) $(INSTALL_DOTNET_NAME)$Sexamples$S$$*
	$(COPY) $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj $(INSTALL_DOTNET_NAME)$Sexamples$S$$*$Sproject.csproj
	$(SED) -i -e "s;/../$(BUILD_DIR)/dotnet/packages;;" $(INSTALL_DOTNET_NAME)$Sexamples$S$$*$Sproject.csproj
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.cs $(INSTALL_DOTNET_NAME)$Sexamples$S$$*
endef

$(foreach sample,$(DOTNET_SAMPLES),$(eval $(call dotnet-sample-archive,$(sample))))

define dotnet-example-archive =
$(INSTALL_DOTNET_NAME)/examples/%/project.csproj: \
 $(TEMP_DOTNET_DIR)/$1/%/%.csproj \
 $(SRC_DIR)/examples/$1/%.cs \
 | $(INSTALL_DOTNET_NAME)/examples
	-$(MKDIR_P) $(INSTALL_DOTNET_NAME)$Sexamples$S$$*
	$(COPY) $(TEMP_DOTNET_DIR)$S$1$S$$*$S$$*.csproj $(INSTALL_DOTNET_NAME)$Sexamples$S$$*$Sproject.csproj
	$(SED) -i -e "s;/../$(BUILD_DIR)/dotnet/packages;;" $(INSTALL_DOTNET_NAME)$Sexamples$S$$*$Sproject.csproj
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.cs $(INSTALL_DOTNET_NAME)$Sexamples$S$$*
endef

$(foreach example,$(DOTNET_EXAMPLES),$(eval $(call dotnet-example-archive,$(example))))

SAMPLE_DOTNET_FILES = \
  $(addsuffix /project.csproj,$(addprefix $(INSTALL_DOTNET_NAME)/examples/,$(basename $(notdir $(wildcard ortools/*/samples/*.cs)))))

EXAMPLE_DOTNET_FILES = \
  $(addsuffix /project.csproj,$(addprefix $(INSTALL_DOTNET_NAME)/examples/,$(basename $(notdir $(wildcard examples/contrib/*.cs))))) \
  $(addsuffix /project.csproj,$(addprefix $(INSTALL_DOTNET_NAME)/examples/,$(basename $(notdir $(wildcard examples/dotnet/*.cs)))))

$(INSTALL_DOTNET_NAME)$(ARCHIVE_EXT): dotnet \
 $(SAMPLE_DOTNET_FILES) \
 $(EXAMPLE_DOTNET_FILES) \
 LICENSE tools/README.dotnet.md tools/Makefile.dotnet.mk
	$(COPY) $(BUILD_DIR)$Sdotnet$Spackages$S*nupkg $(INSTALL_DOTNET_NAME)$S
	$(COPY) LICENSE $(INSTALL_DOTNET_NAME)$SLICENSE
	$(COPY) tools$SREADME.dotnet.md $(INSTALL_DOTNET_NAME)$SREADME.md
	$(COPY) tools$SMakefile.dotnet.mk $(INSTALL_DOTNET_NAME)$SMakefile
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' $(INSTALL_DOTNET_NAME)$SMakefile
ifeq ($(PLATFORM),WIN64)
	$(MKDIR) $(INSTALL_DOTNET_NAME)$Sbin
	$(COPY) $(WHICH) $(INSTALL_DOTNET_NAME)$Sbin$S
	$(ZIP) -r $(INSTALL_DOTNET_NAME)$(ARCHIVE_EXT) $(INSTALL_DOTNET_NAME)
else
	$(TAR) --no-same-owner -czvf $(INSTALL_DOTNET_NAME)$(ARCHIVE_EXT) $(INSTALL_DOTNET_NAME)
endif

# Test archive
TEMP_DOTNET_TEST_DIR := temp_dotnet_test
.PHONY: test_archive_dotnet # Test C++ OR-Tools archive is OK.
test_archive_dotnet: $(INSTALL_DOTNET_NAME)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_DOTNET_TEST_DIR)
	-$(MKDIR) $(TEMP_DOTNET_TEST_DIR)
ifeq ($(PLATFORM),WIN64)
	$(UNZIP) $< -d $(TEMP_DOTNET_TEST_DIR)
else
	$(TAR) -xvf $< -C $(TEMP_DOTNET_TEST_DIR)
endif
	cd $(TEMP_DOTNET_TEST_DIR)$S$(INSTALL_DOTNET_NAME) \
 && $(MAKE) MAKEFLAGS= \
 && $(MAKE) MAKEFLAGS= test

#######################
##  EXAMPLE ARCHIVE  ##
#######################
$(TEMP_DOTNET_DIR)/ortools_examples: | $(TEMP_DOTNET_DIR)
	$(MKDIR) $(TEMP_DOTNET_DIR)$Sortools_examples

$(TEMP_DOTNET_DIR)/ortools_examples/examples: | $(TEMP_DOTNET_DIR)/ortools_examples
	$(MKDIR) $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples

$(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet: | $(TEMP_DOTNET_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet

define dotnet-sample-archive =
$(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet/%.csproj: \
 ortools/$1/samples/%.cs \
 | $(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.cs \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet
	$(COPY) ortools$Sdotnet$SSample.csproj.in \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_PACKAGES_DIR@/./' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_SAMPLE_LANG@/9.0/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFrameworks>netcoreapp3.1;net8.0<\/TargetFrameworks>/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_PROJECT@/$(DOTNET_ORTOOLS_ASSEMBLY_NAME)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@COMPONENT_NAME@/$1/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@SAMPLE_NAME@/$$*/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@SAMPLE_FILE_NAME@/$$*.cs/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
endef

DOTNET_SAMPLES := algorithms graph constraint_solver linear_solver sat
$(foreach sample,$(DOTNET_SAMPLES),$(eval $(call dotnet-sample-archive,$(sample))))

define dotnet-example-archive =
$(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet/%.csproj: \
 examples/$1/%.cs \
 | $(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.cs \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet
	$(COPY) ortools$Sdotnet$SExample.csproj.in \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_PACKAGES_DIR@/./' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_SAMPLE_LANG@/9.0/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_TFM@/<TargetFrameworks>netcoreapp3.1;net8.0<\/TargetFrameworks>/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@DOTNET_PROJECT@/$(DOTNET_ORTOOLS_ASSEMBLY_NAME)/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@COMPONENT_NAME@/$1/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@EXAMPLE_NAME@/$$*/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
	$(SED) -i -e 's/@EXAMPLE_FILE_NAME@/$$*.cs/' \
 $(TEMP_DOTNET_DIR)$Sortools_examples$Sexamples$Sdotnet$S$$*.csproj
endef

DOTNET_EXAMPLES := contrib dotnet
$(foreach example,$(DOTNET_EXAMPLES),$(eval $(call dotnet-example-archive,$(example))))

SAMPLE_DOTNET_FILES = \
  $(addsuffix proj,$(addprefix $(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet/,$(notdir $(wildcard ortools/*/samples/*.cs))))

EXAMPLE_DOTNET_FILES = \
  $(addsuffix proj,$(addprefix $(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet/,$(notdir $(wildcard examples/contrib/*.cs)))) \
  $(addsuffix proj,$(addprefix $(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet/,$(notdir $(wildcard examples/dotnet/*.cs))))

.PHONY: dotnet_examples_archive # Build stand-alone C++ examples archive file for redistribution.
dotnet_examples_archive: \
 $(SAMPLE_DOTNET_FILES) \
 $(EXAMPLE_DOTNET_FILES) \
	| $(TEMP_DOTNET_DIR)/ortools_examples/examples/dotnet
	-$(COPY) tools$SREADME.dotnet.md $(TEMP_DOTNET_DIR)$Sortools_examples$SREADME.md
	$(COPY) LICENSE $(TEMP_DOTNET_DIR)$Sortools_examples
ifeq ($(PLATFORM),WIN64)
	cd $(TEMP_DOTNET_DIR) \
 && ..\$(ZIP) \
 -r ..\or-tools_dotnet_examples_v$(OR_TOOLS_VERSION).zip \
 ortools_examples
else
	cd $(TEMP_DOTNET_DIR) \
 && tar -c -v -z --no-same-owner \
 -f ../or-tools_dotnet_examples_v$(OR_TOOLS_VERSION).tar.gz \
 ortools_examples
endif
	-$(DELREC) $(TEMP_DOTNET_DIR)$Sortools_examples

######################
##  Nuget artifact  ##
######################
.PHONY: nuget_archive # Build .Net "Google.OrTools" Nuget Package
nuget_archive: dotnet | $(TEMP_DOTNET_DIR)
	"$(DOTNET_BIN)" publish $(DOTNET_BUILD_ARGS) --no-build --no-dependencies --no-restore -f net8.0 \
 -o "..$S..$S..$S$(TEMP_DOTNET_DIR)" \
 ortools$Sdotnet$S$(DOTNET_ORTOOLS_ASSEMBLY_NAME)$S$(DOTNET_ORTOOLS_ASSEMBLY_NAME).csproj
	"$(DOTNET_BIN)" pack -c Release $(NUGET_PACK_ARGS) --no-build \
 -o "..$S..$S.." \
 ortools$Sdotnet

.PHONY: nuget_upload # Upload Nuget Package
nuget_upload: nuget_archive
	@echo Uploading Nuget package for "netcoreapp3.1;net8.0".
	$(warning Not Implemented)

endif  # HAS_DOTNET=ON

################
##  Cleaning  ##
################
clean_dotnet:
#	-$(DEL) $(DOTNET_ORTOOLS_SNK_PATH)
	-$(DELREC) $(TEMP_DOTNET_DIR)
	-$(DELREC) $(INSTALL_DOTNET_NAME)
	-$(DELREC) or-tools_dotnet_*
	-$(DELREC) $(TEMP_DOTNET_TEST_DIR)
	-$(DEL) *.nupkg
	-$(DEL) *.snupkg
	-@"$(DOTNET_BIN)" nuget locals all --clear

#############
##  DEBUG  ##
#############
detect_dotnet:
	@echo Relevant info for the dotnet build:
	@echo BUILD_DOTNET = $(BUILD_DOTNET)
	@echo HAS_DOTNET = $(HAS_DOTNET)
	@echo DOTNET_BIN = $(DOTNET_BIN)
	@echo PROTOC = $(PROTOC)
	@echo DOTNET_SNK = $(DOTNET_SNK)
	@echo DOTNET_ORTOOLS_ASSEMBLY_NAME = $(DOTNET_ORTOOLS_ASSEMBLY_NAME)
	@echo INSTALL_DOTNET_NAME = $(INSTALL_DOTNET_NAME)
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif

