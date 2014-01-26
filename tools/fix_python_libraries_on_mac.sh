export P=`otool -L temp/ortools/lib/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo path to change = $P
install_name_tool -change $P @loader_path/../../lib/libortools.dylib temp/ortools/ortools/algorithms/_pywrapknapsack_solver.so
install_name_tool -change $P @loader_path/../../lib/libortools.dylib temp/ortools/ortools/constraint_solver/_pywrapcp.so
install_name_tool -change $P @loader_path/../../lib/libortools.dylib temp/ortools/ortools/graph/_pywrapgraph.so
install_name_tool -change $P @loader_path/../../lib/libortools.dylib temp/ortools/ortools/linear_solver/_pywraplp.so
