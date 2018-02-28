# ---------- FSharp support using SWIG ----------
.PHONY: help_fsharp # Generate list of targets with descriptions.
help_fsharp:
	@echo Use one of the following targets:
ifeq ($(SYSTEM),win)
	@tools\grep.exe "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | tools\sed.exe "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
else
	@grep "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.fsharp.mk | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
endif

BASE_ORTOOLS_DLL_NAME=Google.OrTools
FSHARP_ORTOOLS_DLL_NAME=$(BASE_ORTOOLS_DLL_NAME).FSharp
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

.PHONY: fsharp # Build F# OR-Tools. Set environment variable FSHARP_DEBUG=1 for debug symbols.
fsharp: csharp
ifneq ($(FSHARP_EXECUTABLE),)
	"$(FSHARP_EXECUTABLE)" $(FLAG_PREFIX)target:library $(FLAG_PREFIX)out:bin$S$(FSHARP_ORTOOLS_DLL_NAME).dll $(FLAG_PREFIX)platform:anycpu $(FLAG_PREFIX)nocopyfsharpcore $(FLAG_PREFIX)lib:bin $(FLAG_PREFIX)reference:$(BASE_ORTOOLS_DLL_NAME).dll $(FSHARP_DEBUG) $(FS_SIGNING_FLAGS) ortools$Sfsharp$S$(FSHARP_ORTOOLS_DLL_NAME).fsx
else
	$(warning Cannot find '$(FSHARP_COMPILER)' command which is needed for build. Please make sure it is installed and in system path.)
endif

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
	@echo FSHARP_COMPILER = $(FSHARP_COMPILER)
	@echo FSHARP_EXECUTABLE = "$(FSHARP_EXECUTABLE)"
