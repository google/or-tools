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

// This model solves a multicommodity mono-routing problem with
// capacity constraints and a max usage cost structure.  This means
// that given a graph with capacity on edges, and a set of demands
// (source, destination, traffic), the goal is to assign one unique
// path for each demand such that the cost is minimized.  The cost is
// defined by the maximum ratio utilization (traffic/capacity) for all
// arcs.  There is also a penalty associated with an traffic of an arc
// being above the comfort zone, 85% of the capacity by default.
// Please note that constraint programming is well suited here because
// we cannot have multiple active paths for a single demand.
// Otherwise, a approach based on a linear solver is a better match.

// A random problem generator is also included.

#include <atomic>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/random/uniform_int_distribution.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/graph/shortestpaths.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/util/time_limit.h"

// ----- Data Generator -----
ABSL_FLAG(int, clients, 0,
          "Number of network clients nodes. If equal to zero, "
          "then all backbones nodes are also client nodes.");
ABSL_FLAG(int, backbones, 0, "Number of backbone nodes");
ABSL_FLAG(int, demands, 0, "Number of network demands.");
ABSL_FLAG(int, traffic_min, 0, "Min traffic of a demand.");
ABSL_FLAG(int, traffic_max, 0, "Max traffic of a demand.");
ABSL_FLAG(int, min_client_degree, 0,
          "Min number of connections from a client to the backbone.");
ABSL_FLAG(int, max_client_degree, 0,
          "Max number of connections from a client to the backbone.");
ABSL_FLAG(int, min_backbone_degree, 0,
          "Min number of connections from a backbone node to the rest of "
          "the backbone nodes.");
ABSL_FLAG(int, max_backbone_degree, 0,
          "Max number of connections from a backbone node to the rest of "
          "the backbone nodes.");
ABSL_FLAG(int, max_capacity, 0, "Max traffic on any arc.");
ABSL_FLAG(int, fixed_charge_cost, 0, "Fixed charged cost when using an arc.");
ABSL_FLAG(int, seed, 0, "Random seed");

// ----- CP Model -----
ABSL_FLAG(double, comfort_zone, 0.85,
          "Above this limit in 1/1000th, the link is said to be "
          "congestioned.");
ABSL_FLAG(int, extra_hops, 6,
          "When creating all paths for a demand, we look at paths with "
          "maximum length 'shortest path + extra_hops'");
ABSL_FLAG(int, max_paths, 1200, "Max number of possible paths for a demand.");

// ----- Reporting -----
ABSL_FLAG(bool, print_model, false, "Print details of the model.");

// ----- Sat parameters -----
ABSL_FLAG(std::string, params, "", "Sat parameters.");

namespace operations_research {
namespace sat {
// ---------- Data and Data Generation ----------
static const int64_t kDisconnectedDistance = -1LL;

// ----- Data -----
// Contains problem data. It assumes capacities are symmetrical:
//   (capacity(i->j) == capacity(j->i)).
// Demands are not symmetrical.
class NetworkRoutingData {
 public:
  NetworkRoutingData()
      : name_(""), num_nodes_(-1), max_capacity_(-1), fixed_charge_cost_(-1) {}

  // Name of the problem.
  const std::string& name() const { return name_; }

  // Properties of the model.
  int num_nodes() const { return num_nodes_; }

  int num_arcs() const { return all_arcs_.size(); }

  int num_demands() const { return all_demands_.size(); }

  // Returns the capacity of an arc, and 0 if the arc is not defined.
  int Capacity(int node1, int node2) const {
    return gtl::FindWithDefault(
        all_arcs_,
        std::make_pair(std::min(node1, node2), std::max(node1, node2)), 0);
  }

  // Returns the demand between the source and the destination, and 0 if
  // there are no demands between the source and the destination.
  int Demand(int source, int destination) const {
    return gtl::FindWithDefault(all_demands_,
                                std::make_pair(source, destination), 0);
  }

  // External building API.
  void set_num_nodes(int num_nodes) { num_nodes_ = num_nodes; }
  void AddArc(int node1, int node2, int capacity) {
    all_arcs_[std::make_pair(std::min(node1, node2), std::max(node1, node2))] =
        capacity;
  }
  void AddDemand(int source, int destination, int traffic) {
    all_demands_[std::make_pair(source, destination)] = traffic;
  }
  void set_name(absl::string_view name) { name_ = name; }
  void set_max_capacity(int max_capacity) { max_capacity_ = max_capacity; }
  void set_fixed_charge_cost(int cost) { fixed_charge_cost_ = cost; }

 private:
  std::string name_;
  int num_nodes_;
  int max_capacity_;
  int fixed_charge_cost_;
  std::map<std::pair<int, int>, int> all_arcs_;
  std::map<std::pair<int, int>, int> all_demands_;
};

// ----- Data Generation -----

// Random generator of problem. This generator creates a random
// problem. This problem uses a special topology. There are
// 'num_backbones' nodes and 'num_clients' nodes. if 'num_clients' is
// null, then all backbones nodes are also client nodes. All traffic
// originates and terminates in client nodes. Each client node is
// connected to 'min_client_degree' - 'max_client_degree' backbone
// nodes. Each backbone node is connected to 'min_backbone_degree' -
// 'max_backbone_degree' other backbone nodes. There are 'num_demands'
// demands, with a traffic between 'traffic_min' and 'traffic_max'.
// Each arc has a capacity of 'max_capacity'. Using an arc incurs a
// fixed cost of 'fixed_charge_cost'.
class NetworkRoutingDataBuilder {
 public:
  NetworkRoutingDataBuilder(int num_clients, int num_backbones, int num_demands,
                            int traffic_min, int traffic_max,
                            int min_client_degree, int max_client_degree,
                            int min_backbone_degree, int max_backbone_degree,
                            int max_capacity, int fixed_charge_cost)
      : num_clients_(num_clients),
        num_backbones_(num_backbones),
        num_demands_(num_demands),
        traffic_min_(traffic_min),
        traffic_max_(traffic_max),
        min_client_degree_(min_client_degree),
        max_client_degree_(max_client_degree),
        min_backbone_degree_(min_backbone_degree),
        max_backbone_degree_(max_backbone_degree),
        max_capacity_(max_capacity),
        fixed_charge_cost_(fixed_charge_cost),
        rand_gen_(0),
        uniform_backbones_(0, num_backbones_ - 1),
        uniform_clients_(0, num_clients_ - 1),
        uniform_demands_(0, num_demands_ - 1),
        uniform_traffic_(traffic_min, traffic_max),
        uniform_client_degree_(min_client_degree_, max_client_degree_),
        uniform_backbone_degree_(min_backbone_degree_, max_backbone_degree_),
        uniform_source_(num_clients_ == 0 ? 0 : num_backbones_,
                        num_clients_ == 0 ? num_backbones - 1
                                          : num_clients_ + num_backbones_ - 1) {
    CHECK_GE(num_backbones, 1);
    CHECK_GE(num_clients, 0);
    CHECK_GE(num_demands, 1);
    CHECK_LE(num_demands, num_clients == 0 ? num_backbones * num_backbones
                                           : num_clients * num_backbones);
    CHECK_GE(max_client_degree, min_client_degree);
    CHECK_GE(max_backbone_degree, min_backbone_degree);
    CHECK_GE(traffic_max, 1);
    CHECK_GE(traffic_max, traffic_min);
    CHECK_GE(traffic_min, 1);
    CHECK_GE(max_backbone_degree, 2);
    CHECK_GE(max_client_degree, 2);
    CHECK_LE(max_client_degree, num_backbones);
    CHECK_LE(max_backbone_degree, num_backbones);
    CHECK_GE(max_capacity, 1);
  }

  void Build(int seed, NetworkRoutingData* const data) {
    const int size = num_backbones_ + num_clients_;
    InitData(size, seed);
    BuildGraph();
    CreateDemands(data);
    FillData(seed, data);
  }

 private:
  void InitData(int size, int seed) {
    network_.clear();
    network_.resize(size);
    for (int i = 0; i < size; ++i) {
      network_[i].resize(size, false);
    }
    degrees_.clear();
    degrees_.resize(size, 0);
    rand_gen_.seed(seed);
  }

  void BuildGraph() {
    const int size = num_backbones_ + num_clients_;

    // First we create the backbone nodes.
    for (int i = 1; i < num_backbones_; ++i) {
      absl::uniform_int_distribution<int> source(0, i - 1);
      const int j = source(rand_gen_);
      CHECK_LT(j, i);
      AddEdge(i, j);
    }

    absl::btree_set<int> to_complete;
    absl::btree_set<int> not_full;
    for (int i = 0; i < num_backbones_; ++i) {
      if (degrees_[i] < min_backbone_degree_) {
        to_complete.insert(i);
      }
      if (degrees_[i] < max_backbone_degree_) {
        not_full.insert(i);
      }
    }
    while (!to_complete.empty() && not_full.size() > 1) {
      const int node1 = *(to_complete.begin());
      int node2 = node1;
      while (node2 == node1 || degrees_[node2] >= max_backbone_degree_) {
        node2 = uniform_backbones_(rand_gen_);
      }
      AddEdge(node1, node2);
      if (degrees_[node1] >= min_backbone_degree_) {
        to_complete.erase(node1);
      }
      if (degrees_[node2] >= min_backbone_degree_) {
        to_complete.erase(node2);
      }
      if (degrees_[node1] >= max_backbone_degree_) {
        not_full.erase(node1);
      }
      if (degrees_[node2] >= max_backbone_degree_) {
        not_full.erase(node2);
      }
    }

    // Then create the client nodes connected to the backbone nodes.
    // If num_client is 0, then backbone nodes are also client nodes.
    for (int i = num_backbones_; i < size; ++i) {
      const int degree = uniform_client_degree_(rand_gen_);
      while (degrees_[i] < degree) {
        const int j = uniform_backbones_(rand_gen_);
        if (!network_[i][j]) {
          AddEdge(i, j);
        }
      }
    }
  }

  void CreateDemands(NetworkRoutingData* const data) {
    while (data->num_demands() < num_demands_) {
      const int source = uniform_source_(rand_gen_);
      int dest = source;
      while (dest == source) {
        dest = uniform_source_(rand_gen_);
      }
      const int traffic = uniform_traffic_(rand_gen_);
      data->AddDemand(source, dest, traffic);
    }
  }

  void FillData(int seed, NetworkRoutingData* const data) {
    const int size = num_backbones_ + num_clients_;

    const std::string name = absl::StrFormat(
        "mp_c%i_b%i_d%i.t%i-%i.cd%i-%i.bd%i-%i.mc%i.fc%i.s%i", num_clients_,
        num_backbones_, num_demands_, traffic_min_, traffic_max_,
        min_client_degree_, max_client_degree_, min_backbone_degree_,
        max_backbone_degree_, max_capacity_, fixed_charge_cost_, seed);
    data->set_name(name);

    data->set_num_nodes(size);
    int num_arcs = 0;
    for (int i = 0; i < size - 1; ++i) {
      for (int j = i + 1; j < size; ++j) {
        if (network_[i][j]) {
          data->AddArc(i, j, max_capacity_);
          num_arcs++;
        }
      }
    }
    data->set_max_capacity(max_capacity_);
    data->set_fixed_charge_cost(fixed_charge_cost_);
  }

  void AddEdge(int i, int j) {
    degrees_[i]++;
    degrees_[j]++;
    network_[i][j] = true;
    network_[j][i] = true;
  }

  const int num_clients_;
  const int num_backbones_;
  const int num_demands_;
  const int traffic_min_;
  const int traffic_max_;
  const int min_client_degree_;
  const int max_client_degree_;
  const int min_backbone_degree_;
  const int max_backbone_degree_;
  const int max_capacity_;
  const int fixed_charge_cost_;

  std::vector<std::vector<bool>> network_;
  std::vector<int> degrees_;
  std::mt19937 rand_gen_;
  absl::uniform_int_distribution<int> uniform_backbones_;
  absl::uniform_int_distribution<int> uniform_clients_;
  absl::uniform_int_distribution<int> uniform_demands_;
  absl::uniform_int_distribution<int> uniform_traffic_;
  absl::uniform_int_distribution<int> uniform_client_degree_;
  absl::uniform_int_distribution<int> uniform_backbone_degree_;
  absl::uniform_int_distribution<int> uniform_source_;
};

// ---------- Solving the Problem ----------

// Useful data struct to hold demands.
struct Demand {
 public:
  Demand(int the_source, int the_destination, int the_traffic)
      : source(the_source),
        destination(the_destination),
        traffic(the_traffic) {}
  int source;
  int destination;
  int traffic;
};

class NetworkRoutingSolver {
 public:
  typedef absl::flat_hash_set<int> OnePath;

  NetworkRoutingSolver() : num_nodes_(-1) {}

  void ComputeAllPathsForOneDemandAndOnePathLength(int demand_index,
                                                   int max_length,
                                                   int max_paths) {
    // We search for paths of length exactly 'max_length'.
    CpModelBuilder cp_model;
    std::vector<IntVar> arc_vars;
    std::vector<IntVar> node_vars;
    for (int i = 0; i < max_length; ++i) {
      node_vars.push_back(cp_model.NewIntVar(Domain(0, num_nodes_ - 1)));
    }
    for (int i = 0; i < max_length - 1; ++i) {
      arc_vars.push_back(cp_model.NewIntVar(Domain(-1, count_arcs() - 1)));
    }

    for (int i = 0; i < max_length - 1; ++i) {
      std::vector<IntVar> tmp_vars;
      tmp_vars.push_back(node_vars[i]);
      tmp_vars.push_back(node_vars[i + 1]);
      tmp_vars.push_back(arc_vars[i]);
      TableConstraint table = cp_model.AddAllowedAssignments(
          {node_vars[i], node_vars[i + 1], arc_vars[i]});
      for (const auto& tuple : arcs_data_) {
        table.AddTuple(tuple);
      }
    }

    const Demand& demand = demands_array_[demand_index];
    cp_model.AddEquality(node_vars[0], demand.source);
    cp_model.AddEquality(node_vars[max_length - 1], demand.destination);
    cp_model.AddAllDifferent(arc_vars);
    cp_model.AddAllDifferent(node_vars);

    Model model;
    // Create an atomic Boolean that will be periodically checked by the limit.
    std::atomic<bool> stopped(false);
    model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stopped);

    model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
      const int path_id = all_paths_[demand_index].size();
      all_paths_[demand_index].resize(path_id + 1);
      for (int arc_index = 0; arc_index < max_length - 1; ++arc_index) {
        const int arc = SolutionIntegerValue(r, arc_vars[arc_index]);
        all_paths_[demand_index].back().insert(arc);
      }
      if (all_paths_[demand_index].size() >= max_paths) {
        stopped = true;
      }
    }));

    SatParameters parameters;
    parameters.set_enumerate_all_solutions(true);
    model.Add(NewSatParameters(parameters));

    SolveCpModel(cp_model.Build(), &model);
  }

  // This method will fill the all_paths_ data structure. all_paths
  // contains, for each demand, a vector of possible paths, stored as
  // a hash_set of arc indices.
  int ComputeAllPaths(int extra_hops, int max_paths) {
    int num_paths = 0;
    for (int demand_index = 0; demand_index < demands_array_.size();
         ++demand_index) {
      const int min_path_length = all_min_path_lengths_[demand_index];
      for (int max_length = min_path_length + 1;
           max_length <= min_path_length + extra_hops + 1; ++max_length) {
        ComputeAllPathsForOneDemandAndOnePathLength(demand_index, max_length,
                                                    max_paths);
        if (all_paths_[demand_index].size() >= max_paths) {
          break;
        }
      }
      num_paths += all_paths_[demand_index].size();
    }
    return num_paths;
  }

  void AddArcData(int64_t source, int64_t destination, int arc_id) {
    arcs_data_.push_back({source, destination, arc_id});
  }

  void InitArcInfo(const NetworkRoutingData& data) {
    const int num_arcs = data.num_arcs();
    capacity_.clear();
    capacity_.resize(num_nodes_);
    for (int node_index = 0; node_index < num_nodes_; ++node_index) {
      capacity_[node_index].resize(num_nodes_, 0);
    }
    int arc_id = 0;
    for (int i = 0; i < num_nodes_ - 1; ++i) {
      for (int j = i + 1; j < num_nodes_; ++j) {
        const int capacity = data.Capacity(i, j);
        if (capacity > 0) {
          AddArcData(i, j, arc_id);
          AddArcData(j, i, arc_id);
          arc_id++;
          arc_capacity_.push_back(capacity);
          capacity_[i][j] = capacity;
          capacity_[j][i] = capacity;
          if (absl::GetFlag(FLAGS_print_model)) {
            LOG(INFO) << "Arc " << i << " <-> " << j << " with capacity "
                      << capacity;
          }
        }
      }
    }
    CHECK_EQ(arc_id, num_arcs);
  }

  int InitDemandInfo(const NetworkRoutingData& data) {
    const int num_demands = data.num_demands();
    int total_demand = 0;
    for (int i = 0; i < num_nodes_; ++i) {
      for (int j = 0; j < num_nodes_; ++j) {
        const int traffic = data.Demand(i, j);
        if (traffic > 0) {
          demands_array_.push_back(Demand(i, j, traffic));
          total_demand += traffic;
        }
      }
    }
    CHECK_EQ(num_demands, demands_array_.size());
    return total_demand;
  }

  int64_t InitShortestPaths(const NetworkRoutingData& data) {
    const int num_demands = data.num_demands();
    int64_t total_cumulated_traffic = 0;
    all_min_path_lengths_.clear();
    std::vector<int> paths;
    for (int demand_index = 0; demand_index < num_demands; ++demand_index) {
      paths.clear();
      const Demand& demand = demands_array_[demand_index];
      CHECK(DijkstraShortestPath(
          num_nodes_, demand.source, demand.destination,
          [this](int x, int y) { return HasArc(x, y); }, kDisconnectedDistance,
          &paths));
      all_min_path_lengths_.push_back(paths.size() - 1);
    }

    for (int i = 0; i < num_demands; ++i) {
      const int min_path_length = all_min_path_lengths_[i];
      total_cumulated_traffic += min_path_length * demands_array_[i].traffic;
    }
    return total_cumulated_traffic;
  }

  int InitPaths(const NetworkRoutingData& data, int extra_hops, int max_paths) {
    const int num_demands = data.num_demands();
    LOG(INFO) << "Computing all possible paths ";
    LOG(INFO) << "  - extra hops = " << extra_hops;
    LOG(INFO) << "  - max paths per demand = " << max_paths;

    all_paths_.clear();
    all_paths_.resize(num_demands);
    const int num_paths = ComputeAllPaths(extra_hops, max_paths);
    for (int demand_index = 0; demand_index < num_demands; ++demand_index) {
      const Demand& demand = demands_array_[demand_index];
      LOG(INFO) << "Demand from " << demand.source << " to "
                << demand.destination << " with traffic " << demand.traffic
                << ", and " << all_paths_[demand_index].size()
                << " possible paths.";
    }
    return num_paths;
  }

  void Init(const NetworkRoutingData& data, int extra_hops, int max_paths) {
    LOG(INFO) << "Model " << data.name();
    num_nodes_ = data.num_nodes();
    const int num_arcs = data.num_arcs();
    const int num_demands = data.num_demands();

    InitArcInfo(data);
    const int total_demand = InitDemandInfo(data);
    const int64_t total_cumulated_traffic = InitShortestPaths(data);
    const int num_paths = InitPaths(data, extra_hops, max_paths);

    // ----- Report Problem Sizes -----

    LOG(INFO) << "Model created:";
    LOG(INFO) << "  - " << num_nodes_ << " nodes";
    LOG(INFO) << "  - " << num_arcs << " arcs";
    LOG(INFO) << "  - " << num_demands << " demands";
    LOG(INFO) << "  - a total traffic of " << total_demand;
    LOG(INFO) << "  - a minimum cumulated traffic of "
              << total_cumulated_traffic;
    LOG(INFO) << "  - " << num_paths << " possible paths for all demands";
  }

  // ----- Callback for Dijkstra Shortest Path -----

  int64_t HasArc(int i, int j) {
    if (capacity_[i][j] > 0) {
      return 1;
    } else {
      return kDisconnectedDistance;  // disconnected distance.
    }
  }

  // ----- Main Solve routine -----

  int64_t Solve() {
    LOG(INFO) << "Solving model";
    const int num_demands = demands_array_.size();
    const int num_arcs = count_arcs();

    // ----- Build Model -----
    CpModelBuilder cp_model;
    std::vector<std::vector<IntVar>> path_vars(num_demands);

    // Node - Graph Constraint.
    for (int demand_index = 0; demand_index < num_demands; ++demand_index) {
      for (int arc = 0; arc < num_arcs; ++arc) {
        path_vars[demand_index].push_back(IntVar(cp_model.NewBoolVar()));
      }
      // Fill Tuple Set for AllowedAssignment constraint.
      TableConstraint path_ct =
          cp_model.AddAllowedAssignments(path_vars[demand_index]);
      for (const auto& one_path : all_paths_[demand_index]) {
        std::vector<int64_t> tuple(count_arcs(), 0);
        for (const int arc : one_path) {
          tuple[arc] = 1;
        }
        path_ct.AddTuple(tuple);
      }
    }
    // Traffic variables and objective definition.
    std::vector<IntVar> traffic_vars(num_arcs);
    std::vector<IntVar> normalized_traffic_vars(num_arcs);
    std::vector<BoolVar> comfortable_traffic_vars(num_arcs);
    int64_t max_normalized_traffic = 0;
    for (int arc_index = 0; arc_index < num_arcs; ++arc_index) {
      int64_t sum_of_traffic = 0;
      LinearExpr traffic_expr;
      for (int i = 0; i < path_vars.size(); ++i) {
        sum_of_traffic += demands_array_[i].traffic;
        traffic_expr += path_vars[i][arc_index] * demands_array_[i].traffic;
      }
      const IntVar traffic_var = cp_model.NewIntVar(Domain(0, sum_of_traffic));
      traffic_vars[arc_index] = traffic_var;
      cp_model.AddEquality(traffic_expr, traffic_var);

      const int64_t capacity = arc_capacity_[arc_index];
      IntVar scaled_traffic =
          cp_model.NewIntVar(Domain(0, sum_of_traffic * 1000));
      cp_model.AddEquality(traffic_var * 1000, scaled_traffic);
      IntVar normalized_traffic =
          cp_model.NewIntVar(Domain(0, sum_of_traffic * 1000 / capacity));
      max_normalized_traffic =
          std::max(max_normalized_traffic, sum_of_traffic * 1000 / capacity);
      cp_model.AddDivisionEquality(normalized_traffic, scaled_traffic,
                                   capacity);
      normalized_traffic_vars[arc_index] = normalized_traffic;
      const BoolVar comfort = cp_model.NewBoolVar();
      const int64_t safe_capacity =
          static_cast<int64_t>(capacity * absl::GetFlag(FLAGS_comfort_zone));
      cp_model.AddGreaterThan(traffic_var, safe_capacity)
          .OnlyEnforceIf(comfort);
      cp_model.AddLessOrEqual(traffic_var, safe_capacity)
          .OnlyEnforceIf(Not(comfort));
      comfortable_traffic_vars[arc_index] = comfort;
    }

    const IntVar max_usage_cost =
        cp_model.NewIntVar(Domain(0, max_normalized_traffic));
    cp_model.AddMaxEquality(max_usage_cost, normalized_traffic_vars);

    cp_model.Minimize(LinearExpr::Sum(comfortable_traffic_vars) +
                      max_usage_cost);

    Model model;
    if (!absl::GetFlag(FLAGS_params).empty()) {
      model.Add(NewSatParameters(absl::GetFlag(FLAGS_params)));
    }
    int num_solutions = 0;
    model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
      LOG(INFO) << "Solution " << num_solutions;
      const double percent = SolutionIntegerValue(r, max_usage_cost) / 10.0;
      int num_non_comfortable_arcs = 0;
      for (const BoolVar comfort : comfortable_traffic_vars) {
        num_non_comfortable_arcs += SolutionBooleanValue(r, comfort);
      }
      if (num_non_comfortable_arcs > 0) {
        LOG(INFO) << "*** Found a solution with a max usage of " << percent
                  << "%, and " << num_non_comfortable_arcs
                  << " links above the comfort zone";
      } else {
        LOG(INFO) << "*** Found a solution with a max usage of " << percent
                  << "%";
      }
      num_solutions++;
    }));
    const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
    return response.objective_value();
  }

 private:
  int count_arcs() const { return arcs_data_.size() / 2; }

  std::vector<std::vector<int64_t>> arcs_data_;
  std::vector<int> arc_capacity_;
  std::vector<Demand> demands_array_;
  int num_nodes_;
  std::vector<int64_t> all_min_path_lengths_;
  std::vector<std::vector<int>> capacity_;
  std::vector<std::vector<OnePath>> all_paths_;
};

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);

  operations_research::sat::NetworkRoutingData data;
  operations_research::sat::NetworkRoutingDataBuilder builder(
      absl::GetFlag(FLAGS_clients), absl::GetFlag(FLAGS_backbones),
      absl::GetFlag(FLAGS_demands), absl::GetFlag(FLAGS_traffic_min),
      absl::GetFlag(FLAGS_traffic_max), absl::GetFlag(FLAGS_min_client_degree),
      absl::GetFlag(FLAGS_max_client_degree),
      absl::GetFlag(FLAGS_min_backbone_degree),
      absl::GetFlag(FLAGS_max_backbone_degree),
      absl::GetFlag(FLAGS_max_capacity),
      absl::GetFlag(FLAGS_fixed_charge_cost));
  builder.Build(absl::GetFlag(FLAGS_seed), &data);
  operations_research::sat::NetworkRoutingSolver solver;
  solver.Init(data, absl::GetFlag(FLAGS_extra_hops),
              absl::GetFlag(FLAGS_max_paths));
  LOG(INFO) << "Final cost = " << solver.Solve();
  return EXIT_SUCCESS;
}
