// Copyright 2010-2017 Google
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

#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/file.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/base/status.h"

namespace operations_research {

template <typename GraphType>
class LinearSumAssignment;

// Given a LinearSumAssigment object representing an assignment problem
// description, outputs the problem in DIMACS format in the output file.
// For a description of the format, see
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
template <typename GraphType>
void PrintDimacsAssignmentProblem(
    const LinearSumAssignment<GraphType>& assignment,
    const TailArrayManager<GraphType>& tail_array_manager,
    const std::string& output_filename);

template <typename GraphType>
void PrintDimacsAssignmentProblem(
    const LinearSumAssignment<GraphType>& assignment,
    const TailArrayManager<GraphType>& tail_array_manager,
    const std::string& output_filename) {
  File* output = File::Open(output_filename, "w");
  const GraphType& graph(assignment.Graph());
  std::string output_line =
      StringPrintf("p asn %d %d\n", graph.num_nodes(), graph.num_arcs());
  CHECK_OK(file::WriteString(output, output_line, file::Defaults()));

  for (typename LinearSumAssignment<GraphType>::BipartiteLeftNodeIterator
           node_it(assignment);
       node_it.Ok(); node_it.Next()) {
    output_line = StringPrintf("n %d\n", node_it.Index() + 1);
    CHECK_OK(file::WriteString(output, output_line, file::Defaults()));
  }

  tail_array_manager.BuildTailArrayFromAdjacencyListsIfForwardGraph();

  for (typename GraphType::ArcIterator arc_it(assignment.Graph()); arc_it.Ok();
       arc_it.Next()) {
    ArcIndex arc = arc_it.Index();
    output_line = StringPrintf("a %d %d %lld\n", graph.Tail(arc) + 1,
                               graph.Head(arc) + 1, assignment.ArcCost(arc));
    CHECK_OK(file::WriteString(output, output_line, file::Defaults()));
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
