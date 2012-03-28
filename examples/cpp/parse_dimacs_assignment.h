// Copyright 2010-2012 Google
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
// Function for reading and parsing a file in DIMACS format:
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
//

#ifndef OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_

#include <string>

#include "graph/ebert_graph.h"

namespace operations_research {

template <typename GraphType> class LinearSumAssignment;

// Reads an assignment problem description from the given file in
// DIMACS format and returns a LinearSumAssignment object representing
// the problem description. For a description of the format, see
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
//
// Also returns an error message (empty if no error) and a handle on
// the underlying graph representation. The error_message pointer must
// not be NULL because we insist on returning an explanatory message
// in the case of error. The graph_handle pointer must not be NULL
// because unless we pass a non-const pointer to the graph
// representation back to the caller, the caller lacks a good way to
// free the underlying graph (which isn't owned by the
// LinearAssignment instance).
LinearSumAssignment<ForwardStarGraph>* ParseDimacsAssignment(
    const string& filename,
    string* error_message,
    ForwardStarGraph** graph);

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_
