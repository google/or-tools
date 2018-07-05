# ---------- FSharp support using SWIG ----------
.PHONY: help_fsharp # Generate list of F# targets with descriptions.
help_fsharp:
	@echo Use one of the following F# targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

FSHARP_ORTOOLS_DLL_NAME=OrTools.FSharp
FSHARP_ORTOOLS_DLL_TEST=OrTools.FSharp.Test

FSHARP_ORTOOLS_NUSPEC_FILE=$(FSHARP_ORTOOLS_DLL_NAME).nuspec

CLEAN_FILES=Google.$(FSHARP_ORTOOLS_DLL_NAME).* FSharp.Core.dll System.ValueTuple.dll

# Check for required build tools
ifeq ($(SYSTEM), win)
  ifneq ($(PATH_TO_DOTNET_COMPILER),)
  DOTNET_EXECUTABLE := $(PATH_TO_DOTNET_COMPILER)
  else
  DOTNET_COMPILER ?= dotnet.exe
  DOTNET_EXECUTABLE := $(shell $(WHICH) $(DOTNET_COMPILER) 2>nul)
  endif
else # UNIX
  ifneq ($(PATH_TO_DOTNET_COMPILER),)
  DOTNET_EXECUTABLE := $(PATH_TO_DOTNET_COMPILER)
  else
  DOTNET_COMPILER ?= dotnet
  DOTNET_EXECUTABLE := $(shell which $(DOTNET_COMPILER))
  endif
endif

DOTNET_TARGET_FRAMEWORK:=net462

FSHARP_LIB_DIR :=
ifeq ($(PLATFORM),MACOSX)
FSHARP_LIB_DIR = env DYLD_FALLBACK_LIBRARY_PATH=$(LIB_DIR)
endif
ifeq ($(PLATFORM),LINUX)
FSHARP_LIB_DIR = env LD_LIBRARY_PATH=$(LIB_DIR)
endif

.PHONY: fsharp # Build F# OR-Tools.
fsharp: csharp
ifneq ($(DOTNET_EXECUTABLE),)
	$(SED) -i -e "s/0.0.0.0/$(OR_TOOLS_VERSION)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
	"$(DOTNET_EXECUTABLE)" build -f $(DOTNET_TARGET_FRAMEWORK) ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj -o ..$S..$S..$Sbin$S
else
	$(warning Cannot find 'dotnet' command which is needed for build. Please make sure it is installed and in system path.)
endif

.PHONY: test_fsharp # Test F# OR-Tools.
test_fsharp: fsharp
	"$(DOTNET_EXECUTABLE)" restore --packages "ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_TEST)$Spackages" "ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_TEST)$S$(FSHARP_ORTOOLS_DLL_TEST).fsproj"
ifneq ($(SYSTEM),win)
	"$(DOTNET_EXECUTABLE)" clean "ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_TEST)$S$(FSHARP_ORTOOLS_DLL_TEST).fsproj"
	"$(DOTNET_EXECUTABLE)" build "ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_TEST)$S$(FSHARP_ORTOOLS_DLL_TEST).fsproj"
	$(FSHARP_LIB_DIR) "$(DOTNET_EXECUTABLE)" "ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_TEST)$Spackages$Sxunit.runner.console$S2.3.1$Stools$Snetcoreapp2.0$Sxunit.console.dll" "ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_TEST)$Sbin$SDebug$Snetcoreapp2.0$SGoogle.$(FSHARP_ORTOOLS_DLL_TEST).dll" -verbose
else
	$(warning Not compatible.)
endif

.PHONY: nuget-pkg_fsharp # Build Nuget Package for distribution.
nuget-pkg_fsharp: fsharp
	"$(DOTNET_EXECUTABLE)" build -f $(DOTNET_TARGET_FRAMEWORK) -c Release ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj
	$(SED) -i -e "s/MMMM/Google.$(FSHARP_ORTOOLS_DLL_NAME)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_NUSPEC_FILE)
	$(SED) -i -e "s/VVVV/$(OR_TOOLS_VERSION)/" ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_NUSPEC_FILE)
	"$(DOTNET_EXECUTABLE)" pack -c Release ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME)$S$(FSHARP_ORTOOLS_DLL_NAME).fsproj


.PHONY: clean_fsharp # Clean F# output from previous build.
clean_fsharp:
	$(foreach var,$(CLEAN_FILES), $(DEL) bin$S$(var);)

.PHONY: detect_fsharp # Show variables used to build F# OR-Tools.
detect_fsharp:
ifeq ($(SYSTEM),win)
	@echo Relevant info for the F# build:
else
	@echo Relevant info for the F\# build:
endif
	@echo DOTNET_EXECUTABLE = "$(DOTNET_EXECUTABLE)"
	@echo MONO_EXECUTABLE = "$(MONO_EXECUTABLE)"
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
