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

#include "util/graph_export.h"

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "base/file.h"

namespace operations_research {

GraphExporter::~GraphExporter() {}

const char GraphExporter::kGreen[] = "#A2CD5A";
const char GraphExporter::kWhite[] = "#FAFAFA";
const char GraphExporter::kBlue[] = "#87CEFA";
const char GraphExporter::kYellow[] = "#FFF68F";
const char GraphExporter::kRed[] = "#A52A2A";

namespace {
// Graph exporter that will write to a file with a given format.
class FileGraphExporter : public GraphExporter {
 public:
  FileGraphExporter(File* const file, GraphExporter::GraphFormat format)
      : file_(file), format_(format) {}
  virtual ~FileGraphExporter() {}

  void Write(const string& string) {
    file_->Write(string.c_str(), string.size());
  }

// Write node in GML or DOT format.
  virtual void WriteNode(const string& name,
                         const string& label,
                         const string& shape,
                         const string& color) {
    if (format_ == GML_FORMAT) {
      Write(StringPrintf("  node [\n"
                         "    name \"%s\"\n"
                         "    label \"%s\"\n"
                         "    graphics [\n"
                         "      type \"%s\"\n"
                         "      fill \"%s\"\n"
                         "    ]\n"
                         "  ]\n",
                         name.c_str(),
                         label.c_str(),
                         shape.c_str(),
                         color.c_str()));
    } else if (format_ == DOT_FORMAT) {
      Write(StringPrintf("%s [shape=%s label=\"%s\" color=%s]\n",
                         name.c_str(),
                         shape.c_str(),
                         label.c_str(),
                         color.c_str()));
    }
  }

  // Adds one link in the generated graph.
  virtual void WriteLink(const string& source,
                         const string& destination,
                         const string& label) {
    switch (format_) {
      case DOT_FORMAT: {
        Write(StringPrintf("%s -- %s [label=%s]\n",
                           source.c_str(),
                           destination.c_str(),
                           label.c_str()));
        break;
      }
      case GML_FORMAT: {
        Write(StringPrintf("  edge [\n"
                           "    label \"%s\"\n"
                           "    source \"%s\"\n"
                           "    target \"%s\"\n"
                           "  ]\n",
                           label.c_str(),
                           source.c_str(),
                           destination.c_str()));
        break;
      }
      default:
        break;
    }
  }

  virtual void WriteHeader(const string& name) {
    switch (format_) {
      case DOT_FORMAT: {
        Write(StringPrintf("graph %s {\n", name.c_str()));
        break;
      }
      case GML_FORMAT: {
        Write(StringPrintf("graph [\n  name \"%s\"\n", name.c_str()));
        break;
      }
      default:
        break;
    }
  }

  virtual void WriteFooter() {
    switch (format_) {
      case DOT_FORMAT: {
        Write("}\n");
        break;
      }
      case GML_FORMAT: {
        Write("]\n");
        break;
      }
      default:
        break;
    }
  }

 private:
  File* const file_;
  const GraphFormat format_;
};
}  // namespace

GraphExporter* GraphExporter::MakeFileExporter(
    File* const file,
    GraphExporter::GraphFormat format) {
  return new FileGraphExporter(file, format);
}
}  // namespace operations_research
