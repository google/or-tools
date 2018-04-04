#!/usr/bin/env bash
set -e
./vrp_svg.py -h
set -x

# Simplest Problem
./vrp_svg.py > vrp.svg

./vrp_svg.py --solution > vrp_solution.svg
./vrp_svg.py --global-span --solution > vrpgs_solution.svg

# Capacity Problem
./vrp_svg.py --capacity > cvrp.svg
./vrp_svg.py --capacity --solution > cvrp_solution.svg

# Time Window Problem
./vrp_svg.py --capacity --time-window > cvrptw.svg
./vrp_svg.py --capacity --time-window --solution > cvrptw_solution.svg

## Fuel Problem
#./vrp_svg.py --fuel > vrpf.svg
#./vrp_svg.py --fuel --solution > vrpf_solution.svg
#
## Ressource Problem
#./vrp_svg.py --resource > vrpr.svg
#./vrp_svg.py --resource --solution > vrpr_solution.svg
