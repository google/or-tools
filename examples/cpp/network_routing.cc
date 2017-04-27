// Copyright 2010-2014 Google
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

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/map_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/graph/shortestpaths.h"
#include "ortools/util/tuple_set.h"
#include "ortools/base/random.h"

// ----- Data Generator -----
DEFINE_int32(clients, 0,
             "Number of network clients nodes. If equal to zero, "
             "then all backbones nodes are also client nodes.");
DEFINE_int32(backbones, 0, "Number of backbone nodes");
DEFINE_int32(demands, 0, "Number of network demands.");
DEFINE_int32(traffic_min, 0, "Min traffic of a demand.");
DEFINE_int32(traffic_max, 0, "Max traffic of a demand.");
DEFINE_int32(min_client_degree, 0,
             "Min number of connections from a client to the backbone.");
DEFINE_int32(max_client_degree, 0,
             "Max number of connections from a client to the backbone.");
DEFINE_int32(min_backbone_degree, 0,
             "Min number of connections from a backbone node to the rest of "
             "the backbone nodes.");
DEFINE_int32(max_backbone_degree, 0,
             "Max number of connections from a backbone node to the rest of "
             "the backbone nodes.");
DEFINE_int32(max_capacity, 0, "Max traffic on any arc.");
DEFINE_int32(fixed_charge_cost, 0, "Fixed charged cost when using an arc.");
DEFINE_int32(seed, 0, "Random seed");

// ----- Reporting -----
DEFINE_bool(print_model, false, "Print model.");
DEFINE_int32(report, 1,
             "Report which links and which demands are "
             "responsible for the congestion.");
DEFINE_int32(log_period, 100000, "Period for the search log");

// ----- CP Model -----
DEFINE_int64(comfort_zone, 850,
             "Above this limit in 1/1000th, the link is said to be "
             "congestioned.");
DEFINE_int32(extra_hops, 6,
             "When creating all paths for a demand, we look at paths with "
             "maximum length 'shortest path + extra_hops'");
DEFINE_int32(max_paths, 1200, "Max number of possible paths for a demand.");

// ----- CP LNS -----
DEFINE_int32(time_limit, 60000,
             "Time limit for search in ms, 0 = no time limit.");
DEFINE_int32(fail_limit, 0, "Failure limit for search, 0 = no limit.");
DEFINE_int32(lns_size, 6, "Number of vars to relax in a lns loop.");
DEFINE_int32(lns_seed, 1, "Seed for the LNS random number generator.");
DEFINE_int32(lns_limit, 30, "Limit the number of failures of the lns loop.");
DEFINE_bool(focus_lns, true, "Focus LNS on highest cost arcs.");

namespace operations_research {
// ---------- Data and Data Generation ----------
static const int64 kDisconnectedDistance = -1LL;

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
    return FindWithDefault(
        all_arcs_,
        std::make_pair(std::min(node1, node2), std::max(node1, node2)), 0);
  }

  // Returns the demand between the source and the destination, and 0 if
  // there are no demands between the source and the destination.
  int Demand(int source, int destination) const {
    return FindWithDefault(all_demands_, std::make_pair(source, destination),
                           0);
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
  void set_name(const std::string& name) { name_ = name; }
  void set_max_capacity(int max_capacity) { max_capacity_ = max_capacity; }
  void set_fixed_charge_cost(int cost) { fixed_charge_cost_ = cost; }

 private:
  std::string name_;
  int num_nodes_;
  int max_capacity_;
  int fixed_charge_cost_;
  std::unordered_map<std::pair<int, int>, int> all_arcs_;
  std::unordered_map<std::pair<int, int>, int> all_demands_;
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
  NetworkRoutingDataBuilder() : random_(0) {}

  void BuildModelFromParameters(int num_clients, int num_backbones,
                                int num_demands, int traffic_min,
                                int traffic_max, int min_client_degree,
                                int max_client_degree, int min_backbone_degree,
                                int max_backbone_degree, int max_capacity,
                                int fixed_charge_cost, int seed,
                                NetworkRoutingData* const data) {
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

    const int size = num_backbones + num_clients;
    InitData(size, seed);
    BuildGraph(num_clients, num_backbones, min_client_degree, max_client_degree,
               min_backbone_degree, max_backbone_degree);
    CreateDemands(num_clients, num_backbones, num_demands, traffic_min,
                  traffic_max, data);
    FillData(num_clients, num_backbones, num_demands, traffic_min, traffic_max,
             min_client_degree, max_client_degree, min_backbone_degree,
             max_backbone_degree, max_capacity, fixed_charge_cost, seed, data);
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
    random_.Reset(seed);
  }

  void BuildGraph(int num_clients, int num_backbones, int min_client_degree,
                  int max_client_degree, int min_backbone_degree,
                  int max_backbone_degree) {
    const int size = num_backbones + num_clients;

    // First we create the backbone nodes.
    for (int i = 1; i < num_backbones; ++i) {
      int j = random_.Uniform(i);
      CHECK_LT(j, i);
      AddEdge(i, j);
    }

    std::unordered_set<int> to_complete;
    std::unordered_set<int> not_full;
    for (int i = 0; i < num_backbones; ++i) {
      if (degrees_[i] < min_backbone_degree) {
        to_complete.insert(i);
      }
      if (degrees_[i] < max_backbone_degree) {
        not_full.insert(i);
      }
    }
    while (!to_complete.empty() && not_full.size() > 1) {
      const int node1 = *(to_complete.begin());
      int node2 = node1;
      while (node2 == node1 || degrees_[node2] >= max_backbone_degree) {
        node2 = random_.Uniform(num_backbones);
      }
      AddEdge(node1, node2);
      if (degrees_[node1] >= min_backbone_degree) {
        to_complete.erase(node1);
      }
      if (degrees_[node2] >= min_backbone_degree) {
        to_complete.erase(node2);
      }
      if (degrees_[node1] >= max_backbone_degree) {
        not_full.erase(node1);
      }
      if (degrees_[node2] >= max_backbone_degree) {
        not_full.erase(node2);
      }
    }

    // Then create the client nodes connected to the backbone nodes.
    // If num_client is 0, then backbone nodes are also client nodes.
    for (int i = num_backbones; i < size; ++i) {
      const int degree = RandomInInterval(min_client_degree, max_client_degree);
      while (degrees_[i] < degree) {
        const int j = random_.Uniform(num_backbones);
        if (!network_[i][j]) {
          AddEdge(i, j);
        }
      }
    }
  }

  void CreateDemands(int num_clients, int num_backbones, int num_demands,
                     int traffic_min, int traffic_max,
                     NetworkRoutingData* const data) {
    while (data->num_demands() < num_demands) {
      const int source = RandomClient(num_clients, num_backbones);
      int dest = source;
      while (dest == source) {
        dest = RandomClient(num_clients, num_backbones);
      }
      const int traffic = RandomInInterval(traffic_min, traffic_max);
      data->AddDemand(source, dest, traffic);
    }
  }

  void FillData(int num_clients, int num_backbones, int num_demands,
                int traffic_min, int traffic_max, int min_client_degree,
                int max_client_degree, int min_backbone_degree,
                int max_backbone_degree, int max_capacity,
                int fixed_charge_cost, int seed,
                NetworkRoutingData* const data) {
    const int size = num_backbones + num_clients;

    const std::string name = StringPrintf(
        "mp_c%i_b%i_d%i.t%i-%i.cd%i-%i.bd%i-%i.mc%i.fc%i.s%i", num_clients,
        num_backbones, num_demands, traffic_min, traffic_max, min_client_degree,
        max_client_degree, min_backbone_degree, max_backbone_degree,
        max_capacity, fixed_charge_cost, seed);
    data->set_name(name);

    data->set_num_nodes(size);
    int num_arcs = 0;
    for (int i = 0; i < size - 1; ++i) {
      for (int j = i + 1; j < size; ++j) {
        if (network_[i][j]) {
          data->AddArc(i, j, max_capacity);
          num_arcs++;
        }
      }
    }
    data->set_max_capacity(max_capacity);
    data->set_fixed_charge_cost(fixed_charge_cost);
  }

  void AddEdge(int i, int j) {
    degrees_[i]++;
    degrees_[j]++;
    network_[i][j] = true;
    network_[j][i] = true;
  }

  int RandomInInterval(int interval_min, int interval_max) {
    CHECK_LE(interval_min, interval_max);
    return random_.Uniform(interval_max - interval_min + 1) + interval_min;
  }

  int RandomClient(int num_clients, int num_backbones) {
    return (num_clients == 0) ? random_.Uniform(num_backbones)
                              : random_.Uniform(num_clients) + num_backbones;
  }

  std::vector<std::vector<bool> > network_;
  std::vector<int> degrees_;
  ACMRandom random_;
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
  typedef std::unordered_set<int> OnePath;

  NetworkRoutingSolver() : arcs_data_(3), num_nodes_(-1) {}

  void ComputeAllPathsForOneDemandAndOnePathLength(int demand_index,
                                                   int max_length,
                                                   int max_paths) {
    // We search for paths of length exactly 'max_length'.
    Solver solver("Counting");
    std::vector<IntVar*> arc_vars;
    std::vector<IntVar*> node_vars;
    solver.MakeIntVarArray(max_length, 0, num_nodes_ - 1, &node_vars);
    solver.MakeIntVarArray(max_length - 1, -1, count_arcs() - 1, &arc_vars);

    for (int i = 0; i < max_length - 1; ++i) {
      std::vector<IntVar*> tmp_vars;
      tmp_vars.push_back(node_vars[i]);
      tmp_vars.push_back(node_vars[i + 1]);
      tmp_vars.push_back(arc_vars[i]);
      solver.AddConstraint(solver.MakeAllowedAssignments(tmp_vars, arcs_data_));
    }

    const Demand& demand = demands_array_[demand_index];
    solver.AddConstraint(solver.MakeEquality(node_vars[0], demand.source));
    solver.AddConstraint(
        solver.MakeEquality(node_vars[max_length - 1], demand.destination));
    solver.AddConstraint(solver.MakeAllDifferent(arc_vars));
    solver.AddConstraint(solver.MakeAllDifferent(node_vars));
    DecisionBuilder* const db = solver.MakePhase(
        node_vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
    solver.NewSearch(db);
    while (solver.NextSolution()) {
      const int path_id = all_paths_[demand_index].size();
      all_paths_[demand_index].resize(path_id + 1);
      for (int arc_index = 0; arc_index < max_length - 1; ++arc_index) {
        const int arc = arc_vars[arc_index]->Value();
        all_paths_[demand_index].back().insert(arc);
      }
      if (all_paths_[demand_index].size() > max_paths) {
        break;
      }
    }
    solver.EndSearch();
  }

  // This method will fill the all_paths_ data structure. all_paths
  // contains, for each demand, a vector of possible paths, stored as
  // a std::unordered_set of arc indices.
  int ComputeAllPaths(int extra_hops, int max_paths) {
    int num_paths = 0;
    for (int demand_index = 0; demand_index < demands_array_.size();
         ++demand_index) {
      const int min_path_length = all_min_path_lengths_[demand_index];
      for (int max_length = min_path_length + 1;
           max_length <= min_path_length + extra_hops + 1; ++max_length) {
        ComputeAllPathsForOneDemandAndOnePathLength(demand_index, max_length,
                                                    max_paths);
        if (all_paths_[demand_index].size() > max_paths) {
          break;
        }
      }
      num_paths += all_paths_[demand_index].size();
    }
    return num_paths;
  }

  void AddArcData(int index, int source, int destination, int arc_id) {
    arcs_data_.Insert3(source, destination, arc_id);
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
          AddArcData(2 * arc_id, i, j, arc_id);
          AddArcData(2 * arc_id + 1, j, i, arc_id);
          arc_id++;
          arc_capacity_.push_back(capacity);
          capacity_[i][j] = capacity;
          capacity_[j][i] = capacity;
          if (FLAGS_print_model) {
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

  int64 InitShortestPaths(const NetworkRoutingData& data) {
    const int num_demands = data.num_demands();
    int64 total_cumulated_traffic = 0;
    all_min_path_lengths_.clear();
    std::vector<int> paths;
    for (int demand_index = 0; demand_index < num_demands; ++demand_index) {
      paths.clear();
      const Demand& demand = demands_array_[demand_index];
      CHECK(DijkstraShortestPath(num_nodes_, demand.source, demand.destination,
                                 [this](int x, int y) { return HasArc(x, y); },
                                 kDisconnectedDistance, &paths));
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

    if (FLAGS_print_model) {
      for (int demand_index = 0; demand_index < num_demands; ++demand_index) {
        const Demand& demand = demands_array_[demand_index];
        LOG(INFO) << "Demand from " << demand.source << " to "
                  << demand.destination << " with traffic " << demand.traffic
                  << ", and " << all_paths_[demand_index].size()
                  << " possible paths.";
      }
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
    const int64 total_cumulated_traffic = InitShortestPaths(data);
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

  // Build the AllowedAssignment constraint with one tuple per path
  // for a given demand.
  void BuildNodePathConstraint(Solver* const solver,
                               const std::vector<IntVar*>& path_vars,
                               int demand_index,
                               std::vector<IntVar*>* decision_vars) {
    // Fill Tuple Set for AllowedAssignment constraint.
    const std::vector<OnePath> paths = all_paths_[demand_index];
    IntTupleSet tuple_set(count_arcs() + 1);
    for (int path_id = 0; path_id < paths.size(); ++path_id) {
      std::vector<int> tuple(count_arcs() + 1);
      tuple[0] = path_id;
      for (const int arc : paths[path_id]) {
        // + 1 because tuple_set.back()[0] contains path_id.
        tuple[arc + 1] = true;
      }
      tuple_set.Insert(tuple);
    }

    const std::string name = StringPrintf("PathDecision_%i", demand_index);
    IntVar* const var = solver->MakeIntVar(0, tuple_set.NumTuples() - 1, name);
    std::vector<IntVar*> tmp_vars;
    tmp_vars.push_back(var);
    for (int i = 0; i < count_arcs(); ++i) {
      tmp_vars.push_back(path_vars[i]);
    }
    solver->AddConstraint(solver->MakeAllowedAssignments(tmp_vars, tuple_set));
    decision_vars->push_back(var);
  }

  // Build traffic variable summing all traffic from all demands
  // going through a single arc.
  void BuildTrafficVariable(Solver* const solver, int arc_index,
                            const std::vector<std::vector<IntVar*> >& path_vars,
                            IntVar** const traffic) {
    std::vector<IntVar*> terms;
    for (int i = 0; i < path_vars.size(); ++i) {
      terms.push_back(
          solver->MakeProd(path_vars[i][arc_index], demands_array_[i].traffic)
              ->Var());
    }
    *traffic = solver->MakeSum(terms)->Var();
  }

  // ----- Implement 'clever' Large Neighborhood Search -----

  class PathBasedLns : public BaseLns {
   public:
    PathBasedLns(const std::vector<IntVar*>& vars, int fragment_size,
                 const std::vector<std::vector<OnePath> >& all_paths,
                 int num_arcs, const std::vector<int64>& actual_usage_costs)
        : BaseLns(vars),
          rand_(FLAGS_lns_seed),
          fragment_size_(fragment_size),
          all_paths_(all_paths),
          num_arcs_(num_arcs),
          actual_usage_costs_(actual_usage_costs) {
      CHECK_GT(fragment_size_, 0);
    }

    ~PathBasedLns() override {}

    void InitFragments() override {
      // We factorize computations that need to be updated only when
      // we have a new solution and not at each fragment.
      arc_wrappers_.clear();
      for (int i = 0; i < actual_usage_costs_.size(); ++i) {
        const int64 cost = actual_usage_costs_[i];
        if (cost != 0) {
          arc_wrappers_.push_back(ArcWrapper(i, cost));
        }
      }
      if (arc_wrappers_.size() > fragment_size_) {
        std::stable_sort(arc_wrappers_.begin(), arc_wrappers_.end());
      }
    }

    bool NextFragment() override {
      // First we select a set of arcs to release.
      std::unordered_set<int> arcs_to_release;
      if (arc_wrappers_.size() <= fragment_size_) {
        // There are not enough used arcs, we will release all of them.
        for (int index = 0; index < arc_wrappers_.size(); ++index) {
          arcs_to_release.insert(arc_wrappers_[index].arc_id);
        }
      } else {
        if (FLAGS_focus_lns) {
          // We select 'fragment_size / 2' most costly arcs.
          for (int index = 0; index < fragment_size_ / 2; ++index) {
            arcs_to_release.insert(arc_wrappers_[index].arc_id);
          }
        }

        // We fill 'arcs_to_release' until we have chosen 'fragment_size_' arcs
        // to release.
        while (arcs_to_release.size() < fragment_size_) {
          const int candidate = rand_.Uniform(arc_wrappers_.size());
          arcs_to_release.insert(arc_wrappers_[candidate].arc_id);
        }
      }

      // We actually free all paths going through any of the selected arcs.
      const int demands = all_paths_.size();
      for (int i = 0; i < demands; ++i) {
        const OnePath& path = all_paths_[i][Value(i)];
        for (const int arc : arcs_to_release) {
          if (ContainsKey(path, arc)) {
            AppendToFragment(i);
            break;
          }
        }
      }
      return true;
    }

   private:
    struct ArcWrapper {
     public:
      ArcWrapper(int i, int64 c) : arc_id(i), cost(c) {}
      int arc_id;
      int64 cost;
      bool operator<(const ArcWrapper& other_arc_wrapper) const {
        return cost > other_arc_wrapper.cost ||
               (cost == other_arc_wrapper.cost &&
                arc_id < other_arc_wrapper.arc_id);
      }
    };

    ACMRandom rand_;
    const int fragment_size_;
    const std::vector<std::vector<OnePath> >& all_paths_;
    const int num_arcs_;
    const std::vector<int64>& actual_usage_costs_;
    std::vector<ArcWrapper> arc_wrappers_;
  };

  // ----- Evaluator for the Decision Builder -----

  static const int kOneThousand = 1000;

  int64 EvaluateMarginalCost(const std::vector<IntVar*>& path_costs, int64 var,
                             int64 val) {
    int64 best_cost = 0;
    const int64 traffic = demands_array_[var].traffic;
    const OnePath& path = all_paths_[var][val];
    for (const int arc : path) {
      const int64 current_percent = path_costs[arc]->Min();
      const int64 current_capacity = arc_capacity_[arc];
      const int64 expected_percent =
          current_percent + traffic * kOneThousand / current_capacity;
      if (expected_percent > best_cost) {
        best_cost = expected_percent;
      }
    }
    return best_cost;
  }

  // ----- Limit the Maximum Number of Discrepancies in the Sub-Search -----

  static Solver::DecisionModification MaxDiscrepancy1(Solver* const solver) {
    if (solver->SearchDepth() - solver->SearchLeftDepth() > 1) {
      return Solver::KEEP_LEFT;
    }
    return Solver::NO_CHANGE;
  }

  class ApplyMaxDiscrepancy : public DecisionBuilder {
   public:
    ~ApplyMaxDiscrepancy() override {}

    Decision* Next(Solver* const solver) override {
      solver->SetBranchSelector([solver]() { return MaxDiscrepancy1(solver); });
      return nullptr;
    }

    std::string DebugString() const override { return "ApplyMaxDiscrepancy"; }
  };

  // ----- Auxilliary Decision Builder to Store the Cost of a Solution -----

  class StoreUsageCosts : public DecisionBuilder {
   public:
    StoreUsageCosts(const std::vector<IntVar*>& vars,
                    std::vector<int64>* values)
        : vars_(vars), values_(values) {}
    ~StoreUsageCosts() override {}

    Decision* Next(Solver* const s) override {
      for (int i = 0; i < vars_.size(); ++i) {
        (*values_)[i] = vars_[i]->Value();
      }
      return nullptr;
    }

   private:
    const std::vector<IntVar*>& vars_;
    std::vector<int64>* const values_;
  };

  // ----- Callback for Dijkstra Shortest Path -----

  int64 HasArc(int i, int j) {
    if (capacity_[i][j] > 0) {
      return 1;
    } else {
      return kDisconnectedDistance;  // disconnected distance.
    }
  }

  // ----- Main Solve routine -----

  int64 LnsSolve(int time_limit, int fail_limit) {
    LOG(INFO) << "Solving model";
    const int num_demands = demands_array_.size();
    const int num_arcs = count_arcs();
    // ----- Build Model -----
    Solver solver("MultiPathSolver");
    std::vector<std::vector<IntVar*> > path_vars(num_demands);
    std::vector<IntVar*> decision_vars;

    // Node - Graph Constraint.
    for (int demand_index = 0; demand_index < num_demands; ++demand_index) {
      solver.MakeBoolVarArray(num_arcs,
                              StringPrintf("path_vars_%i_", demand_index),
                              &path_vars[demand_index]);
      BuildNodePathConstraint(&solver, path_vars[demand_index], demand_index,
                              &decision_vars);
    }
    // Traffic variables.
    std::vector<IntVar*> vtraffic(num_arcs);
    for (int arc_index = 0; arc_index < num_arcs; ++arc_index) {
      BuildTrafficVariable(&solver, arc_index, path_vars, &vtraffic[arc_index]);
      vtraffic[arc_index]->set_name(StringPrintf("traffic_%i", arc_index));
    }

    // Objective Function.
    std::vector<IntVar*> costs;
    std::vector<IntVar*> usage_costs;
    std::vector<IntVar*> comfort_costs;
    for (int arc_index = 0; arc_index < num_arcs; ++arc_index) {
      const int capacity = capacity_[arcs_data_.Value(
          2 * arc_index, 0)][arcs_data_.Value(2 * arc_index, 1)];
      IntVar* const usage_cost =
          solver.MakeDiv(solver.MakeProd(vtraffic[arc_index], kOneThousand),
                         capacity)
              ->Var();
      usage_costs.push_back(usage_cost);
      IntVar* const comfort_cost = solver.MakeIsGreaterCstVar(
          vtraffic[arc_index], capacity * FLAGS_comfort_zone / kOneThousand);
      comfort_costs.push_back(comfort_cost);
    }
    IntVar* const max_usage_cost = solver.MakeMax(usage_costs)->Var();
    IntVar* const sum_comfort_cost = solver.MakeSum(comfort_costs)->Var();
    IntVar* const objective_var =
        solver.MakeSum(max_usage_cost, sum_comfort_cost)->Var();
    std::vector<SearchMonitor*> monitors;
    OptimizeVar* const objective = solver.MakeMinimize(objective_var, 1);
    monitors.push_back(objective);

    // Search Log.
    if (FLAGS_report == 0) {
      SearchMonitor* const search_log =
          solver.MakeSearchLog(FLAGS_log_period, objective);
      monitors.push_back(search_log);
    }

    // DecisionBuilder.
    Solver::IndexEvaluator2 eval_marginal_cost = [this, &usage_costs](
        int64 var, int64 value) {
      return EvaluateMarginalCost(usage_costs, var, value);
    };

    DecisionBuilder* const db = solver.MakePhase(
        decision_vars, Solver::CHOOSE_RANDOM, eval_marginal_cost);

    // Limits.
    if (time_limit != 0 || fail_limit != 0) {
      if (time_limit != 0) {
        LOG(INFO) << "adding time limit of " << time_limit << " ms";
      }
      if (fail_limit != 0) {
        LOG(INFO) << "adding fail limit of " << fail_limit;
      }
      monitors.push_back(solver.MakeLimit(
          time_limit != 0 ? time_limit : kint64max, kint64max,
          fail_limit != 0 ? fail_limit : kint64max, kint64max));
    }

    // Lns Decision Builder.
    LOG(INFO) << "Using Lns with a fragment size of " << FLAGS_lns_size
              << ", and fail limit of " << FLAGS_lns_limit;
    std::vector<int64> actual_usage_costs(num_arcs);
    DecisionBuilder* const store_info =
        solver.RevAlloc(new StoreUsageCosts(usage_costs, &actual_usage_costs));

    LocalSearchOperator* const local_search_operator = solver.RevAlloc(
        new PathBasedLns(decision_vars, FLAGS_lns_size, all_paths_, num_arcs,
                         actual_usage_costs));
    SearchLimit* const lns_limit =
        solver.MakeLimit(kint64max, kint64max, FLAGS_lns_limit, kint64max);
    DecisionBuilder* const inner_db = solver.MakePhase(
        decision_vars, Solver::CHOOSE_RANDOM, eval_marginal_cost);

    DecisionBuilder* const apply = solver.RevAlloc(new ApplyMaxDiscrepancy);
    DecisionBuilder* const max_discrepency_db = solver.Compose(apply, inner_db);
    DecisionBuilder* const ls_db =
        solver.MakeSolveOnce(max_discrepency_db, lns_limit);
    LocalSearchPhaseParameters* const parameters =
        solver.MakeLocalSearchPhaseParameters(
            local_search_operator, solver.Compose(ls_db, store_info));
    DecisionBuilder* const final_db = solver.Compose(
        solver.MakeLocalSearchPhase(decision_vars, db, parameters), store_info);

    // And Now Solve.
    int64 best_cost = kint64max;
    solver.NewSearch(final_db, monitors);
    while (solver.NextSolution()) {
      // Solution Found: Report it.
      const double percent = max_usage_cost->Value() / 10.0;
      const int64 non_comfort = sum_comfort_cost->Value();
      if (non_comfort > 0) {
        LOG(INFO) << "*** Found a solution with a max usage of " << percent
                  << "%, and " << non_comfort
                  << " links above the comfort zone";
      } else {
        LOG(INFO) << "*** Found a solution with a max usage of " << percent
                  << "%";
      }
      best_cost = objective_var->Value();
      if (FLAGS_report > 1) {
        DisplaySolution(num_arcs, max_usage_cost->Value(), usage_costs,
                        path_vars, FLAGS_report > 2, FLAGS_comfort_zone);
      }
    }
    solver.EndSearch();

    return best_cost;
  }
  void DisplaySolution(int num_arcs, int64 max_usage_cost,
                       const std::vector<IntVar*>& usage_costs,
                       const std::vector<std::vector<IntVar*> >& path_vars,
                       bool precise, int64 comfort_zone) {
    // We will show paths above the comfort zone, or above the max
    // utilization minus 5%.
    const int64 kFivePercentInThousandth = 50;
    const int64 cutoff =
        std::min(max_usage_cost - kFivePercentInThousandth, comfort_zone);
    for (int i = 0; i < num_arcs; ++i) {
      const int64 arc_usage = usage_costs[i]->Value();
      if (arc_usage >= cutoff) {
        const int source_index = arcs_data_.Value(2 * i, 0);
        const int destination_index = arcs_data_.Value(2 * i, 1);
        LOG(INFO) << " + Arc " << source_index << " <-> " << destination_index
                  << " has a usage = " << arc_usage / 10.0 << "%, capacity = "
                  << capacity_[source_index][destination_index];
        if (precise) {
          const int num_demands = demands_array_.size();
          for (int demand_index = 0; demand_index < num_demands;
               ++demand_index) {
            if (path_vars[demand_index][i]->Value() == 1) {
              const Demand& demand = demands_array_[demand_index];
              LOG(INFO) << "   - "
                        << StringPrintf("%i -> %i (%i)", demand.source,
                                        demand.destination, demand.traffic);
            }
          }
        }
      }
    }
  }

 private:
  int count_arcs() const { return arcs_data_.NumTuples() / 2; }

  IntTupleSet arcs_data_;
  std::vector<int> arc_capacity_;
  std::vector<Demand> demands_array_;
  int num_nodes_;
  std::vector<int64> all_min_path_lengths_;
  std::vector<std::vector<int> > capacity_;
  std::vector<std::vector<OnePath> > all_paths_;
};

}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::NetworkRoutingData data;
  operations_research::NetworkRoutingDataBuilder builder;
  builder.BuildModelFromParameters(
      FLAGS_clients, FLAGS_backbones, FLAGS_demands, FLAGS_traffic_min,
      FLAGS_traffic_max, FLAGS_min_client_degree, FLAGS_max_client_degree,
      FLAGS_min_backbone_degree, FLAGS_max_backbone_degree, FLAGS_max_capacity,
      FLAGS_fixed_charge_cost, FLAGS_seed, &data);
  operations_research::NetworkRoutingSolver solver;
  solver.Init(data, FLAGS_extra_hops, FLAGS_max_paths);
  LOG(INFO) << "Final cost = "
            << solver.LnsSolve(FLAGS_time_limit, FLAGS_fail_limit);
  return 0;
}
