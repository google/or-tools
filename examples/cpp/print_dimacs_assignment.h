// Copyright 2010-2013 Google
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

#include "base/logging.h"
#include "base/stringprintf.h"
#include "graph/ebert_graph.h"
#include "graph/linear_assignment.h"

namespace operations_research {

template <typename GraphType> class LinearSumAssignment;

// Given a LinearSumAssigment object representing an assignment problem
// description, outputs the problem in DIMACS format in the output file.
// For a description of the format, see
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
template<typename GraphType>
void PrintDimacsAssignmentProblem(
    const LinearSumAssignment<GraphType>& assignment,
    const TailArrayManager<GraphType>& tail_array_manager,
    const std::string& output_filename);

// Implementation is below here.
namespace internal {

void WriteOrDie(const char* buffer,
                size_t item_size,
                size_t buffer_length,
                FILE* fp) {
  size_t written = fwrite(buffer, item_size, buffer_length, fp);
  if (written != buffer_length) {
    fprintf(stderr, "Write failed.\n");
    exit(1);
  }
}

}  // namespace internal


template<typename GraphType>
void PrintDimacsAssignmentProblem(
    const LinearSumAssignment<GraphType>& assignment,
    const TailArrayManager<GraphType>& tail_array_manager,
    const std::string& output_filename) {
  FILE* output = fopen(output_filename.c_str(), "w");
  const GraphType& graph(assignment.Graph());
  std::string output_line = StringPrintf("p asn %d %d\n",
                                    graph.num_nodes(),
                                    graph.num_arcs());
  internal::WriteOrDie(output_line.c_str(), 1,
                       output_line.length(), output);

  for (typename LinearSumAssignment<GraphType>::BipartiteLeftNodeIterator
           node_it(assignment);
       node_it.Ok();
       node_it.Next()) {
    output_line = StringPrintf("n %d\n", node_it.Index() + 1);
    internal::WriteOrDie(output_line.c_str(), 1,
                         output_line.length(), output);
  }

  tail_array_manager.BuildTailArrayFromAdjacencyListsIfForwardGraph();

  for (typename GraphType::ArcIterator arc_it(assignment.Graph());
       arc_it.Ok();
       arc_it.Next()) {
    ArcIndex arc = arc_it.Index();
    output_line = StringPrintf("a %d %d %lld\n",
                               graph.Tail(arc) + 1,
                               graph.Head(arc) + 1,
                               assignment.ArcCost(arc));
    internal::WriteOrDie(output_line.c_str(), 1,
                         output_line.length(), output);
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
