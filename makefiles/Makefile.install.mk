install_deps_dirs:
	-$(MKDIR_P) "$(prefix)$Sinclude$Sgflags"
	-$(MKDIR_P) "$(prefix)$Sinclude$Sglog"
	-$(MKDIR_P) "$(prefix)$Sinclude$Sgoogle"

install_full_cc: install_cc install_deps_dirs
	$(TAR) -cf - -C dependencies$Sinstall$Sinclude gflags | $(TAR) -xpf - -C "$(prefix)$Sinclude"
	$(TAR) -cf - -C dependencies$Sinstall$Sinclude glog | $(TAR) -xpf - -C "$(prefix)$Sinclude"
	$(TAR) -cf - -C dependencies$Sinstall$Sinclude google | $(TAR) -xpf - -C "$(prefix)$Sinclude"
	$(COPY) LICENSE-2.0.txt "$(prefix)"
