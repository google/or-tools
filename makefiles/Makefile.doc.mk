# Generate documentation

cpp-doc:
	doxygen tools/doc/cpp_algorithms.doxy
	doxygen tools/doc/cpp_graph.doxy
	doxygen tools/doc/cpp_linear.doxy
	doxygen tools/doc/cpp_routing.doxy
	doxygen tools/doc/cpp_sat.doxy

dotnet-doc:
	doxygen tools/doc/dotnet.doxy

java-doc:
	doxygen tools/doc/java.doxy

python-doc:
	$(SET_PYTHONPATH) pdoc3 --force --html --template-dir tools/doc/templates -o docs/python/ortools/sat/python/ ortools/sat/python/cp_model.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/util/ ortools/gen/ortools/util/sorted_interval_list.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/linear_solver ortools/gen/ortools/linear_solver/pywraplp.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/constraint_solver ortools/gen/ortools/constraint_solver/pywrapcp.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/algorithms ortools/gen/ortools/algorithms/pywrapknapsack_solver.py
	$(SET_PYTHONPATH) pdoc3 --html --force --template-dir tools/doc/templates -o docs/python/ortools/graph ortools/gen/ortools/graph/pywrapgraph.py
