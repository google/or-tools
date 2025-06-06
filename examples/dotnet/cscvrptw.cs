// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System;
using System.Collections.Generic;
using Google.OrTools.ConstraintSolver;

/// <summary>
///   Sample showing how to model and solve a capacitated vehicle routing
///   problem with time windows using the swig-wrapped version of the vehicle
///   routing library in src/constraint_solver.
/// </summary>
public class CapacitatedVehicleRoutingProblemWithTimeWindows
{
    /// <summary>
    ///   A position on the map with (x, y) coordinates.
    /// </summary>
    class Position
    {
        public Position()
        {
            this.x_ = 0;
            this.y_ = 0;
        }

        public Position(int x, int y)
        {
            this.x_ = x;
            this.y_ = y;
        }

        public int x_;
        public int y_;
    }

    /// <summary>
    ///    A time window with start/end data.
    /// </summary>
    class TimeWindow
    {
        public TimeWindow()
        {
            this.start_ = -1;
            this.end_ = -1;
        }

        public TimeWindow(int start, int end)
        {
            this.start_ = start;
            this.end_ = end;
        }

        public int start_;
        public int end_;
    }

    /// <summary>
    /// Manhattan distance implemented as a callback. It uses an array of
    ///   positions and computes the Manhattan distance between the two
    ///   positions of two different indices.
    /// </summary>
    class Manhattan
    {
        public Manhattan(RoutingIndexManager manager, Position[] locations, int coefficient)
        {
            this.manager_ = manager;
            this.locations_ = locations;
            this.coefficient_ = coefficient;
        }

        public long Call(long first_index, long second_index)
        {
            if (first_index >= locations_.Length || second_index >= locations_.Length)
            {
                return 0;
            }
            int first_node = manager_.IndexToNode(first_index);
            int second_node = manager_.IndexToNode(second_index);
            return (Math.Abs(locations_[first_node].x_ - locations_[second_node].x_) +
                    Math.Abs(locations_[first_node].y_ - locations_[second_node].y_)) *
                   coefficient_;
        }

        private readonly RoutingIndexManager manager_;
        private readonly Position[] locations_;
        private readonly int coefficient_;
    };

    /// <summary>
    ///   A callback that computes the volume of a demand stored in an
    ///   integer array.
    /// </summary>
    class Demand
    {
        public Demand(RoutingIndexManager manager, int[] order_demands)
        {
            this.manager_ = manager;
            this.order_demands_ = order_demands;
        }

        public long Call(long index)
        {
            if (index < order_demands_.Length)
            {
                int node = manager_.IndexToNode(index);
                return order_demands_[node];
            }
            return 0;
        }

        private readonly RoutingIndexManager manager_;
        private readonly int[] order_demands_;
    };

    /// Locations representing either an order location or a vehicle route
    /// start/end.
    private Position[] locations_;
    /// Quantity to be picked up for each order.
    private int[] order_demands_;
    /// Time window in which each order must be performed.
    private TimeWindow[] order_time_windows_;
    /// Penalty cost "paid" for dropping an order.
    private int[] order_penalties_;
    /// Capacity of the vehicles.
    private int vehicle_capacity_ = 0;
    /// Latest time at which each vehicle must end its tour.
    private int[] vehicle_end_time_;
    /// Cost per unit of distance of each vehicle.
    private int[] vehicle_cost_coefficients_;
    /// Vehicle start and end indices. They have to be implemented as int[] due
    /// to the available SWIG-ed interface.
    private int[] vehicle_starts_;
    private int[] vehicle_ends_;

    /// Random number generator to produce data.
    private Random random_generator = new Random(0xBEEF);

    /// <summary>
    ///    Constructs a capacitated vehicle routing problem with time windows.
    /// </summary>
    private CapacitatedVehicleRoutingProblemWithTimeWindows()
    {
    }

    /// <summary>
    ///   Creates order data. Location of the order is random, as well
    ///   as its demand (quantity), time window and penalty.  ///
    ///   </summary>
    /// <param name="number_of_orders"> number of orders to build. </param>
    /// <param name="x_max"> maximum x coordinate in which orders are located.
    /// </param>
    /// <param name="y_max"> maximum y coordinate in which orders are located.
    /// </param>
    /// <param name="demand_max"> maximum quantity of a demand. </param>
    /// <param name="time_window_max"> maximum starting time of the order time
    /// window. </param>
    /// <param name="time_window_width"> duration of the order time window.
    /// </param>
    /// <param name="penalty_min"> minimum pernalty cost if order is dropped.
    /// </param>
    /// <param name="penalty_max"> maximum pernalty cost if order is dropped.
    /// </param>
    private void BuildOrders(int number_of_orders, int number_of_vehicles, int x_max, int y_max, int demand_max,
                             int time_window_max, int time_window_width, int penalty_min, int penalty_max)
    {
        Console.WriteLine("Building orders.");
        locations_ = new Position[number_of_orders + 2 * number_of_vehicles];
        order_demands_ = new int[number_of_orders];
        order_time_windows_ = new TimeWindow[number_of_orders];
        order_penalties_ = new int[number_of_orders];
        for (int order = 0; order < number_of_orders; ++order)
        {
            locations_[order] = new Position(random_generator.Next(x_max + 1), random_generator.Next(y_max + 1));
            order_demands_[order] = random_generator.Next(demand_max + 1);
            int time_window_start = random_generator.Next(time_window_max + 1);
            order_time_windows_[order] = new TimeWindow(time_window_start, time_window_start + time_window_width);
            order_penalties_[order] = random_generator.Next(penalty_max - penalty_min + 1) + penalty_min;
        }
    }

    /// <summary>
    /// Creates fleet data. Vehicle starting and ending locations are
    /// random, as well as vehicle costs per distance unit.
    /// </summary>
    ///
    /// <param name="number_of_orders"> number of orders</param>
    /// <param name="number_of_vehicles"> number of vehicles</param>
    /// <param name="x_max"> maximum x coordinate in which orders are located.
    /// </param>
    /// <param name="y_max"> maximum y coordinate in which orders are located.
    /// </param>
    /// <param name="end_time"> latest end time of a tour of a vehicle. </param>
    /// <param name="capacity"> capacity of a vehicle. </param>
    /// <param name="cost_coefficient_max"> maximum cost per distance unit of a
    /// vehicle (minimum is 1)</param>
    private void BuildFleet(int number_of_orders, int number_of_vehicles, int x_max, int y_max, int end_time,
                            int capacity, int cost_coefficient_max)
    {
        Console.WriteLine("Building fleet.");
        vehicle_capacity_ = capacity;
        vehicle_starts_ = new int[number_of_vehicles];
        vehicle_ends_ = new int[number_of_vehicles];
        vehicle_end_time_ = new int[number_of_vehicles];
        vehicle_cost_coefficients_ = new int[number_of_vehicles];
        for (int vehicle = 0; vehicle < number_of_vehicles; ++vehicle)
        {
            int index = 2 * vehicle + number_of_orders;
            vehicle_starts_[vehicle] = index;
            locations_[index] = new Position(random_generator.Next(x_max + 1), random_generator.Next(y_max + 1));
            vehicle_ends_[vehicle] = index + 1;
            locations_[index + 1] = new Position(random_generator.Next(x_max + 1), random_generator.Next(y_max + 1));
            vehicle_end_time_[vehicle] = end_time;
            vehicle_cost_coefficients_[vehicle] = random_generator.Next(cost_coefficient_max) + 1;
        }
    }

    /// <summary>
    ///   Solves the current routing problem.
    /// </summary>
    private void Solve(int number_of_orders, int number_of_vehicles)
    {
        Console.WriteLine("Creating model with " + number_of_orders + " orders and " + number_of_vehicles +
                          " vehicles.");
        // Finalizing model
        int number_of_locations = locations_.Length;

        RoutingIndexManager manager =
            new RoutingIndexManager(number_of_locations, number_of_vehicles, vehicle_starts_, vehicle_ends_);
        RoutingModel model = new RoutingModel(manager);

        // Setting up dimensions
        const int big_number = 100000;
        Manhattan manhattan_callback = new Manhattan(manager, locations_, 1);
        model.AddDimension(model.RegisterTransitCallback(manhattan_callback.Call), big_number, big_number, false,
                           "time");
        RoutingDimension time_dimension = model.GetDimensionOrDie("time");

        Demand demand_callback = new Demand(manager, order_demands_);
        model.AddDimension(model.RegisterUnaryTransitCallback(demand_callback.Call), 0, vehicle_capacity_, true,
                           "capacity");
        RoutingDimension capacity_dimension = model.GetDimensionOrDie("capacity");

        // Setting up vehicles
        Manhattan[] cost_callbacks = new Manhattan[number_of_vehicles];
        for (int vehicle = 0; vehicle < number_of_vehicles; ++vehicle)
        {
            int cost_coefficient = vehicle_cost_coefficients_[vehicle];
            Manhattan manhattan_cost_callback = new Manhattan(manager, locations_, cost_coefficient);
            cost_callbacks[vehicle] = manhattan_cost_callback;
            int manhattan_cost_index = model.RegisterTransitCallback(manhattan_cost_callback.Call);
            model.SetArcCostEvaluatorOfVehicle(manhattan_cost_index, vehicle);
            time_dimension.CumulVar(model.End(vehicle)).SetMax(vehicle_end_time_[vehicle]);
        }

        // Setting up orders
        for (int order = 0; order < number_of_orders; ++order)
        {
            time_dimension.CumulVar(order).SetRange(order_time_windows_[order].start_, order_time_windows_[order].end_);
            long[] orders = { manager.NodeToIndex(order) };
            model.AddDisjunction(orders, order_penalties_[order]);
        }

        // Solving
        RoutingSearchParameters search_parameters =
            operations_research_constraint_solver.DefaultRoutingSearchParameters();
        search_parameters.FirstSolutionStrategy = FirstSolutionStrategy.Types.Value.AllUnperformed;

        Console.WriteLine("Search...");
        Assignment solution = model.SolveWithParameters(search_parameters);

        if (solution != null)
        {
            String output = "Total cost: " + solution.ObjectiveValue() + "\n";
            // Dropped orders
            String dropped = "";
            for (int order = 0; order < number_of_orders; ++order)
            {
                if (solution.Value(model.NextVar(order)) == order)
                {
                    dropped += " " + order;
                }
            }
            if (dropped.Length > 0)
            {
                output += "Dropped orders:" + dropped + "\n";
            }
            // Routes
            for (int vehicle = 0; vehicle < number_of_vehicles; ++vehicle)
            {
                String route = "Vehicle " + vehicle + ": ";
                if (!model.IsVehicleUsed(solution, vehicle))
                {
                    route += "Empty";
                }
                else
                {
                    long order = model.Start(vehicle);
                    for (; !model.IsEnd(order); order = solution.Value(model.NextVar(order)))
                    {
                        IntVar local_load = capacity_dimension.CumulVar(order);
                        IntVar local_time = time_dimension.CumulVar(order);
                        route += order + " Load(" + solution.Value(local_load) + ") " + "Time(" +
                                 solution.Min(local_time) + ", " + solution.Max(local_time) + ") -> ";
                    }
                    IntVar load = capacity_dimension.CumulVar(order);
                    IntVar time = time_dimension.CumulVar(order);
                    route += order + " Load(" + solution.Value(load) + ") " + "Time(" + solution.Min(time) + ", " +
                             solution.Max(time) + ")";
                }
                output += route + "\n";
            }
            Console.WriteLine(output);
        }
    }

    public static void Main(String[] args)
    {
        CapacitatedVehicleRoutingProblemWithTimeWindows problem = new CapacitatedVehicleRoutingProblemWithTimeWindows();
        int x_max = 20;
        int y_max = 20;
        int demand_max = 3;
        int time_window_max = 24 * 60;
        int time_window_width = 4 * 60;
        int penalty_min = 50;
        int penalty_max = 100;
        int end_time = 24 * 60;
        int cost_coefficient_max = 3;

        int orders = 100;
        int vehicles = 20;
        int capacity = 50;

        problem.BuildOrders(orders, vehicles, x_max, y_max, demand_max, time_window_max, time_window_width, penalty_min,
                            penalty_max);
        problem.BuildFleet(orders, vehicles, x_max, y_max, end_time, capacity, cost_coefficient_max);
        problem.Solve(orders, vehicles);
    }
}
