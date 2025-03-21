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

#include <cstdlib>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/options.h"
#include "ortools/routing/parsers/solomon_parser.h"
#include "ortools/sat/routes_support_graph.pb.h"
#include "ortools/util/file_util.h"

ABSL_FLAG(std::string, input, "",
          "Name of the file containing the input data of the problem, in"
          " Solomon format.");
ABSL_FLAG(std::string, support_graph, "",
          "Name of a RoutesSupportGraphProto file for this problem.");
ABSL_FLAG(std::string, output, "", "Name of the output DOT file.");

namespace operations_research {
namespace sat {
namespace {

void Run() {
  if (absl::GetFlag(FLAGS_input).empty()) {
    LOG(QFATAL) << "Please supply a solomon input file with --input=";
  }
  if (absl::GetFlag(FLAGS_support_graph).empty()) {
    LOG(QFATAL) << "Please supply a support graph file with --support_graph=";
  }
  if (absl::GetFlag(FLAGS_output).empty()) {
    LOG(QFATAL) << "Please supply a DOT output file with --output=";
  }

  routing::SolomonParser parser;
  CHECK(parser.LoadFile(absl::GetFlag(FLAGS_input)));
  RoutesSupportGraphProto support_graph;
  CHECK_OK(ReadFileToProto(absl::GetFlag(FLAGS_support_graph), &support_graph));

  std::string dot = "digraph {\n";
  absl::StrAppend(&dot, "  graph [splines=\"true\"];\n");
  const auto& coordinates = parser.coordinates();
  for (int i = 0; i < coordinates.size(); ++i) {
    absl::StrAppend(&dot, "  ", i, " [label=", i, " pos=\"", coordinates[i].x,
                    ",", coordinates[i].y, "!\"];\n");
  }
  for (const auto& arc : support_graph.arc_lp_values()) {
    absl::StrAppend(&dot, "  ", arc.tail(), " -> ", arc.head(), " [label=\"",
                    arc.lp_value(), "\"];\n");
  }
  absl::StrAppend(&dot, "}\n");
  CHECK_OK(
      file::SetContents(absl::GetFlag(FLAGS_output), dot, file::Defaults()));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research

static const char kUsage[] =
    "Usage: see flags.\nThis utility converts a RoutesSupportGraphProto file "
    "to a DOT file, using the node coordinates from the Solomon input file. It "
    "assumes that the cut file was generated with "
    "//ortools/bench/solomon:solomon_run with the "
    "--cp_model_dump_routes_support_graphs flag.";

int main(int argc, char** argv) {
  InitGoogle(kUsage, &argc, &argv, /*remove_flags=*/true);
  operations_research::sat::Run();
  return EXIT_SUCCESS;
}
