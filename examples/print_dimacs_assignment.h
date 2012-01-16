// Copyright 2011 Google Inc. All Rights Reserved.
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
