# ---------- Archive support ----------
.PHONY: help_archive # Generate list of Archive targets with descriptions.
help_archive:
	@echo Use one of the following Archive targets:
ifeq ($(PLATFORM),WIN64)
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.archive.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/"
	@echo off & echo(
else
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.archive.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo
endif

# Main target

################
##  Cleaning  ##
################
.PHONY: clean_archive # Clean Archive output from previous build.
clean_archive:
	-$(DELREC) $(INSTALL_CPP_NAME)
	-$(DELREC) $(INSTALL_CPP_NAME)$(ARCHIVE_EXT)
	-$(DELREC) $(INSTALL_DOTNET_NAME)
	-$(DELREC) $(INSTALL_DOTNET_NAME)$(ARCHIVE_EXT)
	-$(DELREC) $(INSTALL_JAVA_NAME)
	-$(DELREC) $(INSTALL_JAVA_NAME)$(ARCHIVE_EXT)
	-$(DELREC) $(INSTALL_PYTHON_NAME)
	-$(DELREC) $(INSTALL_PYTHON_NAME)$(ARCHIVE_EXT)

#############
##  DEBUG  ##
#############
.PHONY: detect_archive # Show variables used to build archive OR-Tools.
detect_archive:
	@echo Relevant info for the archive build:
	@echo ARCHIVE_EXT = $(ARCHIVE_EXT)
ifeq ($(PLATFORM),WIN64)
	@echo off & echo(
else
	@echo
endif
