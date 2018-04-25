install_deps_dirs:
	-$(MKDIR_P) "$(prefix)$Sinclude$Sgflags"
	-$(MKDIR_P) "$(prefix)$Sinclude$Sglog"
	-$(MKDIR_P) "$(prefix)$Sinclude$Sgoogle"

install_full_cc: install_cc install_deps_dirs
ifeq ($(SYSTEM),win)
	tools\tar.exe -cf - -C dependencies$Sinstall$Sinclude gflags | tools\tar.exe -xpf - -C "$(prefix)$Sinclude"
	tools\tar.exe -cf - -C dependencies$Sinstall$Sinclude glog | tools\tar.exe -xpf - -C "$(prefix)$Sinclude"
	tools\tar.exe -cf - -C dependencies$Sinstall$Sinclude google | tools\tar.exe -xpf - -C "$(prefix)$Sinclude"
else
	tar -cf - -C dependencies$Sinstall$Sinclude gflags | tar -xpf - -C "$(prefix)$Sinclude"
	tar -cf - -C dependencies$Sinstall$Sinclude glog | tar -xpf - -C "$(prefix)$Sinclude"
	tar -cf - -C dependencies$Sinstall$Sinclude google | tar -xpf - -C "$(prefix)$Sinclude"
endif
	$(COPY) LICENSE-2.0.txt "$(prefix)"
ifeq ($(PLATFORM),MACOSX)
	$(COPY) tools/install_libortools_mac.sh "$(prefix)"
	chmod 775 "$(prefix)/install_libortools_mac.sh"
	cd "$(prefix)" && ./install_libortools_mac.sh && rm install_libortools_mac.sh
endif
