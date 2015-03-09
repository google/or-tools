// Copyright 2011-2014 Google
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
//
//  Common base to use the epix library to visualize
//  routing data (instance) and solutions.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_EPIX_HELPER_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_EPIX_HELPER_H

#include <memory>

#include "constraint_solver/routing.h"
#include "base/join.h"

#include "routing_common/routing_common.h"

namespace operations_research {

class RoutingEpixHelper {
public:
  explicit RoutingEpixHelper(std::ostream & out) : out_(&out) {}
  void SetOuptutStream(std::ostream & out) {
    out_ = &out;
  }
private:
  std::ostream * out_;
};
  
  void PrintEpixBeginFile(std::ostream & out) {
    out << "#include \"epix.h\"" << std::endl;
    out << "using namespace ePiX;" << std::endl;
    out << std::endl;
    out << "int main(int argc, char **argv)" << std::endl;
    out << "{" << std::endl;    
  }
  
  void PrintEpixPreamble(std::ostream & out) {
    out << std::endl;
    out << "unitlength(\"1cm\");" << std::endl;
    out << "picture(" << FLAGS_epix_width << "," << FLAGS_epix_height << ");" << std::endl;
    out << "double radius = " << FLAGS_epix_radius << ";" << std::endl;
    out << "font_size(\"tiny\");" << std::endl;
  }

  void PrintEpixBoundingBox(std::ostream & out, const BoundingBox & P) {
    out << "bounding_box(P(" << P.min_x << "," << P.min_y << "), P(" << P.max_x << "," << P.max_y << "));" << std::endl;
  }


  void PrintEpixBeginFigure(std::ostream & out) {
    out << std::endl;
    out << "begin(); // ---- Figure body starts here ----" << std::endl;
  }

  void PrintEpixEndFigure(std::ostream & out) {
    out << std::endl;
    out << "end(); // ---- End figure; write output file ----" << std::endl;
  }

  void PrintEpixEndFile(std::ostream & out) {
    out << std::endl;
    out << "}" << std::endl;
  }

  void PrintEpixNewLine(std::ostream & out) {
    out << std::endl;
  }

  void PrintEpixRaw(std::ostream & out, const char* s) {
    out << s << std::endl;
  }
  
  void PrintEpixComment(std::ostream & out, const char* s) {
    out << "  //  " << s << std::endl;
  }

  void PrintEpixPoint(std::ostream & out, Point p, RoutingModel::NodeIndex i) {
    out << "  P " << StrCat("P",i.value()) << "(" << p.x << "," << p.y << ");" << std::endl;
    out << StrCat("  Circle C",i.value()) << StrCat("(P",i.value()) << ", radius);" << std::endl;
  }
  
  void PrintEpixSegment(std::ostream & out, int segment_index, RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) {
    out << StrCat("  Segment L", segment_index)  <<"(P" << StrCat(i.value()) << ",P" <<
                      StrCat(j.value()) << ");" << std::endl;
  }

  void PrintEpixDepot(std::ostream & out, RoutingModel::NodeIndex d) {
    out << "  fill(Red());" << std::endl;
    out << StrCat("  C", d.value(), ".draw();") << std::endl;
    out << "  fill(White());" << std::endl;
  }

  void PrintEpixDrawMultiplePoints(std::ostream & out, int size) {
    for (int i = 0; i < size; ++i) {
      out << StrCat("  C",i) << ".draw();" << std::endl;
      if (FLAGS_epix_node_labels) {
        out << StrCat("  label (P", i) << StrCat(",P(0.2,0.1),\"",  i+1 ) << "\",tr);" << std::endl;
      }
    }
  }
  void PrintEpixArrow(std::ostream & out, RoutingModel::NodeIndex from_node, RoutingModel::NodeIndex to_node) {
    out << "arrow " <<"(P" << from_node.value() << ", P" << to_node.value() << ");" << std::endl;
  }

  void PrintEpixDrawMultipleSegments(std::ostream & out, int size) {
    for (int i = 0; i < size; ++i) {
      out << StrCat("  L",i) << ".draw();" << std::endl;
    }
  }

}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_ROUTING_EPIX_HELPER_H