#!/usr/bin/env bash
set -e
./routing_svg.py -h
set -x

# TSP
./routing_svg.py --tsp > tsp.svg
./routing_svg.py --tsp --solution > tsp_solution.svg

# TSP Distance Matrix
./routing_svg.py --tsp > tsp_distance_matrix.svg
./routing_svg.py --tsp --solution > tsp_distance_matrix_solution.svg

# VRP
./routing_svg.py --vrp > vrp.svg
./routing_svg.py --vrp --solution > vrp_solution.svg

# VRP Global Span
./routing_svg.py --global-span > vrp_global_span.svg
./routing_svg.py --global-span --solution > vrp_global_span_solution.svg

# VRP Capacity
./routing_svg.py --capacity > vrp_capacity.svg
./routing_svg.py --capacity --solution > vrp_capacity_solution.svg

# VRP Drop Nodes
./routing_svg.py --drop-nodes > vrp_drop_nodes.svg
./routing_svg.py --drop-nodes --solution > vrp_drop_nodes_solution.svg

# VRP Time Windows
./routing_svg.py --time-windows > vrp_time_windows.svg
./routing_svg.py --time-windows --solution > vrp_time_windows_solution.svg

# Resource Problem
./routing_svg.py --resources > vrp_resources.svg
./routing_svg.py --resources --solution > vrp_resources_solution.svg

# VRP Starts Ends
./routing_svg.py --starts-ends > vrp_starts_ends.svg
./routing_svg.py --starts-ends --solution > vrp_starts_ends_solution.svg

# VRP Pickup Delivery
./routing_svg.py --pickup-delivery > vrp_pickup_delivery.svg
# ANY
./routing_svg.py --pickup-delivery --solution > vrp_pickup_delivery_solution.svg
# FIFO
./routing_svg.py --pickup-delivery --fifo --solution > vrp_pickup_delivery_fifo_solution.svg
# LIFO
./routing_svg.py --pickup-delivery --lifo --solution > vrp_pickup_delivery_lifo_solution.svg

## Fuel Problem
#./routing_svg.py --fuel > vrpf.svg
#./routing_svg.py --fuel --solution > vrpf_solution.svg
