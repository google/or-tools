// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_

#include <string>

#include "graph/linear_assignment.h"

namespace operations_research {

class LinearSumAssignment;

void PrintDimacsAssignmentProblem(const LinearSumAssignment& assignment,
                                  const string& output_filename);

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PRINT_DIMACS_ASSIGNMENT_H_
