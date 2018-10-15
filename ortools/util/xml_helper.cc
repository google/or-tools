// Copyright 2010-2017 Google
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

#include "ortools/util/xml_helper.h"

#include <sstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"

namespace operations_research {

XmlHelper::XmlHelper() {}

void XmlHelper::StartDocument() {
  content_ = "<?xml version=\"1.0\"?>\n";
  direction_down_ = false;
}

void XmlHelper::StartElement(const std::string& name) {
  if (direction_down_) {
    content_.append(">\n");
  }
  tags_.push(name);
  absl::StrAppendFormat(&content_, "<%s", name);
  direction_down_ = true;
}

void XmlHelper::AddAttribute(const std::string& key, int value) {
  AddAttribute(key, absl::StrCat(value));
}

void XmlHelper::AddAttribute(const std::string& key, const std::string& value) {
  std::ostringstream escaped_value;

  for (std::string::const_iterator it = value.begin(); it != value.end();
       ++it) {
    unsigned char c = (unsigned char)*it;

    switch (c) {
      case '"':
        escaped_value << "&quot;";
        break;
      case '\'':
        escaped_value << "&apos;";
        break;
      case '<':
        escaped_value << "&lt;";
        break;
      case '>':
        escaped_value << "&gt;";
        break;
      case '&':
        escaped_value << "&amp;";
        break;
      default:
        escaped_value << c;
    }
  }

  absl::StrAppendFormat(&content_, " %s=\"%s\"", key, escaped_value.str());
}

void XmlHelper::EndElement() {
  std::string tag = tags_.top();

  if (direction_down_) {
    content_.append(" />\n");
  } else {
    absl::StrAppendFormat(&content_, "</%s>\n", tag);
  }
  direction_down_ = false;

  tags_.pop();
}

void XmlHelper::EndDocument() {}

const std::string& XmlHelper::GetContent() const { return content_; }
}  // namespace operations_research
