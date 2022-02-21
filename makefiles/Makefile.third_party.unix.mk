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
# Swig is only needed when building .Net, Java or Python wrapper
UNIX_SWIG_BINARY ?= swig
SWIG_BINARY = $(shell $(WHICH) $(UNIX_SWIG_BINARY))
SWIG_DOXYGEN = -doxygen


# Main target.
.PHONY: third_party # Build OR-Tools Prerequisite
third_party:  Makefile.local dependencies/Makefile

THIRD_PARTY_TARGET = dependencies/Makefile

######################
##  Makefile.local  ##
######################
# Make sure that local file lands correctly across platforms
Makefile.local: makefiles/Makefile.third_party.$(SYSTEM).mk
	-$(DEL) Makefile.local
	@echo Generating Makefile.local
	@echo "# Define UNIX_SWIG_BINARY to use a custom version." >> Makefile.local
	@echo "#   e.g. UNIX_SWIG_BINARY = /opt/swig-x.y.z/bin/swig" >> Makefile.local
	@echo JAVA_HOME = $(JAVA_HOME)>> Makefile.local
	@echo UNIX_PYTHON_VER = $(DETECTED_PYTHON_VERSION)>> Makefile.local
	@echo >> Makefile.local
	@echo "## OPTIONAL DEPENDENCIES ##" >> Makefile.local
	@echo "# Define UNIX_CPLEX_DIR to use CPLEX" >> Makefile.local
	@echo "#   e.g. UNIX_CPLEX_DIR = /opt/CPLEX_Studio-X.Y" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_XPRESS_DIR to use XPRESS MP" >> Makefile.local
	@echo "#   e.g. on Mac OS X: UNIX_XPRESS_DIR = /Applications/FICO\ Xpress/xpressmp" >> Makefile.local
	@echo "#   e.g. on linux: UNIX_XPRESS_DIR = /opt/xpressmp" >> Makefile.local
	@echo >> Makefile.local
	@echo "# SCIP is enabled and built-in by default. To disable SCIP support" >> Makefile.local
	@echo "# completely, uncomment the following line:">> Makefile.local
	@echo "# USE_SCIP = OFF" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Define UNIX_GLPK_DIR to point to a compiled version of GLPK to use it" >> Makefile.local
	@echo "#   e.g. UNIX_GLPK_DIR = /opt/glpk-x.y.z" >> Makefile.local
	@echo >> Makefile.local
	@echo "# Coin OR solvers (CLP, CBC) are enabled and built-in by default." >> Makefile.local
	@echo "# To disable Coin OR support completely, uncomment the following line:">> Makefile.local
	@echo "# USE_COINOR = OFF" >> Makefile.local
	@echo >> Makefile.local

dependencies:
	mkdir dependencies

dependencies/Makefile: | dependencies
	cmake -S . -B dependencies -DBUILD_DEPS=ON -DBUILD_PYTHON=ON -DBUILD_EXAMPLES=OFF -DBUILD_SAMPLES=OFF -DUSE_COINOR=$(USE_COINOR) -DUSE_SCIP=$(USE_SCIP) -DUSE_GLPK=$(USE_GLPK) -DCMAKE_INSTALL_PREFIX=$(OR_ROOT_FULL)

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
	@echo SWIG_VERSION = $(SWIG_VERSION)
	@echo
