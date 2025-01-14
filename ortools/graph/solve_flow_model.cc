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

//
// This code loads flow-graph models (as Dimacs file or binary FlowModel proto)
// and solves them with the OR-tools flow algorithms.
//
// Note(user): only min-cost flow is supported at this point.
// TODO(user): move this DIMACS parser to its own class, like the ones in
// routing/. This change would improve searchability of the parser.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/filesystem.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/path.h"
#include "ortools/base/timer.h"
#include "ortools/graph/flow_problem.pb.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/max_flow.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/util/filelineiter.h"
#include "ortools/util/stats.h"

ABSL_FLAG(std::string, input, "", "Input file of the problem.");
ABSL_FLAG(std::string, output_dimacs, "", "Output problem as a dimacs file.");

namespace operations_research {

// See http://lpsolve.sourceforge.net/5.5/DIMACS_mcf.htm for the dimacs file
// format of a min cost flow problem.
//
// TODO(user): This currently only works for min cost flow problem.
void ConvertFlowModelToDimacs(const FlowModelProto& flow_model,
                              std::string* dimacs) {
  CHECK_EQ(FlowModelProto::MIN_COST_FLOW, flow_model.problem_type());
  dimacs->clear();
  dimacs->append("c Min-Cost flow problem generated from a FlowModelProto.\n");

  // We need to compute the num_nodes from the nodes appearing in the arcs.
  int64_t num_nodes = 0;
  for (int64_t i = 0; i < flow_model.arcs_size(); ++i) {
    num_nodes = std::max(num_nodes, flow_model.arcs(i).tail() + 1);
    num_nodes = std::max(num_nodes, flow_model.arcs(i).head() + 1);
  }

  // Problem size and type.
  const int64_t num_arcs = flow_model.arcs_size();
  dimacs->append("c\nc Problem line (problem_type, num nodes, num arcs).\n");
  dimacs->append(absl::StrFormat("p min %d %d\n", num_nodes, num_arcs));

  // Nodes.
  dimacs->append("c\nc Node descriptor lines (id, supply/demand).\n");
  for (int64_t i = 0; i < flow_model.nodes_size(); ++i) {
    const int64_t id = flow_model.nodes(i).id() + 1;
    const int64_t supply = flow_model.nodes(i).supply();
    if (supply != 0) {
      dimacs->append(absl::StrFormat("n %d %d\n", id, supply));
    }
  }

  // Arcs.
  dimacs->append(
      "c\nc Arc descriptor Lines (tail, head, minflow, maxflow, cost).\n");
  for (int64_t i = 0; i < flow_model.arcs_size(); ++i) {
    const int64_t tail = flow_model.arcs(i).tail() + 1;
    const int64_t head = flow_model.arcs(i).head() + 1;
    const int64_t unit_cost = flow_model.arcs(i).unit_cost();
    const int64_t capacity = flow_model.arcs(i).capacity();
    dimacs->append(absl::StrFormat("a %d %d %d %d %d\n", tail, head, int64_t{0},
                                   capacity, unit_cost));
  }
  dimacs->append("c\n");
}

// Note(user): Going from Dimacs to flow adds an extra copy, but for now we
// don't really care of the Dimacs file reading performance.
// Returns true if the file was converted correctly.
bool ConvertDimacsToFlowModel(absl::string_view file,
                              FlowModelProto* flow_model) {
  flow_model->Clear();
  FlowModelProto::ProblemType problem_type;
  for (const std::string& line : FileLines(file)) {
    if (line.empty()) continue;
    if (line[0] == 'p') {
      if (absl::StartsWith(line, "p min")) {
        problem_type = FlowModelProto::MIN_COST_FLOW;
      } else if (absl::StartsWith(line, "p max")) {
        problem_type = FlowModelProto::MAX_FLOW;
      } else {
        LOG(ERROR) << "Unknown dimacs problem format.";
        return false;
      }
      flow_model->set_problem_type(problem_type);
    }
    if (line[0] == 'n') {
      int64_t id;
      int64_t supply;
      std::stringstream ss(line.substr(1));
      switch (problem_type) {
        case FlowModelProto::MIN_COST_FLOW:
          ss >> id >> supply;
          break;
        case FlowModelProto::MAX_FLOW: {
          std::string type;
          ss >> id >> type;
          supply = (type == "s" ? 1 : -1);
          break;
        }
        default:
          LOG(ERROR) << "Node line before the problem type definition.";
          return false;
      }
      FlowNodeProto* node = flow_model->add_nodes();
      node->set_id(id - 1);
      node->set_supply(supply);
    }
    if (line[0] == 'a') {
      int64_t tail;
      int64_t head;
      int64_t min_capacity = 0;
      int64_t max_capacity = 0;
      int64_t unit_cost = 0;
      std::stringstream ss(line.substr(1));
      switch (problem_type) {
        case FlowModelProto::MIN_COST_FLOW:
          ss >> tail >> head >> min_capacity >> max_capacity >> unit_cost;
          break;
        case FlowModelProto::MAX_FLOW:
          ss >> tail >> head >> max_capacity;
          break;
        default:
          LOG(ERROR) << "Arc line before the problem type definition.";
          return false;
      }
      FlowArcProto* arc = flow_model->add_arcs();
      arc->set_tail(tail - 1);
      arc->set_head(head - 1);
      arc->set_capacity(max_capacity);
      arc->set_unit_cost(unit_cost);
      if (min_capacity != 0) {
        LOG(ERROR) << "We do not support minimum capacity.";
        return false;
      }
    }
  }
  return true;
}

// Type of graph to use.
typedef util::ReverseArcStaticGraph<> Graph;

// Loads a FlowModelProto proto into the MinCostFlow class and solves it.
void SolveMinCostFlow(const FlowModelProto& flow_model, double* loading_time,
                      double* solving_time) {
  WallTimer timer;
  timer.Start();

  // Compute the number of nodes.
  int64_t num_nodes = 0;
  for (int i = 0; i < flow_model.arcs_size(); ++i) {
    num_nodes = std::max(num_nodes, flow_model.arcs(i).tail() + 1);
    num_nodes = std::max(num_nodes, flow_model.arcs(i).head() + 1);
  }

  // Build the graph.
  Graph graph(num_nodes, flow_model.arcs_size());
  for (int i = 0; i < flow_model.arcs_size(); ++i) {
    graph.AddArc(flow_model.arcs(i).tail(), flow_model.arcs(i).head());
  }
  std::vector<Graph::ArcIndex> permutation;
  graph.Build(&permutation);

  GenericMinCostFlow<Graph> min_cost_flow(&graph);
  for (int i = 0; i < flow_model.arcs_size(); ++i) {
    const Graph::ArcIndex image = i < permutation.size() ? permutation[i] : i;
    min_cost_flow.SetArcUnitCost(image, flow_model.arcs(i).unit_cost());
    min_cost_flow.SetArcCapacity(image, flow_model.arcs(i).capacity());
  }
  for (int i = 0; i < flow_model.nodes_size(); ++i) {
    min_cost_flow.SetNodeSupply(flow_model.nodes(i).id(),
                                flow_model.nodes(i).supply());
  }

  *loading_time = timer.Get();
  absl::PrintF("%f,", *loading_time);
  fflush(stdout);

  timer.Start();
  CHECK(min_cost_flow.Solve());
  CHECK_EQ(GenericMinCostFlow<Graph>::OPTIMAL, min_cost_flow.status());
  *solving_time = timer.Get();
  absl::PrintF("%f,", *solving_time);
  absl::PrintF("%d", min_cost_flow.GetOptimalCost());
  fflush(stdout);
}

// Loads a FlowModelProto proto into the MaxFlow class and solves it.
void SolveMaxFlow(const FlowModelProto& flow_model, double* loading_time,
                  double* solving_time) {
  WallTimer timer;
  timer.Start();

  // Compute the number of nodes.
  int64_t num_nodes = 0;
  for (int i = 0; i < flow_model.arcs_size(); ++i) {
    num_nodes = std::max(num_nodes, flow_model.arcs(i).tail() + 1);
    num_nodes = std::max(num_nodes, flow_model.arcs(i).head() + 1);
  }

  // Build the graph.
  Graph graph(num_nodes, flow_model.arcs_size());
  for (int i = 0; i < flow_model.arcs_size(); ++i) {
    graph.AddArc(flow_model.arcs(i).tail(), flow_model.arcs(i).head());
  }
  std::vector<Graph::ArcIndex> permutation;
  graph.Build(&permutation);

  // Find source & sink.
  Graph::NodeIndex source = -1;
  Graph::NodeIndex sink = -1;
  CHECK_EQ(2, flow_model.nodes_size());
  for (int i = 0; i < flow_model.nodes_size(); ++i) {
    if (flow_model.nodes(i).supply() > 0) {
      source = flow_model.nodes(i).id();
    }
    if (flow_model.nodes(i).supply() < 0) {
      sink = flow_model.nodes(i).id();
    }
  }
  CHECK_NE(source, -1);
  CHECK_NE(sink, -1);

  // Create the max flow instance and set the arc capacities.
  GenericMaxFlow<Graph> max_flow(&graph, source, sink);
  for (int i = 0; i < flow_model.arcs_size(); ++i) {
    const Graph::ArcIndex image = i < permutation.size() ? permutation[i] : i;
    max_flow.SetArcCapacity(image, flow_model.arcs(i).capacity());
  }

  *loading_time = timer.Get();
  absl::PrintF("%f,", *loading_time);
  fflush(stdout);

  timer.Start();
  CHECK(max_flow.Solve());
  CHECK_EQ(GenericMaxFlow<Graph>::OPTIMAL, max_flow.status());
  *solving_time = timer.Get();
  absl::PrintF("%f,", *solving_time);
  absl::PrintF("%d", max_flow.GetOptimalFlow());
  fflush(stdout);
}

}  // namespace operations_research

using operations_research::FlowModelProto;
using operations_research::SolveMaxFlow;
using operations_research::SolveMinCostFlow;
using operations_research::TimeDistribution;

int main(int argc, char** argv) {
  InitGoogle(
      "Runs the OR-tools min-cost flow on a pattern of files given by --input. "
      "The files must be in Dimacs text format or in binary FlowModelProto "
      "format.",
      &argc, &argv, true);
  absl::SetFlag(&FLAGS_stderrthreshold, 0);
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(FATAL) << "Please specify input pattern via --input=...";
  }
  std::vector<std::string> file_list;
  file::Match(absl::GetFlag(FLAGS_input), &file_list, file::Defaults())
      .IgnoreError();

  TimeDistribution parsing_time_distribution("Parsing time summary");
  TimeDistribution loading_time_distribution("Loading time summary");
  TimeDistribution solving_time_distribution("Solving time summary");

  absl::PrintF(
      "file_name, parsing_time, loading_time, solving_time, optimal_cost\n");
  for (int i = 0; i < file_list.size(); ++i) {
    const std::string file_name = file_list[i];
    absl::PrintF("%s,", file::Basename(file_name));
    fflush(stdout);

    // Parse the input as a proto.
    double parsing_time = 0;
    operations_research::FlowModelProto proto;
    if (absl::EndsWith(file_name, ".bin")) {
      ScopedWallTime timer(&parsing_time);
      std::string raw_data;
      CHECK_OK(file::GetContents(file_name, &raw_data, file::Defaults()));
      proto.ParseFromString(raw_data);
    } else {
      ScopedWallTime timer(&parsing_time);
      if (!ConvertDimacsToFlowModel(file_name, &proto)) continue;
    }
    absl::PrintF("%f,", parsing_time);
    fflush(stdout);

    // TODO(user): improve code to convert many files.
    if (!absl::GetFlag(FLAGS_output_dimacs).empty()) {
      LOG(INFO) << "Converting first input file to dimacs format.";
      double time = 0;
      {
        ScopedWallTime timer(&time);
        std::string dimacs;
        ConvertFlowModelToDimacs(proto, &dimacs);
        CHECK_OK(file::SetContents(absl::GetFlag(FLAGS_output_dimacs), dimacs,
                                   file::Defaults()));
      }
      LOG(INFO) << "Done: " << time << "s.";
      return EXIT_SUCCESS;
    }

    double loading_time = 0;
    double solving_time = 0;
    switch (proto.problem_type()) {
      case FlowModelProto::MIN_COST_FLOW:
        SolveMinCostFlow(proto, &loading_time, &solving_time);
        break;
      case FlowModelProto::MAX_FLOW:
        SolveMaxFlow(proto, &loading_time, &solving_time);
        break;
      default:
        LOG(ERROR) << "Problem type not supported: " << proto.problem_type();
    }
    absl::PrintF("\n");

    parsing_time_distribution.AddTimeInSec(parsing_time);
    loading_time_distribution.AddTimeInSec(loading_time);
    solving_time_distribution.AddTimeInSec(solving_time);
  }
  absl::PrintF("%s", parsing_time_distribution.StatString());
  absl::PrintF("%s", loading_time_distribution.StatString());
  absl::PrintF("%s", solving_time_distribution.StatString());
  return EXIT_SUCCESS;
}
