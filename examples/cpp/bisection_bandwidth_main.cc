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

// Computes the minimum bisection bandwidth of a graph
// (see https://en.wikipedia.org/wiki/Bisection_bandwidth).

#include <cstdlib>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/graph_io.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"

ABSL_FLAG(std::string, input, "",
          "Directed graph file name (see google3/ortools/graph/io.h)");

ABSL_FLAG(std::string, params, "num_workers:16,log_search_progress:true",
          "Sat solver parameters");

namespace operations_research {
void Solve(const util::ListGraph<>& graph) {
  CHECK_EQ(graph.num_nodes() % 2, 0);
  sat::CpModelBuilder cp_model;

  // Whether each node is selected in the first half of the bisection.
  std::vector<sat::BoolVar> selected;
  for (int i = 0; i < graph.num_nodes(); ++i) {
    selected.push_back(
        cp_model.NewBoolVar().WithName(absl::StrCat("Selected_", i)));
  }

  // The number of selected nodes must be exactly half the number of nodes.
  sat::LinearExpr num_selected = sat::LinearExpr::Sum(selected);
  cp_model.AddEquality(num_selected, graph.num_nodes() / 2);

  // The number of edges with vertices in different halves of the bisection.
  sat::LinearExpr cut_size;
  for (const int arc : graph.AllForwardArcs()) {
    const int head = graph.Head(arc);
    const int tail = graph.Tail(arc);
    sat::BoolVar cut_edge =
        cp_model.NewBoolVar().WithName(absl::StrCat("Cut_", head, "_", tail));
    cp_model.AddImplication({selected[head], selected[tail].Not()}, {cut_edge});
    cp_model.AddImplication({selected[head].Not(), selected[tail]}, {cut_edge});
    cut_size += cut_edge;
  }

  cp_model.Minimize(cut_size);

  sat::CpModelProto cp_model_proto = cp_model.Build();
  sat::SatParameters params;
  ProtobufTextFormatMergeFromString(absl::GetFlag(FLAGS_params), &params);
  const sat::CpSolverResponse response =
      sat::SolveWithParameters(cp_model_proto, params);

  if (response.status() == sat::OPTIMAL) {
    LOG(INFO) << "Bisection bandwidth: " << response.objective_value();
  } else if (response.status() == sat::FEASIBLE) {
    LOG(INFO) << "Bisection bandwidth upper bound: "
              << response.objective_value();
    LOG(INFO) << "Bisection bandwidth lower bound: "
              << response.best_objective_bound();

  } else {
    LOG(ERROR) << "Unexpected error " << response.status();
  }
}
}  // namespace operations_research
int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, /*remove_flags=*/true);

  const absl::StatusOr<util::ListGraph<>*> graph =
      util::ReadGraphFile<util::ListGraph<>>(
          absl::GetFlag(FLAGS_input), /*directed=*/true,
          /*num_nodes_with_color_or_null=*/nullptr);
  if (graph.ok()) {
    operations_research::Solve(*graph.value());
    return EXIT_SUCCESS;
  } else {
    LOG(ERROR) << "Can't read graph: " << graph.status();
    return EXIT_FAILURE;
  }
}
