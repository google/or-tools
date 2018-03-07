BASE_ORTOOLS_DLL_NAME=Google.OrTools
FSHARP_ORTOOLS_DLL_NAME=$(BASE_ORTOOLS_DLL_NAME).FSharp

FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE=$(BASE_ORTOOLS_DLL_NAME).AssemblyInfo.fsx
FSHARP_ORTOOLS_NUSPEC_FILE=$(FSHARP_ORTOOLS_DLL_NAME).nuspec

CLEAN_FILES=$(FSHARP_ORTOOLS_DLL_NAME).*

# Check for required build tools
ifeq ($(SYSTEM), win)
	FSHARP_COMPILER:=fsc
	NUGET_CMD:=nuget.exe
	FLAG_PREFIX:=/
else
	FSHARP_COMPILER:=fsharpc
	NUGET_CMD:=nuget
	FLAG_PREFIX:=--
endif

FSHARP_COMPILER_CHECK:=$(shell which $(FSHARP_COMPILER))

# Check whether to build Debug or Release version
ifeq (${FSHARP_DEBUG}, 1)
	FSHARP_DEBUG = $(FLAG_PREFIX)debug
endif

# Check for key pair for strong naming
ifdef CLR_KEYFILE
	FS_SIGNING_FLAGS = $(FLAG_PREFIX)keyfile:$(CLR_KEYFILE)
endif

.PHONY: default
default: fsharp-help

.PHONY: fsharp-help # Generate list of targets with descriptions.
fsharp-help:
	$(info Use one of the following targets:)
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20

.PHONY: fsharp # Build F# OR-Tools. Set environment variable FSHARP_DEBUG=1 for debug symbols.
fsharp:
ifneq ($(FSHARP_COMPILER_CHECK),)
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE) ortools$Sfsharp$S$(FSHARP_ORTOOLS_NUSPEC_FILE)
	$(FSHARP_COMPILER) $(FLAG_PREFIX)target:library $(FLAG_PREFIX)out:bin$S$(FSHARP_ORTOOLS_DLL_NAME).dll $(FLAG_PREFIX)platform:anycpu $(FLAG_PREFIX)nocopyfsharpcore $(FLAG_PREFIX)lib:bin $(FLAG_PREFIX)reference:$(BASE_ORTOOLS_DLL_NAME).dll $(FSHARP_DEBUG) $(FS_SIGNING_FLAGS) ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME).fsx ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_ASSEMBLYINFO_FILE)
else
	$(error Cannot find '$(FSHARP_COMPILER)' command which is needed for build. Please make sure it is installed and in system path.)
endif

.PHONY: fsharp-build-nuget # Build Nuget Package for distribution.
fsharp-build-nuget: fsharp
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_NUSPEC_FILE)
	$(NUGET_CMD) pack ortools$Sfsharp$S$(FSHARP_ORTOOLS_NUSPEC_FILE) -Basepath . -OutputDirectory $(ORTOOLS_NUGET_DIR)

.PHONY: fsharp-clean # Clean output from previous build.
fsharp-clean:
	@rm bin$S$(CLEAN_FILES)
