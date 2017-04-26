// Copyright 2010-2014 Google
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


#ifndef OR_TOOLS_UTIL_XML_HELPER_H_
#define OR_TOOLS_UTIL_XML_HELPER_H_

#include <stack>
#include <string>
#include <utility>
#include "ortools/base/macros.h"

namespace operations_research {

// Lightweight XML writer optimized for CPViz output. As it supports only
// the features required by CPViz, it should not be used to generate
// general-purpose XML data.
class XmlHelper {
 public:
  XmlHelper();

  // Starts a new XML document.
  void StartDocument();

  // Starts a new element.
  void StartElement(const std::string& name);

  // Adds a key-value pair to the current element.
  void AddAttribute(const std::string& key, int value);

  // Adds a key-value pair to the current element.
  void AddAttribute(const std::string& key, const std::string& value);

  // Ends the current element and goes back to the previous element.
  void EndElement();

  // Ends the document.
  void EndDocument();

  // Returns the XML content written so far.
  const std::string& GetContent() const;

 private:
  typedef std::pair<char, std::string> EscapePair;
  std::string content_;
  std::stack<std::string> tags_;
  bool direction_down_;

  DISALLOW_COPY_AND_ASSIGN(XmlHelper);
};
}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_XML_HELPER_H_
