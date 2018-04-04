#!/usr/bin/env python
# This Python file uses the following encoding: utf-8
# Copyright 2018 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generate SVG for a VRP problem.

   Manhattan average block 750x264ft -> 228x80m
   src: https://nyti.ms/2GDoRIe "NY Times: Know Your distance"

   Distances are in meters and time in minutes.
"""

from __future__ import print_function
import argparse
import math
import sys
from six.moves import xrange
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2

###########################
# Problem Data Definition #
###########################
class Vehicle():
    """Stores the property of a vehicle"""
    def __init__(self):
        """Initializes the vehicle properties"""
        self._capacity = 15
        # Travel speed: 5km/h to convert in m/min
        self._speed = 5 / 3.6 * 60

    @property
    def capacity(self):
        """Gets vehicle capacity"""
        return self._capacity

    @property
    def speed(self):
        """Gets the average travel speed of a vehicle"""
        return self._speed

class CityBlock():
    """City block definition"""
    @property
    def width(self):
        """Gets Block size West to East"""
        return 228/2

    @property
    def height(self):
        """Gets Block size North to South"""
        return 80

class DataProblem():
    """Stores the data for the problem"""
    def __init__(self):
        """Initializes the data for the problem"""
        self._vehicle = Vehicle()
        self._num_vehicles = 4

        self._city_block = CityBlock()

        # Locations in block unit
        locations = \
                [(4, 4), # depot
                 (2, 0), (8, 0), # row 0
                 (0, 1), (1, 1),
                 (5, 2), (7, 2),
                 (3, 3), (6, 3),
                 (5, 5), (8, 5),
                 (1, 6), (2, 6),
                 (3, 7), (6, 7),
                 (0, 8), (7, 8)]
                # locations in meters using the block dimension defined
        self._locations = [(
            loc[0]*self.city_block.width,
            loc[1]*self.city_block.height) for loc in locations]
        self._depot = 0

        self._demands = \
            [0, # depot
             1, 1, # row 0
             2, 4,
             2, 4,
             8, 8,
             1, 2,
             1, 2,
             4, 4,
             8, 8]

        self._time_windows = \
            [(0, 0),
             (75, 85), (75, 85), # 1, 2
             (60, 70), (45, 55), # 3, 4
             (0, 8), (50, 60), # 5, 6
             (0, 10), (10, 20), # 7, 8
             (0, 10), (75, 85), # 9, 10
             (85, 95), (5, 15), # 11, 12
             (15, 25), (10, 20), # 13, 14
             (45, 55), (30, 40)] # 15, 16

    @property
    def vehicle(self):
        """Gets a vehicle"""
        return self._vehicle

    @property
    def num_vehicles(self):
        """Gets number of vehicles"""
        return self._num_vehicles

    @property
    def city_block(self):
        """Gets a city block"""
        return self._city_block

    @property
    def locations(self):
        """Gets locations"""
        return self._locations

    @property
    def num_locations(self):
        """Gets number of locations"""
        return len(self.locations)

    @property
    def depot(self):
        """Gets depot location index"""
        return self._depot

    @property
    def demands(self):
        """Gets demands at each location"""
        return self._demands

    @property
    def time_per_demand_unit(self):
        """Gets the time (in s) to load a demand"""
        return 5 # 5 minutes/unit

    @property
    def time_windows(self):
        """Gets (start time, end time) for each locations"""
        return self._time_windows

    def manhattan_distance(self, from_node, to_node):
        """Computes the Manhattan distance between two nodes"""
        return (abs(self.locations[from_node][0] - self.locations[to_node][0]) +
                abs(self.locations[from_node][1] - self.locations[to_node][1]))

#######################
# Problem Constraints #
#######################
class CreateDistanceEvaluator(object): # pylint: disable=too-few-public-methods
    """Creates callback to return distance between points."""
    def __init__(self, data):
        """Initializes the distance matrix."""
        self._distance = {}

        # precompute distance between location to have distance callback in O(1)
        for from_node in xrange(data.num_locations):
            self._distance[from_node] = {}
            for to_node in xrange(data.num_locations):
                if from_node == to_node:
                    self._distance[from_node][to_node] = 0
                else:
                    self._distance[from_node][to_node] = (
                        data.manhattan_distance(from_node, to_node))

    def distance(self, from_node, to_node):
        """Returns the manhattan distance between the two nodes"""
        return self._distance[from_node][to_node]

def add_global_span_constraints(routing, data, dist_callback):
    """Add Global Span constraint"""
    distance = "Distance"
    routing.AddDimension(
        dist_callback,
        0, # null slack
        3000, # maximum distance per vehicle
        True, # start cumul to zero
        distance)
    distance_dimension = routing.GetDimensionOrDie(distance)
    # Try to minimize the max distance among vehicles.
    # /!\ It doesn't mean the standard deviation is minimized
    distance_dimension.SetGlobalSpanCostCoefficient(100)

class CreateDemandEvaluator(object): # pylint: disable=too-few-public-methods
    """Creates callback to get demands at each location."""
    def __init__(self, data):
        """Initializes the demand array."""
        self._demands = data.demands

    def demand(self, from_node, to_node):
        """Returns the demand of the current node"""
        del to_node
        return self._demands[from_node]

def add_capacity_constraints(routing, data, demand_callback):
    """Adds capacity constraint"""
    capacity = "Capacity"
    routing.AddDimension(
        demand_callback,
        0, # null capacity slack
        data.vehicle.capacity,
        True, # start cumul to zero
        capacity)

class CreateTimeEvaluator(object):
    """Creates callback to get total times between locations."""
    @staticmethod
    def service_time(data, node):
        """Gets the service time for the specified location."""
        return data.demands[node] * data.time_per_demand_unit

    @staticmethod
    def travel_time(data, from_node, to_node):
        """Gets the travel times between two locations."""
        if from_node == to_node:
            travel_time = 0
        else:
            travel_time = data.manhattan_distance(from_node, to_node) / data.vehicle.speed
        return travel_time

    def __init__(self, data):
        """Initializes the total time matrix."""
        self._total_time = {}
        # precompute total time to have time callback in O(1)
        for from_node in xrange(data.num_locations):
            self._total_time[from_node] = {}
            for to_node in xrange(data.num_locations):
                if from_node == to_node:
                    self._total_time[from_node][to_node] = 0
                else:
                    self._total_time[from_node][to_node] = (
                        self.service_time(data, from_node) +
                        self.travel_time(data, from_node, to_node))

    def time_evaluator(self, from_node, to_node):
        """Returns the total time between the two nodes"""
        return self._total_time[from_node][to_node]

def add_time_window_constraints(routing, data, time_evaluator):
    """Add Global Span constraint"""
    time = "Time"
    routing.AddDimension(
        time_evaluator,
        320, # waiting allowed
        320, # maximum time per vehicle
        True, # start cumul to zero
        time)
    time_dimension = routing.GetDimensionOrDie(time)
    for count, time_window in enumerate(data.time_windows):
        time_dimension.CumulVar(count).SetRange(time_window[0], time_window[1])

###########
# Printer #
###########
class GoogleColorPalette():
    """Google color codes palette"""
    def __init__(self):
        """Initialize Google ColorPalette"""
        self._colors = [
            ('blue', r'#4285F4'),
            ('red', r'#EA4335'),
            ('yellow', r'#FBBC05'),
            ('green', r'#34A853'),
            ('black', r'#101010'),
            ('white', r'#FFFFFF')]

    def __getitem__(self, key):
        """Gets color name from idx"""
        return self._colors[key][0]

    def __len__(self):
        """Gets the number of colors"""
        return len(self._colors)

    @property
    def colors(self):
        """Gets the colors list"""
        return self._colors

    def name(self, idx):
        """Return color name from idx"""
        return self._colors[idx][0]

    def value(self, idx):
        """Return color value from idx"""
        return self._colors[idx][1]

    def value_from_name(self, name):
        """Return color value from name"""
        return dict(self._colors)[name]

class SVG():
    """SVG draw primitives"""
    @staticmethod
    def header(size, margin):
        """Writes header"""
        print(r'<svg xmlns:xlink="http://www.w3.org/1999/xlink" '
              'xmlns="http://www.w3.org/2000/svg" version="1.1"\n'
              'width="{width}" height="{height}" '
              'viewBox="-{margin} -{margin} {width} {height}">'.format(
                  width=size[0]+2*margin,
                  height=size[1]+2*margin,
                  margin=margin))

    @staticmethod
    def definitions(colors):
        """Writes definitions"""
        print(r'<!-- Need this definition to make an arrow marker,'
              ' from https://www.w3.org/TR/svg-markers/ -->')
        print(r'<defs>')
        for color in colors:
            print(r'  <marker id="arrow_{colorname}" viewBox="0 0 16 16" '
                  'refX="8" refY="8" markerUnits="strokeWidth" markerWidth="5" markerHeight="5" '
                  'orient="auto">'.format(colorname=color[0]))
            print(r'    <path d="M 0 0 L 16 8 L 0 16 z" stroke="none" fill="{color}"/>'.format(
                color=color[1]))
            print(r'  </marker>')
        print(r'</defs>')

    @staticmethod
    def footer():
        """Writes svg footer"""
        print(r'</svg>')

    @staticmethod
    def draw_line(position_1, position_2, size, fg_color):
        """Draws a line"""
        line_style = (
            r'style="stroke-width:{sz};stroke:{fg};fill:none"').format(
                sz=size,
                fg=fg_color)
        print(r'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" {style}/>'.format(
            x1=position_1[0],
            y1=position_1[1],
            x2=position_2[0],
            y2=position_2[1],
            style=line_style))

    @staticmethod
    def draw_polyline(position_1, position_2, size, fg_color, colorname):
        """Draws a line with arrow maker in the middle"""
        polyline_style = (
            r'style="stroke-width:{sz};stroke:{fg};fill:none;'
            'marker-mid:url(#arrow_{colorname})"').format(
                sz=size,
                fg=fg_color,
                colorname=colorname)
        print(r'<polyline points="{x1},{y1} {x2},{y2} {x3},{y3}" {style}/>'.format(
            x1=position_1[0],
            y1=position_1[1],
            x2=(position_1[0] + position_2[0]) / 2,
            y2=(position_1[1] + position_2[1]) / 2,
            x3=position_2[0],
            y3=position_2[1],
            style=polyline_style))

    @staticmethod
    def draw_circle(position, radius, size, fg_color, bg_color='white'):
        """Print a circle"""
        circle_style = (
            r'style="stroke-width:{sz};stroke:{fg};fill:{bg}"').format(
                sz=size,
                fg=fg_color,
                bg=bg_color)
        print(r'<circle cx="{cx}" cy="{cy}" r="{r}" {style}/>'.format(
            cx=position[0],
            cy=position[1],
            r=radius,
            style=circle_style))

    @staticmethod
    def draw_text(text, position, size, fg_color='none', bg_color='black'):
        """Print a middle centred text"""
        text_style = (
            r'style="text-anchor:middle;font-weight:bold;'
            'font-size:{sz};stroke:{fg};fill:{bg}"').format(
                sz=size,
                fg=fg_color,
                bg=bg_color)
        print(r'<text x="{x}" y="{y}" dy="{dy}" {style}>{txt}</text>'.format(
            x=position[0],
            y=position[1],
            dy=size / 3,
            style=text_style,
            txt=text))

class SVGPrinter():
    """Generate Problem as svg file to stdout"""
    def __init__(self, args, data, routing=None, assignment=None):
        """Initializes the printer"""
        self._args = args
        self._data = data
        self._routing = routing
        self._assignment = assignment
        # Design variables
        self._color_palette = GoogleColorPalette()
        self._svg = SVG()
        self.radius = min(self.data.city_block.width, self.data.city_block.height) / 3
        self.stroke_width = self.radius / 4

    @property
    def color_palette(self):
        """Gets the color palette"""
        return self._color_palette

    @property
    def svg(self):
        """Gets the svg"""
        return self._svg

    @property
    def data(self):
        """Gets the Data Problem"""
        return self._data

    def draw_grid(self):
        """Draws the city grid"""
        print(r'<!-- Print city streets -->')
        color = '#969696'
        # Horizontal streets
        for i in xrange(9):
            p_1 = [0, i*self.data.city_block.height]
            p_2 = [8*self.data.city_block.width, p_1[1]]
            self.svg.draw_line(p_1, p_2, 2, color)
        # Vertical streets
        for i in xrange(9):
            p_1 = [i*self.data.city_block.width, 0]
            p_2 = [p_1[0], 8*self.data.city_block.height]
            self.svg.draw_line(p_1, p_2, 2, color)

    def draw_depot(self):
        """Draws the depot"""
        print(r'<!-- Print depot -->')
        color = self.color_palette.value_from_name('black')
        loc = self.data.locations[self.data.depot]
        self.svg.draw_circle(loc, self.radius, self.stroke_width, color, 'white')
        self.svg.draw_text(self.data.depot, loc, self.radius, 'none', color)

    def draw_locations(self):
        """Draws all the locations but the depot"""
        print(r'<!-- Print locations -->')
        color = self.color_palette.value(0)
        for idx, loc in enumerate(self.data.locations):
            if idx == self.data.depot:
                continue
            self.svg.draw_circle(loc, self.radius, self.stroke_width, color, 'white')
            self.svg.draw_text(idx, loc, self.radius, 'none', color)

    def draw_demands(self):
        """Draws all the demands"""
        print(r'<!-- Print demands -->')
        for idx, loc in enumerate(self.data.locations):
            if idx == self.data.depot:
                continue
            demand = self.data.demands[idx]
            position = [x+y for x, y in zip(loc, [self.radius*1.2, self.radius*1.1])]
            color = self.color_palette.value(int(math.log(demand, 2)))
            self.svg.draw_text(demand, position, self.radius, 'white', color)

    def draw_time_windows(self):
        """Draws all the time windows"""
        print(r'<!-- Print time windows -->')
        for idx, loc in enumerate(self.data.locations):
            if idx == self.data.depot:
                continue
            time_window = self.data.time_windows[idx]
            position = [x+y for x, y in zip(loc, [self.radius*0, -self.radius*1.6])]
            self.svg.draw_text(
                '[{t1},{t2}]'.format(t1=time_window[0], t2=time_window[1]),
                position,
                self.radius*0.75,
                'none', 'black')
##############
##  ROUTES  ##
##############
    def routes(self):
        """Creates the route list from the assignment"""
        if self._assignment is None:
            print('<!-- No solution found. -->')
            return []
        routes = []
        for vehicle_id in xrange(self.data.num_vehicles):
            index = self._routing.Start(vehicle_id)
            route = []
            while not self._routing.IsEnd(index):
                node_index = self._routing.IndexToNode(index)
                route.append(node_index)
                index = self._assignment.Value(self._routing.NextVar(index))
            node_index = self._routing.IndexToNode(index)
            route.append(node_index)
            routes.append(route)
        return routes

    def draw_route(self, route, color, colorname):
        """Draws a Route"""
        # First print route
        previous_loc_idx = None
        for loc_idx in route:
            if previous_loc_idx != None and previous_loc_idx != loc_idx:
                self.svg.draw_polyline(
                    self.data.locations[previous_loc_idx],
                    self.data.locations[loc_idx],
                    self.stroke_width,
                    color,
                    colorname)
            previous_loc_idx = loc_idx
        # Then print location along the route
        for loc_idx in route:
            if loc_idx != self.data.depot:
                loc = self.data.locations[loc_idx]
                self.svg.draw_circle(loc, self.radius, self.stroke_width, color, 'white')
                self.svg.draw_text(loc_idx, loc, self.radius, 'none', color)

    def draw_routes(self):
        """Draws the routes"""
        print(r'<!-- Print routes -->')
        for route_idx, route in enumerate(self.routes()):
            print(r'<!-- Print route {idx} -->'.format(idx=route_idx))
            color = self.color_palette.value(route_idx)
            colorname = self.color_palette.name(route_idx)
            self.draw_route(route, color, colorname)

    def tw_routes(self):
        """Creates the route time window list from the assignment"""
        if self._assignment is None:
            print('<!-- No solution found. -->')
            return []
        time_dimension = self._routing.GetDimensionOrDie('Time')
        loc_routes = []
        tw_routes = []
        for vehicle_id in xrange(self.data.num_vehicles):
            index = self._routing.Start(vehicle_id) # ignore depot
            index = self._assignment.Value(self._routing.NextVar(index))
            loc_route = []
            tw_route = []
            while not self._routing.IsEnd(index):
                node_index = self._routing.IndexToNode(index)
                loc_route.append(node_index)
                time_var = time_dimension.CumulVar(index)
                #route_time = self.assignment.Value(time_var)
                t_min = self._assignment.Min(time_var)
                t_max = self._assignment.Max(time_var)
                tw_route.append((t_min, t_max))
                index = self._assignment.Value(self._routing.NextVar(index))
            loc_routes.append(loc_route)
            tw_routes.append(tw_route)
        return zip(loc_routes, tw_routes)

    def draw_tw_route(self, locations, tw_route, color):
        """Draws the time windows for a Route"""
        for loc_idx, tw in zip(locations, tw_route):
            loc = self.data.locations[loc_idx]
            position = [x+y for x, y in zip(loc, [self.radius*0, self.radius*1.8])]
            self.svg.draw_text(
                '[{t1},{t2}]'.format(t1=tw[0], t2=tw[1]),
                position,
                self.radius*0.75,
                'white', color)

    def draw_tw_routes(self):
        """Draws the time window routes"""
        print(r'<!-- Print time window routes -->')
        for route_idx, loc_tw in enumerate(self.tw_routes()):
            print(r'<!-- Print time window route {idx} -->'.format(idx=route_idx))
            color = self.color_palette.value(route_idx)
            self.draw_tw_route(loc_tw[0], loc_tw[1], color)

    def print(self):
        """Prints a full svg document on stdout"""
        margin = self.radius*2 + 2
        size = [8*self.data.city_block.width, 8*self.data.city_block.height]
        self.svg.header(size, margin)
        self.svg.definitions(self.color_palette.colors)
        self.draw_grid()
        if self._args['solution'] is False:
            self.draw_locations()
        else:
            self.draw_routes()
        self.draw_depot()
        if self._args['capacity'] is True:
            self.draw_demands()
        if self._args['time_window'] is True:
            self.draw_time_windows()
        if self._args['time_window'] is True and self._args['solution'] is True:
            self.draw_tw_routes()
        self.svg.footer()

########
# Main #
########
def main():
    """Entry point of the program"""
    parser = argparse.ArgumentParser(description='Output VRP as svg image.')
    parser.add_argument(
        '-gs', '--global-span',
        action='store_true',
        help='use global span constraints')
    parser.add_argument(
        '-c', '--capacity',
        action='store_true',
        help='use capacity constraints')
    parser.add_argument(
        '-tw', '--time-window',
        action='store_true',
        help='use time-window constraints')
    parser.add_argument(
        '-pd', '--pickup-delivery',
        action='store_true',
        help='use pickup & delivery constraints')
    parser.add_argument(
        '-f', '--fuel',
        action='store_true',
        help='use fuel constraints')
    parser.add_argument(
        '-r', '--resource',
        action='store_true',
        help='use resource constraints')
    parser.add_argument(
        '-s', '--solution',
        action='store_true',
        help='print solution')
    args = vars(parser.parse_args())

    # Instanciate the data problem.
    data = DataProblem()

    # Create solver if needed
    if args['solution'] is True:
        # Create Routing Model
        routing = pywrapcp.RoutingModel(data.num_locations, data.num_vehicles, data.depot)
        # Define weight of each edge
        dist_callback = CreateDistanceEvaluator(data).distance
        routing.SetArcCostEvaluatorOfAllVehicles(dist_callback)
        if args['global_span'] is True:
            add_global_span_constraints(routing, data, dist_callback)
        if args['capacity'] is True:
            demand_evaluator = CreateDemandEvaluator(data).demand
            add_capacity_constraints(routing, data, demand_evaluator)
        if args['time_window'] is True:
            time_evaluator = CreateTimeEvaluator(data).time_evaluator
            add_time_window_constraints(routing, data, time_evaluator)
        if args['fuel'] is True:
            add_fuel_constraints(routing, data)
        if args['resource'] is True:
            add_ressource_constraints(routing, data)
        # Setting first solution heuristic (cheapest addition).
        search_parameters = pywrapcp.RoutingModel.DefaultSearchParameters()
        search_parameters.first_solution_strategy = (
            routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
        # Solve the problem.
        assignment = routing.SolveWithParameters(search_parameters)
        printer = SVGPrinter(args, data, routing, assignment)
    else:
        # Print svg on cout
        printer = SVGPrinter(args, data)
    printer.print()

if __name__ == '__main__':
    main()
