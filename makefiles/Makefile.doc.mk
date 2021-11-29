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
doxy-doc: cc python java dotnet
	bash -c "command -v doxygen"
	python3 tools/doc/gen_ref_doc.py

.PHONY: java-doc # Create Javadoc ref documentation.
java-doc: java
	tools/doc/gen_javadoc.sh

.PHONY: python-doc # Create python documentation.
python-doc:
	bash -c "command -v pdoc"
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ \
 --no-search -d google \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 ortools/sat/python/cp_model.py
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ortools/util/ \
 --no-search -d google \
 --footer-text "OR-Tools ${ORTOOLS_MAJOR}.${ORTOOLS_MINOR}" \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 ortools/gen/ortools/util/sorted_interval_list.py
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ortools/linear_solver/ \
 --no-search -d google \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 ortools/gen/ortools/linear_solver/pywraplp.py
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ortools/constraint_solver/ \
 --no-search -d google \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 ortools/gen/ortools/constraint_solver/pywrapcp.py
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ortools/algorithms/ \
 --no-search -d google \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 ortools/gen/ortools/algorithms/pywrapknapsack_solver.py
	$(SET_PYTHONPATH) pdoc \
 --logo https://developers.google.com/optimization/images/orLogo.png \
 -o docs/python/ortools/graph/ \
 --no-search -d google \
 --footer-text "OR-Tools ${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}" \
 ortools/gen/ortools/graph/pywrapgraph.py
