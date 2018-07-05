install_deps_dirs:
	-$(MKDIR_P) "$(prefix)$Sinclude$Sgflags"
	-$(MKDIR_P) "$(prefix)$Sinclude$Sglog"
	-$(MKDIR_P) "$(prefix)$Sinclude$Sgoogle"

install_full_cc: install_cc install_deps_dirs
	$(TAR) -cf - -C dependencies$Sinstall$Sinclude gflags | $(TAR) -xpf - -C "$(prefix)$Sinclude"
	$(TAR) -cf - -C dependencies$Sinstall$Sinclude glog | $(TAR) -xpf - -C "$(prefix)$Sinclude"
	$(TAR) -cf - -C dependencies$Sinstall$Sinclude google | $(TAR) -xpf - -C "$(prefix)$Sinclude"
	$(COPY) LICENSE-2.0.txt "$(prefix)"
ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools/install_libortools_mac.sh "$(prefix)"
	chmod 775 "$(prefix)/install_libortools_mac.sh"
	cd "$(prefix)" && ./install_libortools_mac.sh && rm install_libortools_mac.sh
endif
