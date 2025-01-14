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

# Generate documentation
.PHONY: help_doc # Generate list of Documentation targets with descriptions.
help_doc:
	@echo Use one of the following Documentation targets:
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.doc.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo


# Main target
.PHONY: doc # Create doxygen and python documentation.
doc: doxy-doc python-doc java-doc

.PHONY: doxy-doc # Create doxygen ref documentation.
doxy-doc: cpp python java dotnet
	bash -c "command -v doxygen"
	python3 tools/doc/gen_ref_doc.py

.PHONY: java-doc # Create Javadoc ref documentation.
java-doc: java
	bash -c "command -v mvn"
	tools/doc/gen_javadoc.sh

.PHONY: python-doc # Create python documentation.
python-doc: python
	bash -c "command -v pdoc"
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ \
 --no-search -d google \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 build_make/python/ortools
