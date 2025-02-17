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

# ---------- Java support ----------
.PHONY: help_java # Generate list of Java targets with descriptions.
help_java:
	@echo Use one of the following Java targets:
ifeq ($(PLATFORM),WIN64)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.java.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.java.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t24
	@echo
endif

# Main targets.
.PHONY: detect_java # Show variables used to build Java OR-Tools.
.PHONY: java_runtime # Build Java OR-Tools runtime.
.PHONY: java # Build Java OR-Tools.
.PHONY: check_java # Quick check only running Java OR-Tools samples.
.PHONY: test_java # Test Java OR-Tools using various examples.
.PHONY: archive_java # Generate OR-Tools's maven archive.
.PHONY: clean_java # Clean Java output from previous build.

ifeq ($(HAS_JAVA),OFF)
java_runtime: java
java:
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo MVN_BIN = $(MVN_BIN)
	$(warning Either JAVA support was turned off, or the makefile cannot\
 find 'java' or 'maven' command which is needed for build. \
 Please make sure it is installed and in system path. \
 Or turn java support ON.)
check_java: java
test_java: java
package_java: java

else  # HAS_JAVA=ON

JAVA_BUILD_PATH := $(BUILD_DIR)$Sjava
TEMP_JAVA_DIR := temp_java
JAVA_ORTOOLS_PACKAGE := com.google.ortools

# OR Tools unique library.
java:
	$(MAKE) third_party BUILD_JAVA=ON
	cmake --build $(BUILD_DIR) --target install --config $(BUILD_TYPE) -j $(JOBS) -v

# Detect RuntimeIDentifier
ifeq ($(OS),Windows)
  JAVA_NATIVE_IDENTIFIER := win32-x86-64
else
  ifeq ($(OS),Linux)
    ifeq ($(CPU),aarch64)
      JAVA_NATIVE_IDENTIFIER := linux-aarch64
    else
      JAVA_NATIVE_IDENTIFIER := linux-x86-64
    endif
  else
    ifeq ($(OS),Darwin)
      ifeq ($(CPU),arm64)
        JAVA_NATIVE_IDENTIFIER := darwin-aarch64
      else
        JAVA_NATIVE_IDENTIFIER := darwin-x86-64
      endif
    else
    $(error OS unknown !)
    endif
  endif
endif

# All libraries and dependencies
JAVA_ORTOOLS_NATIVE_PROJECT := ortools-$(JAVA_NATIVE_IDENTIFIER)
JAVA_ORTOOLS_PROJECT := ortools-java
JAVA_NATIVE_PATH := $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Ssrc$Smain
JAVA_PATH := $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Ssrc$Smain

$(TEMP_JAVA_DIR):
	$(MKDIR) $(TEMP_JAVA_DIR)

.PHONY: package_java
package_java: java
	-$(DEL) *.jar
	$(COPY) $(JAVA_BUILD_PATH)$Sortools-java$Starget$S*.jar .
	$(COPY) $(JAVA_BUILD_PATH)$Sortools-$(JAVA_NATIVE_IDENTIFIER)$Starget$S*.jar .

###################
##  Java SOURCE  ##
###################
ifeq ($(SOURCE_SUFFIX),.java) # Those rules will be used if SOURCE contain a .java file

SOURCE_PROJECT_DIR := $(SOURCE)
SOURCE_PROJECT_DIR := $(subst /$(SOURCE_NAME).java,, $(SOURCE_PROJECT_DIR))
SOURCE_PROJECT_PATH = $(subst /,$S,$(SOURCE_PROJECT_DIR))

.PHONY: build # Build a Java program.
build: $(SOURCE) #$(SOURCE_PROJECT_DIR)/pom.xml  java
	$(warning This is not supported.\
 Prefer to place your java file in a samples/ or examples/ java directory\
 and call "make rjava_$(SOURCE_NAME)" target instead.)
#	cd $(SOURCE_PROJECT_PATH) && "$(MVN_BIN)" compile -b $(ARGS)
#	cd $(SOURCE_PROJECT_PATH) && "$(MVN_BIN)" pack -c Release

.PHONY: run # Run a Java program.
run: build
endif

###################################
##  Java Samples/Examples/Tests  ##
###################################
JAVA_SRC_DIR := src/main/java/com/google/ortools
JAVA_SRC_PATH := $(subst /,$S,$(JAVA_SRC_DIR))

JAVA_TEST_DIR := src/test/java/com/google/ortools
JAVA_TEST_PATH := $(subst /,$S,$(JAVA_TEST_DIR))

# Samples
define java-sample-target =
$(TEMP_JAVA_DIR)/$1: | $(TEMP_JAVA_DIR)
	-$(MKDIR) $(TEMP_JAVA_DIR)$S$1

$(TEMP_JAVA_DIR)/$1/%: \
 $(SRC_DIR)/ortools/$1/samples/%.java \
 | $(TEMP_JAVA_DIR)/$1
	-$(MKDIR) $(TEMP_JAVA_DIR)$S$1$S$$*

$(TEMP_JAVA_DIR)/$1/%/pom.xml: \
 $(SRC_DIR)/ortools/$1/samples/%.java \
 ${SRC_DIR}/ortools/java/pom-sample.xml.in \
 | $(TEMP_JAVA_DIR)/$1/%
	$(SED) -e "s/@JAVA_PACKAGE@/$(JAVA_ORTOOLS_PACKAGE)/" \
 ortools$Sjava$Spom-sample.xml.in \
 > $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_SAMPLE_PROJECT@/$$*/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_MAIN_CLASS@/com.google.ortools.$2.samples.$$*/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_PROJECT@/$(JAVA_ORTOOLS_PROJECT)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml

$(TEMP_JAVA_DIR)/$1/%/$(JAVA_SRC_DIR)/%.java: \
 $(SRC_DIR)/ortools/$1/samples/%.java \
 | $(TEMP_JAVA_DIR)/$1/%
	$(MKDIR_P) $(TEMP_JAVA_DIR)$S$1$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.java \
 $(TEMP_JAVA_DIR)$S$1$S$$*$S$(JAVA_SRC_PATH)

rjava_%: \
 java \
 $(SRC_DIR)/ortools/$1/samples/%.java \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $(TEMP_JAVA_DIR)/$1/%/$(JAVA_SRC_DIR)/%.java \
 FORCE
	cd $(TEMP_JAVA_DIR)$S$1$S$$* && "$(MVN_BIN)" compile -B
	cd $(TEMP_JAVA_DIR)$S$1$S$$* && "$(MVN_BIN)" exec:java $(ARGS)
endef

JAVA_SAMPLES := init algorithms graph constraint_solver linear_solver sat util
$(foreach sample,$(JAVA_SAMPLES),$(eval $(call java-sample-target,$(sample),$(subst _,,$(sample)))))

# Examples
define java-example-target =
$(TEMP_JAVA_DIR)/$1: | $(TEMP_JAVA_DIR)
	-$(MKDIR) $(TEMP_JAVA_DIR)$S$1

$(TEMP_JAVA_DIR)/$1/%: \
 $(SRC_DIR)/examples/$1/%.java \
 | $(TEMP_JAVA_DIR)/$1
	-$(MKDIR) $(TEMP_JAVA_DIR)$S$1$S$$*

$(TEMP_JAVA_DIR)/$1/%/pom.xml: \
 $(SRC_DIR)/examples/$1/%.java \
 ${SRC_DIR}/ortools/java/pom-sample.xml.in \
 | $(TEMP_JAVA_DIR)/$1/%
	$(SED) -e "s/@JAVA_PACKAGE@/$(JAVA_ORTOOLS_PACKAGE)/" \
 ortools$Sjava$Spom-sample.xml.in \
 > $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_SAMPLE_PROJECT@/$$*/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_MAIN_CLASS@/com.google.ortools.$1.$$*/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_PROJECT@/$(JAVA_ORTOOLS_PROJECT)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml

$(TEMP_JAVA_DIR)/$1/%/$(JAVA_SRC_DIR)/%.java: \
 $(SRC_DIR)/examples/$1/%.java \
 | $(TEMP_JAVA_DIR)/$1/%
	$(MKDIR_P) $(TEMP_JAVA_DIR)$S$1$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.java \
 $(TEMP_JAVA_DIR)$S$1$S$$*$S$(JAVA_SRC_PATH)

rjava_%: \
 java \
 $(SRC_DIR)/examples/$1/%.java \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $(TEMP_JAVA_DIR)/$1/%/$(JAVA_SRC_DIR)/%.java \
 FORCE
	cd $(TEMP_JAVA_DIR)$S$1$S$$* && "$(MVN_BIN)" compile -B
	cd $(TEMP_JAVA_DIR)$S$1$S$$* && "$(MVN_BIN)" exec:java $(ARGS)
endef

JAVA_EXAMPLES := contrib java
$(foreach example,$(JAVA_EXAMPLES),$(eval $(call java-example-target,$(example))))

# Tests
define java-test-target =
$(TEMP_JAVA_DIR)/$1/%: \
 $(SRC_DIR)/ortools/$1/java/%.java \
 | $(TEMP_JAVA_DIR)/$1
	-$(MKDIR) $(TEMP_JAVA_DIR)$S$1$S$$*

$(TEMP_JAVA_DIR)/$1/%/pom.xml: \
 $(SRC_DIR)/ortools/$1/java/%.java \
 ${SRC_DIR}/ortools/java/pom-test.xml.in \
 | $(TEMP_JAVA_DIR)/$1/%
	$(SED) -e "s/@JAVA_PACKAGE@/$(JAVA_ORTOOLS_PACKAGE)/" \
 ortools$Sjava$Spom-test.xml.in \
 > $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_TEST_PROJECT@/$$*/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(OR_TOOLS_PATCH)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml
	$(SED) -i -e 's/@JAVA_PROJECT@/$(JAVA_ORTOOLS_PROJECT)/' \
 $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml

$(TEMP_JAVA_DIR)/$1/%/$(JAVA_TEST_DIR)/%.java: \
 $(SRC_DIR)/ortools/$1/java/%.java \
 | $(TEMP_JAVA_DIR)/$1/%
	$(MKDIR_P) $(TEMP_JAVA_DIR)$S$1$S$$*$S$(JAVA_TEST_PATH)
	$(COPY) $(SRC_DIR)$Sortools$S$1$Sjava$S$$*.java \
 $(TEMP_JAVA_DIR)$S$1$S$$*$S$(JAVA_TEST_PATH)

rjava_%: \
 java \
 $(SRC_DIR)/ortools/$1/java/%.java \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $(TEMP_JAVA_DIR)/$1/%/$(JAVA_TEST_DIR)/%.java \
 FORCE
	cd $(TEMP_JAVA_DIR)$S$1$S$$* && "$(MVN_BIN)" compile -B
	cd $(TEMP_JAVA_DIR)$S$1$S$$* && "$(MVN_BIN)" test $(ARGS)
endef

JAVA_TESTS := init algorithms graph constraint_solver linear_solver sat util
$(foreach test,$(JAVA_TESTS),$(eval $(call java-test-target,$(test))))

####################
##  Test targets  ##
####################

.PHONY: test_java_algorithms_samples # Build and Run all Java Algorithms Samples (located in ortools/algorithms/samples)
test_java_algorithms_samples: \
 rjava_Knapsack

.PHONY: test_java_constraint_solver_samples # Build and Run all Java CP Samples (located in ortools/constraint_solver/samples)
test_java_constraint_solver_samples: \
 rjava_SimpleCpProgram \
 rjava_SimpleRoutingProgram \
 rjava_Tsp \
 rjava_TspCities \
 rjava_TspCircuitBoard \
 rjava_TspDistanceMatrix \
 rjava_Vrp \
 rjava_VrpBreaks \
 rjava_VrpCapacity \
 rjava_VrpDropNodes \
 rjava_VrpGlobalSpan \
 rjava_VrpInitialRoutes \
 rjava_VrpPickupDelivery \
 rjava_VrpPickupDeliveryFifo \
 rjava_VrpPickupDeliveryLifo \
 rjava_VrpResources \
 rjava_VrpSolutionCallback \
 rjava_VrpStartsEnds \
 rjava_VrpTimeWindows \
 rjava_VrpWithTimeLimit

.PHONY: test_java_graph_samples # Build and Run all Java Graph Samples (located in ortools/graph/samples)
test_java_graph_samples: \
 rjava_AssignmentLinearSumAssignment \
 rjava_AssignmentMinFlow \
 rjava_BalanceMinFlow \
 rjava_SimpleMaxFlowProgram \
 rjava_SimpleMinCostFlowProgram

.PHONY: test_java_linear_solver_samples # Build and Run all Java LP Samples (located in ortools/linear_solver/samples)
test_java_linear_solver_samples: \
 rjava_AssignmentMip \
 rjava_BasicExample \
 rjava_BinPackingMip \
 rjava_LinearProgrammingExample \
 rjava_MipVarArray \
 rjava_MultipleKnapsackMip \
 rjava_SimpleLpProgram \
 rjava_SimpleMipProgram \
 rjava_StiglerDiet

.PHONY: test_java_model_builder_samples # Build and Run all Java MB Samples (located in ortools/model_builder/samples)
test_java_model_builder_samples: \
 rjava_SimpleLpProgramMb \
 rjava_SimpleMipProgramMb

.PHONY: test_java_sat_samples # Build and Run all Java SAT Samples (located in ortools/sat/samples)
test_java_sat_samples: \
 rjava_AssignmentGroupsSat \
 rjava_AssignmentSat \
 rjava_AssignmentTaskSizesSat \
 rjava_AssignmentTeamsSat \
 rjava_AssumptionsSampleSat \
 rjava_BinPackingProblemSat \
 rjava_BoolOrSampleSat \
 rjava_ChannelingSampleSat \
 rjava_CpIsFunSat \
 rjava_CpSatExample \
 rjava_EarlinessTardinessCostSampleSat \
 rjava_IntervalSampleSat \
 rjava_LiteralSampleSat \
 rjava_MinimalJobshopSat \
 rjava_MultipleKnapsackSat \
 rjava_NQueensSat \
 rjava_NoOverlapSampleSat \
 rjava_NonLinearSat \
 rjava_NursesSat \
 rjava_OptionalIntervalSampleSat \
 rjava_RabbitsAndPheasantsSat \
 rjava_RankingSampleSat \
 rjava_ReifiedSampleSat \
 rjava_ScheduleRequestsSat \
 rjava_SearchForAllSolutionsSampleSat \
 rjava_SimpleSatProgram \
 rjava_SolutionHintingSampleSat \
 rjava_SolveAndPrintIntermediateSolutionsSampleSat \
 rjava_SolveWithTimeLimitSampleSat \
 rjava_StepFunctionSampleSat \
 rjava_StopAfterNSolutionsSampleSat

.PHONY: check_java
check_java: \
 test_java_algorithms_samples \
 test_java_constraint_solver_samples \
 test_java_graph_samples \
 test_java_linear_solver_samples \
 test_java_model_builder_samples \
 test_java_sat_samples \
 rjava_LinearProgramming \
 rjava_IntegerProgramming

.PHONY: test_java_tests # Build and Run all Java Tests (located in examples/tests)
test_java_tests: \
 rjava_InitTest \
 rjava_KnapsackSolverTest \
 rjava_FlowTest \
 rjava_LinearSolverTest \
 rjava_ModelBuilderTest \
 rjava_ConstraintSolverTest \
 rjava_RoutingSolverTest \
 rjava_CpModelTest \
 rjava_CpSolverTest \
 rjava_LinearExprTest

.PHONY: test_java_contrib # Build and Run all Java Contrib (located in examples/contrib)
test_java_contrib: \
 rjava_AllDifferentExcept0 \
 rjava_AllInterval \
 rjava_Circuit \
 rjava_CoinsGridMIP \
 rjava_ColoringMIP \
 rjava_CoveringOpl \
 rjava_Crossword \
 rjava_DeBruijn \
 rjava_Diet \
 rjava_DietMIP \
 rjava_DivisibleBy9Through1 \
 rjava_GolombRuler \
 rjava_KnapsackMIP \
 rjava_LeastDiff \
 rjava_MagicSquare \
 rjava_Map2 \
 rjava_Map \
 rjava_Minesweeper \
 rjava_MultiThreadTest \
 rjava_NQueens2 \
 rjava_NQueens \
 rjava_Partition \
 rjava_QuasigroupCompletion \
 rjava_SendMoreMoney2 \
 rjava_SendMoreMoney \
 rjava_SendMostMoney \
 rjava_Seseman \
 rjava_SetCovering2 \
 rjava_SetCovering3 \
 rjava_SetCovering4 \
 rjava_SetCoveringDeployment \
 rjava_SetCovering \
 rjava_SimpleRoutingTest \
 rjava_StableMarriage \
 rjava_StiglerMIP \
 rjava_Strimko2 \
 rjava_Sudoku \
 rjava_SurvoPuzzle \
 rjava_ToNum \
 rjava_WhoKilledAgatha \
 rjava_Xkcd \
 rjava_YoungTableaux

.PHONY: test_java_java # Build and Run all Java Examples (located in examples/java)
test_java_java: \
 rjava_CapacitatedVehicleRoutingProblemWithTimeWindows \
 rjava_FlowExample \
 rjava_IntegerProgramming \
 rjava_LinearAssignmentAPI \
 rjava_LinearProgramming \
 rjava_RabbitsPheasants \
 rjava_RandomTsp

.PHONY: test_java
test_java: \
 check_java \
 test_java_tests \
 test_java_contrib \
 test_java_java

###############
##  Archive  ##
###############
archive_java: $(INSTALL_JAVA_NAME)$(ARCHIVE_EXT)

$(INSTALL_JAVA_NAME):
	$(MKDIR) $(INSTALL_JAVA_NAME)

$(INSTALL_JAVA_NAME)/examples: | $(INSTALL_JAVA_NAME)
	$(MKDIR) $(INSTALL_JAVA_NAME)$Sexamples

define java-sample-archive =
$(INSTALL_JAVA_NAME)/examples/%/pom.xml: \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $(SRC_DIR)/ortools/$1/samples/%.java \
 | $(INSTALL_JAVA_NAME)/examples
	-$(MKDIR_P) $(INSTALL_JAVA_NAME)$Sexamples$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml $(INSTALL_JAVA_NAME)$Sexamples$S$$*
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.java $(INSTALL_JAVA_NAME)$Sexamples$S$$*$S$(JAVA_SRC_PATH)
endef

$(foreach sample,$(JAVA_SAMPLES),$(eval $(call java-sample-archive,$(sample))))

define java-example-archive =
$(INSTALL_JAVA_NAME)/examples/%/pom.xml: \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $(SRC_DIR)/examples/$1/%.java \
 | $(INSTALL_JAVA_NAME)/examples
	-$(MKDIR_P) $(INSTALL_JAVA_NAME)$Sexamples$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml $(INSTALL_JAVA_NAME)$Sexamples$S$$*
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.java $(INSTALL_JAVA_NAME)$Sexamples$S$$*$S$(JAVA_SRC_PATH)
endef

$(foreach example,$(JAVA_EXAMPLES),$(eval $(call java-example-archive,$(example))))

SAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(INSTALL_JAVA_NAME)/examples/,$(basename $(notdir $(wildcard ortools/*/samples/*.java)))))

EXAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(INSTALL_JAVA_NAME)/examples/,$(basename $(notdir $(wildcard examples/contrib/*.java))))) \
  $(addsuffix /pom.xml,$(addprefix $(INSTALL_JAVA_NAME)/examples/,$(basename $(notdir $(wildcard examples/java/*.java)))))

$(INSTALL_JAVA_NAME)$(ARCHIVE_EXT): java \
 $(SAMPLE_JAVA_FILES) \
 $(EXAMPLE_JAVA_FILES) \
 LICENSE tools/README.java.md tools/Makefile.java.mk
	$(COPY) $(JAVA_BUILD_PATH)$Sortools-java$Starget$S*.jar $(INSTALL_JAVA_NAME)$S
	$(COPY) $(JAVA_BUILD_PATH)$Sortools-$(JAVA_NATIVE_IDENTIFIER)$Starget$S*.jar $(INSTALL_JAVA_NAME)$S
	$(COPY) LICENSE $(INSTALL_JAVA_NAME)$SLICENSE
	$(COPY) tools$SREADME.java.md $(INSTALL_JAVA_NAME)$SREADME.md
	$(COPY) tools$SMakefile.java.mk $(INSTALL_JAVA_NAME)$SMakefile
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' $(INSTALL_JAVA_NAME)$SMakefile
ifeq ($(PLATFORM),WIN64)
	$(MKDIR) $(INSTALL_JAVA_NAME)$Sbin
	$(COPY) $(WHICH) $(INSTALL_JAVA_NAME)$Sbin$S
	$(COPY) $(TOUCH) $(INSTALL_JAVA_NAME)$Sbin$S
	$(ZIP) -r $(INSTALL_JAVA_NAME)$(ARCHIVE_EXT) $(INSTALL_JAVA_NAME)
else
	$(TAR) --no-same-owner -czvf $(INSTALL_JAVA_NAME)$(ARCHIVE_EXT) $(INSTALL_JAVA_NAME)
endif

# Test archive
TEMP_JAVA_TEST_DIR := temp_java_test
.PHONY: test_archive_java # Test C++ OR-Tools archive is OK.
test_archive_java: $(INSTALL_JAVA_NAME)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_JAVA_TEST_DIR)
	-$(MKDIR) $(TEMP_JAVA_TEST_DIR)
ifeq ($(PLATFORM),WIN64)
	$(UNZIP) $< -d $(TEMP_JAVA_TEST_DIR)
else
	$(TAR) -xvf $< -C $(TEMP_JAVA_TEST_DIR)
endif
	cd $(TEMP_JAVA_TEST_DIR)$S$(INSTALL_JAVA_NAME) \
 && $(MAKE) MAKEFLAGS= \
 && $(MAKE) MAKEFLAGS= test

########################
##  Publish Java Pkg  ##
########################
.PHONY: publish_java_runtime
publish_java_runtime: java_runtime
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT) && "$(MVN_BIN)" deploy

.PHONY: publish_java
publish_java: java
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT) && "$(MVN_BIN)" deploy

#######################
##  EXAMPLE ARCHIVE  ##
#######################
$(TEMP_JAVA_DIR)/ortools_examples: | $(TEMP_JAVA_DIR)
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples

$(TEMP_JAVA_DIR)/ortools_examples/examples: | $(TEMP_JAVA_DIR)/ortools_examples
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples

$(TEMP_JAVA_DIR)/ortools_examples/examples/java: | $(TEMP_JAVA_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava

define java-sample-archive =
$(TEMP_JAVA_DIR)/ortools_examples/examples/java/%/pom.xml: \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 ortools/$1/samples/%.java \
 | $(TEMP_JAVA_DIR)/ortools_examples/examples/java
	-$(MKDIR_P) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(SRC_DIR)$Sortools$S$1$Ssamples$S$$*.java \
 $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml \
 $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava$S$$*
endef

$(foreach sample,$(JAVA_SAMPLES),$(eval $(call java-sample-archive,$(sample))))

define java-example-archive =
$(TEMP_JAVA_DIR)/ortools_examples/examples/java/%/pom.xml: \
 $(TEMP_JAVA_DIR)/$1/%/pom.xml \
 examples/$1/%.java \
 | $(TEMP_JAVA_DIR)/ortools_examples/examples/java
	-$(MKDIR_P) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(SRC_DIR)$Sexamples$S$1$S$$*.java \
 $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava$S$$*$S$(JAVA_SRC_PATH)
	$(COPY) $(TEMP_JAVA_DIR)$S$1$S$$*$Spom.xml \
 $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava$S$$*
endef

$(foreach example,$(JAVA_EXAMPLES),$(eval $(call java-example-archive,$(example))))

SAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_JAVA_DIR)/ortools_examples/examples/java/,$(basename $(notdir $(wildcard ortools/*/samples/*.java)))))

EXAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_JAVA_DIR)/ortools_examples/examples/java/,$(basename $(notdir $(wildcard examples/contrib/*.java))))) \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_JAVA_DIR)/ortools_examples/examples/java/,$(basename $(notdir $(wildcard examples/java/*.java)))))

.PHONY: java_examples_archive # Build stand-alone Java examples archive file for redistribution.
java_examples_archive: \
 $(SAMPLE_JAVA_FILES) \
 $(EXAMPLE_JAVA_FILES) \
 | $(TEMP_JAVA_DIR)/ortools_examples/examples/java
	$(COPY) tools$SREADME.java.md $(TEMP_JAVA_DIR)$Sortools_examples$SREADME.md
	$(COPY) LICENSE $(TEMP_JAVA_DIR)$Sortools_examples
ifeq ($(PLATFORM),WIN64)
	cd $(TEMP_JAVA_DIR) \
 && ..\$(ZIP) \
 -r ..\or-tools_java_examples_v$(OR_TOOLS_VERSION).zip \
 ortools_examples
else
	cd $(TEMP_JAVA_DIR) \
 && tar -c -v -z --no-same-owner \
 -f ../or-tools_java_examples_v$(OR_TOOLS_VERSION).tar.gz \
 ortools_examples
endif
	-$(DELREC) $(TEMP_JAVA_DIR)$Sortools_examples

endif  # HAS_JAVA=ON

################
##  Cleaning  ##
################
clean_java:
	-$(DELREC) $(TEMP_JAVA_DIR)
	-$(DELREC) $(INSTALL_JAVA_NAME)
	-$(DELREC) or-tools_java_*
	-$(DELREC) $(TEMP_JAVA_TEST_DIR)
	-$(DEL) *.jar

#############
##  DEBUG  ##
#############
detect_java:
	@echo Relevant info for the Java build:
	@echo BUILD_JAVA = $(BUILD_JAVA)
	@echo HAS_JAVA = $(HAS_JAVA)
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo MVN_BIN = $(MVN_BIN)
	@echo JAVA_ORTOOLS_PACKAGE = $(JAVA_ORTOOLS_PACKAGE)
	@echo JAVA_ORTOOLS_NATIVE_PROJECT = $(JAVA_ORTOOLS_NATIVE_PROJECT)
	@echo JAVA_ORTOOLS_PROJECT = $(JAVA_ORTOOLS_PROJECT)
	@echo INSTALL_JAVA_NAME = $(INSTALL_JAVA_NAME)
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif
