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
SWIG_BINARY = swig.exe

# language targets
BUILD_PYTHON ?= OFF
BUILD_JAVA ?= OFF
BUILD_DOTNET ?= OFF

# Main target.
dependencies:
	mkdir dependencies

.PHONY: third_party # Build OR-Tools Prerequisite
third_party: \
  Makefile.local \
  dependencies/ortools.sln | dependencies

THIRD_PARTY_TARGET = dependencies/ortools.sln

######################
##  Makefile.local  ##
######################
# Make sure that local file lands correctly across platforms
Makefile.local: makefiles/Makefile.third_party.$(SYSTEM).mk
	-$(DEL) Makefile.local
	@echo $(SELECTED_PATH_TO_PYTHON)>> Makefile.local
	@echo # >> Makefile.local
	@echo ## OPTIONAL DEPENDENCIES ## >> Makefile.local
	@echo # Define WINDOWS_CPLEX_DIR to point to a installation directory of the CPLEX Studio >> Makefile.local
	@echo #   e.g.: WINDOWS_CPLEX_DIR = C:\Progra~1\CPLEX_STUDIO2010\IBM\ILOG\CPLEX_STUDIO2010 >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_CPLEX_VERSION to specify the suffix of the library >> Makefile.local
	@echo #   e.g.: WINDOWS_CPLEX_VERSION = 2010 >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define WINDOWS_XPRESS_DIR to point to a installation directory of the XPRESS-MP >> Makefile.local
	@echo #   e.g.: WINDOWS_XPRESS_DIR = C:\xpressmp>> Makefile.local
	@echo # >> Makefile.local
	@echo # SCIP is enabled and built-in by default. >> Makefile.local
	@echo # To disable support, uncomment the following line: >> Makefile.local
	@echo # USE_SCIP = OFF >> Makefile.local
	@echo # >> Makefile.local
	@echo # Coin OR solvers (CLP, CBC) are enabled and built-in by default. >> Makefile.local
	@echo # To disable support, uncomment the following line: >> Makefile.local
	@echo # USE_COINOR = OFF >> Makefile.local
	@echo # >> Makefile.local
	@echo # Define absolute paths without trailing "\". E.g. "C:\Foo\Bar" >> Makefile.local
	@echo # Paths must be without spaces, try to use 'dir "directory*" /x' to get the shortname without space of each directory >> Makefile.local
	@echo #   e.g. dir "%ProgramFiles%*" /x >> Makefile.local

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
	-DCMAKE_INSTALL_PREFIX=$(OR_ROOT_FULL)

.PHONY: clean_third_party # Clean everything. Remember to also delete archived dependencies, i.e. in the event of download failure, etc.
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
	@echo WINDOWS_SCIP_DIR = $(WINDOWS_SCIP_DIR)
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
