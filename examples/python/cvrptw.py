#!/usr/bin/python
# -*- coding: utf-8 -*-

"""
  Capacitated Vehicle Routing Problem with Time Windows (and optional orders).
  A description of the problem can be found here:
  http://en.wikipedia.org/wiki/Vehicle_routing_problem.
  The variant which is tackled by this model includes a capacity dimension,
  time windows and optional orders, with a penalty cost if orders are not
  performed. For the sake of simplicty, orders are randomly located and
  distances are computed using the L2 distance. Distances are assumed
  to be in Km and times in seconds.
"""

import random
import math
from collections import namedtuple
from ortools.constraint_solver import pywrapcp

AVERAGE_SPEED=10
CUSTOMER_COUNT = 10
VEHICLE_COUNT = 2
VEHICLE_CAPACITY = 20
MAX_TIME=24*3600
MAX_X = 100
MAX_Y = 100


customers = []

#Customer helper class, every customer will have a 2hr window for delivery
Customer = namedtuple("Customer", ['index', 'demand', 'x', 'y', 'time_start', 'time_stop'])

def length(customer1, customer2):
    return round(math.sqrt((customers[customer1][2] - customers[customer2][2])**2 + (customers[customer1][3]- customers[customer2][3])**2))

def time_plus_service_time(customer1,customer2):
    return round( length(customer1,customer2) / AVERAGE_SPEED) + 10


def solve_it():


    print "customer_count", CUSTOMER_COUNT
    print "vehicle_count", VEHICLE_COUNT
    print "vehicle_capacity", VEHICLE_CAPACITY



    #Generate randomly distributed customers
    for i in range(1, CUSTOMER_COUNT+1):
        ts = int((MAX_TIME - 3600*2) *random.random())
        customer = Customer(i-1, 1, int(MAX_X*random.random()),
                                    int(MAX_Y*random.random()),ts,ts+2*3600)
        print "customer={}, time_start={}, time_stop={}".format(i ,customer.time_start, customer.time_stop)

        customers.append(customer)

    print ' ----------------------------------------------- Parameters ------------------------------------------------'
    routing = pywrapcp.RoutingModel(CUSTOMER_COUNT, VEHICLE_COUNT)

    # print routing.solver()

    parameters = pywrapcp.RoutingSearchParameters()
    # Setting first solution heuristic (cheapest addition).
    parameters.first_solution = 'PathCheapestArc'
    # Disabling Large Neighborhood Search, comment out to activate it.
    #parameters.no_lns = True
    parameters.guided_local_search=True

    print ' ----------------------------------------------- Transport Cost ------------------------------------------------'
    routing.SetCost(length)
    routing.SetDepot(0)

    print ' ----------------------------------------------- Demand -------------------------------------'
    demands=[]
    for i in range(0, CUSTOMER_COUNT):
        demands.append(customers[i][1])
    routing.AddVectorDimension(demands, VEHICLE_CAPACITY, True, "Demand")

    print ' ----------------------------------------------- ServiceTimes -------------------------------------'
    routing.AddDimension(time_plus_service_time,MAX_TIME,MAX_TIME,True,"Time")

    time_dimension  = routing.GetDimensionOrDie("Time")

    for i in range(1, routing.nodes()):
        customer = customers[i]
        time_dimension.CumulVar(i).SetRange(customer.time_start,customer.time_stop)


    print ' ----------------------------------------------- SKipCustomerAtCost -------------------------------------'
    # Add a penalty for skipping a particular customer
    kPenalty = 1000000000;
    for order in range(1, routing.nodes() ):
        routing.AddDisjunction([order], kPenalty)


    print ' ----------------------------------------------- Solve ------------------------------------------------'

    assignment = routing.Solve()

    # Inspect and print assignment
    if assignment:
        Objective=assignment.ObjectiveValue()
        print "obj:", assignment.ObjectiveValue()
        print "strategy:",parameters.first_solution
        print "heuristic:",parameters.no_lns
        print "staus:",routing.status()

        # Inspect solution.
        print "routing.vehicles:",routing.vehicles()
        routes=[]
        for i in range(0,routing.vehicles()):
            route_number = i
            routes.append([])
            node = routing.Start(route_number)
            route=[]
            route.append(0)
            if routing.IsVehicleUsed(assignment,i):
                while True:
                    node = assignment.Value(routing.NextVar(node))
                    if not routing.IsEnd(node):
                        route.append(int(node))
                    else :
                        break
            route.append(0)
            routes[route_number].append(route)

        #Let's compute the objective manually this will not take into account when a customer is skipped
        obj = 0
        for v in range(0, VEHICLE_COUNT):
            vehicle_tour = routes[v][0]
            if len(vehicle_tour) > 0:
                for i in range(0, len(vehicle_tour)-1):
                    obj += round(length(int(vehicle_tour[i]),int(vehicle_tour[i+1])),0)

        print "calculated obj",obj

        outputData = 'routes:\n'
        for i in range(0,routing.vehicles()):
            for j in range(0, len(routes[i])):
                outputData+=str(routes[i][j])+ ' '
            outputData += '\n'

        print outputData

    else:
      print('No solution found.')



if __name__ == '__main__':
    solve_it()