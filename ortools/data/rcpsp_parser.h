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

// A Project Scheduling Library parser.
// See: http://www.om-db.wi.tum.de/psplib/  # PSP-Lib homepage.

#ifndef OR_TOOLS_DATA_RCPSP_PARSER_H_
#define OR_TOOLS_DATA_RCPSP_PARSER_H_

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/data/rcpsp.pb.h"

namespace operations_research {
namespace data {
namespace rcpsp {

// RCPSP parser.
// Parse a RCPSP problem and load it into a RcpspProblem proto.
// See description of the problem in ./rcpsp.proto
class RcpspParser {
 public:
  RcpspParser();

  // We keep the fully qualified name for swig.
  ::operations_research::data::rcpsp::RcpspProblem problem() const {
    return rcpsp_;
  }

  // Returns false if an error occurred.
  bool ParseFile(const std::string& file_name);

 private:
  enum LoadStatus {
    NOT_STARTED,
    HEADER_SECTION,
    PROJECT_SECTION,
    INFO_SECTION,
    PRECEDENCE_SECTION,
    REQUEST_SECTION,
    RESOURCE_SECTION,
    RESOURCE_MIN_SECTION,
    PARSING_FINISHED,
    ERROR_FOUND
  };

  void ProcessRcpspLine(const std::string& line);
  void ProcessPattersonLine(const std::string& line);
  void ProcessRcpspMaxLine(const std::string& line);
  void ReportError(const std::string& line);
  // Sets the number of declared tasks, and initialize data structures
  // accordingly.
  void SetNumDeclaredTasks(int t);
  int strtoint32(const std::string& word);
  int64 strtoint64(const std::string& word);

  std::string basedata_;
  int64 seed_;
  LoadStatus load_status_;
  int num_declared_tasks_;
  int current_task_;
  std::vector<std::vector<int> > temp_delays_;
  std::vector<int> recipe_sizes_;
  int unreads_;
  RcpspProblem rcpsp_;
};

}  // namespace rcpsp
}  // namespace data
}  // namespace operations_research

#endif  // OR_TOOLS_DATA_RCPSP_PARSER_H_
