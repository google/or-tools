// Copyright 2010-2013 Google
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

#include "util/graph_export.h"

#include "base/unique_ptr.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"

namespace operations_research {

GraphExporter::~GraphExporter() {}

namespace {
class GraphSyntax {
 public:
  virtual ~GraphSyntax() {}

  // Node in the right syntax.
  virtual std::string Node(const std::string& name, const std::string& label,
                      const std::string& shape, const std::string& color) = 0;
  // Adds one link in the generated graph.
  virtual std::string Link(const std::string& source, const std::string& destination,
                      const std::string& label) = 0;
  // File header.
  virtual std::string Header(const std::string& name) = 0;

  // File footer.
  virtual std::string Footer() = 0;
};

class DotSyntax : public GraphSyntax {
 public:
  virtual ~DotSyntax() {}

  virtual std::string Node(const std::string& name, const std::string& label,
                      const std::string& shape, const std::string& color) {
    return StringPrintf("%s [shape=%s label=\"%s\" color=%s]\n", name.c_str(),
                        shape.c_str(), label.c_str(), color.c_str());
  }

  // Adds one link in the generated graph.
  virtual std::string Link(const std::string& source, const std::string& destination,
                      const std::string& label) {
    return StringPrintf("%s -> %s [label=%s]\n", source.c_str(),
                        destination.c_str(), label.c_str());
  }

  // File header.
  virtual std::string Header(const std::string& name) {
    return StringPrintf("graph %s {\n", name.c_str());
  }

  // File footer.
  virtual std::string Footer() { return "}\n"; }
};

class GmlSyntax : public GraphSyntax {
 public:
  virtual ~GmlSyntax() {}

  virtual std::string Node(const std::string& name, const std::string& label,
                      const std::string& shape, const std::string& color) {
    return StringPrintf(
        "  node [\n"
        "    name \"%s\"\n"
        "    label \"%s\"\n"
        "    graphics [\n"
        "      type \"%s\"\n"
        "      fill \"%s\"\n"
        "    ]\n"
        "  ]\n",
        name.c_str(), label.c_str(), shape.c_str(), color.c_str());
  }

  // Adds one link in the generated graph.
  virtual std::string Link(const std::string& source, const std::string& destination,
                      const std::string& label) {
    return StringPrintf(
        "  edge [\n"
        "    label \"%s\"\n"
        "    source \"%s\"\n"
        "    target \"%s\"\n"
        "  ]\n",
        label.c_str(), source.c_str(), destination.c_str());
  }

  // File header.
  virtual std::string Header(const std::string& name) {
    return StringPrintf(
        "graph [\n"
        "  name \"%s\"\n",
        name.c_str());
  }

  // File footer.
  virtual std::string Footer() { return "]\n"; }
};

// Graph exporter that will write to a file with a given format.
// Takes ownership of the GraphSyntax parameter.
class FileGraphExporter : public GraphExporter {
 public:
  FileGraphExporter(File* const file, GraphSyntax* const syntax)
      : file_(file), syntax_(syntax) {}

  virtual ~FileGraphExporter() {}

  // Write node in GML or DOT format.
  virtual void WriteNode(const std::string& name, const std::string& label,
                         const std::string& shape, const std::string& color) {
    Append(syntax_->Node(name, label, shape, color));
  }

  // Adds one link in the generated graph.
  virtual void WriteLink(const std::string& source, const std::string& destination,
                         const std::string& label) {
    Append(syntax_->Link(source, destination, label));
  }

  virtual void WriteHeader(const std::string& name) {
    Append(syntax_->Header(name));
  }

  virtual void WriteFooter() { Append(syntax_->Footer()); }

 private:
  void Append(const std::string& str) { file_->Write(str.c_str(), str.size()); }

  File* const file_;
  std::unique_ptr<GraphSyntax> syntax_;
};
}  // namespace

GraphExporter* GraphExporter::MakeFileExporter(
    File* const file, GraphExporter::GraphFormat format) {
  GraphSyntax* syntax = NULL;
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
  CHECK(syntax != NULL);
  return new FileGraphExporter(file, syntax);
}
}  // namespace operations_research
