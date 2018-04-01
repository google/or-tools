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
DOTNET_EXECUTABLE := $(shell $(WHICH) dotnet.exe 2>nul)
FLAG_PREFIX := /
else # UNIX
FSHARP_COMPILER := fsharpc
FSHARP_EXECUTABLE := $(shell which $(FSHARP_COMPILER))
DOTNET_EXECUTABLE := $(shell which dotnet)
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

FSHARP_LIB_DIR :=
ifeq ($(PLATFORM),MACOSX)
FSHARP_LIB_DIR = env DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR)
endif
ifeq ($(PLATFORM),LINUX)
FSHARP_LIB_DIR = env LD_LIBRARY_PATH=$(LIB_DIR)
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
	"$(NUGET_EXECUTABLE)" pack ortools$Sfsharp$S$(FSHARP_ORTOOLS_NUSPEC_FILE) -Basepath . -OutputDirectory $(ORTOOLS_NUGET_DIR)

.PHONY: test_fsharp # Tests for F# OR-Tools
test_fsharp: fsharp
ifeq ($(SYSTEM),win)
	"$(FSHARP_EXECUTABLE)" $(FLAG_PREFIX)target:exe $(FLAG_PREFIX)out:bin$Sequality.exe $(FLAG_PREFIX)platform:anycpu $(FLAG_PREFIX)lib:bin examples$Sfsharp$Sequality.fsx
	bin$Sequality.exe
else
	ifneq ($(DOTNET_EXECUTABLE),)
		"$(DOTNET_EXECUTABLE)" restore --packages "ortools$Sfsharp$Stest$Spackages" "ortools$Sfsharp$Stest$S$(FSHARP_ORTOOLS_DLL_NAME).Test.fsproj"
		"$(DOTNET_EXECUTABLE)" build "ortools$Sfsharp$Stest$S$(FSHARP_ORTOOLS_DLL_NAME).Test.fsproj" -o ".$Sbin"
		$(FSHARP_LIB_DIR) "$(DOTNET_EXECUTABLE)" "ortools$Sfsharp$Stest$Spackages$Sxunit.runner.console$S2.3.1$Stools$Snetcoreapp2.0$Sxunit.console.dll" "ortools$Sfsharp$Stest$Sbin$S$(FSHARP_ORTOOLS_DLL_NAME).Test.dll"
	else
		$(warning Cannot find '$(DOTNET_EXECUTABLE)' command which is needed to run tests. Please make sure it is installed and in system path.)
	endif
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
	@echo DOTNET_EXECUTABLE = "$(DOTNET_EXECUTABLE)"
	@echo NUGET_EXECUTABLE = "$(NUGET_EXECUTABLE)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
