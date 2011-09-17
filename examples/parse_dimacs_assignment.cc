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

#include "examples/parse_dimacs_assignment.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "graph/ebert_graph.h"
#include "graph/linear_assignment.h"

DEFINE_bool(assignment_maximize_cost, false,
            "Negate costs so a max-cost assignment is found.");
DEFINE_bool(assignment_optimize_layout, true,
            "Optimize graph layout for speed.");

namespace operations_research {

struct ParserState {
  ParserState()
  : bad(false),
    expect_last_line(false),
    nodes_described(false),
    reason(NULL),
    num_left_nodes(0) { }
  bool bad;
  bool expect_last_line;
  bool nodes_described;
  const char* reason;
  NodeIndex num_left_nodes;
  scoped_ptr<string> bad_line;
};

static void ParseProblemLine(const char* line,
                             ParserState* state,
                             StarGraph** graph) {
  static const char* kIncorrectProblemLine =
      "Incorrect assignment problem line.";
  static const char* kAssignmentProblemType = "asn";
  char problem_type[4];
  NodeIndex num_nodes;
  ArcIndex num_arcs;

  if ((sscanf(line, "%*c%3s%d%d",  // NOLINT
              problem_type,
              &num_nodes,
              &num_arcs) != 3) ||
      (strncmp(kAssignmentProblemType,
               problem_type,
               strlen(kAssignmentProblemType)) != 0)) {
    state->bad = true;
    state->reason = kIncorrectProblemLine;
    state->bad_line.reset(new string(line));
    return;
  }

  *graph = new StarGraph(num_nodes, num_arcs);
}

static void ParseNodeLine(const char* line,
                          ParserState* state) {
  NodeIndex node_id;
  if (sscanf(line, "%*c%d", &node_id) != 1) {  // NOLINT
    state->bad = true;
    state->reason = "Syntax error in node desciption.";
    state->bad_line.reset(new string(line));
    return;
  }
  if (state->nodes_described) {
    state->bad = true;
    state->reason = "All node description must precede first arc description.";
    state->bad_line.reset(new string(line));
    return;
  }
  state->num_left_nodes = ::std::max(state->num_left_nodes, node_id);
}

static void ParseArcLine(const char* line,
                         ParserState* state,
                         StarGraph* graph,
                         LinearSumAssignment** assignment) {
  if (graph == NULL) {
    state->bad = true;
    state->reason =
        "Problem specification line must precede any arc specification.";
    state->bad_line.reset(new string(line));
    return;
  }
  if (!state->nodes_described) {
    state->nodes_described = true;
    DCHECK(*assignment == NULL);
    *assignment = new LinearSumAssignment(*graph, state->num_left_nodes);
  }
  NodeIndex tail;
  NodeIndex head;
  CostValue cost;
  if (sscanf(line, "%*c%d%d%lld", &tail, &head, &cost) != 3) {  // NOLINT
    state->bad = true;
    state->reason = "Syntax error in arc descriptor.";
    state->bad_line.reset(new string(line));
  }
  ArcIndex arc = graph->AddArc(tail - 1, head - 1);
  (*assignment)->SetArcCost(arc, FLAGS_assignment_maximize_cost ? -cost : cost);
}

// Parameters out of style-guide order because this function is used
// as a callback that varies the input line.
static void ParseOneLine(ParserState* state,
                         StarGraph** graph,
                         LinearSumAssignment** assignment,
                         char* line) {
  if (state->bad) {
    return;
  }

  if (state->expect_last_line) {
    state->bad = true;
    state->reason = "Input line is too long.";
    // state->bad_line was already set when we noticed the line
    // didn't end with '\n'.
    return;
  }

  size_t length = strlen(line);
  // The final line might not end with newline. Any other line
  // that seems not to is actually a line that was too long
  // for our input buffer.
  if (line[length - 1] != '\n') {
    state->expect_last_line = true;
    // Prepare for the worst; we might need to inform the user of
    // an error on this line even though we can't detect the error
    // yet.
    state->bad_line.reset(new string(line));
  }


  switch (line[0]) {
    case 'p': {
      // Problem-specification line
      ParseProblemLine(line, state, graph);
      break;
    }
    case 'c': {
      // Comment; do nothing.
      return;
    }
    case 'n': {
      // Node line defining a node on the left side
      ParseNodeLine(line, state);
      break;
    }
    case 'a': {
      ParseArcLine(line, state, *graph, assignment);
      break;
    }
    case '0':
    case '\n':
        break;
    default: {
      state->bad = true;
      state->reason = "Unknown line type in the input.";
      state->bad_line.reset(new string(line));
      break;
    }
  }
}

void ParseFileByLines(const string& filename,
                      Callback1<char*>* line_parser) {
  FILE* fp = fopen(filename.c_str(), "r");
  const int kMaximumLineSize = 1024;
  char line[kMaximumLineSize];
  if (fp != NULL) {
    char* result;
    do {
      result = fgets(line, kMaximumLineSize, fp);
      if (result != NULL) {
        line_parser->Run(result);
      }
    } while (result != NULL);
  }
  delete line_parser;
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
LinearSumAssignment* ParseDimacsAssignment(const string& filename,
                                           string* error_message,
                                           StarGraph** graph_handle) {
  CHECK_NOTNULL(error_message);
  CHECK_NOTNULL(graph_handle);
  StarGraph* graph = NULL;
  LinearSumAssignment* assignment = NULL;
  ParserState state;
  Callback1<char*>* cb =
      NewPermanentCallback(ParseOneLine, &state, &graph, &assignment);
  // ParseFileByLines takes ownership of cb and deletes it.
  ParseFileByLines(filename, cb);

  if (state.bad) {
    *error_message = state.reason;
    *error_message = *error_message + ": \"" + *state.bad_line + "\"";
    return NULL;
  }
  if (graph == NULL) {
    *error_message = "empty graph description";
    return NULL;
  }
  if (FLAGS_assignment_optimize_layout) {
    assignment->OptimizeGraphLayout(graph);
  }
  *error_message = "";
  // Return a handle on the graph to the caller so the caller can free
  // the graph's memory, because the LinearSumAssignment object does
  // not take ownership of the graph and hence will not free it.
  *graph_handle = graph;
  return assignment;
}

}  // namespace operations_research
