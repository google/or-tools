// Copyright 2010-2021 Google LLC
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

using Google.OrTools.Sat;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

/// <summary>
/// This model solves a multicommodity mono-routing problem with
/// capacity constraints and a max usage cost structure.  This means
/// that given a graph with capacity on edges, and a set of demands
/// (source, destination, traffic), the goal is to assign one unique
/// path for each demand such that the cost is minimized.  The cost is
/// defined by the maximum ratio utilization (traffic/capacity) for all
/// arcs.  There is also a penalty associated with an traffic of an arc
/// being above the comfort zone, 85% of the capacity by default.
/// Please note that constraint programming is well suited here because
/// we cannot have multiple active paths for a single demand.
/// Otherwise, a approach based on a linear solver is a better match.
/// A random problem generator is also included.
/// </summary>
public class NetworkRoutingSat
{
    private static int clients = 0;           // Number of network clients nodes. If equal to zero, then all
                                              // backbones nodes are also client nodes.
    private static int backbones = 0;         // "Number of backbone nodes"
    private static int demands = 0;           // "Number of network demands."
    private static int trafficMin = 0;        // "Min traffic of a demand."
    private static int trafficMax = 0;        // "Max traffic of a demand."
    private static int minClientDegree = 0;   //"Min number of connections from a client to the backbone."
    private static int maxClientDegree = 0;   //"Max number of connections from a client to the backbone."
    private static int minBackboneDegree = 0; //"Min number of connections from a backbone node to the rest of the
                                              // backbone nodes."
    private static int maxBackboneDegree = 0; // "Max number of connections from a backbone node to
                                              // the rest of the backbone nodes."
    private static int maxCapacity = 0;       //"Max traffic on any arc."
    private static int fixedChargeCost = 0;   //"Fixed charged cost when using an arc."
    private static int seed = 0;              //"Random seed"
    private static double comfortZone = 0.85; // "Above this limit in 1/1000th, the link is said to
                                              // be congested."
    private static int extraHops = 6;         // "When creating all paths for a demand, we look at paths with
                                              // maximum length 'shortest path + extra_hops'"
    private static int maxPaths = 1200;       //"Max number of possible paths for a demand."
    private static bool printModel = false;   //"Print details of the model."
    private static string parameters = "";    // "Sat parameters."

    private const long kDisconnectedDistance = -1L;

    static void Main(string[] args)
    {
        readArgs(args);
        var builder = new NetworkRoutingDataBuilder();
        var data = builder.BuildModelFromParameters(clients, backbones, demands, trafficMin, trafficMax,
                                                    minClientDegree, maxClientDegree, minBackboneDegree,
                                                    maxBackboneDegree, maxCapacity, fixedChargeCost, seed);
        var solver = new NetworkRoutingSolver();
        solver.Init(data, extraHops, maxPaths);
        var cost = solver.Solve();
        Console.WriteLine($"Final cost = {cost}");
    }

    private static void readArgs(string[] args)
    {
        readInt(args, ref clients, nameof(clients));
        readInt(args, ref backbones, nameof(backbones));
        readInt(args, ref demands, nameof(demands));
        readInt(args, ref trafficMin, nameof(trafficMin));
        readInt(args, ref trafficMax, nameof(trafficMax));
        readInt(args, ref minClientDegree, nameof(minClientDegree));
        readInt(args, ref maxClientDegree, nameof(maxClientDegree));
        readInt(args, ref minBackboneDegree, nameof(minBackboneDegree));
        readInt(args, ref maxBackboneDegree, nameof(maxBackboneDegree));
        readInt(args, ref maxCapacity, nameof(maxCapacity));
        readInt(args, ref fixedChargeCost, nameof(fixedChargeCost));
        readInt(args, ref seed, nameof(seed));
        readDouble(args, ref comfortZone, nameof(comfortZone));
        readInt(args, ref extraHops, nameof(extraHops));
        readInt(args, ref maxPaths, nameof(maxPaths));
        readBoolean(args, ref printModel, nameof(printModel));
        readString(args, ref parameters, nameof(parameters));
    }

    private static void readDouble(string[] args, ref double setting, string arg)
    {
        var v = getArgValue(args, arg);
        if (v.IsSet)
        {
            setting = Convert.ToDouble(v.Value);
        }
    }

    private static void readInt(string[] args, ref int setting, string arg)
    {
        var v = getArgValue(args, arg);
        if (v.IsSet)
        {
            setting = Convert.ToInt32(v.Value);
        }
    }

    private static void readBoolean(string[] args, ref bool setting, string arg)
    {
        var v = getArgValue(args, arg);
        if (v.IsSet)
        {
            setting = Convert.ToBoolean(v.Value);
        }
    }

    private static void readString(string[] args, ref string setting, string arg)
    {
        var v = getArgValue(args, arg);
        if (v.IsSet)
        {
            setting = v.Value;
        }
    }

    private static (bool IsSet, string Value) getArgValue(string[] args, string arg)
    {
        string lookup = $"--{arg}=";

        var item = args.FirstOrDefault(x => x.StartsWith(lookup));

        if (string.IsNullOrEmpty(item))
        {
            return (false, string.Empty);
        }

        return (true, item.Replace(lookup, string.Empty));
    }

    /// <summary>
    /// Contains problem data. It assumes capacities are symmetrical:
    ///   (capacity(i->j) == capacity(j->i)).
    /// Demands are not symmetrical.
    /// </summary>
    public class NetworkRoutingData
    {
        private Dictionary<(int source, int destination), int> _arcs =
            new Dictionary<(int source, int destination), int>();
        private Dictionary<(int node1, int node2), int> _demands = new Dictionary<(int node1, int node2), int>();

        public int NumberOfNodes { get; set; } = -1;

        public int NumberOfArcs
        {
            get {
                return _arcs.Count();
            }
        }

        public int NumberOfDemands
        {
            get {
                return _demands.Count();
            }
        }

        public int MaximumCapacity { get; set; } = -1;
        public int FixedChargeCost { get; set; } = -1;
        public string Name { get; set; } = string.Empty;

        public void AddDemand(int source, int destination, int traffic)
        {
            var pair = (source, destination);
            if (!_demands.ContainsKey(pair))
                _demands.Add(pair, traffic);
        }

        public void AddArc(int node1, int node2, int capacity)
        {
            _arcs.Add((Math.Min(node1, node2), Math.Max(node1, node2)), capacity);
        }

        public int Demand(int source, int destination)
        {
            var pair = (source, destination);
            if (_demands.TryGetValue(pair, out var demand))
                return demand;

            return 0;
        }

        public int Capacity(int node1, int node2)
        {
            var pair = (Math.Min(node1, node2), Math.Max(node1, node2));
            if (_arcs.TryGetValue(pair, out var capacity))
                return capacity;

            return 0;
        }
    }

    /// <summary>
    /// Random generator of problem. This generator creates a random
    /// problem. This problem uses a special topology. There are
    /// 'numBackbones' nodes and 'numClients' nodes. if 'numClients' is
    /// null, then all backbones nodes are also client nodes. All traffic
    /// originates and terminates in client nodes. Each client node is
    /// connected to 'minClientDegree' - 'maxClientDegree' backbone
    /// nodes. Each backbone node is connected to 'minBackboneDegree' -
    /// 'maxBackboneDegree' other backbone nodes. There are 'numDemands'
    /// demands, with a traffic between 'trafficMin' and 'trafficMax'.
    /// Each arc has a capacity of 'maxCapacity'. Using an arc incurs a
    /// fixed cost of 'fixedChargeCost'.
    /// </summary>
    public class NetworkRoutingDataBuilder
    {
        private List<List<bool>> _network;
        private List<int> _degrees;
        private Random _random;

        public NetworkRoutingData BuildModelFromParameters(int numClients, int numBackbones, int numDemands,
                                                           int trafficMin, int trafficMax, int minClientDegree,
                                                           int maxClientDegree, int minBackboneDegree,
                                                           int maxBackboneDegree, int maxCapacity, int fixedChargeCost,
                                                           int seed)
        {
            Debug.Assert(numBackbones >= 1);
            Debug.Assert(numClients >= 0);
            Debug.Assert(numDemands >= 1);
            Debug.Assert(numDemands <= (numClients == 0 ? numBackbones * numBackbones : numClients * numBackbones));
            Debug.Assert(maxClientDegree >= minClientDegree);
            Debug.Assert(maxBackboneDegree >= minBackboneDegree);
            Debug.Assert(trafficMax >= 1);
            Debug.Assert(trafficMax >= trafficMin);
            Debug.Assert(trafficMin >= 1);
            Debug.Assert(maxBackboneDegree >= 2);
            Debug.Assert(maxClientDegree >= 2);
            Debug.Assert(maxClientDegree <= numBackbones);
            Debug.Assert(maxBackboneDegree <= numBackbones);
            Debug.Assert(maxCapacity >= 1);

            int size = numBackbones + numClients;
            initData(size, seed);
            buildGraph(numClients, numBackbones, minClientDegree, maxClientDegree, minBackboneDegree,
                       maxBackboneDegree);
            NetworkRoutingData data = new NetworkRoutingData();
            createDemands(numClients, numBackbones, numDemands, trafficMin, trafficMax, data);
            fillData(numClients, numBackbones, numDemands, trafficMin, trafficMax, minClientDegree, maxClientDegree,
                     minBackboneDegree, maxBackboneDegree, maxCapacity, fixedChargeCost, seed, data);

            return data;
        }

        private void initData(int size, int seed)
        {
            _network = new List<List<bool>>(size);
            for (int i = 0; i < size; i++)
            {
                _network.Add(new List<bool>(size));
                for (int j = 0; j < size; j++)
                {
                    _network[i].Add(false);
                }
            }

            _degrees = new List<int>(size);
            for (int i = 0; i < size; i++)
            {
                _degrees.Add(0);
            }

            _random = new Random(seed);
        }

        private void buildGraph(int numClients, int numBackbones, int minClientDegree, int maxClientDegree,
                                int minBackboneDegree, int maxBackboneDegree)
        {
            int size = numBackbones + numClients;
            for (int i = 1; i < numBackbones; i++)
            {
                int j = randomUniform(i);
                addEdge(i, j);
            }

            List<int> notFull = new List<int>();
            HashSet<int> toComplete = new HashSet<int>();

            for (int i = 0; i < numBackbones; i++)
            {
                if (_degrees[i] < minBackboneDegree)
                {
                    toComplete.Add(i);
                }

                if (_degrees[i] < maxBackboneDegree)
                {
                    notFull.Add(i);
                }
            }

            while (toComplete.Any() && notFull.Count > 1)
            {
                int node1 = getNextToComplete(toComplete);
                int node2 = node1;
                while (node2 == node1 || _degrees[node2] >= maxBackboneDegree)
                {
                    node2 = randomUniform(numBackbones);
                }

                addEdge(node1, node2);

                if (_degrees[node1] >= minBackboneDegree)
                {
                    toComplete.Remove(node1);
                }

                if (_degrees[node2] >= minBackboneDegree)
                {
                    toComplete.Remove(node2);
                }

                if (_degrees[node1] >= maxBackboneDegree)
                {
                    notFull.Remove(node1);
                }

                if (_degrees[node2] >= maxBackboneDegree)
                {
                    notFull.Remove(node2);
                }
            }

            // Then create the client nodes connected to the backbone nodes.
            // If numClient is 0, then backbone nodes are also client nodes.

            for (int i = numBackbones; i < size; i++)
            {
                int degree = randomInInterval(minClientDegree, maxClientDegree);

                while (_degrees[i] < degree)
                {
                    int j = randomUniform(numBackbones);
                    if (!_network[i][j])
                    {
                        addEdge(i, j);
                    }
                }
            }
        }

        private int getNextToComplete(HashSet<int> toComplete)
        {
            return toComplete.Last();
        }

        private void createDemands(int numClients, int numBackbones, int numDemands, int trafficMin, int trafficMax,
                                   NetworkRoutingData data)
        {
            while (data.NumberOfDemands < numDemands)
            {
                int source = randomClient(numClients, numBackbones);
                int dest = source;
                while (dest == source)
                {
                    dest = randomClient(numClients, numBackbones);
                }

                int traffic = randomInInterval(trafficMin, trafficMax);
                data.AddDemand(source, dest, traffic);
            }
        }

        private void fillData(int numClients, int numBackbones, int numDemands, int trafficMin, int trafficMax,
                              int minClientDegree, int maxClientDegree, int minBackboneDegree, int maxBackboneDegree,
                              int maxCapacity, int fixedChargeCost, int seed, NetworkRoutingData data)
        {
            int size = numBackbones + numClients;
            string name =
                $"mp_c{numClients}_b{numBackbones}_d{numDemands}.t{trafficMin}-{trafficMax}.cd{minClientDegree}-{maxClientDegree}.bd{minBackboneDegree}-{maxBackboneDegree}.mc{maxCapacity}.fc{fixedChargeCost}.s{seed}";
            data.Name = name;

            data.NumberOfNodes = size;
            int numArcs = 0;
            for (int i = 0; i < size - 1; i++)
            {
                for (int j = i + 1; j < size; j++)
                {
                    if (_network[i][j])
                    {
                        data.AddArc(i, j, maxCapacity);
                        numArcs++;
                    }
                }
            }

            data.MaximumCapacity = maxCapacity;
            data.FixedChargeCost = fixedChargeCost;
        }

        private void addEdge(int i, int j)
        {
            _degrees[i]++;
            _degrees[j]++;
            _network[i][j] = true;
            _network[j][i] = true;
        }

        private int randomInInterval(int intervalMin, int intervalMax)
        {
            var p = randomUniform(intervalMax - intervalMin + 1) + intervalMin;
            return p;
        }

        private int randomClient(int numClients, int numBackbones)
        {
            var p = (numClients == 0) ? randomUniform(numBackbones) : randomUniform(numClients) + numBackbones;
            return p;
        }

        private int randomUniform(int max)
        {
            var r = _random.Next(max);
            return r;
        }
    }

    [DebuggerDisplay("Source {Source} Destination {Destination} Traffic {Traffic}")]
    public struct Demand
    {
        public Demand(int source, int destination, int traffic)
        {
            Source = source;
            Destination = destination;
            Traffic = traffic;
        }

        public int Source { get; }
        public int Destination { get; }
        public int Traffic { get; }
    }

    public class NetworkRoutingSolver
    {
        private List<(long source, long destination, int arcId)> _arcsData =
            new List<(long source, long destination, int arcId)>();
        private List<int> _arcCapacity = new List<int>();
        private List<Demand> _demands = new List<Demand>();
        private List<int> _allMinPathLengths = new List<int>();
        private List<List<int>> _capacity;
        private List<List<HashSet<int>>> _allPaths;

        public int NumberOfNodes { get; private set; } = -1;

        private int countArcs
        {
            get {
                return _arcsData.Count / 2;
            }
        }

        public void ComputeAllPathsForOneDemandAndOnePathLength(int demandIndex, int maxLength, int maxPaths)
        {
            // We search for paths of length exactly 'maxLength'.
            CpModel cpModel = new CpModel();
            var arcVars = new List<IntVar>();
            var nodeVars = new List<IntVar>();

            for (int i = 0; i < maxLength; i++)
            {
                nodeVars.Add(cpModel.NewIntVar(0, NumberOfNodes - 1, string.Empty));
            }

            for (int i = 0; i < maxLength - 1; i++)
            {
                arcVars.Add(cpModel.NewIntVar(-1, countArcs - 1, string.Empty));
            }

            var arcs = getArcsData();

            for (int i = 0; i < maxLength - 1; i++)
            {
                var tmpVars = new List<IntVar>();
                tmpVars.Add(nodeVars[i]);
                tmpVars.Add(nodeVars[i + 1]);
                tmpVars.Add(arcVars[i]);
                var table = cpModel.AddAllowedAssignments(tmpVars, arcs);
            }

            var demand = _demands[demandIndex];
            cpModel.Add(nodeVars[0] == demand.Source);
            cpModel.Add(nodeVars[maxLength - 1] == demand.Destination);
            cpModel.AddAllDifferent(arcVars);
            cpModel.AddAllDifferent(nodeVars);

            var solver = new CpSolver();
            solver.StringParameters = "enumerate_all_solutions:true";

            var solutionPrinter =
                new FeasibleSolutionChecker(demandIndex, ref _allPaths, maxLength, arcVars, maxPaths, nodeVars);
            var status = solver.Solve(cpModel, solutionPrinter);
        }

        private long[,] getArcsData()
        {
            long[,] arcs = new long[_arcsData.Count, 3];

            for (int i = 0; i < _arcsData.Count; i++)
            {
                var data = _arcsData[i];
                arcs[i, 0] = data.source;
                arcs[i, 1] = data.destination;
                arcs[i, 2] = data.arcId;
            }

            return arcs;
        }

        public int ComputeAllPaths(int extraHops, int maxPaths)
        {
            int numPaths = 0;
            for (int demandIndex = 0; demandIndex < _demands.Count; demandIndex++)
            {
                int minPathLength = _allMinPathLengths[demandIndex];

                for (int maxLength = minPathLength + 1; maxLength <= minPathLength + extraHops + 1; maxLength++)
                {
                    ComputeAllPathsForOneDemandAndOnePathLength(demandIndex, maxLength, maxPaths);

                    if (_allPaths[demandIndex].Count >= maxPaths)
                        break;
                }

                numPaths += _allPaths[demandIndex].Count;
            }

            return numPaths;
        }

        public void AddArcData(long source, long destination, int arcId)
        {
            _arcsData.Add((source, destination, arcId));
        }

        public void InitArcInfo(NetworkRoutingData data)
        {
            int numArcs = data.NumberOfArcs;
            _capacity = new List<List<int>>(NumberOfNodes);

            for (int nodeIndex = 0; nodeIndex < NumberOfNodes; nodeIndex++)
            {
                _capacity.Add(new List<int>(NumberOfNodes));
                for (int i = 0; i < NumberOfNodes; i++)
                {
                    _capacity[nodeIndex].Add(0);
                }
            }

            int arcId = 0;
            for (int i = 0; i < NumberOfNodes - 1; i++)
            {
                for (int j = i + 1; j < NumberOfNodes; j++)
                {
                    int capacity = data.Capacity(i, j);
                    if (capacity > 0)
                    {
                        AddArcData(i, j, arcId);
                        AddArcData(j, i, arcId);
                        arcId++;
                        _arcCapacity.Add(capacity);
                        _capacity[i][j] = capacity;
                        _capacity[j][i] = capacity;

                        if (printModel)
                        {
                            Console.WriteLine($"Arc {i} <-> {j} with capacity {capacity}");
                        }
                    }
                }
            }

            Debug.Assert(arcId == numArcs);
        }

        public int InitDemandInfo(NetworkRoutingData data)
        {
            int numDemands = data.NumberOfDemands;
            int totalDemand = 0;
            for (int i = 0; i < NumberOfNodes; i++)
            {
                for (int j = 0; j < NumberOfNodes; j++)
                {
                    int traffic = data.Demand(i, j);
                    if (traffic > 0)
                    {
                        _demands.Add(new Demand(i, j, traffic));
                        totalDemand += traffic;
                    }
                }
            }

            Debug.Assert(numDemands == _demands.Count);

            return totalDemand;
        }

        public long InitShortestPaths(NetworkRoutingData data)
        {
            int numDemands = data.NumberOfDemands;
            long totalCumulatedTraffic = 0L;
            _allMinPathLengths.Clear();
            var paths = new List<int>();

            for (int demandIndex = 0; demandIndex < numDemands; demandIndex++)
            {
                paths.Clear();
                var demand = _demands[demandIndex];
                var r = DijkstraShortestPath(NumberOfNodes, demand.Source, demand.Destination,
                                             ((int x, int y)p) => hasArc(p.x, p.y), kDisconnectedDistance, paths);

                _allMinPathLengths.Add(paths.Count - 1);
                var minPathLength = _allMinPathLengths[demandIndex];
                totalCumulatedTraffic += minPathLength * demand.Traffic;
            }

            return totalCumulatedTraffic;
        }

        public int InitPaths(NetworkRoutingData data, int extraHops, int maxPaths)
        {
            var numDemands = data.NumberOfDemands;
            Console.WriteLine("Computing all possible paths ");
            Console.WriteLine($"    - extra hops = {extraHops}");
            Console.WriteLine($"    - max paths per demand = {maxPaths}");

            _allPaths = new List<List<HashSet<int>>>(numDemands);

            var numPaths = ComputeAllPaths(extraHops, maxPaths);

            for (int demandIndex = 0; demandIndex < numDemands; demandIndex++)
            {
                var demand = _demands[demandIndex];
                Console.WriteLine(
                    $"Demand from {demand.Source} to {demand.Destination} with traffic {demand.Traffic}, amd {_allPaths[demandIndex].Count} possible paths.");
            }

            return numPaths;
        }

        public void Init(NetworkRoutingData data, int extraHops, int maxPaths)
        {
            Console.WriteLine($"Model {data.Name}");
            NumberOfNodes = data.NumberOfNodes;
            var numArcs = data.NumberOfArcs;
            var numDemands = data.NumberOfDemands;

            InitArcInfo(data);
            var totalDemand = InitDemandInfo(data);
            var totalAccumulatedTraffic = InitShortestPaths(data);
            var numPaths = InitPaths(data, extraHops, maxPaths);

            Console.WriteLine("Model created:");
            Console.WriteLine($"    - {NumberOfNodes} nodes");
            Console.WriteLine($"    - {numArcs} arcs");
            Console.WriteLine($"    - {numDemands} demands");
            Console.WriteLine($"    - a total traffic of {totalDemand}");
            Console.WriteLine($"    - a minimum cumulated traffic of {totalAccumulatedTraffic}");
            Console.WriteLine($"    - {numPaths} possible paths for all demands");
        }

        private long hasArc(int i, int j)
        {
            if (_capacity[i][j] > 0)
                return 1;
            else
                return kDisconnectedDistance;
        }

        public long Solve()
        {
            Console.WriteLine("Solving model");
            var numDemands = _demands.Count;
            var numArcs = countArcs;

            CpModel cpModel = new CpModel();

            var pathVars = new List<List<IntVar>>(numDemands);

            for (int demandIndex = 0; demandIndex < numDemands; demandIndex++)
            {
                pathVars.Add(new List<IntVar>());

                for (int arc = 0; arc < numArcs; arc++)
                {
                    pathVars[demandIndex].Add(cpModel.NewBoolVar(""));
                }

                long[,] tuples = new long[_allPaths[demandIndex].Count, numArcs];

                int pathCount = 0;
                foreach (var set in _allPaths[demandIndex])
                {
                    foreach (var arc in set)
                    {
                        tuples[pathCount, arc] = 1;
                    }

                    pathCount++;
                }

                var pathCt = cpModel.AddAllowedAssignments(pathVars[demandIndex], tuples);
            }

            var trafficVars = new List<IntVar>(numArcs);
            var normalizedTrafficVars = new List<IntVar>(numArcs);
            var comfortableTrafficVars = new List<IntVar>(numArcs);

            long maxNormalizedTraffic = 0;

            for (int arcIndex = 0; arcIndex < numArcs; arcIndex++)
            {
                long sumOfTraffic = 0;

                var vars = new List<IntVar>();
                var traffics = new List<int>();

                for (int i = 0; i < pathVars.Count; i++)
                {
                    sumOfTraffic += _demands[i].Traffic;
                    vars.Add(pathVars[i][arcIndex]);
                    traffics.Add(_demands[i].Traffic);
                }

                var sum = LinearExpr.ScalProd(vars, traffics);
                var trafficVar = cpModel.NewIntVar(0, sumOfTraffic, $"trafficVar{arcIndex}");
                trafficVars.Add(trafficVar);
                cpModel.Add(sum == trafficVar);

                var capacity = _arcCapacity[arcIndex];
                var scaledTraffic = cpModel.NewIntVar(0, sumOfTraffic * 1000, $"scaledTrafficVar{arcIndex}");
                var scaledTrafficVar = trafficVar * 1000;
                cpModel.Add(scaledTrafficVar == scaledTraffic);

                var normalizedTraffic =
                    cpModel.NewIntVar(0, sumOfTraffic * 1000 / capacity, $"normalizedTraffic{arcIndex}");

                maxNormalizedTraffic = Math.Max(maxNormalizedTraffic, sumOfTraffic * 1000 / capacity);
                cpModel.AddDivisionEquality(normalizedTraffic, scaledTraffic, cpModel.NewConstant(capacity));
                normalizedTrafficVars.Add(normalizedTraffic);
                var comfort = cpModel.NewBoolVar($"comfort{arcIndex}");
                var safeCapacity = (long)(capacity * comfortZone);
                cpModel.Add(trafficVar > safeCapacity).OnlyEnforceIf(comfort);
                cpModel.Add(trafficVar <= safeCapacity).OnlyEnforceIf(comfort.Not());
                comfortableTrafficVars.Add(comfort);
            }

            var maxUsageCost = cpModel.NewIntVar(0, maxNormalizedTraffic, "maxUsageCost");
            cpModel.AddMaxEquality(maxUsageCost, normalizedTrafficVars);

            var obj = new List<IntVar>() { maxUsageCost };
            obj.AddRange(comfortableTrafficVars);
            cpModel.Minimize(LinearExpr.Sum(obj));

            CpSolver solver = new CpSolver();
            solver.StringParameters = parameters + " enumerate_all_solutions:true";

            CpSolverStatus status =
                solver.Solve(cpModel, new FeasibleSolutionChecker2(maxUsageCost, comfortableTrafficVars, trafficVars));

            return (long)solver.ObjectiveValue;
        }
    }

    private class DijkstraSP
    {
        private const long kInfinity = long.MaxValue / 2;

        private readonly Func<(int, int), long> _graph;
        private readonly int[] _predecessor;
        private readonly List<Element> _elements;
        private readonly AdjustablePriorityQueue<Element> _frontier;
        private readonly List<int> _notVisited = new List<int>();
        private readonly List<int> _addedToFrontier = new List<int>();

        public DijkstraSP(int nodeCount, int startNode, Func<(int, int), long> graph, long disconnectedDistance)
        {
            NodeCount = nodeCount;
            StartNode = startNode;
            this._graph = graph;
            DisconnectedDistance = disconnectedDistance;
            _predecessor = new int[nodeCount];
            _elements = new List<Element>(nodeCount);
            _frontier = new AdjustablePriorityQueue<Element>();
        }

        public int NodeCount { get; }
        public int StartNode { get; }
        public long DisconnectedDistance { get; }

        public bool ShortestPath(int endNode, List<int> nodes)
        {
            initialize();
            bool found = false;
            while (!_frontier.IsEmpty)
            {
                long distance;
                int node = selectClosestNode(out distance);
                if (distance == kInfinity)
                {
                    found = false;
                    break;
                }
                else if (node == endNode)
                {
                    found = true;
                    break;
                }
                update(node);
            }

            if (found)
            {
                findPath(endNode, nodes);
            }

            return found;
        }

        private void initialize()
        {
            for (int i = 0; i < NodeCount; i++)
            {
                _elements.Add(new Element { Node = i });

                if (i == StartNode)
                {
                    _predecessor[i] = -1;
                    _elements[i].Distance = 0;
                    _frontier.Add(_elements[i]);
                }
                else
                {
                    _elements[i].Distance = kInfinity;
                    _predecessor[i] = StartNode;
                    _notVisited.Add(i);
                }
            }
        }

        private int selectClosestNode(out long distance)
        {
            var node = _frontier.Top().Node;
            distance = _frontier.Top().Distance;
            _frontier.Pop();
            _notVisited.Remove(node);
            _addedToFrontier.Remove(node);
            return node;
        }

        private void update(int node)
        {
            foreach (var otherNode in _notVisited)
            {
                var graphNode = _graph((node, otherNode));

                if (graphNode != DisconnectedDistance)
                {
                    if (!_addedToFrontier.Contains(otherNode))
                    {
                        _frontier.Add(_elements[otherNode]);
                        _addedToFrontier.Add(otherNode);
                    }

                    var otherDistance = _elements[node].Distance + graphNode;

                    if (_elements[otherNode].Distance > otherDistance)
                    {
                        _elements[otherNode].Distance = otherDistance;
                        _frontier.NoteChangedPriority(_elements[otherNode]);
                        _predecessor[otherNode] = node;
                    }
                }
            }
        }

        private void findPath(int dest, List<int> nodes)
        {
            var j = dest;
            nodes.Add(j);
            while (_predecessor[j] != -1)
            {
                nodes.Add(_predecessor[j]);
                j = _predecessor[j];
            }
        }
    }

    public static bool DijkstraShortestPath(int nodeCount, int startNode, int endNode, Func<(int, int), long> graph,
                                            long disconnectedDistance, List<int> nodes)
    {
        DijkstraSP bf = new DijkstraSP(nodeCount, startNode, graph, disconnectedDistance);
        return bf.ShortestPath(endNode, nodes);
    }

    [DebuggerDisplay("Node = {Node}, HeapIndex = {HeapIndex}, Distance = {Distance}")]
    private class Element : IHasHeapIndex, IComparable<Element>
    {
        public int HeapIndex { get; set; } = -1;
        public long Distance { get; set; } = 0;
        public int Node { get; set; } = -1;

        public int CompareTo(Element other)
        {
            if (this.Distance > other.Distance)
                return -1;

            if (this.Distance < other.Distance)
                return 1;

            return 0;
        }
    }

    private class AdjustablePriorityQueue<T>
        where T : class, IHasHeapIndex, IComparable<T>
    {
        private readonly List<T> _elems = new List<T>();

        public void Add(T val)
        {
            _elems.Add(val);
            adjustUpwards(_elems.Count - 1);
        }

        public void Remove(T val)
        {
            var i = val.HeapIndex;
            if (i == _elems.Count - 1)
            {
                _elems.RemoveAt(_elems.Count - 1);
                return;
            }

            _elems[i] = _elems.Last();
            _elems[i].HeapIndex = i;
            _elems.RemoveAt(_elems.Count - 1);
            NoteChangedPriority(_elems[i]);
        }

        public bool Contains(T val)
        {
            var i = val.HeapIndex;
            if (i < 0 || i >= _elems.Count || _elems[i].CompareTo(val) != 0)
                return false;

            return true;
        }

        public T Top()
        {
            return _elems[0];
        }

        public void Pop()
        {
            Remove(Top());
        }

        public int Size()
        {
            return _elems.Count;
        }

        public bool IsEmpty
        {
            get {
                return !_elems.Any();
            }
        }

        public void Clear()
        {
            _elems.Clear();
        }

        public void CheckValid()
        {
            for (int i = 0; i < _elems.Count; i++)
            {
                var leftChild = 1 + 2 * i;
                if (leftChild < _elems.Count)
                {
                    var compare = _elems[i].CompareTo(_elems[leftChild]);
                    Debug.Assert(compare >= 0);
                }

                int rightChild = leftChild + 1;
                if (rightChild < _elems.Count)
                {
                    var compare = _elems[i].CompareTo(_elems[rightChild]);
                    Debug.Assert(compare >= 0);
                }
            }
        }

        public void NoteChangedPriority(T val)
        {
            if (_elems.Count == 0)
                return;

            var i = val.HeapIndex;
            var parent = (i - 1) / 2;
            if (_elems[parent].CompareTo(val) == -1)
            {
                adjustUpwards(i);
            }
            else
            {
                adjustDownwards(i);
            }
        }

        private void adjustUpwards(int i)
        {
            var t = _elems[i];
            while (i > 0)
            {
                var parent = (i - 1) / 2;
                if (_elems[parent].CompareTo(t) != -1)
                {
                    break;
                }

                _elems[i] = _elems[parent];
                _elems[i].HeapIndex = i;
                i = parent;
            }

            _elems[i] = t;
            t.HeapIndex = i;
        }

        private void adjustDownwards(int i)
        {
            var t = _elems[i];
            while (true)
            {
                var leftChild = 1 + 2 * i;
                if (leftChild >= _elems.Count)
                {
                    break;
                }

                var rightChild = leftChild + 1;
                var next = (rightChild < _elems.Count && _elems[leftChild].CompareTo(_elems[rightChild]) == -1)
                               ? rightChild
                               : leftChild;

                if (t.CompareTo(_elems[next]) != -1)
                {
                    break;
                }

                _elems[i] = _elems[next];
                _elems[i].HeapIndex = i;
                i = next;
            }

            _elems[i] = t;
            t.HeapIndex = i;
        }
    }

    public interface IHasHeapIndex
    {
        int HeapIndex { get; set; }
    }

    private class FeasibleSolutionChecker : CpSolverSolutionCallback
    {
        public FeasibleSolutionChecker(int demandIndex, ref List<List<HashSet<int>>> allPaths, int maxLength,
                                       List<IntVar> arcVars, int maxPaths, List<IntVar> nodeVars)
        {
            DemandIndex = demandIndex;
            AllPaths = allPaths;
            MaxLength = maxLength;
            ArcVars = arcVars;
            MaxPaths = maxPaths;
            NodeVars = nodeVars;
        }

        public int DemandIndex { get; }
        public List<List<HashSet<int>>> AllPaths { get; }
        public int MaxLength { get; }
        public List<IntVar> ArcVars { get; }
        public int MaxPaths { get; }
        public List<IntVar> NodeVars { get; }

        public override void OnSolutionCallback()
        {
            if (AllPaths.Count < DemandIndex + 1)
                AllPaths.Add(new List<HashSet<int>>());

            int pathId = AllPaths[DemandIndex].Count;
            AllPaths[DemandIndex].Add(new HashSet<int>());

            for (int i = 0; i < MaxLength - 1; i++)
            {
                int arc = (int)this.SolutionIntegerValue(ArcVars[i].GetIndex());
                AllPaths[DemandIndex][pathId].Add(arc);
            }

            if (AllPaths[DemandIndex].Count() >= MaxPaths)
            {
                StopSearch();
            }
        }
    }

    private class FeasibleSolutionChecker2 : CpSolverSolutionCallback
    {
        public IntVar MaxUsageCost { get; }
        public List<IntVar> ComfortableTrafficVars { get; }
        public List<IntVar> TrafficVars { get; }
        private int _numSolutions = 0;

        public FeasibleSolutionChecker2(IntVar maxUsageCost, List<IntVar> comfortableTrafficVars,
                                        List<IntVar> trafficVars)
        {
            MaxUsageCost = maxUsageCost;
            ComfortableTrafficVars = comfortableTrafficVars;
            TrafficVars = trafficVars;
        }

        public override void OnSolutionCallback()
        {
            Console.WriteLine($"Solution {_numSolutions}");
            var percent = SolutionIntegerValue(MaxUsageCost.GetIndex()) / 10.0;
            int numNonComfortableArcs = 0;

            foreach (var comfort in ComfortableTrafficVars)
            {
                numNonComfortableArcs += SolutionBooleanValue(comfort.GetIndex()) ? 1 : 0;
            }

            if (numNonComfortableArcs > 0)
            {
                Console.WriteLine(
                    $"*** Found a solution with a max usage of {percent}%, and {numNonComfortableArcs} links above the comfort zone");
            }
            else
            {
                Console.WriteLine($"*** Found a solution with a max usage of {percent}%");
            }

            _numSolutions++;
        }
    }
}
