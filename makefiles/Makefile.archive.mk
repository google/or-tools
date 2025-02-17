# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
