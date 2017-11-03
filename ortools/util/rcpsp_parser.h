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

// A Project Scheduling Library parser.
// See: http://www.om-db.wi.tum.de/psplib/  # PSP-Lib homepage.

#ifndef OR_TOOLS_UTIL_RCPSP_PARSER_H_
#define OR_TOOLS_UTIL_RCPSP_PARSER_H_

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/util/rcpsp.pb.h"

namespace operations_research {
namespace util {
namespace rcpsp {

// RCPSP parser.
//
// The problem description is as follows:
//
// You have a set of resources. They all have a maximum capacity, and
// can be renewable or not.
//
// You have a set of tasks. Each task has a list of successors, and a
// list of recipes. Each recipe consists of a duration, and a list of
// demands, one per resource.
//
// The tasks dependencies form a DAG with a single source and a single end.
// Both source and end tasks have a zero duration, and no resource consumption.
//
// In case the problem is of type RCPSP/Max. The data contains an additional
// array of delays per task. This flattened array contains the following
// information for task i with mode mi and successor j with mode mj, then
// start(i) + delay[i, mi, j, mj] <= start(j). This subsumes the normal
// successor predecence of the non RCPSP/Max variation, i.e.:
//   start(i) + duration(i, mi) <= start(j).
//
// In the normal case, the objective is to minimize the makespan of the problem.
//
// In the resource investment problem, there is no makespan. It is
// replaced by a strict deadline, and each task must finish before
// this deadline.  In that case, resources have a unit cost, and the
// objective is to minimize the sum of resource cost.
//
// In the consumer/producer case, tasks have a zero duration, and demands can be
// negative. The constraint states that at each time point, the sum of demands
// happening before or during this time must be between the min and max
// capacity. Note that in that case, both min and max capacity can be negative.
// Furthermore, if 0 is not in [min_capacity, max_capacity], then a sufficient
// set of events must happen at time 0 such that the sum of their demands must
// fall inside the capacity interval.
//
// The supported file formats are:
//   - standard psplib (.sm and .mm):
//     http://www.om-db.wi.tum.de/psplib/data.html
//   - rcpsp problem in the patterson format (.rcp):
//     http://www.om-db.wi.tum.de/psplib/dataob.html
//   - rcpsp/max (.sch):
//     https://www.wiwi.tu-clausthal.de/de/abteilungen/produktion/forschung/
//           schwerpunkte/project-generator/rcpspmax/
//     https://www.wiwi.tu-clausthal.de/de/abteilungen/produktion/forschung/
//           schwerpunkte/project-generator/mrcpspmax/
//   - resource investment problem with max delay (.sch):
//     https://www.wiwi.tu-clausthal.de/de/abteilungen/produktion/forschung/
//           schwerpunkte/project-generator/ripmax/
class RcpspParser {
 public:
  RcpspParser();

  ::operations_research::util::rcpsp::RcpspProblem problem() const {
    return rcpsp_;
  }

  bool LoadFile(const std::string& file_name);

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

  std::string basedata_;
  int64 seed_;
  LoadStatus load_status_;
  int declared_tasks_;
  int current_task_;
  std::vector<std::vector<int>> temp_delays_;
  std::vector<int> recipe_sizes_;
  int unreads_;
  RcpspProblem rcpsp_;
};

}  // namespace rcpsp
}  // namespace util
}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_RCPSP_PARSER_H_
