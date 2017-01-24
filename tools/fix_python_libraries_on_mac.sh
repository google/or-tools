export P=`otool -L $1/ortools/ortools/libortools.dylib | grep -v ':' | grep libortools | cut -d '(' -f 1`
echo PYPI_ARCHIVE_TEMP_DIR = $1
echo path to change = $P
install_name_tool -change $P @loader_path/../libortools.dylib $1/ortools/ortools/algorithms/_pywrapknapsack_solver.so
install_name_tool -change $P @loader_path/../libortools.dylib $1/ortools/ortools/constraint_solver/_pywrapcp.so
install_name_tool -change $P @loader_path/../libortools.dylib $1/ortools/ortools/graph/_pywrapgraph.so
install_name_tool -change $P @loader_path/../libortools.dylib $1/ortools/ortools/linear_solver/_pywraplp.so
