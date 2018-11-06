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

#include "ortools/data/jobshop_scheduling_parser.h"

#include <cmath>

#include "google/protobuf/wrappers.pb.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/filelineiter.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/split.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/data/jobshop_scheduling.pb.h"

DEFINE_int64(jssp_scaling_up_factor, 100000L,
             "Scaling factor for floating point penalties.");

namespace operations_research {
namespace data {
namespace jssp {

void JsspParser::SetJobs(int job_count) {
  CHECK_GT(job_count, 0);
  declared_job_count_ = job_count;
  problem_.clear_jobs();
  for (int i = 0; i < job_count; ++i) {
    problem_.add_jobs()->set_name(absl::StrCat("J", i));
  }
}

void JsspParser::SetMachines(int machine_count) {
  CHECK_GT(machine_count, 0);
  declared_machine_count_ = machine_count;
  problem_.clear_machines();
  for (int i = 0; i < machine_count; ++i) {
    problem_.add_machines()->set_name(absl::StrCat("M", i));
  }
}

bool JsspParser::ParseFile(const std::string& filename) {
  problem_.Clear();
  // Try to detect the type of the data file.
  //  - fjs suffix -> Flexible Jobshop
  //  - txt suffix -> Taillard or time dependent scheduling.
  if (absl::EndsWith(filename, "fjs")) {
    problem_type_ = FLEXIBLE;
  } else if (absl::EndsWith(filename, ".txt")) {
    problem_type_ = TAILLARD;
  } else {
    problem_type_ = JSSP;
  }
  for (const std::string& line : FileLines(filename)) {
    if (line.empty()) {
      continue;
    }
    switch (problem_type_) {
      case JSSP: {
        ProcessJsspLine(line);
        break;
      }
      case TAILLARD: {
        ProcessTaillardLine(line);
        break;
      }
      case FLEXIBLE: {
        ProcessFlexibleLine(line);
        break;
      }
      case SDST: {
        ProcessSdstLine(line);
        break;
      }
      case TARDINESS: {
        ProcessTardinessLine(line);
        break;
      }
      default: {
        LOG(FATAL) << "Should not be here.";
        break;
      }
    }
  }
  return parser_state_ != ERROR;
}

void JsspParser::ProcessJsspLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  switch (parser_state_) {
    case START: {
      if (words.size() == 2 && words[0] == "instance") {
        problem_.set_name(words[1]);
        parser_state_ = NAME_READ;
        current_job_index_ = 0;
      }
      break;
    }
    case NAME_READ: {
      if (words.size() == 2) {
        SetJobs(atoi32(words[0]));
        SetMachines(atoi32(words[1]));
        problem_.set_makespan_cost_per_time_unit(1L);
        parser_state_ = JOB_COUNT_READ;
      }
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_EQ(words.size(), declared_machine_count_ * 2);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int i = 0; i < declared_machine_count_; ++i) {
        const int machine_id = atoi32(words[2 * i]);
        const int64 duration = atoi64(words[2 * i + 1]);
        Task* const task = job->add_tasks();
        task->add_machine(machine_id);
        task->add_duration(duration);
      }
      current_job_index_++;
      if (current_job_index_ == declared_job_count_) {
        parser_state_ = DONE;
      }
      break;
    }
    default: {
      LOG(FATAL) << "Should not be here with state " << parser_state_;
    }
  }
}

void JsspParser::ProcessTaillardLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());

  switch (parser_state_) {
    case START: {
      if (words.size() == 2) {  // Switch to SDST parser.
        problem_type_ = SDST;
        ProcessSdstLine(line);
        return;
      } else if (words.size() == 3) {  // Switch to TARDINESS parser.
        problem_type_ = TARDINESS;
        ProcessTardinessLine(line);
        return;
      }
      if (words.size() == 1 && atoi32(words[0]) > 0) {
        parser_state_ = JOB_COUNT_READ;
        SetJobs(atoi32(words[0]));
      }
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_EQ(1, words.size());
      SetMachines(atoi32(words[0]));
      problem_.set_makespan_cost_per_time_unit(1L);
      parser_state_ = MACHINE_COUNT_READ;
      break;
    }
    case MACHINE_COUNT_READ: {
      CHECK_EQ(1, words.size());
      const int seed = atoi32(words[0]);
      problem_.set_seed(seed);
      parser_state_ = SEED_READ;
      break;
    }
    case SEED_READ:
      ABSL_FALLTHROUGH_INTENDED;
    case JOB_READ: {
      CHECK_EQ(1, words.size());
      current_job_index_ = atoi32(words[0]);
      parser_state_ = JOB_ID_READ;
      break;
    }
    case JOB_ID_READ: {
      CHECK_EQ(1, words.size());
      parser_state_ = JOB_LENGTH_READ;
      break;
    }
    case JOB_LENGTH_READ: {
      CHECK_EQ(declared_machine_count_, words.size());
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int i = 0; i < declared_machine_count_; ++i) {
        const int64 duration = atoi64(words[i]);
        Task* const task = job->add_tasks();
        task->add_machine(i);
        task->add_duration(duration);
      }
      parser_state_ =
          current_job_index_ == declared_job_count_ - 1 ? DONE : JOB_READ;
      break;
    }
    default: {
      LOG(FATAL) << "Should not be here with state " << parser_state_;
    }
  }
}
void JsspParser::ProcessFlexibleLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  switch (parser_state_) {
    case START: {
      CHECK_GE(words.size(), 2);
      SetJobs(atoi32(words[0]));
      SetMachines(atoi32(words[1]));
      problem_.set_makespan_cost_per_time_unit(1L);
      parser_state_ = JOB_COUNT_READ;
      break;
    }
    case JOB_COUNT_READ: {
      const int operations_count = atoi32(words[0]);
      int index = 1;
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int operation = 0; operation < operations_count; ++operation) {
        const int alternatives_count = atoi32(words[index++]);
        Task* const task = job->add_tasks();
        for (int alt = 0; alt < alternatives_count; alt++) {
          // Machine id are 1 based.
          const int machine_id = atoi32(words[index++]) - 1;
          const int64 duration = atoi64(words[index++]);
          task->add_machine(machine_id);
          task->add_duration(duration);
        }
      }
      CHECK_LE(index, words.size());  // Ignore CR at the end of the line.
      current_job_index_++;
      if (current_job_index_ == declared_job_count_) {
        parser_state_ = DONE;
      }
      break;
    }
    default: {
      LOG(FATAL) << "Should not be here with state " << parser_state_;
    }
  }
}
void JsspParser::ProcessSdstLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  switch (parser_state_) {
    case START: {
      if (words.size() == 2) {
        SetJobs(atoi32(words[0]));
        SetMachines(atoi32(words[1]));
        problem_.set_makespan_cost_per_time_unit(1L);
        parser_state_ = JOB_COUNT_READ;
        current_machine_index_ = 0;
      }
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_EQ(words.size(), declared_machine_count_ * 2);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int i = 0; i < declared_machine_count_; ++i) {
        const int machine_id = atoi32(words[2 * i]);
        const int64 duration = atoi64(words[2 * i + 1]);
        Task* const task = job->add_tasks();
        task->add_machine(machine_id);
        task->add_duration(duration);
      }
      current_job_index_++;
      if (current_job_index_ == declared_job_count_) {
        parser_state_ = JOBS_READ;
      }
      break;
    }
    case JOBS_READ: {
      CHECK_EQ(1, words.size());
      CHECK_EQ("SSD", words[0]);
      parser_state_ = SSD_READ;
      break;
    }
    case SSD_READ: {
      CHECK_EQ(1, words.size());
      CHECK_EQ(words[0], absl::StrCat("M", current_machine_index_)) << line;
      current_job_index_ = 0;
      parser_state_ = MACHINE_READ;
      break;
    }
    case MACHINE_READ: {
      CHECK_EQ(declared_job_count_, words.size());
      Machine* const machine =
          problem_.mutable_machines(current_machine_index_);
      for (const std::string& w : words) {
        const int64 t = atoi64(w);
        machine->mutable_transition_time_matrix()->add_transition_time(t);
      }
      if (++current_job_index_ == declared_job_count_) {
        parser_state_ = ++current_machine_index_ == declared_machine_count_
                            ? DONE
                            : SSD_READ;
      }
      break;
    }
    default: {
      LOG(FATAL) << "Should not be here with state " << parser_state_
                 << "with line " << line;
    }
  }
}

void JsspParser::ProcessTardinessLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  switch (parser_state_) {
    case START: {
      CHECK_EQ(3, words.size());
      SetJobs(atoi32(words[0]));
      SetMachines(atoi32(words[1]));
      parser_state_ = JOB_COUNT_READ;
      current_job_index_ = 0;
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_GE(words.size(), 6);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      const int64 est = atoi64(words[0]);
      if (est != 0L) {
        job->mutable_earliest_start()->set_value(est);
      }
      job->set_late_due_date(atoi64(words[1]));
      const double weight = stod(words[2]);
      const int64 tardiness =
          static_cast<int64>(round(weight * FLAGS_jssp_scaling_up_factor));
      job->set_lateness_cost_per_time_unit(tardiness);
      const int num_operations = atoi32(words[3]);
      for (int i = 0; i < num_operations; ++i) {
        const int machine_id = atoi32(words[4 + 2 * i]) - 1;  // 1 based.
        const int64 duration = atoi64(words[5 + 2 * i]);
        Task* const task = job->add_tasks();
        task->add_machine(machine_id);
        task->add_duration(duration);
      }
      current_job_index_++;
      if (current_job_index_ == declared_job_count_) {
        // Fix tardiness weights if all integer from start.
        bool all_integral = true;
        for (const Job& job : problem_.jobs()) {
          if (job.lateness_cost_per_time_unit() %
                  FLAGS_jssp_scaling_up_factor !=
              0) {
            all_integral = false;
            break;
          }
        }
        if (all_integral) {
          for (Job& job : *problem_.mutable_jobs()) {
            job.set_lateness_cost_per_time_unit(
                job.lateness_cost_per_time_unit() /
                FLAGS_jssp_scaling_up_factor);
          }
        } else {
          problem_.mutable_scaling_factor()->set_value(
              1.0L / FLAGS_jssp_scaling_up_factor);
        }
        parser_state_ = DONE;
      }
      break;
    }
    default: {
      LOG(FATAL) << "Should not be here with state " << parser_state_
                 << "with line " << line;
    }
  }
}
}  // namespace jssp
}  // namespace data
}  // namespace operations_research
