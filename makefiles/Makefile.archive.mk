
archive: $(INSTALL_DIR)$(ARCHIVE_EXT)

$(INSTALL_DIR)$(ARCHIVE_EXT): $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) csharp java create_dirs cc_archive dotnet_archive java_archive  $(PATCHELF)
ifeq "$(SYSTEM)" "win"
	cd temp && ..$Stools$Szip.exe -r ..$S$(INSTALL_DIR).zip $(INSTALL_DIR)
else
ifeq ($(PLATFORM),LINUX)
	tools/fix_libraries_on_linux.sh
else
	$(COPY) tools/install_libortools_mac.sh temp/$(INSTALL_DIR)
	chmod 775 temp/$(INSTALL_DIR)/install_libortools_mac.sh
	cd temp/$(INSTALL_DIR) && ./install_libortools_mac.sh && rm install_libortools_mac.sh
endif
	cd temp && tar -c -v -z --no-same-owner -f ..$S$(INSTALL_DIR).tar.gz $(INSTALL_DIR)
endif
	-$(DELREC) temp

create_dirs:
	-$(DELREC) temp
	$(MKDIR) temp
	$(MKDIR) temp$S$(INSTALL_DIR)
	$(MKDIR) temp$S$(INSTALL_DIR)$Slib
	$(MKDIR) temp$S$(INSTALL_DIR)$Sobjs
	$(MKDIR) temp$S$(INSTALL_DIR)$Sbin
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Salgorithms
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sbase
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sconstraint_solver
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sgflags
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sglog
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sbop
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sglop
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sgoogle
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sgraph
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Slinear_solver
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Slp_data
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sport
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Ssat
	$(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sutil
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scpp
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scsharp
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution$SProperties
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scom
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scom$Sgoogle
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scom$Sgoogle$Sortools
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Scom$Sgoogle$Sortools$Ssamples
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Set_jobshop
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sflexible_jobshop
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sjobshop
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Smultidim_knapsack
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Scvrptw
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Spdptw
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Srogo
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sfsharp
	$(MKDIR) temp$S$(INSTALL_DIR)$Sexamples$Sfsharp$Slib


#credits
	$(COPY) LICENSE-2.0.txt temp$S$(INSTALL_DIR)
	$(COPY) tools$SREADME.cc.java.csharp temp$S$(INSTALL_DIR)$SREADME
	$(COPY) tools$SMakefile.cc temp$S$(INSTALL_DIR)$SMakefile

cc_archive: cc

	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)fap.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) examples$Scpp$S*.cc temp$S$(INSTALL_DIR)$Sexamples$Scpp
	$(COPY) examples$Scpp$S*.h temp$S$(INSTALL_DIR)$Sexamples$Scpp
	$(COPY) ortools$Salgorithms$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Salgorithms
	$(COPY) ortools$Sbase$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sbase
	$(COPY) ortools$Sconstraint_solver$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sconstraint_solver
	$(COPY) ortools$Sgen$Sortools$Sconstraint_solver$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sconstraint_solver
	$(COPY) ortools$Sbop$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sbop
	$(COPY) ortools$Sgen$Sortools$Sbop$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sbop
	$(COPY) ortools$Sglop$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sglop
	$(COPY) ortools$Sgen$Sortools$Sglop$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sglop
	$(COPY) ortools$Sgraph$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sgraph
	$(COPY) ortools$Sgen$Sortools$Sgraph$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sgraph
	$(COPY) ortools$Slinear_solver$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Slinear_solver
	$(COPY) ortools$Slp_data$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Slp_data
	$(COPY) ortools$Sgen$Sortools$Slinear_solver$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Slinear_solver
	$(COPY) ortools$Sport$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sport
	$(COPY) ortools$Ssat$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Ssat
	$(COPY) ortools$Sgen$Sortools$Ssat$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Ssat
	$(COPY) ortools$Sutil$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sortools$Sutil

ifeq "$(SYSTEM)" "win"
	$(COPY) tools$Smake.exe temp$S$(INSTALL_DIR)
	cd temp$S$(INSTALL_DIR) && ..$S..$Stools$Star.exe -C ..$S.. -c -v --exclude *svn* --exclude *roadef* examples$Sdata | ..$S..$Stools$Star.exe xvm

	cd temp$S$(INSTALL_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v gflags | ..$S..$S..$Stools$Star.exe xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v glog | ..$S..$S..$Stools$Star.exe xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v google | ..$S..$S..$Stools$Star.exe xvm
else
	$(COPY) -R examples$Sdata$Set_jobshop$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Set_jobshop
	$(COPY) -R examples$Sdata$Sflexible_jobshop$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sflexible_jobshop
	$(COPY) -R examples$Sdata$Sjobshop$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sjobshop
	$(COPY) -R examples$Sdata$Smultidim_knapsack$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Smultidim_knapsack
	$(COPY) -R examples$Sdata$Scvrptw$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Scvrptw
	$(COPY) -R examples$Sdata$Spdptw$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Spdptw

	cd temp$S$(INSTALL_DIR)$Sinclude && tar -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v gflags | tar xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && tar -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v glog | tar xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && tar -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v google | tar xvm
endif

dotnet_archive: csharp

	$(COPY) bin$SGoogle.Protobuf.dll temp$S$(INSTALL_DIR)$Sbin
	$(COPY) bin$S$(CLR_ORTOOLS_DLL_NAME).dll temp$S$(INSTALL_DIR)$Sbin
	$(COPY) examples$Scsharp$S*.cs temp$S$(INSTALL_DIR)$Sexamples$Scsharp
	$(COPY) examples$Sfsharp$S*fsx temp$S$(INSTALL_DIR)$Sexamples$Sfsharp
	$(COPY) examples$Sfsharp$SREADME.md temp$S$(INSTALL_DIR)$Sexamples$Sfsharp
	$(COPY) examples$Sfsharp$Slib$S* temp$S$(INSTALL_DIR)$Sexamples$Sfsharp$Slib
	$(COPY) examples$Scsharp$Ssolution$SProperties$S*.cs temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution$SProperties
	$(COPY) examples$Sdata$Sdiscrete_tomography$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
	$(COPY) examples$Sdata$Sfill_a_pix$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	$(COPY) examples$Sdata$Sminesweeper$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	$(COPY) examples$Sdata$Srogo$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Srogo
	$(COPY) examples$Sdata$Ssurvo_puzzle$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	$(COPY) examples$Sdata$Squasigroup_completion$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion

ifeq "$(SYSTEM)" "win"
	$(COPY) examples$Scsharp$SCsharp_examples.sln temp$S$(INSTALL_DIR)$Sexamples$Scsharp
	$(COPY) examples$Scsharp$Ssolution$S*.csproj temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution
	$(COPY) examples$Scsharp$Ssolution$Sapp.config temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution
else
	$(COPY) lib$Slib$(CLR_ORTOOLS_DLL_NAME).so temp$S$(INSTALL_DIR)$Sbin
endif

java_archive: java
	$(COPY) lib$S*.jar temp$S$(INSTALL_DIR)$Slib
	$(COPY) lib$S$(LIB_PREFIX)jni*.$(JNI_LIB_EXT) temp$S$(INSTALL_DIR)$Slib

	$(COPY) examples$Sdata$Sdiscrete_tomography$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
	$(COPY) examples$Sdata$Sfill_a_pix$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	$(COPY) examples$Sdata$Sminesweeper$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	$(COPY) examples$Sdata$Srogo$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Srogo
	$(COPY) examples$Sdata$Ssurvo_puzzle$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	$(COPY) examples$Sdata$Squasigroup_completion$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion
	$(COPY) examples$Scom$Sgoogle$Sortools$Ssamples$S*.java temp$S$(INSTALL_DIR)$Sexamples$Scom$Sgoogle$Sortools$Ssamples

TEMP_FZ_DIR = temp_fz
fz_archive: cc fz
	-$(DELREC) $(TEMP_FZ_DIR)
	mkdir $(TEMP_FZ_DIR)
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Slib
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_cp
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_sat
	mkdir $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sexamples
	$(COPY) LICENSE-2.0.txt $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)
	$(COPY) bin$Sfz$E $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin$Sfzn-or-tools$E
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) ortools$Sflatzinc$Smznlib_cp$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_cp
	$(COPY) ortools$Sflatzinc$Smznlib_sat$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc_sat
	$(COPY) examples$Sflatzinc$S* $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sexamples
ifeq "$(SYSTEM)" "win"
	cd $(TEMP_FZ_DIR) && ..$Stools$Szip.exe -r ..$S$(FZ_INSTALL_DIR).zip $(FZ_INSTALL_DIR)
else
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)$Spatchelf --set-rpath '$$ORIGIN/../lib' $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sbin$Sfzn-or-tools
endif
ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools$Sfix_fz_libraries_on_mac.sh $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)
	chmod u+x $(TEMP_FZ_DIR)/$(FZ_INSTALL_DIR)$Sfix_fz_libraries_on_mac.sh
	cd $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR) && .$Sfix_fz_libraries_on_mac.sh
	$(RM) $(TEMP_FZ_DIR)$S$(FZ_INSTALL_DIR)$Sfix_fz_libraries_on_mac.sh
endif
	cd $(TEMP_FZ_DIR) && tar cvzf ..$S$(FZ_INSTALL_DIR).tar.gz $(FZ_INSTALL_DIR)
endif
	-$(DELREC) $(TEMP_FZ_DIR)


test_archive: $(INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DELREC) temp
	$(MKDIR) temp
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
ifeq "$(SYSTEM)" "win"
	tools$Sunzip.exe $(INSTALL_DIR).zip -d temp
else
	tar -x -v -f $(INSTALL_DIR).tar.gz -C temp
endif
	cd temp$S$(INSTALL_DIR) && $(MAKE) test && cd ../.. && $(RENAME) lib2 lib && echo "archive test succeeded" || ( cd ../.. && $(RENAME) lib2 lib && echo "archive test failed" && exit 1)

TEMP_FZ_TEST_DIR = temp_test_fz
test_fz_archive: $(FZ_INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DELREC) $(TEMP_FZ_TEST_DIR)
	$(MKDIR) $(TEMP_FZ_TEST_DIR)
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
ifeq "$(SYSTEM)" "win"
	tools$Sunzip.exe $(FZ_INSTALL_DIR).zip -d $(TEMP_FZ_TEST_DIR)
else
	tar -x -v -f $(FZ_INSTALL_DIR).tar.gz -C $(TEMP_FZ_TEST_DIR)
endif
	cd $(TEMP_FZ_TEST_DIR)$S$(FZ_INSTALL_DIR) && .$Sbin$S$(FZ_EXE) examples$Scircuit_test.fzn && cd ../.. && $(RENAME) lib2 lib && echo "fz archive test succeeded" || ( cd ../.. && $(RENAME) lib2 lib && echo "fz archive test failed" && exit 1)


ifeq "$(PYTHON3)" "true"
    build_release: clean python test_python
    pre_release: pypi_archive
    release: pypi_upload
else #platform check

ifeq "$(SYSTEM)" "win"

ifeq "$(VISUAL_STUDIO_YEAR)" "2013"
    build_release: clean all test
    pre_release: archive test_archive
    release:
else
ifeq "$(VISUAL_STUDIO_YEAR)" "2015"
    build_release: clean all test fz
    pre_release: archive test_archive fz_archive test_fz_archive python_examples_archive pypi_archive
    release: pypi_upload nuget_upload
endif #ifeq "$(VISUAL_STUDIO_YEAR)" "2015"

endif # ifeq"$(VISUAL_STUDIO_YEAR)" "2013"

else # unix

ifeq "$(PLATFORM)" "LINUX"
ifeq "$(DISTRIBUTION_NUMBER)" "14.04"
    build_release: clean all test fz
    pre_release: archive test_archive fz_archive test_fz_archive python_examples_archive pypi_archive
    release: pypi_upload
else
ifeq "$(DISTRIBUTION_NUMBER)" "16.04"
    build_release: clean all test fz
    pre_release: archive test_archive fz_archive test_fz_archive
    release:
endif # ifeq "$(DISTRIBUTION_NUMBER)" "16.04"
endif # ifeq "$(DISTRIBUTION_NUMBER)" "14.04"
endif # ifeq "$(PLATFORM)" "LINUX"

ifeq "$(PLATFORM)" "MACOSX"
    build_release: clean all test fz
    pre_release: archive test_archive fz_archive test_fz_archive pypi_archive
    release: pypi_upload
endif #ifeq "$(PLATFORM)" "MACOSX"

endif #ifeq "$(SYSTEM)" "win"
endif #ifeq "$(PYTHON3)" "true"
