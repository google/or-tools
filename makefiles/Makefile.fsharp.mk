# ---------- FSharp support using SWIG ----------
.PHONY: help_fsharp # Generate list of F# targets with descriptions.
help_fsharp:
	@echo Use one of the following F# targets:
ifeq ($(SYSTEM),win)
	@tools\grep.exe "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | tools\sed.exe "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

BASE_ORTOOLS_DLL_NAME=Google.OrTools
FSHARP_ORTOOLS_DLL_NAME=$(BASE_ORTOOLS_DLL_NAME).FSharp

FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE=$(BASE_ORTOOLS_DLL_NAME).AssemblyInfo.fsx
FSHARP_ORTOOLS_NUSPEC_FILE=$(FSHARP_ORTOOLS_DLL_NAME).nuspec

CLEAN_FILES=$(FSHARP_ORTOOLS_DLL_NAME).*

# Check for required build tools
ifeq ($(SYSTEM), win)
FSHARP_COMPILER := fsc.exe
FSHARP_EXECUTABLE := $(shell $(WHICH) $(FSHARP_COMPILER) 2>nul)
FLAG_PREFIX := /
else # UNIX
FSHARP_COMPILER := fsharpc
FSHARP_EXECUTABLE := $(shell which $(FSHARP_COMPILER))
FLAG_PREFIX := --
endif

# Check whether to build Debug or Release version
ifeq (${FSHARP_DEBUG}, 1)
FSHARP_DEBUG = $(FLAG_PREFIX)debug
endif

# Check for key pair for strong naming
ifdef CLR_KEYFILE
FS_SIGNING_FLAGS := $(FLAG_PREFIX)keyfile:$(CLR_KEYFILE)
endif

ifeq ("$(VisualStudioVersion)", "15.0")
FS_NOCPYCORE := $(FLAG_PREFIX)nocopyfsharpcore
else
FS_NOCPYCORE :=
endif

.PHONY: fsharp # Build F# OR-Tools. Set environment variable FSHARP_DEBUG=1 for debug symbols.
fsharp: csharp
ifneq ($(FSHARP_EXECUTABLE),)
	$(SED) -i -e "s/MMMM/$(FSHARP_ORTOOLS_DLL_NAME)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE)
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE)
	"$(FSHARP_EXECUTABLE)" $(FLAG_PREFIX)target:library $(FLAG_PREFIX)out:bin$S$(FSHARP_ORTOOLS_DLL_NAME).dll $(FLAG_PREFIX)platform:anycpu $(FS_NOCPYCORE) $(FLAG_PREFIX)lib:bin $(FLAG_PREFIX)reference:$(BASE_ORTOOLS_DLL_NAME).dll $(FSHARP_DEBUG) $(FS_SIGNING_FLAGS) ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME).fsx ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE)
else
	$(warning Cannot find '$(FSHARP_COMPILER)' command which is needed for build. Please make sure it is installed and in system path.)
endif

.PHONY: nuget-pkg_fsharp # Build Nuget Package for distribution.
nuget-pkg_fsharp: fsharp
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_NUSPEC_FILE)
	$(NUGET_EXECUTABLE) pack ortools$Sfsharp$S$(FSHARP_ORTOOLS_NUSPEC_FILE) -Basepath . -OutputDirectory $(ORTOOLS_NUGET_DIR)

.PHONY: test_fsharp # Test F# OR-Tools using various examples.
ifneq ($(FSHARP_EXECUTABLE),)
test_fsharp: test_fsharp_examples
BUILT_LANGUAGES +=, F\#
else
test_fsharp: fsharp
endif

.PHONY: clean_fsharp # Clean F# output from previous build.
clean_fsharp:
	-$(DEL) bin$S$(CLEAN_FILES)

.PHONY: detect_fsharp # Show variables used to build F# OR-Tools.
detect_fsharp:
ifeq ($(SYSTEM),win)
	@echo Relevant info for the F# build:
else
	@echo Relevant info for the F\# build:
endif
	@echo FSHARP_COMPILER = $(FSHARP_COMPILER)
	@echo FSHARP_EXECUTABLE = "$(FSHARP_EXECUTABLE)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
