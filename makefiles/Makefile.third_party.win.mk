.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.win.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(

# Checks if the user has overwritten default libraries and binaries.
BUILD_TYPE ?= Release
USE_COINOR ?= ON
USE_SCIP ?= ON
USE_GLPK ?= OFF
USE_CPLEX ?= OFF
USE_XPRESS ?= OFF
PROTOC = "$(OR_TOOLS_TOP)\\bin\\protoc.exe"

# Main target.
$(BUILD_DIR):
	mkdir $(BUILD_DIR)

.PHONY: third_party # Build OR-Tools Prerequisite
third_party: $(BUILD_DIR)/ortools.sln | $(BUILD_DIR)

THIRD_PARTY_TARGET = $(BUILD_DIR)/ortools.sln
GENERATOR ?= $(CMAKE_PLATFORM)

$(BUILD_DIR)/ortools.sln: | $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) -DBUILD_DEPS=ON \
	-DBUILD_PYTHON=$(BUILD_PYTHON) \
	-DBUILD_JAVA=$(BUILD_JAVA) \
	-DBUILD_DOTNET=$(BUILD_DOTNET) \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_SAMPLES=OFF \
	-DUSE_COINOR=$(USE_COINOR) \
	-DUSE_SCIP=$(USE_SCIP) \
	-DUSE_GLPK=$(USE_GLPK) \
	-DUSE_CPLEX=$(USE_CPLEX) \
	-DUSE_XPRESS=$(USE_XPRESS) \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_INSTALL_PREFIX=$(OR_ROOT_FULL) \
	-G $(GENERATOR)

.PHONY: clean_third_party # clean everything.
clean_third_party:
	-$(DEL) Makefile.local
	-$(DELREC) $(BUILD_DIR)\\*
	-$(DELREC) include
	-$(DELREC) share
	-$(DELREC) lib\\*.lib
	-$(DELREC) lib\\cmake
	-$(DELREC) lib\\pkgconfig

.PHONY: detect_third_party # Show variables used to find third party
detect_third_party:
	@echo Relevant info on third party:
	@echo BUILD_TYPE = $(BUILD_TYPE)
	@echo USE_GLOP = ON
	@echo USE_PDLP = ON
	@echo USE_COINOR = $(USE_COINOR)
	@echo USE_SCIP = $(USE_SCIP)
	@echo USE_GLPK = $(USE_GLPK)
	@echo USE_CPLEX = $(USE_CPLEX)
	@echo USE_XPRESS = $(USE_XPRESS)
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
