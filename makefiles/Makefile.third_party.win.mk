.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.win.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(

# Checks if the user has overwritten default libraries and binaries.
USE_COINOR ?= ON
USE_SCIP ?= ON
USE_GLPK ?= OFF
PROTOC = "$(OR_TOOLS_TOP)\\bin\\protoc.exe"

# language targets
BUILD_PYTHON ?= OFF
BUILD_JAVA ?= OFF
BUILD_DOTNET ?= OFF
BUILD_TYPE ?= Release

# Main target.
dependencies:
	mkdir dependencies

.PHONY: third_party # Build OR-Tools Prerequisite
third_party: dependencies/ortools.sln | dependencies

THIRD_PARTY_TARGET = dependencies/ortools.sln
GENERATOR ?= $(CMAKE_PLATFORM)

dependencies/ortools.sln: | dependencies
	cmake -S . -B dependencies -DBUILD_DEPS=ON \
	-DBUILD_PYTHON=$(BUILD_PYTHON) \
	-DBUILD_JAVA=$(BUILD_JAVA) \
	-DBUILD_DOTNET=$(BUILD_DOTNET) \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_SAMPLES=OFF \
	-DUSE_COINOR=$(USE_COINOR) \
	-DUSE_SCIP=$(USE_SCIP) \
	-DUSE_GLPK=$(USE_GLPK) \
	-DCMAKE_INSTALL_PREFIX=$(OR_ROOT_FULL) \
	-DCMAKE_BUILD_TYPE=Release \
	-G $(GENERATOR)

.PHONY: clean_third_party
clean_third_party:
	-$(DEL) Makefile.local
	-$(DELREC) dependencies\\*
	-$(DELREC) include
	-$(DELREC) share
	-$(DELREC) lib\\*.lib
	-$(DELREC) lib\\cmake
	-$(DELREC) lib\\pkgconfig

.PHONY: detect_third_party # Show variables used to find third party
detect_third_party:
	@echo Relevant info on third party:
	@echo USE_COINOR = $(USE_COINOR)
	@echo USE_SCIP = $(USE_SCIP)
ifdef WINDOWS_GLPK_DIR
	@echo WINDOWS_GLPK_DIR = $(WINDOWS_GLPK_DIR)
	@echo GLPK_INC = $(GLPK_INC)
	@echo DYNAMIC_GLPK_LNK = $(DYNAMIC_GLPK_LNK)
	@echo STATIC_GLPK_LNK = $(STATIC_GLPK_LNK)
endif
ifdef WINDOWS_CPLEX_DIR
	@echo WINDOWS_CPLEX_DIR = $(WINDOWS_CPLEX_DIR)
	@echo CPLEX_INC = $(CPLEX_INC)
	@echo DYNAMIC_CPLEX_LNK = $(DYNAMIC_CPLEX_LNK)
	@echo STATIC_CPLEX_LNK = $(STATIC_CPLEX_LNK)
endif
ifdef WINDOWS_XPRESS_DIR
	@echo WINDOWS_XPRESS_DIR = $(WINDOWS_XPRESS_DIR)
	@echo XPRESS_INC = $(XPRESS_INC)
	@echo XPRESS_LNK = $(XPRESS_LNK)
endif
	@echo off & echo(
