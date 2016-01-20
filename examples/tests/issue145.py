#!/usr/bin/python
# -*- coding: utf-8 -*-


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


#THIS IS THE FUNCTION THAT CAUSES PROBLEMS
def collect_cumulative_dimension_at_node(routing_model, assignment_solution, routes, dimension="Time"):

    result = list()
    for index,route in enumerate(routes):
        row = list()
        for index,el in enumerate(route[1:-1]):

            #when only the transition times all seems ok
            cum_time=assignment_solution.Value( routing_model.CumulVar(el,dimension))

            transit_time=0
            idle_time=0

            #Comment out next two lines and this works
            #BOOM !!! Collecting transit times will cause segmentation fault
            transit_time = assignment_solution.Value(routing_model.TransitVar(el,dimension))

            #BOOM !!! Collecting idle times will cause segmentation fault
            idle_time = assignment_solution.Value(routing_model.SlackVar(el,dimension))
            row.append((el,cum_time,transit_time,idle_time))
        result.append(row)

    return result



def solve_it():


    print("customer_count = {0}".format(CUSTOMER_COUNT))
    print("vehicle_count = {0}".format(VEHICLE_COUNT))
    print("vehicle_capacity = {0}".format(VEHICLE_CAPACITY))

    random.seed(42)

    #Generate randomly distributed customers
    for i in range(1, CUSTOMER_COUNT+1):
        ts = int((MAX_TIME - 3600*2) *random.random())
        customer = Customer(i-1, 1, int(MAX_X*random.random()),
                                    int(MAX_Y*random.random()),ts,ts+2*3600)
        print("customer={}, time_start={}, time_stop={}".format(i ,customer.time_start, customer.time_stop))

        customers.append(customer)

    print(' ----------------------------------------------- Parameters ------------------------------------------------')
    routing = pywrapcp.RoutingModel(CUSTOMER_COUNT, VEHICLE_COUNT)


    parameters = pywrapcp.RoutingSearchParameters()
    parameters.first_solution = 'PathCheapestArc'
    parameters.guided_local_search=True

    print(' ----------------------------------------------- Transport Cost ------------------------------------------------')
    routing.SetCost(length)
    routing.SetDepot(0)

    print(' ----------------------------------------------- ServiceTimes -------------------------------------')
    routing.AddDimension(time_plus_service_time,MAX_TIME,MAX_TIME,True,"Time")
    time_dimension  = routing.GetDimensionOrDie("Time")

    for i in range(1, routing.nodes()):
        customer = customers[i]
        time_dimension.CumulVar(i).SetRange(customer.time_start,customer.time_stop)
        routing.AddToAssignment(time_dimension.TransitVar(i))
        routing.AddToAssignment(time_dimension.SlackVar(i))
    print(' ----------------------------------------------- Solve ------------------------------------------------')
    assignment = routing.Solve()


    # Inspect solution.
    print("routing.vehicles:", routing.vehicles())
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
    routes=routes[0]


    print(collect_cumulative_dimension_at_node(routing, assignment, routes, dimension="Time"))


if __name__ == '__main__':
    solve_it()