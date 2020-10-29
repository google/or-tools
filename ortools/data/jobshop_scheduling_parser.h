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

#ifndef OR_TOOLS_DATA_JOBSHOP_SCHEDULING_PARSER_H_
#define OR_TOOLS_DATA_JOBSHOP_SCHEDULING_PARSER_H_

#include "absl/strings/match.h"
#include "ortools/base/integral_types.h"
#include "ortools/data/jobshop_scheduling.pb.h"

namespace operations_research {
namespace data {
namespace jssp {

class JsspParser {
 public:
  enum ProblemType {
    UNDEFINED,
    JSSP,
    TAILLARD,
    FLEXIBLE,
    SDST,
    TARDINESS,
    PSS,
    EARLY_TARDY,
  };

  enum ParserState {
    START,
    JOB_COUNT_READ,
    MACHINE_COUNT_READ,
    SEED_READ,
    JOB_ID_READ,
    JOB_LENGTH_READ,
    JOB_READ,
    NAME_READ,
    JOBS_READ,
    SSD_READ,
    MACHINE_READ,
    PARSING_ERROR,
    DONE
  };

  ~JsspParser() {}

  // Parses a file to load a jobshop problem.
  // Tries to auto detect the file format.
  bool ParseFile(const std::string& filename);

  // Returns the loaded problem.
  const JsspInputProblem& problem() const { return problem_; }

 private:
  void ProcessJsspLine(const std::string& line);
  void ProcessTaillardLine(const std::string& line);
  void ProcessFlexibleLine(const std::string& line);
  void ProcessSdstLine(const std::string& line);
  void ProcessTardinessLine(const std::string& line);
  void ProcessPssLine(const std::string& line);
  void ProcessEarlyTardyLine(const std::string& line);

  void SetJobs(int job_count);
  void SetMachines(int machine_count);
  int strtoint32(const std::string& word);
  int64 strtoint64(const std::string& word);

  JsspInputProblem problem_;
  int declared_machine_count_ = -1;
  int declared_job_count_ = -1;
  int current_job_index_ = 0;
  int current_machine_index_ = 0;
  int transition_index_ = 0;
  ProblemType problem_type_ = UNDEFINED;
  ParserState parser_state_ = START;
};

}  // namespace jssp
}  // namespace data
}  // namespace operations_research

#endif  // OR_TOOLS_DATA_JOBSHOP_SCHEDULING_PARSER_H_
