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

#ifndef OR_TOOLS_DATA_JOBSHOP_SCHEDULING_PARSER_H_
#define OR_TOOLS_DATA_JOBSHOP_SCHEDULING_PARSER_H_

#include "ortools/base/match.h"
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
    ERROR,
    DONE
  };

  JsspParser()
      : declared_machine_count_(-1),
        declared_job_count_(-1),
        current_job_index_(0),
        current_machine_index_(0),
        problem_type_(UNDEFINED),
        parser_state_(START) {}

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

  void SetJobs(int job_count);
  void SetMachines(int machine_count);

  JsspInputProblem problem_;
  int declared_machine_count_;
  int declared_job_count_;
  int current_job_index_;
  int current_machine_index_;
  ProblemType problem_type_;
  ParserState parser_state_;
};

}  // namespace jssp
}  // namespace data
}  // namespace operations_research

#endif  // OR_TOOLS_DATA_JOBSHOP_SCHEDULING_PARSER_H_
