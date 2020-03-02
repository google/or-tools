# Generate documentation
.PHONY: help_doc # Generate list of Documentation targets with descriptions.
help_doc:
	@echo Use one of the following Documentation targets:
	@$(GREP) "^.PHONY: .* #" $(CURDIR)/makefiles/Makefile.doc.mk | $(SED) "s/\.PHONY: \(.*\) # \(.*\)/\1\t\2/" | expand -t20
	@echo


# Main target
.PHONY: doc # Create doxygen and python documentation.
doc: doxy-doc python-doc

.PHONY: doxy-doc # Create doxygen ref documentation.
doxy-doc: cc python java dotnet
	bash -c "command -v doxygen"
	python3 tools/doc/gen_ref_doc.py

.PHONY: python-doc # Create python documentation.
python-doc:
	bash -c "command -v pdoc3"
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/sat/python/ ortools/sat/python/cp_model.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/util/ ortools/gen/ortools/util/sorted_interval_list.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/linear_solver ortools/gen/ortools/linear_solver/pywraplp.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/constraint_solver ortools/gen/ortools/constraint_solver/pywrapcp.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/algorithms ortools/gen/ortools/algorithms/pywrapknapsack_solver.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/graph ortools/gen/ortools/graph/pywrapgraph.py
