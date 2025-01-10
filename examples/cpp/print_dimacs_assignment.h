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
// Function for outputting an assignment problem in DIMACS format:
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
//
#ifndef OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_

#include <cstdio>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/graph/linear_assignment.h"

namespace operations_research {

// Given a LinearSumAssignment object representing an assignment problem
// description, outputs the problem in DIMACS format in the output file.
// For a description of the format, see
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
template <typename GraphType>
void PrintDimacsAssignmentProblem(
    const LinearSumAssignment<GraphType>& assignment,
    absl::string_view output_filename) {
  File* output;
  CHECK_OK(file::Open(output_filename, "w", &output, file::Defaults()));
  const GraphType& graph(assignment.Graph());
  std::string output_line =
      absl::StrFormat("p asn %d %d\n", graph.num_nodes(), graph.num_arcs());
  CHECK_OK(file::WriteString(output, output_line, file::Defaults()));

  for (typename LinearSumAssignment<GraphType>::BipartiteLeftNodeIterator
           node_it(assignment);
       node_it.Ok(); node_it.Next()) {
    output_line = absl::StrFormat("n %d\n", node_it.Index() + 1);
    CHECK_OK(file::WriteString(output, output_line, file::Defaults()));
  }

  for (const auto& arc : graph.AllForwardArcs()) {
    output_line = absl::StrFormat("a %d %d %d\n", graph.Tail(arc) + 1,
                                  graph.Head(arc) + 1, assignment.ArcCost(arc));
    CHECK_OK(file::WriteString(output, output_line, file::Defaults()));
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
