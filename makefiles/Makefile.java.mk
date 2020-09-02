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

JAVA_OR_TOOLS_LIBS= $(LIB_DIR)/com.google.ortools$J
JAVA_OR_TOOLS_NATIVE_LIBS := $(LIB_DIR)/$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT)
JAVAFLAGS = -Djava.library.path=$(LIB_DIR)

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

# Main target
.PHONY: java # Build Java OR-Tools.
.PHONY: test_java # Test Java OR-Tools using various examples.
.PHONY: package_java # Create Java OR-Tools maven package.
ifndef HAS_JAVA
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
package_java: java
else
java: $(JAVA_OR_TOOLS_LIBS)
check_java: check_java_pimpl
test_java: test_java_pimpl
package_java: java
	@echo NOT IMPLEMENTED
BUILT_LANGUAGES +=, Java
endif

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

$(JAVA_OR_TOOLS_NATIVE_LIBS): \
 $(OR_TOOLS_LIBS) \
 $(OBJ_DIR)/swig/constraint_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/knapsack_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/graph_java_wrap.$O \
 $(OBJ_DIR)/swig/linear_solver_java_wrap.$O \
 $(OBJ_DIR)/swig/sat_java_wrap.$O \
 $(OBJ_DIR)/swig/util_java_wrap.$O
	$(DYNAMIC_LD) $(LD_OUT)$(LIB_DIR)$S$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT) \
 $(OBJ_DIR)$Sswig$Sconstraint_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sknapsack_solver_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Sgraph_java_wrap.$O \
 $(OBJ_DIR)$Sswig$Slinear_solver_java_wrap.$O \
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

$(GEN_DIR)/java/com/google/ortools/sat/CpModel.java: \
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

$(JAVA_OR_TOOLS_LIBS): \
 $(JAVA_OR_TOOLS_NATIVE_LIBS) \
 $(LIB_DIR)/protobuf.jar \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/SolverParameters.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/SearchLimitProtobuf.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingParameters.java \
 $(GEN_DIR)/java/com/google/ortools/constraintsolver/RoutingEnums.java \
 $(GEN_DIR)/java/com/google/ortools/linearsolver/MPModelProto.java \
 $(GEN_DIR)/java/com/google/ortools/sat/SatParameters.java \
 $(GEN_DIR)/java/com/google/ortools/util/OptionalBoolean.java \
 $(GEN_DIR)/java/com/google/ortools/sat/CpModel.java | \
 $(CLASS_DIR)/com/google/ortools
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
$(CLASS_DIR)/$(SOURCE_NAME): $(SOURCE) $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$(SOURCE_NAME)
	-$(MKDIR_P) $(CLASS_DIR)$S$(SOURCE_NAME)
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$(SOURCE_NAME) \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(SOURCE_PATH)

$(LIB_DIR)/$(SOURCE_NAME)$J: $(CLASS_DIR)/$(SOURCE_NAME) | $(LIB_DIR)
	-$(DEL) $(LIB_DIR)$S$(SOURCE_NAME)$J
	"$(JAR_BIN)" cvf $(LIB_DIR)$S$(SOURCE_NAME)$J -C $(CLASS_DIR)$S$(SOURCE_NAME) .

.PHONY: build # Build a Java program.
build: $(LIB_DIR)/$(SOURCE_NAME)$J

.PHONY: run # Run a Java program.
run: build
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$(SOURCE_NAME)$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(SOURCE_NAME) $(ARGS)
endif

#############################
##  Java Examples/Samples  ##
#############################
$(CLASS_DIR)/%: $(TEST_DIR)/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(TEST_PATH)$S$*.java

$(CLASS_DIR)/%: $(JAVA_EX_DIR)/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(JAVA_EX_PATH)$S$*.java

$(CLASS_DIR)/%: $(CONTRIB_EX_DIR)/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $(CONTRIB_EX_PATH)$S$*.java

$(CLASS_DIR)/%: $(SRC_DIR)/ortools/algorithms/samples/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 ortools$Salgorithms$Ssamples$S$*.java

$(CLASS_DIR)/%: $(SRC_DIR)/ortools/constraint_solver/samples/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 ortools$Sconstraint_solver$Ssamples$S$*.java

$(CLASS_DIR)/%: $(SRC_DIR)/ortools/graph/samples/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 ortools$Sgraph$Ssamples$S$*.java

$(CLASS_DIR)/%: $(SRC_DIR)/ortools/linear_solver/samples/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 ortools$Slinear_solver$Ssamples$S$*.java

$(CLASS_DIR)/%: $(SRC_DIR)/ortools/sat/samples/%.java $(JAVA_OR_TOOLS_LIBS) | $(CLASS_DIR)
	-$(DELREC) $(CLASS_DIR)$S$*
	-$(MKDIR_P) $(CLASS_DIR)$S$*
	"$(JAVAC_BIN)" -encoding UTF-8 -d $(CLASS_DIR)$S$* \
 -cp $(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 ortools$Ssat$Ssamples$S$*.java

$(LIB_DIR)/%$J: $(CLASS_DIR)/% | $(LIB_DIR)
	-$(DEL) $(LIB_DIR)$S$*.jar
	"$(JAR_BIN)" cvf $(LIB_DIR)$S$*.jar -C $(CLASS_DIR)$S$* .

rjava_%: $(TEST_DIR)/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 $* $(ARGS)

rjava_%: $(JAVA_EX_DIR)/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.examples.$* $(ARGS)

rjava_%: $(CONTRIB_EX_DIR)/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.contrib.$* $(ARGS)

rjava_%: $(SRC_DIR)/ortools/algorithms/samples/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.algorithms.samples.$* $(ARGS)

rjava_%: $(SRC_DIR)/ortools/constraint_solver/samples/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.constraintsolver.samples.$* $(ARGS)

rjava_%: $(SRC_DIR)/ortools/graph/samples/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.graph.samples.$* $(ARGS)

rjava_%: $(SRC_DIR)/ortools/linear_solver/samples/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.linearsolver.samples.$* $(ARGS)

rjava_%: $(SRC_DIR)/ortools/sat/samples/%.java $(LIB_DIR)/%$J FORCE
	"$(JAVA_BIN)" -Xss2048k $(JAVAFLAGS) \
 -cp $(LIB_DIR)$S$*$J$(CPSEP)$(LIB_DIR)$Scom.google.ortools.jar$(CPSEP)$(LIB_DIR)$Sprotobuf.jar \
 com.google.ortools.sat.samples.$* $(ARGS)

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
 rjava_BinPackingMip \
 rjava_LinearProgrammingExample \
 rjava_MipVarArray \
 rjava_MultipleKnapsackMip \
 rjava_SimpleLpProgram \
 rjava_SimpleMipProgram

.PHONY: test_java_sat_samples # Build and Run all Java SAT Samples (located in ortools/sat/samples)
test_java_sat_samples: \
 rjava_AssignmentSat \
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
 rjava_TestLinearSolver \
 rjava_TestConstraintSolver \
 rjava_TestRoutingSolver \
 rjava_TestSatSolver \

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
	-$(DEL) $(OBJ_DIR)$Sswig$S*_java_wrap.$O
	-$(DEL) $(LIB_DIR)$S$(LIB_PREFIX)jni*.$(JNI_LIB_EXT)
	-$(DEL) $(LIB_DIR)$S*.jar

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
	@echo JAVA_OR_TOOLS_LIBS = $(JAVA_OR_TOOLS_LIBS)
	@echo SWIG_BINARY = $(SWIG_BINARY)
	@echo SWIG_INC = $(SWIG_INC)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
