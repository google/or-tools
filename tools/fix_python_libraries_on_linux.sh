dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' $1/ortools/ortools/algorithms/_pywrapknapsack_solver.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' $1/ortools/ortools/constraint_solver/_pywrapcp.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' $1/ortools/ortools/graph/_pywrapgraph.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' $1/ortools/ortools/linear_solver/_pywraplp.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' $1/ortools/ortools/sat/_pywrapsat.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' $1/ortools/ortools/data/_pywraprcpsp.so
