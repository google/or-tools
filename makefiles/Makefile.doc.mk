# Generate documentation

cpp-doc:
	doxygen tools/cpp_graph.doxy
	doxygen tools/cpp_linear.doxy
	doxygen tools/cpp_routing.doxy
	doxygen tools/cpp_sat.doxy

dotnet-doc:
	doxygen tools/dotnet.doxy

java-doc:
	doxygen tools/java.doxy

python-doc:
	$(SET_PYTHONPATH) pdoc3 --html -o docs/python/ortools/sat/python/ ortools/sat/python/cp_model.py
	$(SET_PYTHONPATH) pdoc3 --html -o docs/python/ortools/util/ ortools/gen/ortools/util/sorted_interval_list.py
