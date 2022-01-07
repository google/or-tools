# ---------- Java support using SWIG ----------
.PHONY: help_java # Generate list of Java targets with descriptions.
help_java:
	@echo Use one of the following Java targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.java.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.java.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t24
	@echo
endif

# Check for required build tools
HAS_JAVA = true
ifndef JAVAC_BIN
HAS_JAVA =
endif
ifndef JAR_BIN
HAS_JAVA =
endif
ifndef JAVA_BIN
HAS_JAVA =
endif
ifndef MVN_BIN
HAS_JAVA =
endif

TEMP_JAVA_DIR = temp_java
JAVA_ORTOOLS_PACKAGE := com.google.ortools
JAVA_ORTOOLS_JAR := $(LIB_DIR)/$(JAVA_ORTOOLS_PACKAGE)$J
JAVA_ORTOOLS_NATIVE_LIBS := $(LIB_DIR)/$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT)
JAVAFLAGS := -Djava.library.path=$(LIB_DIR)

# Main target
.PHONY: java_runtime # Build Java OR-Tools runtime.
.PHONY: java # Build Java OR-Tools.
.PHONY: test_java # Test Java OR-Tools using various examples.
.PHONY: package_java # Create Java OR-Tools maven package.
.PHONY: publish_java_runtime # Publish Java OR-Tools runtime maven package to oss.sonatype.org.
.PHONY: publish_java # Publish Java OR-Tools maven package to oss.sonatype.org.
ifndef HAS_JAVA
java_runtime: java
java:
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo MVN_BIN = $(MVN_BIN)
	$(warning Cannot find 'java' or 'maven' command which is needed for build. \
 Please make sure it is installed and in system path. \
 Check Makefile.local for more information.)
check_java: java
test_java: java
publish_java_runtime: java
publish_java: java
else
java_runtime: java_runtime_pimpl
java: java_pimpl
check_java: check_java_pimpl
test_java: test_java_pimpl
publish_java_runtime: publish_java_runtime_pimpl
publish_java: publish_java_pimpl
BUILT_LANGUAGES +=, Java
endif
package_java: java

# Detect RuntimeIDentifier
ifeq ($(OS),Windows)
JAVA_NATIVE_IDENTIFIER=win32-x86-64
else
  ifeq ($(OS),Linux)
  JAVA_NATIVE_IDENTIFIER=linux-x86-64
  else
    ifeq ($(OS),Darwin)
    JAVA_NATIVE_IDENTIFIER=darwin-x86-64
    else
    $(error OS unknown !)
    endif
  endif
endif

# All libraries and dependencies
JAVA_ORTOOLS_NATIVE_PROJECT := ortools-$(JAVA_NATIVE_IDENTIFIER)
JAVA_ORTOOLS_PROJECT := ortools-java
JAVA_NATIVE_PATH := $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Ssrc$Smain
JAVA_PATH := $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Ssrc$Smain

$(GEN_DIR)/java/com/google/ortools/algorithms:
	-$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Salgorithms

$(GEN_DIR)/java/com/google/ortools/constraintsolver:
	-$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sconstraintsolver

$(GEN_DIR)/java/com/google/ortools/graph:
	-$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sgraph

$(GEN_DIR)/java/com/google/ortools/linearsolver:
	-$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Slinearsolver

$(GEN_DIR)/java/com/google/ortools/flatzinc:
	-$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sflatzinc

$(GEN_DIR)/java/com/google/ortools/init:
	$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sinit

$(GEN_DIR)/java/com/google/ortools/sat:
	-$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Ssat

$(GEN_DIR)/java/com/google/ortools/util:
	$(MKDIR_P) $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sutil

$(CLASS_DIR):
	-$(MKDIR_P) $(CLASS_DIR)

$(CLASS_DIR)/com: | $(CLASS_DIR)
	-$(MKDIR) $(CLASS_DIR)$Scom

$(CLASS_DIR)/com/google: | $(CLASS_DIR)/com
	-$(MKDIR) $(CLASS_DIR)$Scom$Sgoogle

$(CLASS_DIR)/com/google/ortools: | $(CLASS_DIR)/com/google
	-$(MKDIR) $(CLASS_DIR)$Scom$Sgoogle$Sortools

################
##  JAVA JNI  ##
################
$(GEN_DIR)/ortools/constraint_solver/constraint_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/constraint_solver/java/constraint_solver.i \
 $(SRC_DIR)/ortools/constraint_solver/java/routing.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/proto.i \
 $(CP_DEPS) \
 $(SRC_DIR)/ortools/constraint_solver/routing.h \
 | $(GEN_DIR)/ortools/constraint_solver $(GEN_DIR)/java/com/google/ortools/constraintsolver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_java_wrap.cc \
 -package com.google.ortools.constraintsolver \
 -module main \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sconstraintsolver \
 $(SRC_DIR)$Sortools$Sconstraint_solver$Sjava$Srouting.i

$(OBJ_DIR)/swig/constraint_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/constraint_solver/constraint_solver_java_wrap.cc \
 $(CP_DEPS) \
 $(SRC_DIR)/ortools/constraint_solver/routing.h \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Sconstraint_solver$Sconstraint_solver_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O

$(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/algorithms/java/knapsack_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(SRC_DIR)/ortools/algorithms/knapsack_solver.h \
 | $(GEN_DIR)/ortools/algorithms $(GEN_DIR)/java/com/google/ortools/algorithms
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_java_wrap.cc \
 -package com.google.ortools.algorithms \
 -module main \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Salgorithms \
 $(SRC_DIR)$Sortools$Salgorithms$Sjava$Sknapsack_solver.i

$(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/algorithms/knapsack_solver_java_wrap.cc \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Salgorithms$Sknapsack_solver_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sknapsack_solver_java_wrap.$O

$(GEN_DIR)/ortools/graph/graph_java_wrap.cc: \
 $(SRC_DIR)/ortools/graph/java/graph.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(GRAPH_DEPS) \
 | $(GEN_DIR)/ortools/graph $(GEN_DIR)/java/com/google/ortools/graph
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Sgraph$Sgraph_java_wrap.cc \
 -package com.google.ortools.graph \
 -module main \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sgraph \
 $(SRC_DIR)$Sortools$Sgraph$Sjava$Sgraph.i

$(OBJ_DIR)/swig/graph_java_wrap.$O: \
 $(GEN_DIR)/ortools/graph/graph_java_wrap.cc \
 $(GRAPH_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Sgraph$Sgraph_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sgraph_java_wrap.$O

$(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc: \
 $(SRC_DIR)/ortools/linear_solver/java/linear_solver.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SRC_DIR)/ortools/util/java/vector.i \
 $(LP_DEPS) \
 | $(GEN_DIR)/ortools/linear_solver $(GEN_DIR)/java/com/google/ortools/linearsolver
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_java_wrap.cc \
 -package com.google.ortools.linearsolver \
 -module main_research_linear_solver \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Slinearsolver \
 $(SRC_DIR)$Sortools$Slinear_solver$Sjava$Slinear_solver.i

$(OBJ_DIR)/swig/linear_solver_java_wrap.$O: \
 $(GEN_DIR)/ortools/linear_solver/linear_solver_java_wrap.cc \
 $(LP_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Slinear_solver$Slinear_solver_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Slinear_solver_java_wrap.$O

$(GEN_DIR)/ortools/sat/sat_java_wrap.cc: \
 $(SRC_DIR)/ortools/sat/java/sat.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(SAT_DEPS) \
 | $(GEN_DIR)/ortools/sat $(GEN_DIR)/java/com/google/ortools/sat
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java \
 -o $(GEN_PATH)$Sortools$Ssat$Ssat_java_wrap.cc \
 -package com.google.ortools.sat \
 -module main \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Ssat \
 $(SRC_DIR)$Sortools$Ssat$Sjava$Ssat.i

$(OBJ_DIR)/swig/sat_java_wrap.$O: \
 $(GEN_DIR)/ortools/sat/sat_java_wrap.cc \
 $(SAT_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Ssat$Ssat_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Ssat_java_wrap.$O

$(GEN_DIR)/ortools/init/init_java_wrap.cc: \
 $(SRC_DIR)/ortools/init/java/init.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(INIT_DEPS) \
 | $(GEN_DIR)/ortools/init $(GEN_DIR)/java/com/google/ortools/init
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Sinit$Sinit_java_wrap.cc \
 -package com.google.ortools.init \
 -module main \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sinit \
 $(SRC_DIR)$Sortools$Sinit$Sjava$Sinit.i

$(OBJ_DIR)/swig/init_java_wrap.$O: \
 $(GEN_DIR)/ortools/init/init_java_wrap.cc \
 $(INIT_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Sinit$Sinit_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sinit_java_wrap.$O

$(GEN_DIR)/ortools/util/util_java_wrap.cc: \
 $(SRC_DIR)/ortools/util/java/sorted_interval_list.i \
 $(SRC_DIR)/ortools/base/base.i \
 $(UTIL_DEPS) \
 | $(GEN_DIR)/ortools/util $(GEN_DIR)/java/com/google/ortools/util
	$(SWIG_BINARY) $(SWIG_INC) -I$(INC_DIR) -c++ -java $(SWIG_DOXYGEN) \
 -o $(GEN_PATH)$Sortools$Sutil$Sutil_java_wrap.cc \
 -package com.google.ortools.util \
 -module main \
 -outdir $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sutil \
 $(SRC_DIR)$Sortools$Sutil$Sjava$Ssorted_interval_list.i

$(OBJ_DIR)/swig/util_java_wrap.$O: \
 $(GEN_DIR)/ortools/util/util_java_wrap.cc \
 $(UTIL_DEPS) \
 | $(OBJ_DIR)/swig
	$(CCC) $(JNIFLAGS) $(JAVA_INC) \
 -c $(GEN_PATH)$Sortools$Sutil$Sutil_java_wrap.cc \
 $(OBJ_OUT)$(OBJ_DIR)$Sswig$Sutil_java_wrap.$O

$(JAVA_ORTOOLS_NATIVE_LIBS): \
 $(OR_TOOLS_LIBS) \
 $(OBJ_DIR)/swig/constraint_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/graph_java_wrap.$O \
 $(OBJ_DIR)/swig/linear_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/init_java_wrap.$O \
 $(OBJ_DIR)/swig/sat_java_wrap.$O \
 $(OBJ_DIR)/swig/util_java_wrap.$O
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT) \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Slinear_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sinit_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Ssat_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sutil_java_wrap.$O \
 $(OR_TOOLS_LNK) \
 $(OR_TOOLS_LDFLAGS)

############
##  JAVA  ##
############
$(LIB_DIR)/protobuf.jar: \
 dependencies/install/lib/protobuf.jar \
 | $(LIB_DIR)
	$(COPY) dependencies$Sinstall$Slib$Sprotobuf.jar $(LIB_DIR)

$(GEN_DIR)/java/com/google/ortools/constraintsolver/SearchLimitProtobuf.java: \
 $(SRC_DIR)/ortools/constraint_solver/search_limit.proto \
 | $(GEN_DIR)/java/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Sconstraint_solver$Ssearch_limit.proto

$(GEN_DIR)/java/com/google/ortools/constraintsolver/SolverParameters.java: \
 $(SRC_DIR)/ortools/constraint_solver/solver_parameters.proto \
 | $(GEN_DIR)/java/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Sconstraint_solver$Ssolver_parameters.proto

$(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingParameters.java: \
 $(SRC_DIR)/ortools/constraint_solver/routing_parameters.proto \
 | $(GEN_DIR)/java/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_parameters.proto

$(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingEnums.java: \
 $(SRC_DIR)/ortools/constraint_solver/routing_enums.proto \
 | $(GEN_DIR)/java/com/google/ortools/constraintsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Sconstraint_solver$Srouting_enums.proto

$(GEN_DIR)/java/com/google/ortools/linearsolver/MPModelProto.java: \
 $(SRC_DIR)/ortools/linear_solver/linear_solver.proto \
 | $(GEN_DIR)/java/com/google/ortools/linearsolver
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Slinear_solver$Slinear_solver.proto

$(GEN_DIR)/java/com/google/ortools/sat/CpModelProto.java: \
 $(SRC_DIR)/ortools/sat/cp_model.proto \
 | $(GEN_DIR)/java/com/google/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Ssat$Scp_model.proto

$(GEN_DIR)/java/com/google/ortools/sat/SatParameters.java: \
 $(SRC_DIR)/ortools/sat/sat_parameters.proto \
 | $(GEN_DIR)/java/com/google/ortools/sat
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Ssat$Ssat_parameters.proto

$(GEN_DIR)/java/com/google/ortools/util/OptionalBoolean.java: \
 $(SRC_DIR)/ortools/util/optional_boolean.proto \
 | $(GEN_DIR)/java/com/google/ortools/util
	$(PROTOC) --proto_path=$(SRC_DIR) --java_out=$(GEN_PATH)$Sjava $(SRC_DIR)$Sortools$Sutil$Soptional_boolean.proto

$(JAVA_ORTOOLS_JAR): \
 $(JAVA_ORTOOLS_NATIVE_LIBS) \
 $(LIB_DIR)/protobuf.jar \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/SolverParameters.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/SearchLimitProtobuf.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingParameters.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingEnums.java \
 $(GEN_DIR)/java/com/google/ortools/linearsolver/MPModelProto.java \
 $(GEN_DIR)/java/com/google/ortools/sat/CpModelProto.java \
 $(GEN_DIR)/java/com/google/ortools/sat/SatParameters.java \
 $(GEN_DIR)/java/com/google/ortools/util/OptionalBoolean.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/Loader.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/IntIntToLongFunction.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/JavaDecisionBuilder.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/LongTernaryOperator.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/LongTernaryPredicate.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Constant.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Constraint.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/CpModel.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/CpSolver.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/CpSolverSolutionCallback.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Difference.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/IntVar.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/IntervalVar.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/LinearExpr.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Literal.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/NotBooleanVariable.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/ScalProd.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Sum.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/SumOfVariables.java \
 | $(CLASS_DIR)/com/google/ortools
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR) \
 -cp $(LIB_DIR)$Sprotobuf.jar \
 $(SRC_DIR)$Sortools$Sjava$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java \
 $(SRC_DIR)$Sortools$Sjava$Scom$Sgoogle$Sortools$Ssat$S*.java \
 $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sconstraintsolver$S*.java \
 $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Ssat$S*.java \
 $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sutil$S*.java \
 $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Salgorithms$S*.java \
 $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Sgraph$S*.java \
 $(GEN_PATH)$Sjava$Scom$Sgoogle$Sortools$Slinearsolver$S*.java
	"$(JAR_BIN)" cvf $(LIB_DIR)$Scom.google.ortools.jar -C $(CLASS_DIR) com$Sgoogle$Sortools$S

###################
##  Java SOURCE  ##
###################
ifeq ($(SOURCE_SUFFIX),.java) # Those rules will be used if SOURCE contain a .java file

SOURCE_PROJECT_DIR := $(subst /com/google/ortools,, $(SOURCE))
SOURCE_PROJECT_DIR := $(subst /src/main/java,, $(SOURCE_PROJECT_DIR))
SOURCE_PROJECT_DIR := $(subst /$(SOURCE_NAME).java,, $(SOURCE_PROJECT_DIR))
SOURCE_PROJECT_PATH = $(subst /,$S,$(SOURCE_PROJECT_DIR))

.PHONY: build # Build a Maven java program.
build: $(SOURCE_PROJECT_DIR)/pom.xml $(SOURCE)
	cd $(SOURCE_PROJECT_PATH) && "$(MVN_BIN)" compile -B

.PHONY: run # Run a Maven Java program.
run: build
	cd $(SOURCE_PROJECT_PATH) && "$(MVN_BIN)" exec:java $(ARGS)
endif

ifneq ($(ORTOOLS_TOKEN),)
GPG_SIGN=
else
GPG_SIGN= -Dgpg.skip=true
endif

GPG_ARGS ?= <arg>--pinentry-mode</arg><arg>loopback</arg>

###################
## Maven package ##
###################
$(TEMP_JAVA_DIR):
	$(MKDIR) $(TEMP_JAVA_DIR)

#########################
# Java Runtime Package ##
#########################
$(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT): | $(TEMP_JAVA_DIR)
	$(MKDIR) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)

$(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT)/pom.xml: \
 ${SRC_DIR}/ortools/java/pom-native.xml.in \
 | $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT)
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sjava$Spom-native.xml.in \
 > $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Spom.xml
	$(SED) -i -e 's/@JAVA_PACKAGE@/$(JAVA_ORTOOLS_PACKAGE)/' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Spom.xml
	$(SED) -i -e 's/@JAVA_NATIVE_PROJECT@/$(JAVA_ORTOOLS_NATIVE_PROJECT)/' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Spom.xml
	$(SED) -i -e 's;@GPG_ARGS@;$(GPG_ARGS);' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Spom.xml

.PHONY: java_runtime_pimpl
java_runtime_pimpl: $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT)/timestamp

$(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT)/timestamp: \
 $(JAVA_ORTOOLS_NATIVE_LIBS) \
 $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT)/pom.xml
	$(MKDIR_P) $(JAVA_NATIVE_PATH)$Sresources$S$(JAVA_ORTOOLS_NATIVE_PROJECT)
	$(COPY) $(subst /,$S,$(JAVA_ORTOOLS_NATIVE_LIBS)) $(JAVA_NATIVE_PATH)$Sresources$S$(JAVA_ORTOOLS_NATIVE_PROJECT)
ifeq ($(SYSTEM),unix)
	$(COPY) $(OR_TOOLS_LIBS) $(JAVA_NATIVE_PATH)$Sresources$S$(JAVA_ORTOOLS_NATIVE_PROJECT)
endif
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT) && "$(MVN_BIN)" compile -B
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT) && "$(MVN_BIN)" package -B
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT) && "$(MVN_BIN)" install -B $(GPG_SIGN)
	$(TOUCH) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Stimestamp

####################
##  JAVA Package  ##
####################
ifeq ($(UNIVERSAL_JAVA_PACKAGE),1)
JAVA_ORTOOLS_POM=pom-full.xml.in
else
JAVA_ORTOOLS_POM=pom-local.xml.in
endif

$(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_PROJECT): | $(TEMP_JAVA_DIR)
	$(MKDIR) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)

$(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_PROJECT)/pom.xml: \
 ${SRC_DIR}/ortools/java/$(JAVA_ORTOOLS_POM) \
 | $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_PROJECT)
	$(SED) -e "s/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/" \
 ortools$Sjava$S$(JAVA_ORTOOLS_POM) \
 > $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Spom.xml
	$(SED) -i -e 's/@JAVA_PACKAGE@/$(JAVA_ORTOOLS_PACKAGE)/' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Spom.xml
	$(SED) -i -e 's/@JAVA_NATIVE_PROJECT@/$(JAVA_ORTOOLS_NATIVE_PROJECT)/' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Spom.xml
	$(SED) -i -e 's/@JAVA_PROJECT@/$(JAVA_ORTOOLS_PROJECT)/' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Spom.xml
	$(SED) -i -e 's;@GPG_ARGS@;$(GPG_ARGS);' \
 $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Spom.xml

.PHONY: java_pimpl
java_pimpl: $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_PROJECT)/timestamp

$(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_PROJECT)/timestamp: \
 $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_NATIVE_PROJECT)/timestamp \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/SolverParameters.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/SearchLimitProtobuf.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingParameters.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingEnums.java \
 $(GEN_DIR)/java/com/google/ortools/linearsolver/MPModelProto.java \
 $(GEN_DIR)/java/com/google/ortools/sat/CpModelProto.java \
 $(GEN_DIR)/java/com/google/ortools/sat/SatParameters.java \
 $(GEN_DIR)/java/com/google/ortools/util/OptionalBoolean.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/Loader.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/IntIntToLongFunction.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/JavaDecisionBuilder.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/LongTernaryOperator.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/constraintsolver/LongTernaryPredicate.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Constraint.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/CpModel.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/CpSolver.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/CpSolverSolutionCallback.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Difference.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/IntVar.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/IntervalVar.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/LinearExpr.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/Literal.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/NotBooleanVariable.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/ScalProd.java \
 $(SRC_DIR)/ortools/java/com/google/ortools/sat/SumOfVariables.java \
 $(TEMP_JAVA_DIR)/$(JAVA_ORTOOLS_PROJECT)/pom.xml
	$(MKDIR_P) $(JAVA_PATH)$Sjava
ifeq ($(SYSTEM),unix)
	$(COPYREC) $(SRC_DIR)$Sortools$Sjava$Scom $(GEN_PATH)$Sjava$Scom $(JAVA_PATH)$Sjava
else
	$(COPYREC) /E /I $(SRC_DIR)$Sortools$Sjava$Scom $(JAVA_PATH)$Sjava$Scom
	$(COPYREC) /E /I $(GEN_PATH)$Sjava$Scom $(JAVA_PATH)$Sjava$Scom
endif
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT) && "$(MVN_BIN)" compile -B
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT) && "$(MVN_BIN)" package -B
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT) && "$(MVN_BIN)" install -B $(GPG_SIGN)
	$(TOUCH) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Stimestamp

#############################
##  Java Examples/Samples  ##
#############################
JAVA_SRC_DIR := src/main/java/com/google/ortools
JAVA_SRC_PATH := $(subst /,$S,$(JAVA_SRC_DIR))

JAVA_TEST_DIR := src/test/java/com/google/ortools
JAVA_TEST_PATH := $(subst /,$S,$(JAVA_TEST_DIR))

JAVA_SAMPLES := algorithms graph constraint_solver linear_solver sat

define java-sample-target =
$$(TEMP_JAVA_DIR)/$1: | $$(TEMP_JAVA_DIR)
	-$$(MKDIR) $$(TEMP_JAVA_DIR)$$S$1

$$(TEMP_JAVA_DIR)/$1/%: \
 $$(SRC_DIR)/ortools/$1/samples/%.java \
 | $$(TEMP_JAVA_DIR)/$1
	-$$(MKDIR) $$(TEMP_JAVA_DIR)$$S$1$$S$$*

$$(TEMP_JAVA_DIR)/$1/%/pom.xml: \
 $${SRC_DIR}/ortools/java/pom-sample.xml.in \
 | $$(TEMP_JAVA_DIR)/$1/%
	$$(SED) -e "s/@JAVA_PACKAGE@/$$(JAVA_ORTOOLS_PACKAGE)/" \
 ortools$$Sjava$$Spom-sample.xml.in \
 > $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@JAVA_SAMPLE_PROJECT@/$$*/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@JAVA_MAIN_CLASS@/com.google.ortools.$2.samples.$$*/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION@/$$(OR_TOOLS_VERSION)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$$(OR_TOOLS_MAJOR)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$$(OR_TOOLS_MINOR)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$$(GIT_REVISION)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@JAVA_PROJECT@/$$(JAVA_ORTOOLS_PROJECT)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml

$$(TEMP_JAVA_DIR)/$1/%/$$(JAVA_SRC_DIR)/%.java: \
 $$(SRC_DIR)/ortools/$1/samples/%.java \
 | $$(TEMP_JAVA_DIR)/$1/%
	$$(MKDIR_P) $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(SRC_DIR)$$Sortools$$S$1$$Ssamples$$S$$*.java \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$S$$(JAVA_SRC_PATH)

rjava_%: \
 java \
 $$(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $$(TEMP_JAVA_DIR)/$1/%/$$(JAVA_SRC_DIR)/%.java \
 FORCE
	cd $$(TEMP_JAVA_DIR)$$S$1$$S$$* && "$$(MVN_BIN)" compile -B
	cd $$(TEMP_JAVA_DIR)$$S$1$$S$$* && "$$(MVN_BIN)" exec:java $$(ARGS)
endef

$(foreach sample,$(JAVA_SAMPLES),$(eval $(call java-sample-target,$(sample),$(subst _,,$(sample)))))

JAVA_EXAMPLES := contrib java

define java-example-target =
$$(TEMP_JAVA_DIR)/$1: | $$(TEMP_JAVA_DIR)
	-$$(MKDIR) $$(TEMP_JAVA_DIR)$$S$1

$$(TEMP_JAVA_DIR)/$1/%: \
 $$(SRC_DIR)/examples/$1/%.java \
 | $$(TEMP_JAVA_DIR)/$1
	-$$(MKDIR) $$(TEMP_JAVA_DIR)$$S$1$$S$$*

$$(TEMP_JAVA_DIR)/$1/%/pom.xml: \
 $${SRC_DIR}/ortools/java/pom-sample.xml.in \
 | $$(TEMP_JAVA_DIR)/$1/%
	$$(SED) -e "s/@JAVA_PACKAGE@/$$(JAVA_ORTOOLS_PACKAGE)/" \
 ortools$$Sjava$$Spom-sample.xml.in \
 > $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@JAVA_SAMPLE_PROJECT@/$$*/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@JAVA_MAIN_CLASS@/com.google.ortools.$1.$$*/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION@/$$(OR_TOOLS_VERSION)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$$(OR_TOOLS_MAJOR)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$$(OR_TOOLS_MINOR)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$$(GIT_REVISION)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml
	$$(SED) -i -e 's/@JAVA_PROJECT@/$$(JAVA_ORTOOLS_PROJECT)/' \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml

$$(TEMP_JAVA_DIR)/$1/%/$$(JAVA_SRC_DIR)/%.java: \
 $$(SRC_DIR)/examples/$1/%.java \
 | $$(TEMP_JAVA_DIR)/$1/%
	$$(MKDIR_P) $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(SRC_DIR)$$Sexamples$$S$1$$S$$*.java \
 $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$S$$(JAVA_SRC_PATH)

rjava_%: \
 java \
 $$(TEMP_JAVA_DIR)/$1/%/pom.xml \
 $$(TEMP_JAVA_DIR)/$1/%/$$(JAVA_SRC_DIR)/%.java \
 FORCE
	cd $$(TEMP_JAVA_DIR)$$S$1$$S$$* && "$$(MVN_BIN)" compile -B
	cd $$(TEMP_JAVA_DIR)$$S$1$$S$$* && "$$(MVN_BIN)" exec:java $$(ARGS)
endef

$(foreach example,$(JAVA_EXAMPLES),$(eval $(call java-example-target,$(example))))

JAVA_TESTS := tests

$(TEMP_JAVA_DIR)/tests: | $(TEMP_JAVA_DIR)
	-$(MKDIR) $(TEMP_JAVA_DIR)$Stests

$(TEMP_JAVA_DIR)/tests/%: \
 $(SRC_DIR)/examples/tests/%.java \
 | $(TEMP_JAVA_DIR)/tests
	-$(MKDIR) $(TEMP_JAVA_DIR)$Stests$S$*

$(TEMP_JAVA_DIR)/tests/%/pom.xml: \
 ${SRC_DIR}/ortools/java/pom-test.xml.in \
 | $(TEMP_JAVA_DIR)/tests/%
	$(SED) -e "s/@JAVA_PACKAGE@/$(JAVA_ORTOOLS_PACKAGE)/" \
 ortools$Sjava$Spom-test.xml.in \
 > $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml
	$(SED) -i -e 's/@JAVA_TEST_PROJECT@/$*/' \
 $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' \
 $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MAJOR@/$(OR_TOOLS_MAJOR)/' \
 $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_MINOR@/$(OR_TOOLS_MINOR)/' \
 $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml
	$(SED) -i -e 's/@PROJECT_VERSION_PATCH@/$(GIT_REVISION)/' \
 $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml
	$(SED) -i -e 's/@JAVA_PROJECT@/$(JAVA_ORTOOLS_PROJECT)/' \
 $(TEMP_JAVA_DIR)$Stests$S$*$Spom.xml

$(TEMP_JAVA_DIR)/tests/%/$(JAVA_TEST_DIR)/%.java: \
 $(SRC_DIR)/examples/tests/%.java \
 | $(TEMP_JAVA_DIR)/tests/%
	$(MKDIR_P) $(TEMP_JAVA_DIR)$Stests$S$*$S$(JAVA_TEST_PATH)
	$(COPY) $(SRC_DIR)$Sexamples$Stests$S$*.java \
 $(TEMP_JAVA_DIR)$Stests$S$*$S$(JAVA_TEST_PATH)

rjava_%: \
 java \
 $(TEMP_JAVA_DIR)/tests/%/pom.xml \
 $(TEMP_JAVA_DIR)/tests/%/$(JAVA_TEST_DIR)/%.java \
 FORCE
	cd $(TEMP_JAVA_DIR)$Stests$S$* && "$(MVN_BIN)" compile -B
	cd $(TEMP_JAVA_DIR)$Stests$S$* && "$(MVN_BIN)" test $(ARGS)

#############################
##  Java Examples/Samples  ##
#############################
.PHONY: test_java_algorithms_samples # Build and Run all Java Algorithms Samples (located in ortools/algorithms/samples)
test_java_algorithms_samples: \
 rjava_Knapsack

.PHONY: test_java_constraint_solver_samples # Build and Run all Java CP Samples (located in ortools/constraint_solver/samples)
test_java_constraint_solver_samples: \
 rjava_SimpleCpProgram \
 rjava_SimpleRoutingProgram \
 rjava_Tsp \
 rjava_TspCities \
 rjava_TspCircuitBoard \
 rjava_TspDistanceMatrix \
 rjava_Vrp \
 rjava_VrpBreaks \
 rjava_VrpCapacity \
 rjava_VrpDropNodes \
 rjava_VrpGlobalSpan \
 rjava_VrpInitialRoutes \
 rjava_VrpPickupDelivery \
 rjava_VrpPickupDeliveryFifo \
 rjava_VrpPickupDeliveryLifo \
 rjava_VrpResources \
 rjava_VrpStartsEnds \
 rjava_VrpTimeWindows \
 rjava_VrpWithTimeLimit

.PHONY: test_java_graph_samples # Build and Run all Java Graph Samples (located in ortools/graph/samples)
test_java_graph_samples: \

.PHONY: test_java_linear_solver_samples # Build and Run all Java LP Samples (located in ortools/linear_solver/samples)
test_java_linear_solver_samples: \
 rjava_AssignmentMip \
 rjava_BasicExample \
 rjava_BinPackingMip \
 rjava_LinearProgrammingExample \
 rjava_MipVarArray \
 rjava_MultipleKnapsackMip \
 rjava_SimpleLpProgram \
 rjava_SimpleMipProgram \
 rjava_StiglerDiet

.PHONY: test_java_sat_samples # Build and Run all Java SAT Samples (located in ortools/sat/samples)
test_java_sat_samples: \
 rjava_AssignmentSat \
 rjava_AssumptionsSampleSat \
 rjava_BinPackingProblemSat \
 rjava_BoolOrSampleSat \
 rjava_ChannelingSampleSat \
 rjava_CpIsFunSat \
 rjava_EarlinessTardinessCostSampleSat \
 rjava_IntervalSampleSat \
 rjava_LiteralSampleSat \
 rjava_NoOverlapSampleSat \
 rjava_OptionalIntervalSampleSat \
 rjava_RabbitsAndPheasantsSat \
 rjava_RankingSampleSat \
 rjava_ReifiedSampleSat \
 rjava_SearchForAllSolutionsSampleSat \
 rjava_SimpleSatProgram \
 rjava_SolveAndPrintIntermediateSolutionsSampleSat \
 rjava_SolveWithTimeLimitSampleSat \
 rjava_SolutionHintingSampleSat \
 rjava_StepFunctionSampleSat \
 rjava_StopAfterNSolutionsSampleSat

.PHONY: check_java_pimpl
check_java_pimpl: \
 test_java_algorithms_samples \
 test_java_constraint_solver_samples \
 test_java_graph_samples \
 test_java_linear_solver_samples \
 test_java_sat_samples \
 \
 rjava_LinearProgramming \
 rjava_IntegerProgramming

.PHONY: test_java_tests # Build and Run all Java Tests (located in examples/tests)
test_java_tests: \
 rjava_KnapsackSolverTest \
 rjava_FlowTest \
 rjava_LinearSolverTest \
 rjava_ConstraintSolverTest \
 rjava_RoutingSolverTest \
 rjava_LinearExprTest \
 rjava_CpModelTest \
 rjava_CpSolverTest \
 rjava_SatSolverTest \

.PHONY: test_java_contrib # Build and Run all Java Contrib (located in examples/contrib)
test_java_contrib: \
 rjava_AllDifferentExcept0 \
 rjava_AllInterval \
 rjava_Circuit \
 rjava_CoinsGridMIP \
 rjava_ColoringMIP \
 rjava_CoveringOpl \
 rjava_Crossword \
 rjava_DeBruijn \
 rjava_Diet \
 rjava_DietMIP \
 rjava_DivisibleBy9Through1 \
 rjava_GolombRuler \
 rjava_KnapsackMIP \
 rjava_LeastDiff \
 rjava_MagicSquare \
 rjava_Map2 \
 rjava_Map \
 rjava_Minesweeper \
 rjava_MultiThreadTest \
 rjava_NQueens2 \
 rjava_NQueens \
 rjava_Partition \
 rjava_QuasigroupCompletion \
 rjava_SendMoreMoney2 \
 rjava_SendMoreMoney \
 rjava_SendMostMoney \
 rjava_Seseman \
 rjava_SetCovering2 \
 rjava_SetCovering3 \
 rjava_SetCovering4 \
 rjava_SetCoveringDeployment \
 rjava_SetCovering \
 rjava_SimpleRoutingTest \
 rjava_StableMarriage \
 rjava_StiglerMIP \
 rjava_Strimko2 \
 rjava_Sudoku \
 rjava_SurvoPuzzle \
 rjava_ToNum \
 rjava_WhoKilledAgatha \
 rjava_Xkcd \
 rjava_YoungTableaux

.PHONY: test_java_java # Build and Run all Java Examples (located in ortools/examples/java)
test_java_java: \
 rjava_CapacitatedVehicleRoutingProblemWithTimeWindows \
 rjava_FlowExample \
 rjava_IntegerProgramming \
 rjava_LinearAssignmentAPI \
 rjava_LinearProgramming \
 rjava_RabbitsPheasants \
 rjava_RandomTsp

.PHONY: test_java_pimpl
test_java_pimpl: \
 check_java_pimpl \
 test_java_tests \
 test_java_contrib \
 test_java_java

.PHONY: publish_java_runtime_pimpl
publish_java_runtime_pimpl: java_runtime
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT) && "$(MVN_BIN)" deploy

.PHONY: publish_java_pimpl
publish_java_pimpl: java
	cd $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT) && "$(MVN_BIN)" deploy

#######################
##  EXAMPLE ARCHIVE  ##
#######################
TEMP_JAVA_DIR=temp_java

$(TEMP_JAVA_DIR)/ortools_examples: | $(TEMP_JAVA_DIR)
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples

$(TEMP_JAVA_DIR)/ortools_examples/examples: | $(TEMP_JAVA_DIR)/ortools_examples
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples

$(TEMP_JAVA_DIR)/ortools_examples/examples/java: | $(TEMP_JAVA_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sjava

$(TEMP_JAVA_DIR)/ortools_examples/examples/data: | $(TEMP_JAVA_DIR)/ortools_examples/examples
	$(MKDIR) $(TEMP_JAVA_DIR)$Sortools_examples$Sexamples$Sdata

define java-sample-archive =
$$(TEMP_JAVA_DIR)/ortools_examples/examples/java/%/pom.xml: \
 $$(TEMP_JAVA_DIR)/$1/%/pom.xml \
 ortools/$1/samples/%.java \
 | $$(TEMP_JAVA_DIR)/ortools_examples/examples/java
	-$$(MKDIR_P) $$(TEMP_JAVA_DIR)$$Sortools_examples$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(SRC_DIR)$$Sortools$$S$1$$Ssamples$$S$$*.java \
 $$(TEMP_JAVA_DIR)$$Sortools_examples$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml \
 $$(TEMP_JAVA_DIR)$$Sortools_examples$$Sexamples$$Sjava$$S$$*
endef

$(foreach sample,$(JAVA_SAMPLES),$(eval $(call java-sample-archive,$(sample))))

define java-example-archive =
$$(TEMP_JAVA_DIR)/ortools_examples/examples/java/%/pom.xml: \
 $$(TEMP_JAVA_DIR)/$1/%/pom.xml \
 examples/$1/%.java \
 | $$(TEMP_JAVA_DIR)/ortools_examples/examples/java
	-$$(MKDIR_P) $$(TEMP_JAVA_DIR)$$Sortools_examples$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(SRC_DIR)$$Sexamples$$S$1$$S$$*.java \
 $$(TEMP_JAVA_DIR)$$Sortools_examples$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml \
 $$(TEMP_JAVA_DIR)$$Sortools_examples$$Sexamples$$Sjava$$S$$*
endef

$(foreach example,$(JAVA_EXAMPLES),$(eval $(call java-example-archive,$(example))))

SAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_JAVA_DIR)/ortools_examples/examples/java/,$(basename $(notdir $(wildcard ortools/*/samples/*.java)))))

EXAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_JAVA_DIR)/ortools_examples/examples/java/,$(basename $(notdir $(wildcard examples/contrib/*.java))))) \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_JAVA_DIR)/ortools_examples/examples/java/,$(basename $(notdir $(wildcard examples/java/*.java)))))

.PHONY: java_examples_archive # Build stand-alone C++ examples archive file for redistribution.
java_examples_archive: \
 $(SAMPLE_JAVA_FILES) \
 $(EXAMPLE_JAVA_FILES) \
 | $(TEMP_JAVA_DIR)/ortools_examples/examples/java
	$(COPY) tools$SREADME.java.md $(TEMP_JAVA_DIR)$Sortools_examples$SREADME.md
	$(COPY) LICENSE-2.0.txt $(TEMP_JAVA_DIR)$Sortools_examples
ifeq ($(SYSTEM),win)
	cd $(TEMP_JAVA_DIR) \
 && ..\$(ZIP) \
 -r ..\or-tools_java_examples_v$(OR_TOOLS_VERSION).zip \
 ortools_examples
else
	cd $(TEMP_JAVA_DIR) \
 && tar -c -v -z --no-same-owner \
 -f ../or-tools_java_examples_v$(OR_TOOLS_VERSION).tar.gz \
 ortools_examples
endif
	-$(DELREC) $(TEMP_JAVA_DIR)$Sortools_examples

################
##  Cleaning  ##
################
.PHONY: clean_java # Clean Java output from previous build.
clean_java:
	-$(DELREC) $(GEN_PATH)$Sjava
	-$(DELREC) $(OBJ_DIR)$Scom
	-$(DEL) $(CLASS_DIR)$S*.class
	-$(DELREC) $(CLASS_DIR)
	-$(DEL) $(GEN_PATH)$Sortools$Salgorithms$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sconstraint_solver$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sgraph$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Slinear_solver$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Ssat$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sutil$S*java_wrap*
	-$(DEL) $(GEN_PATH)$Sortools$Sinit$S*java_wrap*
	-$(DEL) $(OBJ_DIR)$Sswig$S*_java_wrap.$O
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)jni*.$(JNI_LIB_EXT)
	-$(DEL) $(LIB_DIR)$S*.jar
	-$(DELREC) $(TEMP_JAVA_DIR)

#############
##  DEBUG  ##
#############
.PHONY: detect_java # Show variables used to build Java OR-Tools.
detect_java:
	@echo Relevant info for the Java build:
	@echo JAVA_INC = $(JAVA_INC)
	@echo JNIFLAGS = $(JNIFLAGS)
	@echo JAVA_HOME = $(JAVA_HOME)
	@echo JAVAC_BIN = $(JAVAC_BIN)
	@echo CLASS_DIR = $(CLASS_DIR)
	@echo JAR_BIN = $(JAR_BIN)
	@echo JAVA_BIN = $(JAVA_BIN)
	@echo JAVAFLAGS = $(JAVAFLAGS)
	@echo JAVA_ORTOOLS_JAR = $(JAVA_ORTOOLS_JAR)
	@echo SWIG_BINARY = $(SWIG_BINARY)
	@echo SWIG_INC = $(SWIG_INC)
	@echo MVN_BIN = $(MVN_BIN)
	@echo JAVA_ORTOOLS_PACKAGE = $(JAVA_ORTOOLS_PACKAGE)
	@echo JAVA_ORTOOLS_NATIVE_PROJECT = $(JAVA_ORTOOLS_NATIVE_PROJECT)
	@echo JAVA_ORTOOLS_PROJECT = $(JAVA_ORTOOLS_PROJECT)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
