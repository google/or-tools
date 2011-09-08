// Copyright 2011 Google Inc. All Rights Reserved.

#ifndef OR_TOOLS_GRAPH_PARSE_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_GRAPH_PARSE_DIMACS_ASSIGNMENT_H_

#include <string>

#include "graph/linear_assignment.h"

namespace operations_research {

class LinearSumAssignment;

LinearSumAssignment* ParseDimacsAssignment(const string& filename,
                                           string* error_message,
                                           StarGraph** graph);

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_PARSE_DIMACS_ASSIGNMENT_H_
