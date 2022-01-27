#!/usr/bin/env bash
# usage: ./tools/generate_all_deps.sh > makefiles/Makefile.gen.mk
tools/generate_deps.sh BASE base
tools/generate_deps.sh PORT port base
tools/generate_deps.sh UTIL util base port
tools/generate_deps.sh INIT init
tools/generate_deps.sh SCHEDULING scheduling base port util
tools/generate_deps.sh LP_DATA lp_data util base algorithms linear_solver
tools/generate_deps.sh GLOP glop util base lp_data linear_solver
tools/generate_deps.sh GRAPH graph base util
tools/generate_deps.sh ALGORITHMS algorithms base util graph linear_solver
tools/generate_deps.sh SAT sat base util algorithms graph lp_data glop linear_solver
tools/generate_deps.sh PACKING packing base graph port util linear_solver
tools/generate_deps.sh BOP bop base util lp_data glop sat
tools/generate_deps.sh GSCIP gscip base port
tools/generate_deps.sh GUROBI gurobi base
tools/generate_deps.sh LP linear_solver base util lp_data glop bop gscip gurobi
tools/generate_deps.sh CP constraint_solver base util graph linear_solver sat
