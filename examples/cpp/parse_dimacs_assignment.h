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
// Function for reading and parsing a file in DIMACS format:
// http://lpsolve.sourceforge.net/5.5/DIMACS_asn.htm
//

#ifndef OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_
#define OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/util/filelineiter.h"

ABSL_DECLARE_FLAG(bool, assignment_maximize_cost);

namespace operations_research {

template <typename GraphType>
class DimacsAssignmentParser {
 public:
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  using CostValue = typename LinearSumAssignment<GraphType>::CostValueT;

  explicit DimacsAssignmentParser(absl::string_view filename)
      : filename_(filename), graph_(nullptr), assignment_(nullptr) {}

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
  LinearSumAssignment<GraphType>* Parse(std::string* error_message,
                                        GraphType** graph);

 private:
  void ParseProblemLine(const std::string& line);

  void ParseNodeLine(const std::string& line);

  void ParseArcLine(const std::string& line);

  void ParseOneLine(const std::string& line);

  std::string filename_;

  struct ErrorTrackingState {
    ErrorTrackingState()
        : bad(false),
          nodes_described(false),
          reason(nullptr),
          num_left_nodes(0),
          num_arcs(0) {}

    bool bad;
    bool nodes_described;
    const char* reason;
    NodeIndex num_left_nodes;
    ArcIndex num_arcs;
    std::unique_ptr<std::string> bad_line;
  };

  ErrorTrackingState state_;

  std::unique_ptr<GraphType> graph_;

  LinearSumAssignment<GraphType>* assignment_;
};

// Implementation is below here.
template <typename GraphType>
void DimacsAssignmentParser<GraphType>::ParseProblemLine(
    const std::string& line) {
  static const char* kIncorrectProblemLine =
      "Incorrect assignment problem line.";
  static const char* kAssignmentProblemType = "asn";
  char problem_type[4];
  NodeIndex num_nodes;
  ArcIndex num_arcs;

  if ((sscanf(line.c_str(), "%*c%3s%d%d", problem_type, &num_nodes,
              &num_arcs) != 3) ||
      (strncmp(kAssignmentProblemType, problem_type,
               strlen(kAssignmentProblemType)) != 0)) {
    state_.bad = true;
    state_.reason = kIncorrectProblemLine;
    state_.bad_line.reset(new std::string(line));
    return;
  }

  state_.num_arcs = num_arcs;
  graph_ = std::make_unique<GraphType>(num_nodes, num_arcs);
}

template <typename GraphType>
void DimacsAssignmentParser<GraphType>::ParseNodeLine(const std::string& line) {
  NodeIndex node_id;
  if (sscanf(line.c_str(), "%*c%d", &node_id) != 1) {
    state_.bad = true;
    state_.reason = "Syntax error in node description.";
    state_.bad_line.reset(new std::string(line));
    return;
  }
  if (state_.nodes_described) {
    state_.bad = true;
    state_.reason = "All node description must precede first arc description.";
    state_.bad_line.reset(new std::string(line));
    return;
  }
  state_.num_left_nodes = std::max(state_.num_left_nodes, node_id);
}

template <typename GraphType>
void DimacsAssignmentParser<GraphType>::ParseArcLine(const std::string& line) {
  if (graph_ == nullptr) {
    state_.bad = true;
    state_.reason =
        "Problem specification line must precede any arc specification.";
    state_.bad_line.reset(new std::string(line));
    return;
  }
  if (!state_.nodes_described) {
    state_.nodes_described = true;
    DCHECK(assignment_ == nullptr);
    assignment_ = new LinearSumAssignment<GraphType>(state_.num_left_nodes,
                                                     state_.num_arcs);
  }
  NodeIndex tail;
  NodeIndex head;
  CostValue cost;
  if (sscanf(line.c_str(), "%*c%d%d%" SCNd64, &tail, &head, &cost) != 3) {
    state_.bad = true;
    state_.reason = "Syntax error in arc descriptor.";
    state_.bad_line.reset(new std::string(line));
  }
  ArcIndex arc = graph_->AddArc(tail - 1, head - 1);
  assignment_->SetArcCost(
      arc, absl::GetFlag(FLAGS_assignment_maximize_cost) ? -cost : cost);
}

// Parameters out of style-guide order because this function is used
// as a callback that varies the input line.
template <typename GraphType>
void DimacsAssignmentParser<GraphType>::ParseOneLine(const std::string& line) {
  if (state_.bad) {
    return;
  }
  switch (line[0]) {
    case 'p': {
      // Problem-specification line
      ParseProblemLine(line);
      break;
    }
    case 'c': {
      // Comment; do nothing.
      return;
    }
    case 'n': {
      // Node line defining a node on the left side
      ParseNodeLine(line);
      break;
    }
    case 'a': {
      ParseArcLine(line);
      break;
    }
    case '0':
    case '\n':
      break;
    default: {
      state_.bad = true;
      state_.reason = "Unknown line type in the input.";
      state_.bad_line.reset(new std::string(line));
      break;
    }
  }
}

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
template <typename GraphType>
LinearSumAssignment<GraphType>* DimacsAssignmentParser<GraphType>::Parse(
    std::string* error_message, GraphType** graph_handle) {
  CHECK(error_message != nullptr);
  CHECK(graph_handle != nullptr);

  for (const std::string& line : FileLines(filename_)) {
    if (line.empty()) {
      continue;
    }
    ParseOneLine(line);
  }

  if (state_.bad) {
    *error_message = state_.reason;
    *error_message = *error_message + ": \"" + *state_.bad_line + "\"";
    return nullptr;
  }
  if (graph_ == nullptr) {
    *error_message = "empty graph description";
    return nullptr;
  }
  graph_->Build();
  assignment_->SetGraph(graph_.get());
  *error_message = "";
  // Return a handle on the graph to the caller so the caller can free
  // the graph's memory, because the LinearSumAssignment object does
  // not take ownership of the graph and hence will not free it.
  *graph_handle = graph_.release();
  return assignment_;
}

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_PARSE_DIMACS_ASSIGNMENT_H_
