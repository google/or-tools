// Copyright 2011 Google Inc. All Rights Reserved.
//
// Function for reading and parsing a file in DIMACS format:
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
//

#ifndef OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_

#include <string>

#include "graph/linear_assignment.h"

namespace operations_research {

class LinearSumAssignment;

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
LinearSumAssignment* ParseDimacsAssignment(const string& filename,
                                           string* error_message,
                                           StarGraph** graph);

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_
