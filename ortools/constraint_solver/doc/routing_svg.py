# Copyright 2010-2021 Google LLC
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
"""Generate SVG for a Routing problem."""

# [START import]
import argparse
from ortools.constraint_solver import pywrapcp
from ortools.constraint_solver import routing_enums_pb2
# [END import]


# [START data_model]
class DataModel(object):  # pylint: disable=too-many-instance-attributes
    """Stores the data for the problem."""

    def __init__(self, args):
        # Locations in block units
        locations = \
                [(4, 4),  # depot
                 (2, 0), (8, 0),  # locations to visit
                 (0, 1), (1, 1),
                 (5, 2), (7, 2),
                 (3, 3), (6, 3),
                 (5, 5), (8, 5),
                 (1, 6), (2, 6),
                 (3, 7), (6, 7),
                 (0, 8), (7, 8),]
        # Convert locations in meters using a city block dimension of 114m x 80m.
        self._locations = [(l[0] * 114, l[1] * 80) for l in locations]
        self._distance_matrix = [
            [
                0, 548, 776, 696, 582, 274, 502, 194, 308, 194, 536, 502, 388,
                354, 468, 776, 662
            ],
            [
                548, 0, 684, 308, 194, 502, 730, 354, 696, 742, 1084, 594, 480,
                674, 1016, 868, 1210
            ],
            [
                776, 684, 0, 992, 878, 502, 274, 810, 468, 742, 400, 1278, 1164,
                1130, 788, 1552, 754
            ],
            [
                696, 308, 992, 0, 114, 650, 878, 502, 844, 890, 1232, 514, 628,
                822, 1164, 560, 1358
            ],
            [
                582, 194, 878, 114, 0, 536, 764, 388, 730, 776, 1118, 400, 514,
                708, 1050, 674, 1244
            ],
            [
                274, 502, 502, 650, 536, 0, 228, 308, 194, 240, 582, 776, 662,
                628, 514, 1050, 708
            ],
            [
                502, 730, 274, 878, 764, 228, 0, 536, 194, 468, 354, 1004, 890,
                856, 514, 1278, 480
            ],
            [
                194, 354, 810, 502, 388, 308, 536, 0, 342, 388, 730, 468, 354,
                320, 662, 742, 856
            ],
            [
                308, 696, 468, 844, 730, 194, 194, 342, 0, 274, 388, 810, 696,
                662, 320, 1084, 514
            ],
            [
                194, 742, 742, 890, 776, 240, 468, 388, 274, 0, 342, 536, 422,
                388, 274, 810, 468
            ],
            [
                536, 1084, 400, 1232, 1118, 582, 354, 730, 388, 342, 0, 878,
                764, 730, 388, 1152, 354
            ],
            [
                502, 594, 1278, 514, 400, 776, 1004, 468, 810, 536, 878, 0, 114,
                308, 650, 274, 844
            ],
            [
                388, 480, 1164, 628, 514, 662, 890, 354, 696, 422, 764, 114, 0,
                194, 536, 388, 730
            ],
            [
                354, 674, 1130, 822, 708, 628, 856, 320, 662, 388, 730, 308,
                194, 0, 342, 422, 536
            ],
            [
                468, 1016, 788, 1164, 1050, 514, 514, 662, 320, 274, 388, 650,
                536, 342, 0, 764, 194
            ],
            [
                776, 868, 1552, 560, 674, 1050, 1278, 742, 1084, 810, 1152, 274,
                388, 422, 764, 0, 798
            ],
            [
                662, 1210, 754, 1358, 1244, 708, 480, 856, 514, 468, 354, 844,
                730, 536, 194, 798, 0
            ],
        ]
        self._time_matrix = [
            [0, 6, 9, 8, 7, 3, 6, 2, 3, 2, 6, 6, 4, 4, 5, 9, 7],
            [6, 0, 8, 3, 2, 6, 8, 4, 8, 8, 13, 7, 5, 8, 12, 10, 14],
            [9, 8, 0, 11, 10, 6, 3, 9, 5, 8, 4, 15, 14, 13, 9, 18, 9],
            [8, 3, 11, 0, 1, 7, 10, 6, 10, 10, 14, 6, 7, 9, 14, 6, 16],
            [7, 2, 10, 1, 0, 6, 9, 4, 8, 9, 13, 4, 6, 8, 12, 8, 14],
            [3, 6, 6, 7, 6, 0, 2, 3, 2, 2, 7, 9, 7, 7, 6, 12, 8],
            [6, 8, 3, 10, 9, 2, 0, 6, 2, 5, 4, 12, 10, 10, 6, 15, 5],
            [2, 4, 9, 6, 4, 3, 6, 0, 4, 4, 8, 5, 4, 3, 7, 8, 10],
            [3, 8, 5, 10, 8, 2, 2, 4, 0, 3, 4, 9, 8, 7, 3, 13, 6],
            [2, 8, 8, 10, 9, 2, 5, 4, 3, 0, 4, 6, 5, 4, 3, 9, 5],
            [6, 13, 4, 14, 13, 7, 4, 8, 4, 4, 0, 10, 9, 8, 4, 13, 4],
            [6, 7, 15, 6, 4, 9, 12, 5, 9, 6, 10, 0, 1, 3, 7, 3, 10],
            [4, 5, 14, 7, 6, 7, 10, 4, 8, 5, 9, 1, 0, 2, 6, 4, 8],
            [4, 8, 13, 9, 8, 7, 10, 3, 7, 4, 8, 3, 2, 0, 4, 5, 6],
            [5, 12, 9, 14, 12, 6, 6, 7, 3, 3, 4, 7, 6, 4, 0, 9, 2],
            [9, 10, 18, 6, 8, 12, 15, 8, 13, 9, 13, 3, 4, 5, 9, 0, 9],
            [7, 14, 9, 16, 14, 8, 5, 10, 6, 5, 4, 10, 8, 6, 2, 9, 0],
        ]
        self._time_windows = [
            (0, 5),  # depot
            (7, 12),  # 1
            (10, 15),  # 2
            (5, 14),  # 3
            (5, 13),  # 4
            (0, 5),  # 5
            (5, 10),  # 6
            (0, 10),  # 7
            (5, 10),  # 8
            (0, 5),  # 9
            (10, 16),  # 10
            (10, 15),  # 11
            (0, 5),  # 12
            (5, 10),  # 13
            (7, 12),  # 14
            (10, 15),  # 15
            (5, 15),  # 16
        ]
        if args['drop_nodes']:
            self._demands = [0, 1, 1, 3, 6, 3, 6, 8, 8, 1, 2, 1, 2, 6, 6, 8, 8]
        else:
            self._demands = [0, 1, 1, 2, 4, 2, 4, 8, 8, 1, 2, 1, 2, 4, 4, 8, 8]
        self._pickups_deliveries = [
            [1, 6],
            [2, 10],
            [4, 3],
            [5, 9],
            [7, 8],
            [15, 11],
            [13, 12],
            [16, 14],
        ]

        if args['tsp']:
            self._num_vehicles = 1
        else:
            self._num_vehicles = 4
            self._vehicle_capacities = [15, 15, 15, 15]

        if args['resources']:
            self._vehicle_load_time = 5
            self._vehicle_unload_time = 5

        self._depot = 0
        self._depot_capacity = 2
        self._starts = [1, 2, 15, 16]
        self._ends = [0, 0, 0, 0]

    @property
    def locations(self):
        """Gets the locations."""
        return self._locations

    @property
    def distance_matrix(self):
        """Gets the distance matrix."""
        return self._distance_matrix

    @property
    def time_matrix(self):
        """Gets the time matrix."""
        return self._time_matrix

    @property
    def time_windows(self):
        """Gets the time windows."""
        return self._time_windows

    @property
    def demands(self):
        """Gets the locations demands."""
        return self._demands

    @property
    def pickups_deliveries(self):
        """Gets the pickups deliveries."""
        return self._pickups_deliveries

    @property
    def num_vehicles(self):
        """Gets the number of vehicles."""
        return self._num_vehicles

    @property
    def vehicle_capacities(self):
        """Gets the capacity of each vehicles."""
        return self._vehicle_capacities

    @property
    def vehicle_load_time(self):
        """Gets the load time of each vehicles."""
        return self._vehicle_load_time

    @property
    def vehicle_unload_time(self):
        """Gets the unload time of each vehicles."""
        return self._vehicle_unload_time

    @property
    def depot_capacity(self):
        """Gets the depot capacity."""
        return self._depot_capacity

    @property
    def depot(self):
        """Gets the depot node index."""
        return self._depot

    @property
    def starts(self):
        """Gets the start nodes indices."""
        return self._starts

    @property
    def ends(self):
        """Gets the end nodes indices."""
        return self._ends

    # [END data_model]


###########
# Printer #
###########
class GoogleColorPalette(object):
    """Google color codes palette."""

    def __init__(self):
        """Initialize Google ColorPalette."""
        self._colors = [('blue', r'#4285F4'), ('red', r'#EA4335'),
                        ('yellow', r'#FBBC05'), ('green', r'#34A853'),
                        ('black', r'#101010'), ('white', r'#FFFFFF')]

    def __getitem__(self, key):
        """Gets color name from idx."""
        return self._colors[key][0]

    def __len__(self):
        """Gets the number of colors."""
        return len(self._colors)

    @property
    def colors(self):
        """Gets the colors list."""
        return self._colors

    def name(self, idx):
        """Return color name from idx."""
        return self._colors[idx][0]

    def value(self, idx):
        """Return color value from idx."""
        return self._colors[idx][1]

    def value_from_name(self, name):
        """Return color value from name."""
        return dict(self._colors)[name]


class SVG(object):
    """SVG draw primitives."""

    @staticmethod
    def header(size, margin):
        """Writes header."""
        print(r'<svg xmlns:xlink="http://www.w3.org/1999/xlink" '
              'xmlns="http://www.w3.org/2000/svg" version="1.1"\n'
              'width="{width}" height="{height}" '
              'viewBox="-{margin} -{margin} {width} {height}">'.format(
                  width=size[0] + 2 * margin,
                  height=size[1] + 2 * margin,
                  margin=margin))

    @staticmethod
    def definitions(colors):
        """Writes definitions."""
        print(r'<!-- Need this definition to make an arrow marker,'
              ' from https://www.w3.org/TR/svg-markers/ -->')
        print(r'<defs>')
        for color in colors:
            print(
                r'  <marker id="arrow_{colorname}" viewBox="0 0 16 16" '
                'refX="8" refY="8" markerUnits="strokeWidth" markerWidth="5" markerHeight="5" '
                'orient="auto">'.format(colorname=color[0]))
            print(
                r'    <path d="M 0 0 L 16 8 L 0 16 z" stroke="none" fill="{color}"/>'
                .format(color=color[1]))
            print(r'  </marker>')
        print(r'</defs>')

    @staticmethod
    def footer():
        """Writes svg footer."""
        print(r'</svg>')

    @staticmethod
    def draw_line(position_1, position_2, size, fg_color):
        """Draws a line."""
        line_style = (
            r'style="stroke-width:{sz};stroke:{fg};fill:none"').format(
                sz=size, fg=fg_color)
        print(r'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" {style}/>'.format(
            x1=position_1[0],
            y1=position_1[1],
            x2=position_2[0],
            y2=position_2[1],
            style=line_style))

    @staticmethod
    def draw_polyline(position_1, position_2, size, fg_color, colorname):
        """Draws a line with arrow maker in the middle."""
        polyline_style = (r'style="stroke-width:{sz};stroke:{fg};fill:none;'
                          'marker-mid:url(#arrow_{colorname})"').format(
                              sz=size, fg=fg_color, colorname=colorname)
        print(r'<polyline points="{x1},{y1} {x2},{y2} {x3},{y3}" {style}/>'.
              format(x1=position_1[0],
                     y1=position_1[1],
                     x2=(position_1[0] + position_2[0]) / 2,
                     y2=(position_1[1] + position_2[1]) / 2,
                     x3=position_2[0],
                     y3=position_2[1],
                     style=polyline_style))

    @staticmethod
    def draw_circle(position, radius, size, fg_color, bg_color='white'):
        """Print a circle."""
        circle_style = (
            r'style="stroke-width:{sz};stroke:{fg};fill:{bg}"').format(
                sz=size, fg=fg_color, bg=bg_color)
        print(r'<circle cx="{cx}" cy="{cy}" r="{r}" {style}/>'.format(
            cx=position[0], cy=position[1], r=radius, style=circle_style))

    @staticmethod
    def draw_text(text, position, size, fg_color='none', bg_color='black'):
        """Print a middle centred text."""
        text_style = (r'style="text-anchor:middle;font-weight:bold;'
                      'font-size:{sz};stroke:{fg};fill:{bg}"').format(
                          sz=size, fg=fg_color, bg=bg_color)
        print(r'<text x="{x}" y="{y}" dy="{dy}" {style}>{txt}</text>'.format(
            x=position[0],
            y=position[1],
            dy=size / 3,
            style=text_style,
            txt=text))


class SVGPrinter(object):  # pylint: disable=too-many-instance-attributes
    """Generate Problem as svg file to stdout."""

    # pylint: disable=too-many-arguments
    def __init__(self, args, data, manager=None, routing=None, assignment=None):
        """Initializes the printer."""
        self._args = args
        self._data = data
        self._manager = manager
        self._routing = routing
        self._assignment = assignment
        # Design variables
        self._color_palette = GoogleColorPalette()
        self._svg = SVG()
        # City block size 114mx80m
        self._radius = min(114, 80) / 3
        self._stroke_width = self._radius / 4

    @property
    def data(self):
        """Gets the Data Model."""
        return self._data

    @property
    def manager(self):
        """Gets the RoutingIndexManager."""
        return self._manager

    @property
    def routing(self):
        """Gets the Routing solver."""
        return self._routing

    @property
    def assignment(self):
        """Gets the assignment."""
        return self._assignment

    @property
    def color_palette(self):
        """Gets the color palette."""
        return self._color_palette

    @property
    def svg(self):
        """Gets the svg."""
        return self._svg

    def draw_grid(self):
        """Draws the city grid."""
        print(r'<!-- Print city streets -->')
        color = '#969696'
        # Horizontal streets
        for i in range(9):
            p_1 = [0, i * 80]
            p_2 = [8 * 114, p_1[1]]
            self._svg.draw_line(p_1, p_2, 2, color)
        # Vertical streets
        for i in range(9):
            p_1 = [i * 114, 0]
            p_2 = [p_1[0], 8 * 80]
            self._svg.draw_line(p_1, p_2, 2, color)

    def draw_depot(self):
        """Draws the depot."""
        print(r'<!-- Print depot -->')
        color = self._color_palette.value_from_name('black')
        loc = self._data.locations[self._data.depot]
        self._svg.draw_circle(loc, self._radius, self._stroke_width, color,
                              'white')
        self._svg.draw_text(self._data.depot, loc, self._radius, 'none', color)

    def draw_depots(self):
        """Draws the depot."""
        print(r'<!-- Print depots -->')
        # print starts
        for vehicle_idx, start in enumerate(self._data.starts):
            del vehicle_idx
            color = self._color_palette.value_from_name('black')
            # color = self._color_palette.value(vehicle_idx)
            loc = self._data.locations[start]
            self._svg.draw_circle(loc, self._radius, self._stroke_width, color,
                                  'white')
            self._svg.draw_text(start, loc, self._radius, 'none', color)
        # print end
        color = self._color_palette.value_from_name('black')
        loc = self._data.locations[0]
        self._svg.draw_circle(loc, self._radius, self._stroke_width, color,
                              'white')
        self._svg.draw_text(0, loc, self._radius, 'none', color)

    def draw_locations(self):
        """Draws all the locations but the depot."""
        print(r'<!-- Print locations -->')
        color = self._color_palette.value_from_name('blue')
        if not self._args['starts_ends']:
            for idx, loc in enumerate(self._data.locations):
                if idx == self._data.depot:
                    continue
                self._svg.draw_circle(loc, self._radius, self._stroke_width,
                                      color, 'white')
                self._svg.draw_text(idx, loc, self._radius, 'none', color)
        else:
            for idx, loc in enumerate(self._data.locations):
                if idx in self._data.starts + self._data.ends:
                    continue
                self._svg.draw_circle(loc, self._radius, self._stroke_width,
                                      color, 'white')
                self._svg.draw_text(idx, loc, self._radius, 'none', color)

    def draw_demands(self):
        """Draws all the demands."""
        print(r'<!-- Print demands -->')
        for idx, loc in enumerate(self._data.locations):
            if idx == self._data.depot:
                continue
            demand = self._data.demands[idx]
            position = [
                x + y
                for x, y in zip(loc, [self._radius * 1.2, self._radius * 1.1])
            ]
            color = self._color_palette.value_from_name('red')
            # color = self._color_palette.value(int(math.log(demand, 2)))
            self._svg.draw_text(demand, position, self._radius, 'none', color)

    def draw_pickups_deliveries(self):
        """Draws all pickups deliveries."""
        print(r'<!-- Print pickups deliveries -->')
        colorname = 'red'
        color = self._color_palette.value_from_name(colorname)
        for pickup_delivery in self._data.pickups_deliveries:
            self._svg.draw_polyline(self._data.locations[pickup_delivery[0]],
                                    self._data.locations[pickup_delivery[1]],
                                    self._stroke_width, color, colorname)

    def draw_time_windows(self):
        """Draws all the time windows."""
        print(r'<!-- Print time windows -->')
        for idx, loc in enumerate(self._data.locations):
            if idx == self._data.depot:
                continue
            time_window = self._data.time_windows[idx]
            position = [
                x + y
                for x, y in zip(loc, [self._radius * 0, -self._radius * 1.6])
            ]
            color = self._color_palette.value_from_name('red')
            self._svg.draw_text(
                '[{t1},{t2}]'.format(t1=time_window[0], t2=time_window[1]),
                position, self._radius * 0.75, 'white', color)

##############
##  ROUTES  ##
##############

    def draw_drop_nodes(self):
        """Draws the dropped nodes."""
        print(r'<!-- Print drop nodes -->')
        if self._assignment is None:
            print('<!-- No solution found. -->')
        # Display dropped nodes.
        dropped_nodes = []
        for node in range(self._routing.Size()):
            if self._routing.IsStart(node) or self._routing.IsEnd(node):
                continue
            if self._assignment.Value(self._routing.NextVar(node)) == node:
                dropped_nodes.append(self._manager.IndexToNode(node))
        color = self._color_palette.value_from_name('black')
        for node_idx in dropped_nodes:
            loc = self._data.locations[node_idx]
            self._svg.draw_circle(loc, self._radius, self._stroke_width, color,
                                  'white')
            self._svg.draw_text(node_idx, loc, self._radius, 'none', color)

    def routes(self):
        """Creates the route list from the assignment."""
        if self._assignment is None:
            print('<!-- No solution found. -->')
            return []
        routes = []
        for vehicle_id in range(self._data.num_vehicles):
            index = self._routing.Start(vehicle_id)
            route = []
            while not self._routing.IsEnd(index):
                node_index = self._manager.IndexToNode(index)
                route.append(node_index)
                index = self._assignment.Value(self._routing.NextVar(index))
            node_index = self._manager.IndexToNode(index)
            route.append(node_index)
            routes.append(route)
        return routes

    def draw_route(self, route, color, colorname):
        """Draws a Route."""
        # First print route
        previous_loc_idx = None
        for loc_idx in route:
            if previous_loc_idx and previous_loc_idx != loc_idx:
                self._svg.draw_polyline(self._data.locations[previous_loc_idx],
                                        self._data.locations[loc_idx],
                                        self._stroke_width, color, colorname)
            previous_loc_idx = loc_idx
        # Then print location along the route
        for loc_idx in route:
            if loc_idx != self._data.depot:
                loc = self._data.locations[loc_idx]
                self._svg.draw_circle(loc, self._radius, self._stroke_width,
                                      color, 'white')
                self._svg.draw_text(loc_idx, loc, self._radius, 'none', color)

    def draw_routes(self):
        """Draws the routes."""
        print(r'<!-- Print routes -->')
        for route_idx, route in enumerate(self.routes()):
            print(r'<!-- Print route {idx} -->'.format(idx=route_idx))
            color = self._color_palette.value(route_idx)
            colorname = self._color_palette.name(route_idx)
            self.draw_route(route, color, colorname)

    def tw_routes(self):
        """Creates the route time window list from the assignment."""
        if self._assignment is None:
            print('<!-- No solution found. -->')
            return []
        time_dimension = self._routing.GetDimensionOrDie('Time')
        loc_routes = []
        tw_routes = []
        for vehicle_id in range(self._data.num_vehicles):
            index = self._routing.Start(vehicle_id)
            # index = self._assignment.Value(self._routing.NextVar(index))
            loc_route = []
            tw_route = []
            while True:
                node_index = self._manager.IndexToNode(index)
                loc_route.append(node_index)
                time_var = time_dimension.CumulVar(index)
                t_min = self._assignment.Min(time_var)
                t_max = self._assignment.Max(time_var)
                tw_route.append((t_min, t_max))
                if self._routing.IsEnd(index):
                    break
                index = self._assignment.Value(self._routing.NextVar(index))
            loc_routes.append(loc_route)
            tw_routes.append(tw_route)
        return zip(loc_routes, tw_routes)

    def draw_tw_route(self, route_idx, locations, tw_route, color):
        """Draws the time windows for a Route."""
        is_start = -1
        for loc_idx, time_window in zip(locations, tw_route):
            loc = self._data.locations[loc_idx]
            if loc_idx == 0:  # special case for depot
                position = [
                    x + y for x, y in zip(loc, [
                        self._radius * is_start, self._radius *
                        (1.8 + route_idx)
                    ])
                ]
                is_start = 1
            else:
                position = [
                    x + y
                    for x, y in zip(loc, [self._radius * 0, self._radius * 1.8])
                ]
            self._svg.draw_text('[{t_min}]'.format(t_min=time_window[0]),
                                position, self._radius * 0.75, 'white', color)

    def draw_tw_routes(self):
        """Draws the time window routes."""
        print(r'<!-- Print time window routes -->')
        for route_idx, loc_tw in enumerate(self.tw_routes()):
            print(r'<!-- Print time window route {} -->'.format(route_idx))
            color = self._color_palette.value(route_idx)
            self.draw_tw_route(route_idx, loc_tw[0], loc_tw[1], color)

    def print_to_console(self):
        """Prints a full svg document on stdout."""
        margin = self._radius * 2 + 2
        size = [8 * 114, 8 * 80]
        self._svg.header(size, margin)
        self._svg.definitions(self._color_palette.colors)
        self.draw_grid()
        if not self._args['solution']:
            if self._args['pickup_delivery']:
                self.draw_pickups_deliveries()
            self.draw_locations()
        else:
            self.draw_routes()
            self.draw_drop_nodes()
        if self._args['starts_ends']:
            self.draw_depots()
        else:
            self.draw_depot()
        if self._args['capacity']:
            self.draw_demands()
        if self._args['drop_nodes']:
            self.draw_demands()
        if self._args['time_windows'] or self._args['resources']:
            self.draw_time_windows()
        if ((self._args['time_windows'] or self._args['resources']) and
                self._args['solution']):
            self.draw_tw_routes()
        self._svg.footer()


########
# Main #
########
def main():  # pylint: disable=too-many-locals,too-many-branches
    """Entry point of the program."""
    parser = argparse.ArgumentParser(description='Output VRP as svg image.')
    parser.add_argument('-tsp',
                        '--tsp',
                        action='store_true',
                        help='use 1 vehicle')
    parser.add_argument('-vrp',
                        '--vrp',
                        action='store_true',
                        help='use 4 vehicle')
    parser.add_argument('-gs',
                        '--global-span',
                        action='store_true',
                        help='use global span constraints')
    parser.add_argument('-c',
                        '--capacity',
                        action='store_true',
                        help='use capacity constraints')
    parser.add_argument('-r',
                        '--resources',
                        action='store_true',
                        help='use resources constraints')
    parser.add_argument('-dn',
                        '--drop-nodes',
                        action='store_true',
                        help='allow drop nodes (disjuntion constraints)')
    parser.add_argument('-tw',
                        '--time-windows',
                        action='store_true',
                        help='use time-window constraints')
    parser.add_argument('-se',
                        '--starts-ends',
                        action='store_true',
                        help='use multiple starts & ends')
    parser.add_argument('-pd',
                        '--pickup-delivery',
                        action='store_true',
                        help='use pickup & delivery constraints')
    parser.add_argument('-fifo',
                        '--fifo',
                        action='store_true',
                        help='use pickup & delivery FIFO Policy')
    parser.add_argument('-lifo',
                        '--lifo',
                        action='store_true',
                        help='use pickup & delivery LIFO Policy')
    parser.add_argument('-s',
                        '--solution',
                        action='store_true',
                        help='print solution')
    args = vars(parser.parse_args())

    # Instantiate the data problem.
    # [START data]
    data = DataModel(args)
    # [END data]

    if not args['solution']:
        # Print svg on cout
        printer = SVGPrinter(args, data)
        printer.print_to_console()
        return 0

    # Create the routing index manager.
    # [START index_manager]
    if args['starts_ends']:
        manager = pywrapcp.RoutingIndexManager(len(data.locations),
                                               data.num_vehicles, data.starts,
                                               data.ends)
    else:
        manager = pywrapcp.RoutingIndexManager(len(data.locations),
                                               data.num_vehicles, data.depot)
    # [END index_manager]

    # Create Routing Model.
    # [START routing_model]
    routing = pywrapcp.RoutingModel(manager)

    # [END routing_model]

    # Register distance callback
    def distance_callback(from_index, to_index):
        """Returns the manhattan distance between the two nodes."""
        # Convert from routing variable Index to distance matrix NodeIndex.
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        return data.distance_matrix[from_node][to_node]

    distance_callback_index = routing.RegisterTransitCallback(distance_callback)

    # Register time callback
    def time_callback(from_index, to_index):
        """Returns the manhattan distance travel time between the two nodes."""
        # Convert from routing variable Index to distance matrix NodeIndex.
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        return data.time_matrix[from_node][to_node]

    time_callback_index = routing.RegisterTransitCallback(time_callback)

    # Register demands callback
    def demand_callback(from_index):
        """Returns the demand of the node."""
        # Convert from routing variable Index to demands NodeIndex.
        from_node = manager.IndexToNode(from_index)
        return data.demands[from_node]

    demand_callback_index = routing.RegisterUnaryTransitCallback(
        demand_callback)

    if args['time_windows'] or args['resources']:
        routing.SetArcCostEvaluatorOfAllVehicles(time_callback_index)
    else:
        routing.SetArcCostEvaluatorOfAllVehicles(distance_callback_index)

    if args['global_span'] or args['pickup_delivery']:
        dimension_name = 'Distance'
        routing.AddDimension(distance_callback_index, 0, 3000, True,
                             dimension_name)
        distance_dimension = routing.GetDimensionOrDie(dimension_name)
        distance_dimension.SetGlobalSpanCostCoefficient(100)

    if args['capacity'] or args['drop_nodes']:
        routing.AddDimensionWithVehicleCapacity(demand_callback_index, 0,
                                                data.vehicle_capacities, True,
                                                'Capacity')

    if args['drop_nodes']:
        # Allow to drop nodes.
        penalty = 1000
        for node in range(1, len(data.locations)):
            routing.AddDisjunction([manager.NodeToIndex(node)], penalty)

    if args['pickup_delivery']:
        dimension_name = 'Distance'
        routing.AddDimension(distance_callback_index, 0, 3000, True,
                             dimension_name)
        distance_dimension = routing.GetDimensionOrDie(dimension_name)
        distance_dimension.SetGlobalSpanCostCoefficient(100)
        for request in data.pickups_deliveries:
            pickup_index = manager.NodeToIndex(request[0])
            delivery_index = manager.NodeToIndex(request[1])
            routing.AddPickupAndDelivery(pickup_index, delivery_index)
            routing.solver().Add(
                routing.VehicleVar(pickup_index) == routing.VehicleVar(
                    delivery_index))
            routing.solver().Add(
                distance_dimension.CumulVar(pickup_index) <=
                distance_dimension.CumulVar(delivery_index))
        if args['fifo']:
            routing.SetPickupAndDeliveryPolicyOfAllVehicles(
                pywrapcp.RoutingModel.FIFO)
        if args['lifo']:
            routing.SetPickupAndDeliveryPolicyOfAllVehicles(
                pywrapcp.RoutingModel.LIFO)

    if args['starts_ends']:
        dimension_name = 'Distance'
        routing.AddDimension(distance_callback_index, 0, 2000, True,
                             dimension_name)
        distance_dimension = routing.GetDimensionOrDie(dimension_name)
        distance_dimension.SetGlobalSpanCostCoefficient(100)

    time = 'Time'
    if args['time_windows'] or args['resources']:
        routing.AddDimension(time_callback_index, 30, 30, False, time)
        time_dimension = routing.GetDimensionOrDie(time)
        # Add time window constraints for each location except depot and 'copy' the
        # slack var in the solution object (aka Assignment) to print it.
        for location_idx, time_window in enumerate(data.time_windows):
            if location_idx == 0:
                continue
            index = manager.NodeToIndex(location_idx)
            time_dimension.CumulVar(index).SetRange(time_window[0],
                                                    time_window[1])
            routing.AddToAssignment(time_dimension.SlackVar(index))
        # Add time window constraints for each vehicle start node and 'copy' the
        # slack var in the solution object (aka Assignment) to print it.
        for vehicle_id in range(data.num_vehicles):
            index = routing.Start(vehicle_id)
            time_window = data.time_windows[0]
            time_dimension.CumulVar(index).SetRange(time_window[0],
                                                    time_window[1])
            routing.AddToAssignment(time_dimension.SlackVar(index))

        # Instantiate route start and end times to produce feasible times.
        for vehicle_id in range(data.num_vehicles):
            routing.AddVariableMinimizedByFinalizer(
                time_dimension.CumulVar(routing.End(vehicle_id)))
            routing.AddVariableMinimizedByFinalizer(
                time_dimension.CumulVar(routing.Start(vehicle_id)))

    if args['resources']:
        # Add resource constraints at the depot.
        time_dimension = routing.GetDimensionOrDie(time)
        solver = routing.solver()
        intervals = []
        for i in range(data.num_vehicles):
            # Add loading time at start of routes
            intervals.append(
                solver.FixedDurationIntervalVar(
                    time_dimension.CumulVar(routing.Start(i)),
                    data.vehicle_load_time, 'depot_interval'))
            # Add unloading time at end of routes.
            intervals.append(
                solver.FixedDurationIntervalVar(
                    time_dimension.CumulVar(routing.End(i)),
                    data.vehicle_unload_time, 'depot_interval '))

        depot_usage = [1 for i in range(data.num_vehicles * 2)]
        solver.AddConstraint(
            solver.Cumulative(intervals, depot_usage, data.depot_capacity,
                              'depot'))

    # Setting first solution heuristic (cheapest addition).
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    # pylint: disable=no-member
    if not args['pickup_delivery']:
        search_parameters.first_solution_strategy = (
            routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC)
    else:
        search_parameters.first_solution_strategy = (
            routing_enums_pb2.FirstSolutionStrategy.PARALLEL_CHEAPEST_INSERTION)

    search_parameters.local_search_metaheuristic = (
        routing_enums_pb2.LocalSearchMetaheuristic.GUIDED_LOCAL_SEARCH)
    search_parameters.time_limit.FromSeconds(2)

    # Solve the problem.
    assignment = routing.SolveWithParameters(search_parameters)
    # Print the solution.
    printer = SVGPrinter(args, data, manager, routing, assignment)
    printer.print_to_console()
    return 0


if __name__ == '__main__':
    main()
