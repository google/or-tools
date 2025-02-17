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

VERSION := @PROJECT_VERSION@
# Let's discover something about where we run
ifeq ($(OS),Windows_NT)
OS = Windows
endif
ifeq ($(OS),Windows)
SYSTEM = win
else
SYSTEM = unix
endif

.SECONDARY:

# Unix specific part.
ifeq ($(SYSTEM),unix)
  OS := $(shell uname -s)
  CPU = $(shell uname -m)
  S := /
  TOUCH := touch
  ifeq ($(OS),Linux)
  endif # ifeq($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac Os X
  endif # ifeq($(OS),Darwin)
ifneq ($(JAVA_HOME),)
  JAVA_BIN := $(shell command -v $(JAVA_HOME)/bin/java 2> /dev/null)
else
  JAVA_BIN := $(shell command -v java 2> /dev/null)
endif
  MVN_BIN := $(shell command -v mvn 2> /dev/null)
endif # SYSTEM == unix

# Windows specific part.
ifeq ($(SYSTEM),win)
  CPU = x64
  S := \\
  WHICH := bin$Swhich.exe
  TOUCH := bin$Stouch.exe
  JAVA_BIN := $(shell $(WHICH) java 2> NUL)
  MVN_BIN := $(shell $(WHICH) mvn.cmd 2> NUL)
endif # SYSTEM == win


.PHONY: all
all: detect

.PHONY: detect
detect:
	@echo VERSION = $(VERSION)
	@echo SYSTEM = $(SYSTEM)
	@echo OS = $(OS)
	@echo SHELL = $(SHELL)
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo MVN_BIN = $(MVN_BIN)
	@echo SOURCE = $(SOURCE)
	@echo SOURCE_PATH = $(SOURCE_PATH)
	@echo SOURCE_NAME = $(SOURCE_NAME)
	@echo SOURCE_SUFFIX = $(SOURCE_SUFFIX)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

HAS_JAVA = true
ifndef JAVA_BIN
HAS_JAVA =
endif
ifndef MVN_BIN
HAS_JAVA =
endif

.PHONY: java.log test
ifndef HAS_JAVA
java.log test:
	@echo the command 'java' or 'mvn' was not found in your PATH
	exit 127

else # HAS_JAVA

##############
##  SOURCE  ##
##############
.PHONY: build # Build a Java program.
.PHONY: run # Run a Java program.

# Check SOURCE argument
SOURCE_SUFFIX = $(suffix $(SOURCE))
# will contain “/any/path/foo.cc” on unix and “\\any\\path\\foo.cc” on windows
SOURCE_PATH = $(subst /,$S,$(SOURCE))
SOURCE_NAME = $(basename $(notdir $(SOURCE)))
ifeq ($(SOURCE),) # Those rules will be used if SOURCE is empty

build run:
	$(error no source file provided, the "$@" target must be used like so: \
 make $@ SOURCE=relative/path/to/filename.extension)

else #ifeq SOURCE

ifeq (,$(wildcard $(SOURCE)))
$(error File "$(SOURCE)" does not exist !)
endif
build: $(SOURCE) examples/$(SOURCE_NAME)/pom.xml | java.log
	cd examples$S$(SOURCE_NAME) && "$(MVN_BIN)" compile
run: build
	cd examples$S$(SOURCE_NAME) && "$(MVN_BIN)" exec:java $(ARGS)

endif #ifeq SOURCE

ifeq ($(OS),Windows)
JAVA_NATIVE_IDENTIFIER=win32-x86-64
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
JAVA_NATIVE_PROJECT := ortools-$(JAVA_NATIVE_IDENTIFIER)-@PROJECT_VERSION@.jar
JAVA_PROJECT := ortools-java-@PROJECT_VERSION@.jar

java.log: \
 $(JAVA_NATIVE_PROJECT) \
 $(JAVA_PROJECT)
	"$(MVN_BIN)" \
 org.apache.maven.plugins:maven-install-plugin:3.0.0-M1:install-file \
 -Dfile=$(JAVA_NATIVE_PROJECT)
	"$(MVN_BIN)" \
 org.apache.maven.plugins:maven-install-plugin:3.0.0-M1:install-file \
 -Dfile=$(JAVA_PROJECT)
	@$(TOUCH) $@

test:
	$(MAKE) run SOURCE=examples/LinearProgramming/src/main/java/com/google/ortools/LinearProgramming.java
	$(MAKE) run SOURCE=examples/IntegerProgramming/src/main/java/com/google/ortools/IntegerProgramming.java
	$(MAKE) run SOURCE=examples/RabbitsPheasants/src/main/java/com/google/ortools/RabbitsPheasants.java
	$(MAKE) run SOURCE=examples/Tsp/src/main/java/com/google/ortools/Tsp.java
	$(MAKE) run SOURCE=examples/Vrp/src/main/java/com/google/ortools/Vrp.java
	$(MAKE) run SOURCE=examples/Knapsack/src/main/java/com/google/ortools/Knapsack.java
	$(MAKE) run SOURCE=examples/AllDifferentExcept0/src/main/java/com/google/ortools/AllDifferentExcept0.java
	$(MAKE) run SOURCE=examples/AllInterval/src/main/java/com/google/ortools/AllInterval.java
	$(MAKE) run SOURCE=examples/CapacitatedVehicleRoutingProblemWithTimeWindows/src/main/java/com/google/ortools/CapacitatedVehicleRoutingProblemWithTimeWindows.java
	$(MAKE) run SOURCE=examples/Circuit/src/main/java/com/google/ortools/Circuit.java
	$(MAKE) run SOURCE=examples/CoinsGrid/src/main/java/com/google/ortools/CoinsGrid.java
	$(MAKE) run SOURCE=examples/CoinsGridMIP/src/main/java/com/google/ortools/CoinsGridMIP.java
	$(MAKE) run SOURCE=examples/ColoringMIP/src/main/java/com/google/ortools/ColoringMIP.java
	$(MAKE) run SOURCE=examples/CoveringOpl/src/main/java/com/google/ortools/CoveringOpl.java
	$(MAKE) run SOURCE=examples/Crossword/src/main/java/com/google/ortools/Crossword.java
	$(MAKE) run SOURCE=examples/DeBruijn/src/main/java/com/google/ortools/DeBruijn.java
	$(MAKE) run SOURCE=examples/Diet/src/main/java/com/google/ortools/Diet.java
	$(MAKE) run SOURCE=examples/DietMIP/src/main/java/com/google/ortools/DietMIP.java
	$(MAKE) run SOURCE=examples/DivisibleBy9Through1/src/main/java/com/google/ortools/DivisibleBy9Through1.java
	$(MAKE) run SOURCE=examples/FlowExample/src/main/java/com/google/ortools/FlowExample.java
	$(MAKE) run SOURCE=examples/GolombRuler/src/main/java/com/google/ortools/GolombRuler.java
	$(MAKE) run SOURCE=examples/Issue173/src/main/java/com/google/ortools/Issue173.java
	$(MAKE) run SOURCE=examples/KnapsackMIP/src/main/java/com/google/ortools/KnapsackMIP.java
	$(MAKE) run SOURCE=examples/LeastDiff/src/main/java/com/google/ortools/LeastDiff.java
	$(MAKE) run SOURCE=examples/LinearAssignmentAPI/src/main/java/com/google/ortools/LinearAssignmentAPI.java
	$(MAKE) run SOURCE=examples/MagicSquare/src/main/java/com/google/ortools/MagicSquare.java
	$(MAKE) run SOURCE=examples/Map2/src/main/java/com/google/ortools/Map2.java
	$(MAKE) run SOURCE=examples/Map/src/main/java/com/google/ortools/Map.java
	$(MAKE) run SOURCE=examples/Minesweeper/src/main/java/com/google/ortools/Minesweeper.java
	$(MAKE) run SOURCE=examples/MultiThreadTest/src/main/java/com/google/ortools/MultiThreadTest.java
	$(MAKE) run SOURCE=examples/NQueens2/src/main/java/com/google/ortools/NQueens2.java
	$(MAKE) run SOURCE=examples/NQueens/src/main/java/com/google/ortools/NQueens.java
	$(MAKE) run SOURCE=examples/Partition/src/main/java/com/google/ortools/Partition.java
	$(MAKE) run SOURCE=examples/QuasigroupCompletion/src/main/java/com/google/ortools/QuasigroupCompletion.java
	$(MAKE) run SOURCE=examples/SendMoreMoney2/src/main/java/com/google/ortools/SendMoreMoney2.java
	$(MAKE) run SOURCE=examples/SendMoreMoney/src/main/java/com/google/ortools/SendMoreMoney.java
	$(MAKE) run SOURCE=examples/SendMostMoney/src/main/java/com/google/ortools/SendMostMoney.java
	$(MAKE) run SOURCE=examples/Seseman/src/main/java/com/google/ortools/Seseman.java
	$(MAKE) run SOURCE=examples/SetCovering2/src/main/java/com/google/ortools/SetCovering2.java
	$(MAKE) run SOURCE=examples/SetCovering3/src/main/java/com/google/ortools/SetCovering3.java
	$(MAKE) run SOURCE=examples/SetCovering4/src/main/java/com/google/ortools/SetCovering4.java
	$(MAKE) run SOURCE=examples/SetCoveringDeployment/src/main/java/com/google/ortools/SetCoveringDeployment.java
	$(MAKE) run SOURCE=examples/SetCovering/src/main/java/com/google/ortools/SetCovering.java
	$(MAKE) run SOURCE=examples/SimpleRoutingTest/src/main/java/com/google/ortools/SimpleRoutingTest.java
	$(MAKE) run SOURCE=examples/StableMarriage/src/main/java/com/google/ortools/StableMarriage.java
	$(MAKE) run SOURCE=examples/StiglerMIP/src/main/java/com/google/ortools/StiglerMIP.java
	$(MAKE) run SOURCE=examples/Strimko2/src/main/java/com/google/ortools/Strimko2.java
	$(MAKE) run SOURCE=examples/Sudoku/src/main/java/com/google/ortools/Sudoku.java
	$(MAKE) run SOURCE=examples/SurvoPuzzle/src/main/java/com/google/ortools/SurvoPuzzle.java
	$(MAKE) run SOURCE=examples/ToNum/src/main/java/com/google/ortools/ToNum.java
	$(MAKE) run SOURCE=examples/WhoKilledAgatha/src/main/java/com/google/ortools/WhoKilledAgatha.java
	$(MAKE) run SOURCE=examples/Xkcd/src/main/java/com/google/ortools/Xkcd.java
	$(MAKE) run SOURCE=examples/YoungTableaux/src/main/java/com/google/ortools/YoungTableaux.java
	$(MAKE) run SOURCE=examples/SimpleLpProgram/src/main/java/com/google/ortools/SimpleLpProgram.java
	$(MAKE) run SOURCE=examples/SimpleMipProgram/src/main/java/com/google/ortools/SimpleMipProgram.java
	$(MAKE) run SOURCE=examples/SimpleSatProgram/src/main/java/com/google/ortools/SimpleSatProgram.java
	$(MAKE) run SOURCE=examples/Tsp/src/main/java/com/google/ortools/Tsp.java
	$(MAKE) run SOURCE=examples/Vrp/src/main/java/com/google/ortools/Vrp.java
	$(MAKE) run SOURCE=examples/Knapsack/src/main/java/com/google/ortools/Knapsack.java

endif # HAS_JAVA

############
##  MISC  ##
############
# Include user makefile
-include Makefile.user

print-%	: ; @echo $* = $($*)
