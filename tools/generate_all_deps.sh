tools/generate_deps.sh BASE base
tools/generate_deps.sh UTIL util base
tools/generate_deps.sh LP_DATA lp_data util base algorithms
tools/generate_deps.sh GLOP glop util base linear_solver lp_data
tools/generate_deps.sh GRAPH graph base util
tools/generate_deps.sh ALGORITHMS algorithms base util graph linear_solver
tools/generate_deps.sh SAT sat base util algorithms graph linear_solver lp_data glop
tools/generate_deps.sh BOP bop base util glop lp_data sat
tools/generate_deps.sh LP linear_solver base util glop lp_data bop
tools/generate_deps.sh CP constraint_solver base util graph linear_solver sat
