# ---------- Archive support ----------
.PHONY: help_archive # Generate list of Archive targets with descriptions.
help_archive:
	@echo Use one of the following Archive targets:
ifeq ($(SYSTEM),win)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.archive.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.archive.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

TEMP_ARCHIVE_DIR = temp_archive
TEMP_FZ_DIR = temp_fz_archive
TEMP_DATA_DIR = temp_data

# Main target
.PHONY: archive # Create OR-Tools archive for C++, Java and .Net with examples.
archive: $(INSTALL_DIR)$(ARCHIVE_EXT)

.PHONY: fz_archive # Create OR-Tools flatzinc archive with examples.
fz_archive: $(FZ_INSTALL_DIR)$(ARCHIVE_EXT)

.PHONY: data_archive # Create OR-Tools archive for data examples.
data_archive: $(DATA_INSTALL_DIR)$(ARCHIVE_EXT)

.PHONY: clean_archive # Clean Archive output from previous build.
clean_archive:
	-$(DELREC) $(TEMP_ARCHIVE_DIR)
	-$(DELREC) $(TEMP_FZ_DIR)
	-$(DELREC) $(TEMP_DATA_DIR)
	-$(DELREC) $(TEMP_TEST_DIR)
	-$(DELREC) $(TEMP_FZ_TEST_DIR)
	-$(DEL) $(INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DEL) $(FZ_INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DEL) $(DATA_INSTALL_DIR)$(ARCHIVE_EXT)

$(TEMP_ARCHIVE_DIR):
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)

$(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR): | $(TEMP_ARCHIVE_DIR)
	$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)

$(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples: | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)
	$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples

$(INSTALL_DIR)$(ARCHIVE_EXT): archive_cc archive_java archive_dotnet \
 tools/README.cc.java.dotnet tools/Makefile.cc.java.dotnet
	$(COPY) tools$SREADME.cc.java.dotnet $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$SREADME.md
	$(COPY) tools$SMakefile.cc.java.dotnet $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$SMakefile
	$(SED) -i -e 's/@PROJECT_VERSION@/$(OR_TOOLS_VERSION)/' $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$SMakefile
ifeq ($(SYSTEM),win)
	-$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools$Swin
	$(COPY) tools$Smake.exe $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools
	$(COPY) $(WHICH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools$Swin
	$(COPY) $(TOUCH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools$Swin
	cd $(TEMP_ARCHIVE_DIR) && ..$S$(ZIP) -r ..$S$(INSTALL_DIR)$(ARCHIVE_EXT) $(INSTALL_DIR)
else
	$(TAR) -C $(TEMP_ARCHIVE_DIR) --no-same-owner -czvf $(INSTALL_DIR)$(ARCHIVE_EXT) $(INSTALL_DIR)
endif
#	-$(DELREC) $(TEMP_ARCHIVE_DIR)

###########
##  CPP  ##
###########
$(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/cpp: | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples
	$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp

.PHONY: archive_cc # Add C++ OR-Tools to archive.
archive_cc: cc | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/cpp
	$(MAKE) install_cc prefix=$(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)
	-$(COPY) $(CC_EX_PATH)$S*.h  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) $(CC_EX_PATH)$S*.cc $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) $(CC_EX_PATH)$SREADME.md $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) $(CONTRIB_EX_PATH)$S*.h  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) $(CONTRIB_EX_PATH)$S*.cc $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) ortools$Salgorithms$Ssamples$S*.cc  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) ortools$Sgraph$Ssamples$S*.cc  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) ortools$Slinear_solver$Ssamples$S*.cc  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) ortools$Sconstraint_solver$Ssamples$S*.cc  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) ortools$Srouting$Ssamples$S*.cc  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	-$(COPY) ortools$Ssat$Ssamples$S*.cc  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp

############
##  JAVA  ##
############
$(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/java: | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples
	$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sjava

define java-sample-archive =
$$(TEMP_ARCHIVE_DIR)/$$(INSTALL_DIR)/examples/java/%/pom.xml: \
 $$(TEMP_JAVA_DIR)/$1/%/pom.xml \
 ortools/$1/samples/%.java \
 | $$(TEMP_ARCHIVE_DIR)/$$(INSTALL_DIR)/examples/java
	-$$(MKDIR_P) $$(TEMP_ARCHIVE_DIR)$$S$$(INSTALL_DIR)$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(SRC_DIR)$$Sortools$$S$1$$Ssamples$$S$$*.java \
 $$(TEMP_ARCHIVE_DIR)$$S$$(INSTALL_DIR)$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml \
 $$(TEMP_ARCHIVE_DIR)$$S$$(INSTALL_DIR)$$Sexamples$$Sjava$$S$$*
endef

$(foreach sample,$(JAVA_SAMPLES),$(eval $(call java-sample-archive,$(sample))))

define java-example-archive =
$$(TEMP_ARCHIVE_DIR)/$$(INSTALL_DIR)/examples/java/%/pom.xml: \
 $$(TEMP_JAVA_DIR)/$1/%/pom.xml \
 examples/$1/%.java \
 | $$(TEMP_ARCHIVE_DIR)/$$(INSTALL_DIR)/examples/java
	-$$(MKDIR_P) $$(TEMP_ARCHIVE_DIR)$$S$$(INSTALL_DIR)$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(SRC_DIR)$$Sexamples$$S$1$$S$$*.java \
 $$(TEMP_ARCHIVE_DIR)$$S$$(INSTALL_DIR)$$Sexamples$$Sjava$$S$$*$$S$$(JAVA_SRC_PATH)
	$$(COPY) $$(TEMP_JAVA_DIR)$$S$1$$S$$*$$Spom.xml \
 $$(TEMP_ARCHIVE_DIR)$$S$$(INSTALL_DIR)$$Sexamples$$Sjava$$S$$*
endef

$(foreach example,$(JAVA_EXAMPLES),$(eval $(call java-example-archive,$(example))))

SAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/java/,$(basename $(notdir $(wildcard ortools/*/samples/*.java)))))

EXAMPLE_JAVA_FILES = \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/java/,$(basename $(notdir $(wildcard examples/contrib/*.java))))) \
  $(addsuffix /pom.xml,$(addprefix $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/java/,$(basename $(notdir $(wildcard examples/java/*.java)))))

.PHONY: archive_java # Add Java OR-Tools to archive.
archive_java: java \
 $(SAMPLE_JAVA_FILES) \
 $(EXAMPLE_JAVA_FILES) \
 | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/java
	$(COPY) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Starget$Sortools-*.jar $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)
	$(COPY) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_NATIVE_PROJECT)$Spom.xml $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Spom-runtime.xml
	$(COPY) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Starget$Sortools-*.jar $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)
	$(COPY) $(TEMP_JAVA_DIR)$S$(JAVA_ORTOOLS_PROJECT)$Spom.xml $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Spom-local.xml
	$(COPY) $(JAVA_EX_PATH)$SREADME.md $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sjava

##############
##  DOTNET  ##
##############
$(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/dotnet: | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples
	$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet

.PHONY: archive_dotnet # Add .Net OR-Tools to archive.
archive_dotnet: dotnet | $(TEMP_ARCHIVE_DIR)/$(INSTALL_DIR)/examples/dotnet
	-$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Spackages
	$(COPY) $(TEMP_DOTNET_DIR)$Spackages$S*.nupkg $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Spackages
	-$(COPY) $(DOTNET_EX_PATH)$S*.cs* $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) $(DOTNET_EX_PATH)$S*.fs* $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) $(DOTNET_EX_PATH)$SREADME.md $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) $(CONTRIB_EX_PATH)$S*.cs* $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) $(CONTRIB_EX_PATH)$S*.fs* $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Salgorithms$Ssamples$S*.cs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Salgorithms$Ssamples$S*.fs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Sgraph$Ssamples$S*.cs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Sgraph$Ssamples$S*.fs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Slinear_solver$Ssamples$S*.cs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Slinear_solver$Ssamples$S*.fs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Sconstraint_solver$Ssamples$S*.cs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Sconstraint_solver$Ssamples$S*.fs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Srouting$Ssamples$S*.cs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Srouting$Ssamples$S*.fs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Ssat$Ssamples$S*.cs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(COPY) ortools$Ssat$Ssamples$S*.fs*  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	-$(SED) -i -e 's/\/temp_dotnet//' $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet$S*.*proj
	-$(SED) -i -e 's/..\/..\/..\/packages/..\/..\/packages/' $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet$S*.*proj

$(FZ_INSTALL_DIR)$(ARCHIVE_EXT): fz | $(TEMP_FZ_DIR)
	-$(DELREC) $(TEMP_FZ_DIR)$S*
	$(MAKE) install_cc prefix=$(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)
	-$(DELREC) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sinclude
	-$(DELREC) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin/cbc*
	-$(DELREC) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin/clp*
	-$(DELREC) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin/protoc*
	-$(DELREC) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare/doc
	$(COPY) $(FLATZINC_PATH) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) $(BIN_DIR)$Sfz$E $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin$S$(FZ_EXE)
	$(COPY) $(BIN_DIR)$Sparser_main$E $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin$Sparser-or-tools$E
	-$(MKDIR_P) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare
	-$(MKDIR) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc
	$(COPY) ortools$Sflatzinc$Smznlib$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc
	-$(MKDIR_P) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sexamples
	$(COPY) $(FZ_EX_PATH)$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sexamples
ifeq ($(SYSTEM),win)
	cd $(TEMP_FZ_DIR) && ..$S$(ZIP) -r ..$S$(FZ_INSTALL_DIR)$(ARCHIVE_EXT) $(FZ_INSTALL_DIR)
else
	$(TAR) -C $(TEMP_FZ_DIR) --no-same-owner -czvf $(FZ_INSTALL_DIR)$(ARCHIVE_EXT) $(FZ_INSTALL_DIR)
endif
#	-$(DELREC) $(TEMP_FZ_DIR)

$(TEMP_FZ_DIR):
	-$(MKDIR_P) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)

$(DATA_INSTALL_DIR)$(ARCHIVE_EXT):
	-$(DELREC) $(TEMP_DATA_DIR)
	-$(MKDIR) $(TEMP_DATA_DIR)
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Set_jobshop
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sflexible_jobshop
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sjobshop
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Smultidim_knapsack
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Scvrptw
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Spdptw
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Srogo
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion
	-$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
#credits
	$(COPY) LICENSE-2.0.txt $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)
	$(TAR) -c -v \
--exclude *svn* \
--exclude *roadef* \
--exclude *vector_packing* \
--exclude *nsplib* \
examples$Sdata | $(TAR) -xvm -C $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)
ifeq ($(SYSTEM),win)
	cd $(TEMP_DATA_DIR) && ..$S$(ZIP) -r ..$S$(DATA_INSTALL_DIR)$(ARCHIVE_EXT) $(DATA_INSTALL_DIR)
else
	$(TAR) -C $(TEMP_DATA_DIR) --no-same-owner -czvf $(DATA_INSTALL_DIR)$(ARCHIVE_EXT) $(DATA_INSTALL_DIR)
endif
#	-$(DELREC) $(TEMP_DATA_DIR)

###############
##  TESTING  ##
###############
TEMP_TEST_DIR = temp_test
.PHONY: test_archive
test_archive: $(INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_TEST_DIR)
	-$(MKDIR) $(TEMP_TEST_DIR)
ifeq ($(SYSTEM),win)
	$(UNZIP) $< -d $(TEMP_TEST_DIR)
	cd $(TEMP_TEST_DIR)$S$(INSTALL_DIR) \
 && $(MAKE) MAKEFLAGS= test_cc \
 && $(MAKE) MAKEFLAGS= test_java \
 && $(MAKE) MAKEFLAGS= test_dotnet
else
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
	$(TAR) -xvf $< -C $(TEMP_TEST_DIR)
	( cd $(TEMP_TEST_DIR)$S$(INSTALL_DIR) \
 && $(MAKE) MAKEFLAGS= test_cc \
 && $(MAKE) MAKEFLAGS= test_java \
 && $(MAKE) MAKEFLAGS= test_dotnet \
 ) && $(RENAME) lib2 lib && echo "archive test succeeded" \
 || ( $(RENAME) lib2 lib && echo "archive test failed" && exit 1)
endif

TEMP_FZ_TEST_DIR = temp_fz_test
.PHONY: test_fz_archive
test_fz_archive: $(FZ_INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_FZ_TEST_DIR)
	-$(MKDIR) $(TEMP_FZ_TEST_DIR)
ifeq ($(SYSTEM),win)
	$(UNZIP) $< -d $(TEMP_FZ_TEST_DIR)
	cd $(TEMP_FZ_TEST_DIR)$S$(FZ_INSTALL_DIR) && .$Sbin$S$(FZ_EXE) examples$Scircuit_test.fzn
else
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
	$(TAR) -xvf $< -C $(TEMP_FZ_TEST_DIR)
	( cd $(TEMP_FZ_TEST_DIR)$S$(FZ_INSTALL_DIR) && .$Sbin$S$(FZ_EXE) examples$Scircuit_test.fzn ) && \
$(RENAME) lib2 lib && echo "fz archive test succeeded" || \
( $(RENAME) lib2 lib && echo "fz archive test failed" && exit 1)
endif

.PHONY: detect_archive # Show variables used to build archive OR-Tools.
detect_archive:
	@echo Relevant info for the archive build:
	@echo TEMP_ARCHIVE_DIR = $(TEMP_ARCHIVE_DIR)
	@echo INSTALL_DIR = $(INSTALL_DIR)
	@echo TEMP_FZ_DIR = $(TEMP_FZ_DIR)
	@echo FZ_INSTALL_DIR = $(FZ_INSTALL_DIR)
	@echo TEMP_DATA_DIR = $(TEMP_DATA_DIR)
	@echo DATA_INSTALL_DIR = $(DATA_INSTALL_DIR)
	@echo ARCHIVE_EXT = $(ARCHIVE_EXT)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
