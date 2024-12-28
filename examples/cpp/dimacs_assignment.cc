// Copyright 2010-2024 Google LLC
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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "examples/cpp/parse_dimacs_assignment.h"
#include "examples/cpp/print_dimacs_assignment.h"
#include "ortools/algorithms/hungarian.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/linear_assignment.h"

ABSL_FLAG(bool, assignment_compare_hungarian, false,
          "Compare result and speed against Hungarian method.");
ABSL_FLAG(std::string, assignment_problem_output_file, "",
          "Print the problem to this file in DIMACS format (after layout "
          "is optimized, if applicable).");
ABSL_FLAG(bool, assignment_reverse_arcs, false,
          "Ignored if --assignment_static_graph=true. Use ReverseArcListGraph "
          "if true, ListGraph if false.");
ABSL_FLAG(bool, assignment_static_graph, true,
          "Use the StaticGraph representation, "
          "otherwise ListGraph or ReverseArcListGraph according "
          "to --assignment_reverse_arcs.");

namespace operations_research {

using NodeIndex = int32_t;
using ArcIndex = int32_t;
using CostValue = int64_t;

template <typename GraphType>
CostValue BuildAndSolveHungarianInstance(
    const LinearSumAssignment<GraphType>& assignment) {
  const GraphType& graph = assignment.Graph();
  typedef std::vector<double> HungarianRow;
  typedef std::vector<HungarianRow> HungarianProblem;
  HungarianProblem hungarian_cost;
  hungarian_cost.resize(assignment.NumLeftNodes());
  // First we have to find the biggest cost magnitude so we can
  // initialize the arc costs that aren't really there.
  CostValue largest_cost_magnitude = 0;
  for (const auto arc : graph.AllForwardArcs()) {
    CostValue cost_magnitude = std::abs(assignment.ArcCost(arc));
    largest_cost_magnitude = std::max(largest_cost_magnitude, cost_magnitude);
  }
  double missing_arc_cost = static_cast<double>(
      (assignment.NumLeftNodes() * largest_cost_magnitude) + 1);
  for (HungarianProblem::iterator row = hungarian_cost.begin();
       row != hungarian_cost.end(); ++row) {
    row->resize(assignment.NumNodes() - assignment.NumLeftNodes(),
                missing_arc_cost);
  }
  // We're using a graph representation without forward arcs, so in
  // order to use the generic GraphType::ArcIterator we would
  // need to increase our memory footprint by building the array of
  // arc tails (since we need tails to build the input to the
  // hungarian algorithm). We opt for the alternative of iterating
  // over hte arcs via adjacency lists, which gives us the arc tails
  // implicitly.
  for (const auto tail : graph.AllNodes()) {
    for (const auto arc : graph.OutgoingArcs(tail)) {
      NodeIndex head = graph.Head(arc) - assignment.NumLeftNodes();
      double cost = static_cast<double>(assignment.ArcCost(arc));
      hungarian_cost[tail][head] = cost;
    }
  }
  absl::flat_hash_map<int, int> result;
  absl::flat_hash_map<int, int> wish_this_could_be_null;
  WallTimer timer;
  VLOG(1) << "Beginning Hungarian method.";
  timer.Start();
  MinimizeLinearAssignment(hungarian_cost, &result, &wish_this_could_be_null);
  double elapsed = timer.GetInMs() / 1000.0;
  LOG(INFO) << "Hungarian result computed in " << elapsed << " seconds.";
  double result_cost = 0.0;
  for (int i = 0; i < assignment.NumLeftNodes(); ++i) {
    int mate = result[i];
    result_cost += hungarian_cost[i][mate];
  }
  return static_cast<CostValue>(result_cost);
}

template <typename GraphType>
void DisplayAssignment(const LinearSumAssignment<GraphType>& assignment) {
  for (typename LinearSumAssignment<GraphType>::BipartiteLeftNodeIterator
           node_it(assignment);
       node_it.Ok(); node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    const ArcIndex matching_arc = assignment.GetAssignmentArc(left_node);
    const NodeIndex right_node = assignment.Head(matching_arc);
    VLOG(5) << "assigned (" << left_node << ", " << right_node
            << "): " << assignment.ArcCost(matching_arc);
  }
}

template <typename GraphType>
int SolveDimacsAssignment(int argc, char* argv[]) {
  std::string error_message;
  // Handle on the graph we will need to delete because the
  // LinearSumAssignment object does not take ownership of it.
  GraphType* graph = nullptr;
  DimacsAssignmentParser<GraphType> parser(argv[1]);
  LinearSumAssignment<GraphType>* assignment =
      parser.Parse(&error_message, &graph);
  if (assignment == nullptr) {
    LOG(FATAL) << error_message;
  }
  if (!absl::GetFlag(FLAGS_assignment_problem_output_file).empty()) {
    PrintDimacsAssignmentProblem<GraphType>(
        *assignment, absl::GetFlag(FLAGS_assignment_problem_output_file));
  }
  CostValue hungarian_cost = 0.0;
  bool hungarian_solved = false;
  if (absl::GetFlag(FLAGS_assignment_compare_hungarian)) {
    hungarian_cost = BuildAndSolveHungarianInstance(*assignment);
    hungarian_solved = true;
  }
  WallTimer timer;
  timer.Start();
  bool success = assignment->ComputeAssignment();
  double elapsed = timer.GetInMs() / 1000.0;
  if (success) {
    CostValue cost = assignment->GetCost();
    DisplayAssignment(*assignment);
    LOG(INFO) << "Cost of optimum assignment: " << cost;
    LOG(INFO) << "Computed in " << elapsed << " seconds.";
    LOG(INFO) << assignment->StatsString();
    if (hungarian_solved && (cost != hungarian_cost)) {
      LOG(ERROR) << "Optimum cost mismatch: " << cost << " vs. "
                 << hungarian_cost << ".";
    }
  } else {
    LOG(WARNING) << "Given problem is infeasible.";
  }
  delete assignment;
  delete graph;
  return EXIT_SUCCESS;
}
}  // namespace operations_research

static const char* const kUsageTemplate = "usage: %s <filename>";

using ::operations_research::ArcIndex;
using ::operations_research::NodeIndex;
using ::operations_research::SolveDimacsAssignment;

int main(int argc, char* argv[]) {
  std::string usage;
  if (argc < 1) {
    usage = absl::StrFormat(kUsageTemplate, "solve_dimacs_assignment");
  } else {
    usage = absl::StrFormat(kUsageTemplate, argv[0]);
  }
  InitGoogle(usage.c_str(), &argc, &argv, true);

  if (argc < 2) {
    LOG(FATAL) << usage;
  }

  if (absl::GetFlag(FLAGS_assignment_static_graph)) {
    return SolveDimacsAssignment<::util::StaticGraph<NodeIndex, ArcIndex>>(
        argc, argv);
  } else if (absl::GetFlag(FLAGS_assignment_reverse_arcs)) {
    return SolveDimacsAssignment<
        ::util::ReverseArcListGraph<NodeIndex, ArcIndex>>(argc, argv);
  } else {
    return SolveDimacsAssignment<::util::ListGraph<NodeIndex, ArcIndex>>(argc,
                                                                         argv);
  }
}
