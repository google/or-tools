install: cc_install $(PATCHELF)
ifeq ($(PLATFORM),LINUX)
	dependencies/install/bin/patchelf --set-rpath '$ORIGIN' $(TARGET_DIR)/lib/libcvrptw_lib.so
	dependencies/install/bin/patchelf --set-rpath '$ORIGIN' $(TARGET_DIR)/lib/libdimacs.so
	dependencies/install/bin/patchelf --set-rpath '$ORIGIN' $(TARGET_DIR)/lib/libfap.so
else
ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools/install_libortools_mac.sh $(TARGET_DIR)
	chmod 775 $(TARGET_DIR)/install_libortools_mac.sh
	cd $(TARGET_DIR) && ./install_libortools_mac.sh && rm install_libortools_mac.sh
endif
endif

create_install_dirs:
	-$(MKDIR) $(TARGET_DIR)
	-$(MKDIR) $(TARGET_DIR)$Slib
	-$(MKDIR) $(TARGET_DIR)$Sobjs
	-$(MKDIR) $(TARGET_DIR)$Sbin
	-$(MKDIR) $(TARGET_DIR)$Sinclude
	-$(MKDIR) $(TARGET_DIR)$Sinclude$Sgflags
	-$(MKDIR) $(TARGET_DIR)$Sinclude$Sglog
	-$(MKDIR) $(TARGET_DIR)$Sinclude$Sgoogle
	-$(DELREC) $(TARGET_DIR)$Sinclude$Sortools
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Salgorithms
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Sbase
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Sbop
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Sconstraint_solver
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Sglop
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Sgraph
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Slinear_solver
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Slp_data
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Ssat
	$(MKDIR) $(TARGET_DIR)$Sinclude$Sortools$Sutil
	-$(MKDIR) $(TARGET_DIR)$Sexamples
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Scpp
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Set_jobshop
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Sflexible_jobshop
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Sjobshop
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Smultidim_knapsack
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Scvrptw
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Spdptw
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Sfill_a_pix
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Sminesweeper
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Srogo
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Ssurvo_puzzle
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Squasigroup_completion
	-$(MKDIR) $(TARGET_DIR)$Sexamples$Sdata$Sdiscrete_tomography

cc_install: create_install_dirs cc
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)cvrptw_lib.$(LIB_SUFFIX) $(TARGET_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)dimacs.$(LIB_SUFFIX) $(TARGET_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) $(TARGET_DIR)$Slib
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)fap.$(LIB_SUFFIX) $(TARGET_DIR)$Slib
	$(COPY) examples$Scpp$S*.cc $(TARGET_DIR)$Sexamples$Scpp
	$(COPY) examples$Scpp$S*.h $(TARGET_DIR)$Sexamples$Scpp
	$(COPY) ortools$Salgorithms$S*.h $(TARGET_DIR)$Sinclude$Sortools$Salgorithms
	$(COPY) ortools$Sbase$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sbase
	$(COPY) ortools$Sconstraint_solver$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sconstraint_solver
	$(COPY) ortools$Sgen$Sortools$Sconstraint_solver$S*.pb.h $(TARGET_DIR)$Sinclude$Sortools$Sconstraint_solver
	$(COPY) ortools$Sbop$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sbop
	$(COPY) ortools$Sgen$Sortools$Sbop$S*.pb.h $(TARGET_DIR)$Sinclude$Sortools$Sbop
	$(COPY) ortools$Sglop$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sglop
	$(COPY) ortools$Sgen$Sortools$Sglop$S*.pb.h $(TARGET_DIR)$Sinclude$Sortools$Sglop
	$(COPY) ortools$Sgraph$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sgraph
	$(COPY) ortools$Sgen$Sortools$Sgraph$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sgraph
	$(COPY) ortools$Slinear_solver$S*.h $(TARGET_DIR)$Sinclude$Sortools$Slinear_solver
	$(COPY) ortools$Slp_data$S*.h $(TARGET_DIR)$Sinclude$Sortools$Slp_data
	$(COPY) ortools$Sgen$Sortools$Slinear_solver$S*.pb.h $(TARGET_DIR)$Sinclude$Sortools$Slinear_solver
	$(COPY) ortools$Ssat$S*.h $(TARGET_DIR)$Sinclude$Sortools$Ssat
	$(COPY) ortools$Sgen$Sortools$Ssat$S*.pb.h $(TARGET_DIR)$Sinclude$Sortools$Ssat
	$(COPY) ortools$Sutil$S*.h $(TARGET_DIR)$Sinclude$Sortools$Sutil

ifeq "$(SYSTEM)" "win"
	cd $(TARGET_DIR) && ..$S..$Stools$Star.exe -C ..$S.. -c -v --exclude *svn* --exclude *roadef* examples$Sdata | ..$S..$Stools$Star.exe xvm

	cd $(TARGET_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v gflags | ..$S..$S..$Stools$Star.exe xvm
	cd $(TARGET_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v glog | ..$S..$S..$Stools$Star.exe xvm
	cd $(TARGET_DIR)$Sinclude && ..$S..$S..$Stools$Star.exe -C ..$S..$S..$Sdependencies$Sinstall$Sinclude -c -v google | ..$S..$S..$Stools$Star.exe xvm
else
	$(COPY) -R examples$Sdata$Set_jobshop$S* $(TARGET_DIR)$Sexamples$Sdata$Set_jobshop
	$(COPY) -R examples$Sdata$Sflexible_jobshop$S* $(TARGET_DIR)$Sexamples$Sdata$Sflexible_jobshop
	$(COPY) -R examples$Sdata$Sjobshop$S* $(TARGET_DIR)$Sexamples$Sdata$Sjobshop
	$(COPY) -R examples$Sdata$Smultidim_knapsack$S* $(TARGET_DIR)$Sexamples$Sdata$Smultidim_knapsack
	$(COPY) -R examples$Sdata$Scvrptw$S* $(TARGET_DIR)$Sexamples$Sdata$Scvrptw
	$(COPY) -R examples$Sdata$Spdptw$S* $(TARGET_DIR)$Sexamples$Sdata$Spdptw
	tar -cf - -C dependencies$Sinstall$Sinclude gflags | tar -xpf - -C $(TARGET_DIR)$Sinclude
	tar -cf - -C dependencies$Sinstall$Sinclude glog | tar -xpf - -C $(TARGET_DIR)$Sinclude
	tar -cf - -C dependencies$Sinstall$Sinclude google | tar -xpf - -C $(TARGET_DIR)$Sinclude
endif
	$(COPY) LICENSE-2.0.txt $(TARGET_DIR)
	$(COPY) tools$SREADME.cc.java.csharp $(TARGET_DIR)$SREADME
	$(COPY) tools$SMakefile.cc $(TARGET_DIR)$SMakefile
