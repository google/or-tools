// Copyright 2010-2011 Google
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
#include <cstdlib>
#include "base/hash.h"
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "algorithms/hungarian.h"
#include "graph/ebert_graph.h"
#include "graph/linear_assignment.h"
#include "examples/parse_dimacs_assignment.h"
#include "examples/print_dimacs_assignment.h"

DEFINE_string(assignment_problem_output_file, "",
              "Print the problem to this file in DIMACS format (after layout "
              "is optimized, if applicable).");
DEFINE_bool(assignment_compare_hungarian, false,
            "Compare result and speed against Hungarian method.");

namespace operations_research {

CostValue BuildAndSolveHungarianInstance(
    LinearSumAssignment &assignment) {
  const StarGraph& graph = assignment.Graph();
  typedef std::vector<double> HungarianRow;
  typedef std::vector<HungarianRow> HungarianProblem;
  HungarianProblem hungarian_cost;
  hungarian_cost.resize(assignment.NumLeftNodes());
  // First we have to find the biggest cost magnitude so we can
  // initialize the arc costs that aren't really there.
  CostValue largest_cost_magnitude = 0;
  for (StarGraph::ArcIterator arc_it(graph); arc_it.Ok(); arc_it.Next()) {
    ArcIndex arc = arc_it.Index();
    CostValue cost_magnitude = ::std::abs(assignment.ArcCost(arc));
    largest_cost_magnitude = ::std::max(largest_cost_magnitude, cost_magnitude);
  }
  double missing_arc_cost = static_cast<double>((assignment.NumLeftNodes() *
                                                 largest_cost_magnitude) +
                                                1);
  for (HungarianProblem::iterator row = hungarian_cost.begin();
       row != hungarian_cost.end();
       ++row) {
    row->resize(assignment.NumNodes() - assignment.NumLeftNodes(),
                missing_arc_cost);
  }
  for (StarGraph::ArcIterator arc_it(graph);
       arc_it.Ok();
       arc_it.Next()) {
    ArcIndex arc = arc_it.Index();
    NodeIndex tail = graph.Tail(arc) - StarGraph::kFirstNode;
    NodeIndex head = (graph.Head(arc) - assignment.NumLeftNodes() -
                      StarGraph::kFirstNode);
    double cost = static_cast<double>(assignment.ArcCost(arc));
    hungarian_cost[tail][head] = cost;
  }
  hash_map<int, int> result;
  hash_map<int, int> wish_this_could_be_null;
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

void DisplayAssignment(const LinearSumAssignment& assignment) {
  for (LinearSumAssignment::BipartiteLeftNodeIterator node_it(assignment);
       node_it.Ok();
       node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    const ArcIndex matching_arc = assignment.GetAssignmentArc(left_node);
    const NodeIndex right_node = assignment.Head(matching_arc);
    VLOG(5) << "assigned (" << left_node << ", " << right_node << "): "
            << assignment.ArcCost(matching_arc);
  }
}

static const char* const kUsageTemplate = "usage: %s <filename>";

int solve_dimacs_assignment(int argc, char* argv[]) {
  string usage;
  if (argc < 1) {
    usage = StringPrintf(kUsageTemplate, "solve_dimacs_assignment");
  } else {
    usage = StringPrintf(kUsageTemplate, argv[0]);
  }
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc < 2) {
    LOG(FATAL) << usage;
  }
  string error_message;
  LinearSumAssignment* assignment =
      ParseDimacsAssignment(argv[1], &error_message);
  if (assignment == NULL) {
    LOG(FATAL) << error_message;
  }
  if (!FLAGS_assignment_problem_output_file.empty()) {
    PrintDimacsAssignmentProblem(*assignment,
                                 FLAGS_assignment_problem_output_file);
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
  return 0;
}
}  // namespace operations_research

int main(int argc, char* argv[]) {
  return ::operations_research::solve_dimacs_assignment(argc, argv);
}
