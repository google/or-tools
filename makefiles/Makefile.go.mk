# ---------- Golang support using SWIG ----------
.PHONY: help_go # Generate list of Go targets with descriptions.
help_go:
	@echo Use one of the following Go targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.go.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\ \2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.go.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\ \2/" | expand -t24
	@echo
endif

# Detect go
GO_BIN = $(shell $(WHICH) go)
GO_PATH = $(shell "$(GO_BIN)" env GOPATH)
GO_OR_TOOLS_NATIVE_LIBS := $(LIB_DIR)/$(LIB_PREFIX)goortools.$(SWIG_GO_LIB_SUFFIX)
SET_GOPATH = CGO_LDFLAGS="-L$(OR_TOOLS_TOP)/lib -lgoortools -v" DYLD_LIBRARY_PATH="$(OR_TOOLS_TOP)/dependencies/install/lib:$(OR_TOOLS_TOP)/lib:"
SWIG_GO_FLAG = -cgo -intgosize 64 -v

HAS_GO = true
ifndef GO_BIN
HAS_GO =
endif
ifndef GO_PATH
HAS_GO =
endif

# Main target
.PHONY: go # Build Go OR-Tools.
.PHONY: test_go # Test Go OR-Tools using various examples.
ifndef HAS_GO
go: detect_go
check_go: go
test_go: go
else
go: go_pimpl
check_go: check_go_pimpl
test_go: test_go_pimpl
BUILT_LANGUAGES +=, Golang
endif

# gowraplp
$(GEN_DIR)/ortools/linear_solver/linear_solver_go_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/go/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(LP_DEPS) \
 | $(GEN_DIR)/ortools/linear_solver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -go \
 -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_go_wrap.cc \
 -package sat -module gowraplp \
 -outdir $(GEN_PATH)$Sortools/linear_solver \
 $(SRC_DIR)$Sortools$Slinear_solver$Sgo$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_go_wrap.$O: \
	$(GEN_DIR)/ortools/linear_solver/linear_solver_go_wrap.cc \
	$(SAT_DEPS) \
	| $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
	-c $(GEN_PATH)$Sortools$Slinar_solver$Slinear_solver_go_wrap.cc \
	$(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_go_wrap.$O

# gowrapsat
$(GEN_DIR)/ortools/sat/cp_model.pb.go: \
 $(SRC_DIR)/ortools/sat/cp_model.proto \
 | $(GEN_DIR)/ortools/sat
	test -f $(GO_PATH)/bin/protoc-gen-go && echo $(GO_PATH)/bin/protoc-gen-go || $(GO_BIN) get -u github.com/golang/protobuf/protoc-gen-go
	$(PROTOC) --proto_path=$(SRC_DIR) --go_out=$(GEN_PATH) \
 $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/ortools/sat/sat_parameters.pb.go: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 | $(GEN_DIR)/ortools/sat
	test -f $(GO_PATH)/bin/protoc-gen-go && echo $(GO_PATH)/bin/protoc-gen-go || $(GO_BIN) get -u github.com/golang/protobuf/protoc-gen-go
	$(PROTOC) --proto_path=$(SRC_DIR) --go_out=$(GEN_PATH) \
 $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

$(GEN_DIR)/ortools/sat/sat_go_wrap.cc: \
 $(SRC_DIR)/ortools/sat/go/sat.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SAT_DEPS) \
 | $(GEN_DIR)/ortools/sat
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -go \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_go_wrap.cc \
 -package sat -module gowrapsat \
 -outdir $(GEN_PATH)$Sortools/sat \
 $(SRC_DIR)$Sortools$Ssat$Sgo$Ssat.i

$(OBJ_DIR)/swig/sat_go_wrap.$O: \
	$(GEN_DIR)/ortools/sat/sat_go_wrap.cc \
	$(SAT_DEPS) \
	| $(OBJ_DIR)/swig
	$(CCC) $(CFLAGS) \
	-c $(GEN_PATH)$Sortools$Ssat$Ssat_go_wrap.cc \
	$(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_go_wrap.$O

$(GO_OR_TOOLS_NATIVE_LIBS): \
 $(OR_TOOLS_LIBS) \
 $(OBJ_DIR)/swig/linear_solver_go_wrap.$O \
 $(OBJ_DIR)/swig/sat_go_wrap.$O
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)goortools.$(SWIG_GO_LIB_SUFFIX) \
 $(OBJ_DIR)$Sswig$Slinear_solver_go_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_go_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

go_pimpl: \
	$(GEN_DIR)/ortools/sat/cp_model.pb.go \
	$(GEN_DIR)/ortools/sat/sat_parameters.pb.go \
	$(GEN_DIR)/ortools/linear_solver/linear_solver_go_wrap.cc \
	$(GEN_DIR)/ortools/sat/sat_go_wrap.cc \
	$(GO_OR_TOOLS_NATIVE_LIBS)
	cd $(GEN_PATH)$Sortools$Slinear_solver && \
 $(SETGO_PATH) "$(GO_BIN)" build
	cd $(GEN_PATH)$Sortools$Ssat && \
 $(SETGO_PATH)  "$(GO_BIN)" build

#####################
##  Golang SOURCE  ##
#####################
ifeq ($(SOURCE_SUFFIX),.go) # Those rules will be used if SOURCE contain a .go file
.PHONY: build # Build a Go program.
build: $(SOURCE) $(GO_OR_TOOLS_NATIVE_LIBS) ;

.PHONY: run # Run a Go program.
run: build
	$(SET_GOPATH) "$(GO_BIN)" $(SOURCE_PATH) $(ARGS)
endif

###############################
##  Golang Examples/Samples  ##
###############################
rgo_%: go_pimpl FORCE
	CGO_LDFLAGS="-L$(OR_TOOLS_TOP)/lib -lgoortools -v" \
 DYLD_LIBRARY_PATH="$(OR_TOOLS_TOP)/dependencies/install/lib:$(OR_TOOLS_TOP)/lib:" \
 "$(GO_BIN)" $*


.PHONY: test_go_linear_solver_samples # Build and Run all Go LP Samples (located in ortools/linear_solver/samples)
test_go_linear_solver_samples: \
 rgo_SimpleLpProgram

.PHONY: test_go_sat_samples # Build and Run all Go SAT Samples (located in ortools/sat/samples)
test_go_sat_samples: \
 rgo_SimpleSatProgram

.PHONY: check_go_pimpl
check_go_pimpl: \
 test_go_linear_solver_samples \
 test_go_sat_samples

.PHONY: test_go_go # Build and Run all Go Examples (located in ortools/examples/go)
test_go_go: ;

.PHONY: test_go_pimpl
test_go_pimpl: \
 check_go_pimpl \
 test_go_go

################
##  Cleaning  ##
################
.PHONY: clean_go # Clean Go output from previous build.
clean_go:
	-$(DELREC) $(GEN_PATH)$Sgo
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*go_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*go_wrap*
	-$(DEL) $(OBJ_DIR)$Sswig$S*_go_wrap.$O
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)goortools.$(SWIG_GO_LIB_SUFFIX)


#############
##  DEBUG  ##
#############
.PHONY: detect_go # Show variables used to build Go OR-Tools.
detect_go:
	@echo Relevant info for the Go build:
	@echo These must resolve to proceed
	@echo GO_BIN = $(GO_BIN)
	@echo GO_PATH = $(GO_PATH)
	@echo PROTOC_GEN_GO = $(PROTOC_GEN_GO)
	@echo GO_OR_TOOLS_NATIVE_LIBS = $(GO_OR_TOOLS_NATIVE_LIBS)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
