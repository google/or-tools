// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_UTIL_GRAPH_EXPORT_H_
#define OR_TOOLS_UTIL_GRAPH_EXPORT_H_

#include <string>

#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {
// ----- Export to graph file -----

// This class will allow the creation of a graph representation.
// It supports different formats (see GraphFormat).
class GraphExporter {
 public:
  // Supported graph formats.
  enum GraphFormat {
    DOT_FORMAT,
    GML_FORMAT,
  };

  virtual ~GraphExporter();

  // Write the header of the graph file.
  virtual void WriteHeader(const std::string& name) = 0;

  // Write the footer of the graph file.
  virtual void WriteFooter() = 0;

  // Write node in GML or DOT format.
  virtual void WriteNode(const std::string& name, const std::string& label,
                         const std::string& shape,
                         const std::string& color) = 0;

  // Adds one link in the generated graph.
  virtual void WriteLink(const std::string& source,
                         const std::string& destination,
                         const std::string& label) = 0;

  // Creates a graph exporter that will write to file with a given format.
  static GraphExporter* MakeFileExporter(File* const file,
                                         GraphExporter::GraphFormat format);
};
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_GRAPH_EXPORT_H_
