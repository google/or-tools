
archive: $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) csharp java create_dirs cc_archive dotnet_archive java_archive
ifeq "$(SYSTEM)" "win"
	cd temp && ..$Stools$Szip.exe -r ..$S$(INSTALL_DIR).zip $(INSTALL_DIR)
else
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
	-$(DELREC) temp
	mkdir temp
	mkdir temp$S$(INSTALL_DIR)
	mkdir temp$S$(INSTALL_DIR)$Sbin
	mkdir temp$S$(INSTALL_DIR)$Sshare
	mkdir temp$S$(INSTALL_DIR)$Sshare$Sminizinc
	$(COPY) LICENSE-2.0.txt temp$S$(INSTALL_DIR)
	$(COPY) bin$Sfz.exe temp$S$(INSTALL_DIR)$Sbin$Sfzn-or-tools.exe
	$(COPY) src$Sflatzinc$Smznlib$S*.mzn temp$S$(INSTALL_DIR)$Sshare$Sminizinc
	cd temp && ..$Stools$Szip.exe -r ..$Sor-tools.flatzinc.$(PORT)-$(OR_TOOLS_VERSION).zip $(INSTALL_DIR)
	-$(DELREC) temp
else
fz_archive: $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX)
	
	mkdir temp
	mkdir temp$S$(INSTALL_DIR)
	mkdir temp$S$(INSTALL_DIR)$Sbin
	mkdir temp$S$(INSTALL_DIR)$Sshare
	mkdir temp$S$(INSTALL_DIR)$Sshare$Sminizinc
	$(COPY) LICENSE-2.0.txt temp$S$(INSTALL_DIR)
	$(COPY) bin$Sfz temp$S$(INSTALL_DIR)$Sbin$Sfzn-or-tools
	$(COPY) src$Sflatzinc$Smznlib$S* temp$S$(INSTALL_DIR)$Sshare$Sminizinc
	cd temp && tar cvzf ..$Sor-tools.flatzinc.$(PORT)-$(OR_TOOLS_VERSION).tar.gz $(INSTALL_DIR)
	-$(DELREC) temp
endif



test_archive: archive
	-$(DELREC) temp
	$(MKDIR) temp
ifeq "$(SYSTEM)" "win"
	tools$Sunzip.exe $(INSTALL_DIR).zip -d temp
else
	tar -x -v -f $(INSTALL_DIR).tar.gz -C temp
endif
	cd temp$S$(INSTALL_DIR) && $(MAKE) all test
	-$(DELREC) $(INSTALL_DIR)
	

