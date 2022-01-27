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

#include "ortools/packing/arc_flow_solver.h"

#include <string>

#include "absl/container/btree_map.h"
#include "absl/flags/flag.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/timer.h"
#include "ortools/packing/arc_flow_builder.h"
#include "ortools/packing/vector_bin_packing.pb.h"

ABSL_FLAG(std::string, arc_flow_dump_model, "",
          "File to store the solver specific optimization proto.");

namespace operations_research {
namespace packing {

namespace {
double ConvertVectorBinPackingProblem(const vbp::VectorBinPackingProblem& input,
                                      ArcFlowGraph* graph) {
  WallTimer timer;
  timer.Start();
  const int num_items = input.item_size();
  const int num_dims = input.resource_capacity_size();

  // Collect problem data.
  std::vector<std::vector<int>> shapes(num_items);
  std::vector<int> demands(num_items);
  std::vector<int> capacities(num_dims);
  for (int i = 0; i < num_items; ++i) {
    shapes[i].assign(input.item(i).resource_usage().begin(),
                     input.item(i).resource_usage().end());
    demands[i] = input.item(i).num_copies();
  }
  for (int i = 0; i < num_dims; ++i) {
    capacities[i] = input.resource_capacity(i);
  }

  // Add extra dimensions to encode max_number_of_copies_per_bin.
  for (int i = 0; i < num_items; ++i) {
    const int max_copies = input.item(i).max_number_of_copies_per_bin();
    if (max_copies == 0 || max_copies >= demands[i]) continue;
    capacities.push_back(max_copies);
    for (int j = 0; j < num_items; ++j) {
      shapes[j].push_back(i == j);
    }
  }

  *graph = BuildArcFlowGraph(capacities, shapes, demands);
  const double arc_flow_time = timer.Get();

  VLOG(1) << "The arc-flow grah has " << graph->nodes.size() << " nodes, and "
          << graph->arcs.size() << " arcs. It was created by exploring "
          << graph->num_dp_states
          << " states in the dynamic programming phase in " << arc_flow_time
          << " s";
  return arc_flow_time;
}
}  // namespace

vbp::VectorBinPackingSolution SolveVectorBinPackingWithArcFlow(
    const vbp::VectorBinPackingProblem& problem,
    MPSolver::OptimizationProblemType solver_type,
    const std::string& mip_params, double time_limit, int num_threads,
    int max_bins) {
  ArcFlowGraph graph;
  const double arc_flow_time = ConvertVectorBinPackingProblem(problem, &graph);

  int max_num_bins = 0;
  if (max_bins > 0) {
    max_num_bins = max_bins;
  } else {
    for (const auto& item : problem.item()) {
      max_num_bins += item.num_copies();
    }
  }
  const int num_types = problem.item_size();
  std::vector<std::vector<MPVariable*>> incoming_vars(graph.nodes.size());
  std::vector<std::vector<MPVariable*>> outgoing_vars(graph.nodes.size());
  std::vector<MPVariable*> arc_to_var(graph.arcs.size());
  std::vector<std::vector<MPVariable*>> item_to_vars(num_types);

  MPSolver solver("VectorBinPacking", solver_type);
  CHECK_OK(solver.SetNumThreads(num_threads));

  for (int v = 0; v < graph.arcs.size(); ++v) {
    const ArcFlowGraph::Arc& arc = graph.arcs[v];
    MPVariable* const var =
        solver.MakeIntVar(0, max_num_bins, absl::StrCat("a", v));
    incoming_vars[arc.destination].push_back(var);
    outgoing_vars[arc.source].push_back(var);
    if (arc.item_index != -1) {
      item_to_vars[arc.item_index].push_back(var);
    }
    arc_to_var[v] = var;
  }

  // Per item demand constraint.
  for (int i = 0; i < num_types; ++i) {
    MPConstraint* const ct = solver.MakeRowConstraint(
        problem.item(i).num_copies(), problem.item(i).num_copies());
    for (MPVariable* const var : item_to_vars[i]) {
      ct->SetCoefficient(var, 1.0);
    }
  }

  // Flow conservation.
  for (int n = 1; n < graph.nodes.size() - 1; ++n) {  // Ignore source and sink.
    MPConstraint* const ct = solver.MakeRowConstraint(0.0, 0.0);
    for (MPVariable* const var : incoming_vars[n]) {
      ct->SetCoefficient(var, 1.0);
    }
    for (MPVariable* const var : outgoing_vars[n]) {
      ct->SetCoefficient(var, -1.0);
    }
  }

  MPVariable* const obj_var = solver.MakeIntVar(0, max_num_bins, "obj_var");
  {  // Source.
    MPConstraint* const ct = solver.MakeRowConstraint(0.0, 0.0);
    for (MPVariable* const var : outgoing_vars[/*source*/ 0]) {
      ct->SetCoefficient(var, 1.0);
    }
    ct->SetCoefficient(obj_var, -1.0);
  }

  {  // Sink.
    MPConstraint* const ct = solver.MakeRowConstraint(0.0, 0.0);
    const int sink_node = graph.nodes.size() - 1;
    for (MPVariable* const var : incoming_vars[sink_node]) {
      ct->SetCoefficient(var, 1.0);
    }
    ct->SetCoefficient(obj_var, -1.0);
  }

  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(obj_var, 1.0);

  if (!absl::GetFlag(FLAGS_arc_flow_dump_model).empty()) {
    MPModelProto output_model;
    solver.ExportModelToProto(&output_model);
    CHECK_OK(file::SetTextProto(absl::GetFlag(FLAGS_arc_flow_dump_model),
                                output_model, file::Defaults()));
  }

  solver.EnableOutput();
  solver.SetSolverSpecificParametersAsString(mip_params);
  solver.SetTimeLimit(absl::Seconds(time_limit));
  const MPSolver::ResultStatus result_status = solver.Solve();

  vbp::VectorBinPackingSolution solution;
  solution.set_solve_time_in_seconds(solver.wall_time() / 1000.0);
  solution.set_arc_flow_time_in_seconds(arc_flow_time);
  // Check that the problem has an optimal solution.
  if (result_status == MPSolver::OPTIMAL) {
    solution.set_status(vbp::OPTIMAL);
    solution.set_objective_value(objective->Value());
  } else if (result_status == MPSolver::FEASIBLE) {
    solution.set_status(vbp::FEASIBLE);
    solution.set_objective_value(objective->Value());
  } else if (result_status == MPSolver::INFEASIBLE) {
    solution.set_status(vbp::INFEASIBLE);
  }

  if (result_status == MPSolver::OPTIMAL ||
      result_status == MPSolver::FEASIBLE) {
    // Create the arc flow graph with the flow quantity in each arc.
    struct NextCountItem {
      int next = -1;
      int count = 0;
      int item = -1;
    };
    std::vector<std::vector<NextCountItem>> node_to_next_count_item(
        graph.nodes.size());
    for (int v = 0; v < graph.arcs.size(); ++v) {
      const int count =
          static_cast<int>(std::round(arc_to_var[v]->solution_value()));
      if (count == 0) continue;
      const ArcFlowGraph::Arc& arc = graph.arcs[v];
      node_to_next_count_item[arc.source].push_back(
          {arc.destination, count, arc.item_index});
    }

    // Unroll each possible path and rebuild bins.
    struct NextItem {
      int next = -1;
      int item = -1;
    };
    const auto pop_next_item = [&node_to_next_count_item](int node) {
      CHECK(!node_to_next_count_item[node].empty());
      auto& [next, count, item] = node_to_next_count_item[node].back();
      CHECK_NE(next, -1);
      CHECK_GT(count, 0);
      if (--count == 0) {
        node_to_next_count_item[node].pop_back();
      }
      return NextItem({next, item});
    };

    const int start_node = 0;
    const int end_node = graph.nodes.size() - 1;
    while (!node_to_next_count_item[start_node].empty()) {
      absl::btree_map<int, int> item_count;
      int current = start_node;
      while (current != end_node) {
        const auto& [next, item] = pop_next_item(current);
        if (item != -1) {
          item_count[item]++;
        } else {
          CHECK_EQ(next, end_node);
        }
        current = next;
      }
      vbp::VectorBinPackingOneBinInSolution* bin = solution.add_bins();
      for (const auto& [item, count] : item_count) {
        bin->add_item_indices(item);
        bin->add_item_copies(count);
      }
    }
  }

  return solution;
}

}  // namespace packing
}  // namespace operations_research
