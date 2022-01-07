#!/usr/bin/env python3
from collections import namedtuple
from ortools.constraint_solver import pywrapcp

VEHICLE_COUNT = 30
VEHICLE_CAPACITY = 200
Customer = namedtuple("Customer", ['index', 'demand', 'x', 'y'])

print('Init')

customers = list()
customers.append(Customer(0, 0, 0, 0))
customers.append(Customer(1, 1, 1.0, 1.0))
customers.append(Customer(1, 1, 2.0, 2.0))
customer_count = len(customers)

manager = pywrapcp.RoutingIndexManager(3, VEHICLE_COUNT, 0)
routing = pywrapcp.RoutingModel(manager)

print('Demand Constraint')
demands = []
for i in range(0, customer_count):
    demands.append(customers[i][1])
routing.AddVectorDimension(demands, VEHICLE_CAPACITY, True, "Demand")

print('Adding Costs')


def distance_callback(from_index, to_index):
    #static just for the sake of the example
    return 1

transit_callback_index = routing.RegisterTransitCallback(distance_callback)
routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)

routing.CloseModel()

assignment = routing.Solve(None)

# Inspect solution and extract routes
routes = []
for i in range(0, routing.vehicles()):

    route_number = i
    routes.append([])
    node = routing.Start(route_number)
    route = []
    route.append(0)
    if routing.IsVehicleUsed(assignment, i):
        while True:
            node = assignment.Value(routing.NextVar(node))

            if not routing.IsEnd(node):
                route.append(int(node))
            else:
                break

    route.append(0)
    routes[route_number].append(route)

#This are the routes as list of lists
routes = [el[0] for el in routes]

#Now try to read the routes into a new assigment object fails
assignment2 = routing.ReadAssignmentFromRoutes(routes, True)
