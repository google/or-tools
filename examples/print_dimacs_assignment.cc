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

#include "examples/print_dimacs_assignment.h"

#include <cstdio>
#include <string>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "graph/ebert_graph.h"
#include "graph/linear_assignment.h"

namespace operations_research {

static void WriteOrDie(const char* buffer,
                       size_t item_size,
                       size_t buffer_length,
                       FILE* fp) {
  size_t written = fwrite(buffer, item_size, buffer_length, fp);
  if (written != buffer_length) {
    fprintf(stderr, "Write failed.\n");
    exit(1);
  }
}


void PrintDimacsAssignmentProblem(const LinearSumAssignment& assignment,
                                  const string& output_filename) {
  FILE *output = fopen(output_filename.c_str(), "w");
  const StarGraph& graph(assignment.Graph());
  string output_line = StringPrintf("p asn %lld %lld\n",
                                    graph.num_nodes(),
                                    graph.num_arcs());
  WriteOrDie(output_line.c_str(), 1, output_line.length(),
             output);

  for (LinearSumAssignment::BipartiteLeftNodeIterator node_it(assignment);
       node_it.Ok();
       node_it.Next()) {
    output_line = StringPrintf("n %lld\n", node_it.Index() + 1);
    WriteOrDie(output_line.c_str(), 1, output_line.length(),
               output);
  }

  for (StarGraph::ArcIterator arc_it(assignment.Graph());
       arc_it.Ok();
       arc_it.Next()) {
    ArcIndex arc = arc_it.Index();
    output_line = StringPrintf("a %lld %lld %lld\n",
                               graph.Tail(arc) + 1,
                               graph.Head(arc) + 1,
                               assignment.ArcCost(arc));
    WriteOrDie(output_line.c_str(), 1, output_line.length(),
               output);
  }
}

}  // namespace operations_research
