BASE_ORTOOLS_DLL_NAME=Google.OrTools
FSHARP_ORTOOLS_DLL_NAME=$(BASE_ORTOOLS_DLL_NAME).FSharp
CLEAN_FILES=$(FSHARP_ORTOOLS_DLL_NAME).*

# Check for required build tools
ifeq ($(SYSTEM), win)
  FSHARP_COMPILER:=fsc
  FLAG_PREFIX:=/
  SEP:=$(strip \\)
else
  FSHARP_COMPILER := fsharpc
  FLAG_PREFIX:=--
  SEP:=/
endif

EXECUTABLES = mono $(FSHARP_COMPILER)
CHECK := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $(exec)),some string,$(error "Cannot find '$(exec)' command which is needed for build)))

# Check whether to build Debug or Release version
ifeq (${DEBUG}, 1)
  DEBUG = --debug
endif

.PHONY: default
default: help

.PHONY: help # Generate list of targets with descriptions.
help:
	$(info Use one of the following targets:)
	@grep "^.PHONY: .* #" Makefile-fsharp | sed "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20


.PHONY: fsharp-ortools # Build F# OR-Tools. Set environment variable DEBUG=1 to also build debug symbols.
fsharp-ortools:
	$(FSHARP_COMPILER) $(FLAG_PREFIX)target:library $(FLAG_PREFIX)out:bin$(SEP)$(FSHARP_ORTOOLS_DLL_NAME).dll $(FLAG_PREFIX)platform:anycpu $(FLAG_PREFIX)lib:bin $(FLAG_PREFIX)reference:$(BASE_ORTOOLS_DLL_NAME).dll $(DEBUG) ortools$(SEP)fsharp$(SEP)$(FSHARP_ORTOOLS_DLL_NAME).fsx


.PHONY: fsharp-clean # Clean output from previous build
clean:
	@rm bin$(SEP)$(CLEAN_FILES)