
archive: $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) csharp java create_dirs cc_archive dotnet_archive java_archive  $(PATCHELF)
ifeq "$(SYSTEM)" "win"
	cd temp && ..$Stools$Szip.exe -r ..$S$(INSTALL_DIR).zip $(INSTALL_DIR)
else
ifeq ($(PLATFORM),LINUX)
	tools/fix_libraries_on_linux.sh
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
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sbin$Scpp
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sbin$Scsharp
	        $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Salgorithms
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sbase
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sconstraint_solver
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sgflags
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sbop
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sglop
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sgoogle
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sgraph
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Slinear_solver
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Ssat
	            $(MKDIR) temp$S$(INSTALL_DIR)$Sinclude$Sutil
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


#credits
	$(COPY) LICENSE-2.0.txt temp$S$(INSTALL_DIR)
	$(COPY) tools$SREADME.cc.java.csharp temp$S$(INSTALL_DIR)$SREADME
	$(COPY) tools$SMakefile.cc temp$S$(INSTALL_DIR)$SMakefile

cc_archive:

	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) temp$S$(INSTALL_DIR)$Slib
	$(COPY) examples$Scpp$S*.cc temp$S$(INSTALL_DIR)$Sexamples$Scpp
	$(COPY) examples$Scpp$S*.h temp$S$(INSTALL_DIR)$Sexamples$Scpp
	$(COPY) src$Salgorithms$S*.h temp$S$(INSTALL_DIR)$Sinclude$Salgorithms
	$(COPY) src$Sbase$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sbase
	$(COPY) src$Sconstraint_solver$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sconstraint_solver
	$(COPY) src$Sgen$Sconstraint_solver$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sconstraint_solver
	$(COPY) src$Sbop$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sbop
	$(COPY) src$Sgen$Sbop$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sbop
	$(COPY) src$Sglop$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sglop
	$(COPY) src$Sgen$Sglop$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Sglop
	$(COPY) src$Sgraph$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sgraph
	$(COPY) src$Sgen$Sgraph$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sgraph
	$(COPY) src$Slinear_solver$S*.h temp$S$(INSTALL_DIR)$Sinclude$Slinear_solver
	$(COPY) src$Sgen$Slinear_solver$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Slinear_solver
	$(COPY) src$Ssat$S*.h temp$S$(INSTALL_DIR)$Sinclude$Ssat
	$(COPY) src$Sgen$Ssat$S*.pb.h temp$S$(INSTALL_DIR)$Sinclude$Ssat
	$(COPY) src$Sutil$S*.h temp$S$(INSTALL_DIR)$Sinclude$Sutil




ifeq "$(SYSTEM)" "win"
	cd temp$S$(INSTALL_DIR) && ..$S..$Stools$Star.exe -C ..$S.. -c -v --exclude *svn* --exclude *roadef* examples$Sdata | ..$S..$Stools$Star.exe xvm

	cd temp$S$(INSTALL_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v gflags | ..$S..$S..$Stools$Star.exe xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v google | ..$S..$S..$Stools$Star.exe xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v sparsehash | ..$S..$S..$Stools$Star.exe xvm
else
	$(COPY) -R examples$Sdata$Set_jobshop$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Set_jobshop
	$(COPY) -R examples$Sdata$Sflexible_jobshop$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sflexible_jobshop
	$(COPY) -R examples$Sdata$Sjobshop$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sjobshop
	$(COPY) -R examples$Sdata$Smultidim_knapsack$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Smultidim_knapsack
	$(COPY) -R examples$Sdata$Scvrptw$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Scvrptw
	$(COPY) -R examples$Sdata$Spdptw$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Spdptw

	cd temp$S$(INSTALL_DIR)$Sinclude && tar -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v gflags | tar xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && tar -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v google | tar xvm
	cd temp$S$(INSTALL_DIR)$Sinclude && tar -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v sparsehash | tar xvm

ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools/install_libortools_mac.sh temp/$(INSTALL_DIR)
	chmod 775 temp/$(INSTALL_DIR)/install_libortools_mac.sh
endif
endif

dotnet_archive:

	$(COPY) bin$SGoogle.Protobuf.dll temp$S$(INSTALL_DIR)$Sbin$Scsharp
	$(COPY) bin$S$(CLR_DLL_NAME).dll temp$S$(INSTALL_DIR)$Sbin$Scsharp
	$(COPY) examples$Scsharp$S*.cs temp$S$(INSTALL_DIR)$Sexamples$Scsharp
	$(COPY) examples$Scsharp$Ssolution$SProperties$S*.cs temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution$SProperties
	$(COPY) examples$Sdata$Sdiscrete_tomography$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
	$(COPY) examples$Sdata$Sfill_a_pix$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	$(COPY) examples$Sdata$Sminesweeper$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	$(COPY) examples$Sdata$Srogo$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Srogo
	$(COPY) examples$Sdata$Ssurvo_puzzle$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	$(COPY) examples$Sdata$Squasigroup_completion$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion

ifeq "$(SYSTEM)" "win"
	$(COPY) examples$Scsharp$S*.sln temp$S$(INSTALL_DIR)$Sexamples
	$(COPY) examples$Scsharp$Ssolution$S*.csproj temp$S$(INSTALL_DIR)$Sexamples$Scsharp$Ssolution
else
	$(COPY) lib$Slib$(CLR_DLL_NAME).so temp$S$(INSTALL_DIR)$Sbin$Scsharp
endif

java_archive:
	$(COPY) lib$S*.jar temp$S$(INSTALL_DIR)$Slib
	$(COPY) lib$S$(LIB_PREFIX)jni*.$(JNI_LIB_EXT) temp$S$(INSTALL_DIR)$Slib

	$(COPY) examples$Sdata$Sdiscrete_tomography$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sdiscrete_tomography
	$(COPY) examples$Sdata$Sfill_a_pix$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sfill_a_pix
	$(COPY) examples$Sdata$Sminesweeper$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Sminesweeper
	$(COPY) examples$Sdata$Srogo$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Srogo
	$(COPY) examples$Sdata$Ssurvo_puzzle$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	$(COPY) examples$Sdata$Squasigroup_completion$S* temp$S$(INSTALL_DIR)$Sexamples$Sdata$Squasigroup_completion
	$(COPY) examples$Scom$Sgoogle$Sortools$Ssamples$S*.java temp$S$(INSTALL_DIR)$Sexamples$Scom$Sgoogle$Sortools$Ssamples


ifeq "$(SYSTEM)" "win"
fz_archive: fz
	mkdir temp
	mkdir temp$S$(FZ_INSTALL_DIR)
	mkdir temp$S$(FZ_INSTALL_DIR)$Sbin
	mkdir temp$S$(FZ_INSTALL_DIR)$Slib
	mkdir temp$S$(FZ_INSTALL_DIR)$Sshare
	mkdir temp$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc
	mkdir temp$S$(FZ_INSTALL_DIR)$Sexamples
	$(COPY) LICENSE-2.0.txt temp$S$(FZ_INSTALL_DIR)
	$(COPY) bin$Sfz$E temp$S$(FZ_INSTALL_DIR)$Sbin$Sfzn-or-tools$E
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) temp$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX) temp$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) src$Sflatzinc$Smznlib$S* temp$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc
	$(COPY) examples$Sflatzinc$S* temp$S$(FZ_INSTALL_DIR)$Sexamples
	cd temp && ..$Stools$Szip.exe -r ..$S$(FZ_INSTALL_DIR).zip $(FZ_INSTALL_DIR)
	-$(DELREC) temp
else
fz_archive: $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) $(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX)
	-$(DELREC) temp
	mkdir temp
	mkdir temp$S$(FZ_INSTALL_DIR)
	mkdir temp$S$(FZ_INSTALL_DIR)$Sbin
	mkdir temp$S$(FZ_INSTALL_DIR)$Slib
	mkdir temp$S$(FZ_INSTALL_DIR)$Sshare
	mkdir temp$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc
	mkdir temp$S$(FZ_INSTALL_DIR)$Sexamples
	$(COPY) LICENSE-2.0.txt temp$S$(FZ_INSTALL_DIR)
	$(COPY) bin$Sfz$E temp$S$(FZ_INSTALL_DIR)$Sbin$Sfzn-or-tools$E
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) temp$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)fz.$(LIB_SUFFIX) temp$S$(FZ_INSTALL_DIR)$Slib
	$(COPY) src$Sflatzinc$Smznlib$S* temp$S$(FZ_INSTALL_DIR)$Sshare$Sminizinc
	$(COPY) examples$Sflatzinc$S* temp$S$(FZ_INSTALL_DIR)$Sexamples
ifeq ($(PLATFORM),LINUX)
	$(DEP_BIN_DIR)$Spatchelf --set-rpath '$$ORIGIN/../lib' temp$S$(FZ_INSTALL_DIR)$Sbin$Sfzn-or-tools
endif
ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools$Sfix_fz_libraries_on_mac.sh temp$S$(FZ_INSTALL_DIR)
	chmod u+x temp/$(FZ_INSTALL_DIR)$Sfix_fz_libraries_on_mac.sh
	cd temp$S$(FZ_INSTALL_DIR) && .$Sfix_fz_libraries_on_mac.sh
	$(RM) temp$S$(FZ_INSTALL_DIR)$Sfix_fz_libraries_on_mac.sh
endif
	cd temp && tar cvzf ..$S$(FZ_INSTALL_DIR).tar.gz $(FZ_INSTALL_DIR)
	-$(DELREC) temp
endif


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
ifeq ($(PLATFORM),MACOSX)
	cd temp$S$(INSTALL_DIR) && ./install_libortools_mac.sh
endif
	cd temp$S$(INSTALL_DIR) && $(MAKE) test
	-$(DELREC) $(INSTALL_DIR)
	$(RENAME) lib2 lib

test_fz_archive: $(FZ_INSTALL_DIR)$(ARCHIVE_EXT)
	-$(DELREC) temp
	$(MKDIR) temp
#this is to make sure the archive tests don't use the root libraries
	$(RENAME) lib lib2
ifeq "$(SYSTEM)" "win"
	tools$Sunzip.exe $(FZ_INSTALL_DIR).zip -d temp
else
	tar -x -v -f $(FZ_INSTALL_DIR).tar.gz -C temp
endif
	cd temp$S$(FZ_INSTALL_DIR) && .$Sbin$S$(FZ_EXE) examples$Scircuit_test.fzn
	-$(DELREC) $(INSTALL_DIR)
	$(RENAME) lib2 lib
