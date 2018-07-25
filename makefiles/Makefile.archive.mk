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
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)

$(INSTALL_DIR)$(ARCHIVE_EXT): archive_cc archive_java archive_dotnet
	$(COPY) tools$SREADME.cc.java.dotnet $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$SREADME
	$(COPY) tools$SMakefile.cc.java.dotnet $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$SMakefile
ifeq ($(SYSTEM),win)
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools$Swin
	$(COPY) tools$Smake.exe $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools
	$(COPY) $(WHICH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Stools$Swin
	cd $(TEMP_ARCHIVE_DIR) && ..$S$(ZIP) -r ..$S$(INSTALL_DIR)$(ARCHIVE_EXT) $(INSTALL_DIR)
else
	$(TAR) -C $(TEMP_ARCHIVE_DIR) --no-same-owner -czvf $(INSTALL_DIR)$(ARCHIVE_EXT) $(INSTALL_DIR)
endif
#	-$(DELREC) $(TEMP_ARCHIVE_DIR)

.PHONY: archive_cc # Add C++ OR-Tools to archive.
archive_cc: cc | $(TEMP_ARCHIVE_DIR)
	$(MAKE) install_cc prefix=$(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)
	$(COPY) $(CVRPTW_PATH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Slib
	$(COPY) $(DIMACS_PATH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Slib
	$(COPY) $(FAP_PATH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Slib
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	$(COPY) examples$Scpp$S*.cc $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp
	$(COPY) examples$Scpp$S*.h  $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Scpp

.PHONY: archive_java # Add Java OR-Tools to archive.
archive_java: java | $(TEMP_ARCHIVE_DIR)
	$(COPY) $(LIB_DIR)$Scom.google.ortools.jar $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$Sprotobuf.jar $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)jniortools.$(JNI_LIB_EXT) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Slib
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples
ifeq ($(SYSTEM),win)
	-$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sjava
	$(COPYREC) $(JAVA_EX_PATH) "$(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sjava" /E
else
	$(COPYREC) $(JAVA_EX_PATH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples
endif

.PHONY: archive_dotnet # Add .Net OR-Tools to archive.
archive_dotnet: dotnet | $(TEMP_ARCHIVE_DIR)
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sbin
	"$(DOTNET_BIN)" publish \
-f netstandard2.0 \
-c Release \
-o "..$S..$S..$Stemp_archive$S$(INSTALL_DIR)$Sbin" \
ortools$Sdotnet$S$(ORTOOLS_DLL_NAME)$S$(ORTOOLS_DLL_NAME).csproj
	"$(DOTNET_BIN)" publish \
-f netstandard2.0 \
-c Release \
-o "..$S..$S..$Stemp_archive$S$(INSTALL_DIR)$Sbin" \
ortools$Sdotnet$S$(ORTOOLS_FSHARP_DLL_NAME)$S$(ORTOOLS_FSHARP_DLL_NAME).fsproj
	$(COPY) $(BIN_DIR)$S$(CLR_ORTOOLS_IMPORT_DLL_NAME).$(SWIG_DOTNET_LIB_SUFFIX) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sbin
	$(COPY) $(BIN_DIR)$S$(CLR_PROTOBUF_DLL_NAME)$D $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sbin
	$(MKDIR_P) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples
ifeq ($(SYSTEM),win)
	-$(MKDIR) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet
	$(COPYREC) $(DOTNET_EX_PATH) "$(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples$Sdotnet" /E
else
	$(COPYREC) $(DOTNET_EX_PATH) $(TEMP_ARCHIVE_DIR)$S$(INSTALL_DIR)$Sexamples
endif

$(FZ_INSTALL_DIR)$(ARCHIVE_EXT): fz | $(TEMP_FZ_DIR)
	-$(DELREC) $(TEMP_FZ_DIR)$S*
	$(MAKE) install_cc prefix=$(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)
	$(COPY) $(FLATZINC_PATH) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) $(BIN_DIR)$Sfz$E $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin$S$(FZ_EXE)
	$(COPY) $(BIN_DIR)$Sparser_main$E $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin$Sparser-or-tools$E
	$(MKDIR_P) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare
	$(MKDIR) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_cp
	$(COPY) ortools$Sflatzinc$Smznlib_cp$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_cp
	$(MKDIR) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_sat
	$(COPY) ortools$Sflatzinc$Smznlib_sat$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_sat
	$(MKDIR_P) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sexamples
	$(COPY) examples$Sflatzinc$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sexamples
ifeq ($(SYSTEM),win)
	cd $(TEMP_FZ_DIR) && ..$S$(ZIP) -r ..$S$(FZ_INSTALL_DIR)$(ARCHIVE_EXT) $(FZ_INSTALL_DIR)
else
	$(TAR) -C $(TEMP_FZ_DIR) --no-same-owner -czvf $(FZ_INSTALL_DIR)$(ARCHIVE_EXT) $(FZ_INSTALL_DIR)
endif
#	-$(DELREC) $(TEMP_FZ_DIR)

$(TEMP_FZ_DIR):
	$(MKDIR_P) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)

$(DATA_INSTALL_DIR)$(ARCHIVE_EXT):
	-$(DELREC) $(TEMP_DATA_DIR)
	$(MKDIR) $(TEMP_DATA_DIR)
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Set_jobshop
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sflexible_jobshop
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sjobshop
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Smultidim_knapsack
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Scvrptw
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Spdptw
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Srogo
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion
	$(MKDIR) $(TEMP_DATA_DIR)$S$(DATA_INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
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
	$(MKDIR) $(TEMP_TEST_DIR)
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
ifeq ($(SYSTEM),win)
	$(UNZIP) $< -d $(TEMP_TEST_DIR)
else
	$(TAR) -xvf $< -C $(TEMP_TEST_DIR)
endif
	( cd $(TEMP_TEST_DIR)$S$(INSTALL_DIR) && $(MAKE) all ) && \
$(RENAME) lib2 lib && echo "archive test succeeded" || \
( $(RENAME) lib2 lib && echo "archive test failed" && exit 1)

TEMP_FZ_TEST_DIR = temp_fz_test
.PHONY: test_fz_archive
test_fz_archive: $(FZ_INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_FZ_TEST_DIR)
	$(MKDIR) $(TEMP_FZ_TEST_DIR)
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
ifeq ($(SYSTEM),win)
	$(UNZIP) $< -d $(TEMP_FZ_TEST_DIR)
else
	$(TAR) -xvf $< -C $(TEMP_FZ_TEST_DIR)
endif
	( cd $(TEMP_FZ_TEST_DIR)$S$(FZ_INSTALL_DIR) && .$Sbin$S$(FZ_EXE) examples$Scircuit_test.fzn ) && \
$(RENAME) lib2 lib && echo "fz archive test succeeded" || \
( $(RENAME) lib2 lib && echo "fz archive test failed" && exit 1)

.PHONY: detect_archive # Show variables used to build archive OR-Tools.
detect_archive:
	@echo Relevant info for the archive build:
	@echo INSTALL_DIR = $(INSTALL_DIR)
	@echo FZ_INSTALL_DIR = $(FZ_INSTALL_DIR)
	@echo ARCHIVE_EXT = $(ARCHIVE_EXT)
ifeq ($(SYSTEM),win)
	@echo off & echo(
else
	@echo
endif
