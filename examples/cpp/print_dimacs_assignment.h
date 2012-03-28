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
// Function for outputting an assignment problem in DIMACS format:
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
//
#ifndef OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_

#include <string>

#include "graph/ebert_graph.h"

namespace operations_research {

template <typename GraphType> class LinearSumAssignment;

// Given a LinearSumAssigment object representing an assignment problem
// description, outputs the problem in DIMACS format in the output file.
// For a description of the format, see
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
void PrintDimacsAssignmentProblem(
    const LinearSumAssignment<ForwardStarGraph>& assignment,
    const TailArrayManager<ForwardStarGraph>& tail_array_manager,
    const string& output_filename);

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
