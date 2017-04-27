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


#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/timer.h"
#include "ortools/algorithms/hungarian.h"
#include "examples/cpp/parse_dimacs_assignment.h"
#include "examples/cpp/print_dimacs_assignment.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/linear_assignment.h"

DEFINE_bool(assignment_compare_hungarian, false,
            "Compare result and speed against Hungarian method.");
DEFINE_string(assignment_problem_output_file, "",
              "Print the problem to this file in DIMACS format (after layout "
              "is optimized, if applicable).");
DEFINE_bool(assignment_reverse_arcs, false,
            "Ignored if --assignment_static_graph=true. Use StarGraph "
            "if true, ForwardStarGraph if false.");
DEFINE_bool(assignment_static_graph, true,
            "Use the ForwardStarStaticGraph representation, "
            "otherwise ForwardStarGraph or StarGraph according "
            "to --assignment_reverse_arcs.");

namespace operations_research {

typedef ForwardStarStaticGraph GraphType;

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
  for (typename GraphType::ArcIterator arc_it(graph); arc_it.Ok();
       arc_it.Next()) {
    ArcIndex arc = arc_it.Index();
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
  for (typename GraphType::NodeIterator node_it(graph); node_it.Ok();
       node_it.Next()) {
    NodeIndex node = node_it.Index();
    NodeIndex tail = (node - GraphType::kFirstNode);
    for (typename GraphType::OutgoingArcIterator arc_it(graph, node);
         arc_it.Ok(); arc_it.Next()) {
      ArcIndex arc = arc_it.Index();
      NodeIndex head =
          (graph.Head(arc) - assignment.NumLeftNodes() - GraphType::kFirstNode);
      double cost = static_cast<double>(assignment.ArcCost(arc));
      hungarian_cost[tail][head] = cost;
    }
  }
  std::unordered_map<int, int> result;
  std::unordered_map<int, int> wish_this_could_be_null;
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
  if (!FLAGS_assignment_problem_output_file.empty()) {
    // The following tail array management stuff is done in a generic
    // way so we can plug in different types of graphs for which the
    // TailArrayManager template can be instantiated, even though we
    // know the type of the graph explicitly. In this way, the type of
    // the graph can be switched just by changing the graph type in
    // this file and making no other changes to the code.
    TailArrayManager<GraphType> tail_array_manager(graph);
    PrintDimacsAssignmentProblem<GraphType>(
        *assignment, tail_array_manager, FLAGS_assignment_problem_output_file);
    tail_array_manager.ReleaseTailArrayIfForwardGraph();
  }
  CostValue hungarian_cost = 0.0;
  bool hungarian_solved = false;
  if (FLAGS_assignment_compare_hungarian) {
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
  return 0;
}
}  // namespace operations_research

static const char* const kUsageTemplate = "usage: %s <filename>";

using ::operations_research::ForwardStarStaticGraph;
using ::operations_research::ForwardStarGraph;
using ::operations_research::SolveDimacsAssignment;
using ::operations_research::StarGraph;
using ::operations_research::StringPrintf;

int main(int argc, char* argv[]) {
  std::string usage;
  if (argc < 1) {
    usage = StringPrintf(kUsageTemplate, "solve_dimacs_assignment");
  } else {
    usage = StringPrintf(kUsageTemplate, argv[0]);
  }
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (argc < 2) {
    LOG(FATAL) << usage;
  }

  if (FLAGS_assignment_static_graph) {
    return SolveDimacsAssignment<ForwardStarStaticGraph>(argc, argv);
  } else if (FLAGS_assignment_reverse_arcs) {
    return SolveDimacsAssignment<StarGraph>(argc, argv);
  } else {
    return SolveDimacsAssignment<ForwardStarGraph>(argc, argv);
  }
}
