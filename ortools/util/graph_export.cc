// Copyright 2010-2021 Google LLC
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

#include "ortools/util/graph_export.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "ortools/base/file.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

GraphExporter::~GraphExporter() {}

namespace {
class GraphSyntax {
 public:
  virtual ~GraphSyntax() {}

  // Node in the right syntax.
  virtual std::string Node(const std::string& name, const std::string& label,
                           const std::string& shape,
                           const std::string& color) = 0;
  // Adds one link in the generated graph.
  virtual std::string Link(const std::string& source,
                           const std::string& destination,
                           const std::string& label) = 0;
  // File header.
  virtual std::string Header(const std::string& name) = 0;

  // File footer.
  virtual std::string Footer() = 0;
};

class DotSyntax : public GraphSyntax {
 public:
  ~DotSyntax() override {}

  std::string Node(const std::string& name, const std::string& label,
                   const std::string& shape,
                   const std::string& color) override {
    return absl::StrFormat("%s [shape=%s label=\"%s\" color=%s]\n", name, shape,
                           label, color);
  }

  // Adds one link in the generated graph.
  std::string Link(const std::string& source, const std::string& destination,
                   const std::string& label) override {
    return absl::StrFormat("%s -> %s [label=%s]\n", source, destination, label);
  }

  // File header.
  std::string Header(const std::string& name) override {
    return absl::StrFormat("graph %s {\n", name);
  }

  // File footer.
  std::string Footer() override { return "}\n"; }
};

class GmlSyntax : public GraphSyntax {
 public:
  ~GmlSyntax() override {}

  std::string Node(const std::string& name, const std::string& label,
                   const std::string& shape,
                   const std::string& color) override {
    return absl::StrFormat(
        "  node [\n"
        "    name \"%s\"\n"
        "    label \"%s\"\n"
        "    graphics [\n"
        "      type \"%s\"\n"
        "      fill \"%s\"\n"
        "    ]\n"
        "  ]\n",
        name, label, shape, color);
  }

  // Adds one link in the generated graph.
  std::string Link(const std::string& source, const std::string& destination,
                   const std::string& label) override {
    return absl::StrFormat(
        "  edge [\n"
        "    label \"%s\"\n"
        "    source \"%s\"\n"
        "    target \"%s\"\n"
        "  ]\n",
        label, source, destination);
  }

  // File header.
  std::string Header(const std::string& name) override {
    return absl::StrFormat(
        "graph [\n"
        "  name \"%s\"\n",
        name);
  }

  // File footer.
  std::string Footer() override { return "]\n"; }
};

// Graph exporter that will write to a file with a given format.
// Takes ownership of the GraphSyntax parameter.
class FileGraphExporter : public GraphExporter {
 public:
  FileGraphExporter(File* const file, GraphSyntax* const syntax)
      : file_(file), syntax_(syntax) {}

  ~FileGraphExporter() override {}

  // Write node in GML or DOT format.
  void WriteNode(const std::string& name, const std::string& label,
                 const std::string& shape, const std::string& color) override {
    Append(syntax_->Node(name, label, shape, color));
  }

  // Adds one link in the generated graph.
  void WriteLink(const std::string& source, const std::string& destination,
                 const std::string& label) override {
    Append(syntax_->Link(source, destination, label));
  }

  void WriteHeader(const std::string& name) override {
    Append(syntax_->Header(name));
  }

  void WriteFooter() override { Append(syntax_->Footer()); }

 private:
  void Append(const std::string& str) {
    file::WriteString(file_, str, file::Defaults()).IgnoreError();
  }

  File* const file_;
  std::unique_ptr<GraphSyntax> syntax_;
};
}  // namespace

GraphExporter* GraphExporter::MakeFileExporter(
    File* const file, GraphExporter::GraphFormat format) {
  GraphSyntax* syntax = nullptr;
  switch (format) {
    case GraphExporter::DOT_FORMAT: {
      syntax = new DotSyntax();
      break;
    }
    case GraphExporter::GML_FORMAT: {
      syntax = new GmlSyntax();
      break;
    }
    default:
      LOG(FATAL) << "Unknown graph format";
  }
  CHECK(syntax != nullptr);
  return new FileGraphExporter(file, syntax);
}
}  // namespace operations_research
