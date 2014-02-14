dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' temp/ortools/ortools/constraint_solver/_pywrapcp.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' temp/ortools/ortools/linear_solver/_pywraplp.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' temp/ortools/ortools/algorithms/_pywrapknapsack_solver.so
dependencies/install/bin/patchelf --set-rpath '$ORIGIN/../../ortools' temp/ortools/ortools/graph/_pywrapgraph.so
