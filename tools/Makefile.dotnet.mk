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
  S := /
  ifeq ($(OS),Linux)
  endif # ifeq($(OS),Linux)
  ifeq ($(OS),Darwin) # Assume Mac Os X
  endif # ifeq($(OS),Darwin)
  DOTNET ?= dotnet
  DOTNET_BIN := $(shell command -v $(DOTNET) 2> /dev/null)
endif # SYSTEM == unix

# Windows specific part.
ifeq ($(SYSTEM),win)
  S := \\
  WHICH := bin$Swhich.exe
  DOTNET = dotnet
  DOTNET_BIN := $(shell $(WHICH) $(DOTNET) 2> NUL)
endif # SYSTEM == win

.PHONY: all
all: detect

.PHONY: detect
detect:
	@echo VERSION = $(VERSION)
	@echo SYSTEM = $(SYSTEM)
	@echo OS = $(OS)
	@echo SHELL = $(SHELL)
	@echo DOTNET_BIN = $(DOTNET_BIN)
	@echo SOURCE = $(SOURCE)
	@echo SOURCE_PATH = $(SOURCE_PATH)
	@echo SOURCE_NAME = $(SOURCE_NAME)
	@echo SOURCE_SUFFIX = $(SOURCE_SUFFIX)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif

##############
##  SOURCE  ##
##############
.PHONY: build # Build a .Net C# program.
.PHONY: run # Run a .Net C# program.

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
endif #ifeq SOURCE exist
build: $(SOURCE)
	"$(DOTNET_BIN)" build examples/$(SOURCE_NAME)/project.csproj
run: build
	"$(DOTNET_BIN)" run --no-build --framework net6.0 --project examples/$(SOURCE_NAME)/project.csproj -- $(ARGS)

endif #ifeq SOURCE

##############
##  DOTNET  ##
##############
.PHONY: test
test:
	$(MAKE) run SOURCE=examples/3_jugs_regular/3_jugs_regular.cs
# Linear Solver
	$(MAKE) run SOURCE=examples/SimpleLpProgram/SimpleLpProgram.cs
	$(MAKE) run SOURCE=examples/SimpleMipProgram/SimpleMipProgram.cs
# Constraint Solver
	$(MAKE) run SOURCE=examples/Tsp/Tsp.cs
	$(MAKE) run SOURCE=examples/Vrp/Vrp.cs
# Sat
	$(MAKE) run SOURCE=examples/NursesSat/NursesSat.cs
	$(MAKE) run SOURCE=examples/JobshopSat/JobshopSat.cs
	$(MAKE) run SOURCE=examples/JobshopFt06Sat/JobshopFt06Sat.cs
	$(MAKE) run SOURCE=examples/GateSchedulingSat/GateSchedulingSat.cs
	$(MAKE) run SOURCE=examples/TaskSchedulingSat/TaskSchedulingSat.cs
# Misc
	$(MAKE) run SOURCE=examples/cslinearprogramming/cslinearprogramming.cs
	$(MAKE) run SOURCE=examples/csintegerprogramming/csintegerprogramming.cs
	$(MAKE) run SOURCE=examples/assignment/assignment.cs
	$(MAKE) run SOURCE=examples/alldifferent_except_0/alldifferent_except_0.cs
	$(MAKE) run SOURCE=examples/all_interval/all_interval.cs
	$(MAKE) run SOURCE=examples/a_puzzle/a_puzzle.cs
	$(MAKE) run SOURCE=examples/a_round_of_golf/a_round_of_golf.cs
	$(MAKE) run SOURCE=examples/broken_weights/broken_weights.cs
	$(MAKE) run SOURCE=examples/bus_schedule/bus_schedule.cs
	$(MAKE) run SOURCE=examples/circuit2/circuit2.cs
	$(MAKE) run SOURCE=examples/circuit/circuit.cs
	$(MAKE) run SOURCE=examples/coins3/coins3.cs
	$(MAKE) run SOURCE=examples/coins_grid/coins_grid.cs ARGS="5 2"
	$(MAKE) run SOURCE=examples/combinatorial_auction2/combinatorial_auction2.cs
	$(MAKE) run SOURCE=examples/contiguity_regular/contiguity_regular.cs
	$(MAKE) run SOURCE=examples/contiguity_transition/contiguity_transition.cs
	$(MAKE) run SOURCE=examples/costas_array/costas_array.cs
	$(MAKE) run SOURCE=examples/covering_opl/covering_opl.cs
	$(MAKE) run SOURCE=examples/crew/crew.cs
	$(MAKE) run SOURCE=examples/crossword/crossword.cs
	$(MAKE) run SOURCE=examples/crypta/crypta.cs
	$(MAKE) run SOURCE=examples/crypto/crypto.cs
	$(MAKE) run SOURCE=examples/cscvrptw/cscvrptw.cs
	$(MAKE) run SOURCE=examples/csflow/csflow.cs
	$(MAKE) run SOURCE=examples/csknapsack/csknapsack.cs
	$(MAKE) run SOURCE=examples/csls_api/csls_api.cs
	$(MAKE) run SOURCE=examples/csrabbitspheasants/csrabbitspheasants.cs
	$(MAKE) run SOURCE=examples/cstsp/cstsp.cs
	$(MAKE) run SOURCE=examples/curious_set_of_integers/curious_set_of_integers.cs
	$(MAKE) run SOURCE=examples/debruijn/debruijn.cs
	$(MAKE) run SOURCE=examples/csdiet/csdiet.cs
	$(MAKE) run SOURCE=examples/discrete_tomography/discrete_tomography.cs
	$(MAKE) run SOURCE=examples/divisible_by_9_through_1/divisible_by_9_through_1.cs
	$(MAKE) run SOURCE=examples/dudeney/dudeney.cs
	$(MAKE) run SOURCE=examples/einav_puzzle2/einav_puzzle2.cs
	$(MAKE) run SOURCE=examples/eq10/eq10.cs
	$(MAKE) run SOURCE=examples/eq20/eq20.cs
	$(MAKE) run SOURCE=examples/fill_a_pix/fill_a_pix.cs
	$(MAKE) run SOURCE=examples/furniture_moving/furniture_moving.cs
	$(MAKE) run SOURCE=examples/furniture_moving_intervals/furniture_moving_intervals.cs
	$(MAKE) run SOURCE=examples/futoshiki/futoshiki.cs
	$(MAKE) run SOURCE=examples/golomb_ruler/golomb_ruler.cs
	$(MAKE) run SOURCE=examples/grocery/grocery.cs
	$(MAKE) run SOURCE=examples/hidato_table/hidato_table.cs
	$(MAKE) run SOURCE=examples/just_forgotten/just_forgotten.cs
	$(MAKE) run SOURCE=examples/kakuro/kakuro.cs
	$(MAKE) run SOURCE=examples/kenken2/kenken2.cs
	$(MAKE) run SOURCE=examples/killer_sudoku/killer_sudoku.cs
	$(MAKE) run SOURCE=examples/labeled_dice/labeled_dice.cs
	$(MAKE) run SOURCE=examples/langford/langford.cs
	$(MAKE) run SOURCE=examples/least_diff/least_diff.cs
	$(MAKE) run SOURCE=examples/lectures/lectures.cs
	$(MAKE) run SOURCE=examples/magic_sequence/magic_sequence.cs
	$(MAKE) run SOURCE=examples/magic_square_and_cards/magic_square_and_cards.cs
	$(MAKE) run SOURCE=examples/magic_square/magic_square.cs
	$(MAKE) run SOURCE=examples/map2/map2.cs
	$(MAKE) run SOURCE=examples/map/map.cs
	$(MAKE) run SOURCE=examples/marathon2/marathon2.cs
	$(MAKE) run SOURCE=examples/max_flow_taha/max_flow_taha.cs
	$(MAKE) run SOURCE=examples/max_flow_winston1/max_flow_winston1.cs
	$(MAKE) run SOURCE=examples/minesweeper/minesweeper.cs
	$(MAKE) run SOURCE=examples/mr_smith/mr_smith.cs
#	$(MAKE) run SOURCE=examples/nontransitive_dice/nontransitive_dice.cs # too long
	$(MAKE) run SOURCE=examples/nqueens/nqueens.cs
	$(MAKE) run SOURCE=examples/nurse_rostering_regular/nurse_rostering_regular.cs
	$(MAKE) run SOURCE=examples/nurse_rostering_transition/nurse_rostering_transition.cs
	$(MAKE) run SOURCE=examples/olympic/olympic.cs
	$(MAKE) run SOURCE=examples/organize_day/organize_day.cs
	$(MAKE) run SOURCE=examples/organize_day_intervals/organize_day_intervals.cs
	$(MAKE) run SOURCE=examples/pandigital_numbers/pandigital_numbers.cs
#	$(MAKE) run SOURCE=examples/partition/partition.cs # too long
	$(MAKE) run SOURCE=examples/perfect_square_sequence/perfect_square_sequence.cs
	$(MAKE) run SOURCE=examples/photo_problem/photo_problem.cs
	$(MAKE) run SOURCE=examples/place_number_puzzle/place_number_puzzle.cs
	$(MAKE) run SOURCE=examples/p_median/p_median.cs
	$(MAKE) run SOURCE=examples/post_office_problem2/post_office_problem2.cs
	$(MAKE) run SOURCE=examples/quasigroup_completion/quasigroup_completion.cs
	$(MAKE) run SOURCE=examples/regex/regex.cs
	$(MAKE) run SOURCE=examples/rogo2/rogo2.cs
	$(MAKE) run SOURCE=examples/scheduling_speakers/scheduling_speakers.cs
	$(MAKE) run SOURCE=examples/secret_santa2/secret_santa2.cs
#	$(MAKE) run SOURCE=examples/secret_santa/secret_santa.cs # too long
	$(MAKE) run SOURCE=examples/send_more_money2/send_more_money2.cs
	$(MAKE) run SOURCE=examples/send_more_money/send_more_money.cs
	$(MAKE) run SOURCE=examples/send_most_money/send_most_money.cs
	$(MAKE) run SOURCE=examples/seseman/seseman.cs
	$(MAKE) run SOURCE=examples/set_covering2/set_covering2.cs
	$(MAKE) run SOURCE=examples/set_covering3/set_covering3.cs
	$(MAKE) run SOURCE=examples/set_covering4/set_covering4.cs
	$(MAKE) run SOURCE=examples/set_covering/set_covering.cs
	$(MAKE) run SOURCE=examples/set_covering_deployment/set_covering_deployment.cs
	$(MAKE) run SOURCE=examples/set_covering_skiena/set_covering_skiena.cs
	$(MAKE) run SOURCE=examples/set_partition/set_partition.cs
	$(MAKE) run SOURCE=examples/sicherman_dice/sicherman_dice.cs
	$(MAKE) run SOURCE=examples/ski_assignment/ski_assignment.cs
	$(MAKE) run SOURCE=examples/stable_marriage/stable_marriage.cs
	$(MAKE) run SOURCE=examples/strimko2/strimko2.cs
	$(MAKE) run SOURCE=examples/subset_sum/subset_sum.cs
	$(MAKE) run SOURCE=examples/sudoku/sudoku.cs
	$(MAKE) run SOURCE=examples/survo_puzzle/survo_puzzle.cs
	$(MAKE) run SOURCE=examples/to_num/to_num.cs
	$(MAKE) run SOURCE=examples/traffic_lights/traffic_lights.cs
	$(MAKE) run SOURCE=examples/volsay/volsay.cs
	$(MAKE) run SOURCE=examples/volsay2/volsay2.cs
	$(MAKE) run SOURCE=examples/volsay3/volsay3.cs
	$(MAKE) run SOURCE=examples/wedding_optimal_chart/wedding_optimal_chart.cs
	$(MAKE) run SOURCE=examples/who_killed_agatha/who_killed_agatha.cs
	$(MAKE) run SOURCE=examples/xkcd/xkcd.cs
	$(MAKE) run SOURCE=examples/young_tableaux/young_tableaux.cs
	$(MAKE) run SOURCE=examples/zebra/zebra.cs
#	$(MAKE) run SOURCE=examples/word_square/word_square.cs # depends on /usr/share/dict/words
	$(MAKE) run SOURCE=examples/SimpleLpProgram/SimpleLpProgram.cs
	$(MAKE) run SOURCE=examples/SimpleMipProgram/SimpleMipProgram.cs
	$(MAKE) run SOURCE=examples/Tsp/Tsp.cs
	$(MAKE) run SOURCE=examples/Vrp/Vrp.cs
	$(MAKE) run SOURCE=examples/JobshopSat/JobshopSat.cs
	$(MAKE) run SOURCE=examples/NursesSat/NursesSat.cs

############
##  MISC  ##
############
# Include user makefile
-include Makefile.user

print-%	: ; @echo $* = $($*)
