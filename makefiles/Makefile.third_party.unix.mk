.PHONY: help_third_party # Generate list of Prerequisite targets with descriptions.
help_third_party:
	@echo Use one of the following Prerequisite targets:
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.third_party.unix.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo

# Checks if the user has overwritten default libraries and binaries.
USE_COINOR ?= ON
USE_SCIP ?= ON
USE_GLPK ?= OFF
PROTOC ?= $(OR_TOOLS_TOP)/bin/protoc
BUILD_TYPE ?= Release

# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party:  dependencies/Makefile

THIRD_PARTY_TARGET = dependencies/Makefile

dependencies:
	mkdir dependencies

dependencies/Makefile: | dependencies
	cmake -S . -B dependencies -DBUILD_DEPS=ON \
	-DBUILD_PYTHON=$(BUILD_PYTHON) \
	-DBUILD_JAVA=$(BUILD_JAVA) \
	-DBUILD_DOTNET=$(BUILD_DOTNET) \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_SAMPLES=OFF \
	-DUSE_COINOR=$(USE_COINOR) \
	-DUSE_SCIP=$(USE_SCIP) \
	-DUSE_GLPK=$(USE_GLPK) \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_INSTALL_PREFIX=$(OR_ROOT_FULL)

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
clean_third_party:
	-$(DEL) Makefile.local
	-$(DELREC) dependencies/*
	-$(DELREC) include
	-$(DELREC) share
	-$(DELREC) lib/*.a
	-$(DELREC) lib/cmake
	-$(DELREC) lib/pkgconfig


.PHONY: detect_third_party # Show variables used to find third party
detect_third_party:
	@echo Relevant info on third party:
	@echo USE_COINOR = $(USE_COINOR)
	@echo USE_SCIP = $(USE_SCIP)
ifdef UNIX_GLPK_DIR
	@echo UNIX_GLPK_DIR = $(UNIX_GLPK_DIR)
	@echo GLPK_INC = $(GLPK_INC)
	@echo GLPK_LNK = $(GLPK_LNK)
endif
ifdef UNIX_CPLEX_DIR
	@echo UNIX_CPLEX_DIR = $(UNIX_CPLEX_DIR)
	@echo CPLEX_INC = $(CPLEX_INC)
	@echo CPLEX_LNK = $(CPLEX_LNK)
endif
ifdef UNIX_XPRESS_DIR
	@echo UNIX_XPRESS_DIR = $(UNIX_XPRESS_DIR)
	@echo XPRESS_INC = $(XPRESS_INC)
	@echo XPRESS_LNK = $(XPRESS_LNK)
endif
	@echo
