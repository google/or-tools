create_install_dirs:
	-$(MKDIR) $(TARGET_DIR)
	-$(MKDIR) $(TARGET_DIR)$Slib
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

install_cc: create_install_dirs cc $(PATCHELF)
	$(COPY) $(LIB_DIR)$S$(LIB_PREFIX)ortools.$(LIB_SUFFIX) $(TARGET_DIR)$Slib
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
	tools\tar.exe -cf - -C dependencies$Sinstall$Sinclude gflags | tools\tar.exe -xpf - -C $(TARGET_DIR)$Sinclude
	tools\tar.exe -cf - -C dependencies$Sinstall$Sinclude glog | tools\tar.exe -xpf - -C $(TARGET_DIR)$Sinclude
	tools\tar.exe -cf - -C dependencies$Sinstall$Sinclude google | tools\tar.exe -xpf - -C $(TARGET_DIR)$Sinclude
else
	tar -cf - -C dependencies$Sinstall$Sinclude gflags | tar -xpf - -C $(TARGET_DIR)$Sinclude
	tar -cf - -C dependencies$Sinstall$Sinclude glog | tar -xpf - -C $(TARGET_DIR)$Sinclude
	tar -cf - -C dependencies$Sinstall$Sinclude google | tar -xpf - -C $(TARGET_DIR)$Sinclude
endif
	$(COPY) LICENSE-2.0.txt $(TARGET_DIR)
ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools/install_libortools_mac.sh $(TARGET_DIR)
	chmod 775 $(TARGET_DIR)/install_libortools_mac.sh
	cd $(TARGET_DIR) && ./install_libortools_mac.sh && rm install_libortools_mac.sh
endif
