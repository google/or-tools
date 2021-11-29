// Copyright 2010-2021 Google LLC
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

#include "ortools/scheduling/jobshop_scheduling_parser.h"

#include <cmath>
#include <cstdint>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "google/protobuf/wrappers.pb.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/filelineiter.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/scheduling/jobshop_scheduling.pb.h"

ABSL_FLAG(int64_t, jssp_scaling_up_factor, 100000L,
          "Scaling factor for floating point penalties.");

namespace operations_research {
namespace scheduling {
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
      case PSS: {
        ProcessPssLine(line);
        break;
      }
      case EARLY_TARDY: {
        ProcessEarlyTardyLine(line);
        break;
      }
      default: {
        LOG(FATAL) << "Should not be here.";
        break;
      }
    }
  }
  return parser_state_ != PARSING_ERROR;
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
      } else if (words.size() == 1 && words[0] == "1") {
        problem_type_ = PSS;
      } else if (words.size() == 2) {
        SetJobs(strtoint32(words[0]));
        SetMachines(strtoint32(words[1]));
        problem_type_ = EARLY_TARDY;
        parser_state_ = JOB_COUNT_READ;
      }
      break;
    }
    case NAME_READ: {
      if (words.size() == 2) {
        SetJobs(strtoint32(words[0]));
        SetMachines(strtoint32(words[1]));
        problem_.set_makespan_cost_per_time_unit(1L);
        parser_state_ = JOB_COUNT_READ;
      }
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_GE(words.size(), declared_machine_count_ * 2);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int i = 0; i < declared_machine_count_; ++i) {
        const int machine_id = strtoint32(words[2 * i]);
        const int64_t duration = strtoint64(words[2 * i + 1]);
        Task* const task = job->add_tasks();
        task->add_machine(machine_id);
        task->add_duration(duration);
      }
      if (words.size() == declared_machine_count_ * 2 + 3) {
        // Early Tardy problem in JET format.
        const int due_date = strtoint32(words[declared_machine_count_ * 2]);
        const int early_cost =
            strtoint32(words[declared_machine_count_ * 2 + 1]);
        const int late_cost =
            strtoint32(words[declared_machine_count_ * 2 + 2]);
        job->set_early_due_date(due_date);
        job->set_late_due_date(due_date);
        job->set_earliness_cost_per_time_unit(early_cost);
        job->set_lateness_cost_per_time_unit(late_cost);
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
      if (words.size() == 1 && strtoint32(words[0]) > 0) {
        parser_state_ = JOB_COUNT_READ;
        SetJobs(strtoint32(words[0]));
      }
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_EQ(1, words.size());
      SetMachines(strtoint32(words[0]));
      problem_.set_makespan_cost_per_time_unit(1L);
      parser_state_ = MACHINE_COUNT_READ;
      break;
    }
    case MACHINE_COUNT_READ: {
      CHECK_EQ(1, words.size());
      const int seed = strtoint32(words[0]);
      problem_.set_seed(seed);
      parser_state_ = SEED_READ;
      break;
    }
    case SEED_READ:
      ABSL_FALLTHROUGH_INTENDED;
    case JOB_READ: {
      CHECK_EQ(1, words.size());
      current_job_index_ = strtoint32(words[0]);
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
        const int64_t duration = strtoint64(words[i]);
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
      SetJobs(strtoint32(words[0]));
      SetMachines(strtoint32(words[1]));
      problem_.set_makespan_cost_per_time_unit(1L);
      parser_state_ = JOB_COUNT_READ;
      break;
    }
    case JOB_COUNT_READ: {
      const int operations_count = strtoint32(words[0]);
      int index = 1;
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int operation = 0; operation < operations_count; ++operation) {
        const int alternatives_count = strtoint32(words[index++]);
        Task* const task = job->add_tasks();
        for (int alt = 0; alt < alternatives_count; alt++) {
          // Machine id are 1 based.
          const int machine_id = strtoint32(words[index++]) - 1;
          const int64_t duration = strtoint64(words[index++]);
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
        SetJobs(strtoint32(words[0]));
        SetMachines(strtoint32(words[1]));
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
        const int machine_id = strtoint32(words[2 * i]);
        const int64_t duration = strtoint64(words[2 * i + 1]);
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
        const int64_t t = strtoint64(w);
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
      SetJobs(strtoint32(words[0]));
      SetMachines(strtoint32(words[1]));
      parser_state_ = JOB_COUNT_READ;
      current_job_index_ = 0;
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_GE(words.size(), 6);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      const int64_t est = strtoint64(words[0]);
      if (est != 0L) {
        job->mutable_earliest_start()->set_value(est);
      }
      job->set_late_due_date(strtoint64(words[1]));
      const double weight = std::stod(words[2]);
      const int64_t tardiness = static_cast<int64_t>(
          round(weight * absl::GetFlag(FLAGS_jssp_scaling_up_factor)));
      job->set_lateness_cost_per_time_unit(tardiness);
      const int num_operations = strtoint32(words[3]);
      for (int i = 0; i < num_operations; ++i) {
        const int machine_id = strtoint32(words[4 + 2 * i]) - 1;  // 1 based.
        const int64_t duration = strtoint64(words[5 + 2 * i]);
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
                  absl::GetFlag(FLAGS_jssp_scaling_up_factor) !=
              0) {
            all_integral = false;
            break;
          }
        }
        if (all_integral) {
          for (Job& job : *problem_.mutable_jobs()) {
            job.set_lateness_cost_per_time_unit(
                job.lateness_cost_per_time_unit() /
                absl::GetFlag(FLAGS_jssp_scaling_up_factor));
          }
        } else {
          problem_.mutable_scaling_factor()->set_value(
              1.0L / absl::GetFlag(FLAGS_jssp_scaling_up_factor));
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

void JsspParser::ProcessPssLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  switch (parser_state_) {
    case START: {
      problem_.set_makespan_cost_per_time_unit(1L);
      CHECK_EQ(1, words.size());
      SetJobs(strtoint32(words[0]));
      parser_state_ = JOB_COUNT_READ;
      break;
    }
    case JOB_COUNT_READ: {
      CHECK_EQ(1, words.size());
      SetMachines(strtoint32(words[0]));
      parser_state_ = MACHINE_COUNT_READ;
      current_job_index_ = 0;
      break;
    }
    case MACHINE_COUNT_READ: {
      CHECK_EQ(1, words.size());
      CHECK_EQ(declared_machine_count_, strtoint32(words[0]));
      if (++current_job_index_ == declared_job_count_) {
        parser_state_ = JOB_LENGTH_READ;
        current_job_index_ = 0;
        current_machine_index_ = 0;
      }
      break;
    }
    case JOB_LENGTH_READ: {
      CHECK_EQ(4, words.size());
      CHECK_EQ(0, strtoint32(words[2]));
      CHECK_EQ(0, strtoint32(words[3]));
      const int machine_id = strtoint32(words[0]) - 1;
      const int duration = strtoint32(words[1]);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      Task* const task = job->add_tasks();
      task->add_machine(machine_id);
      task->add_duration(duration);
      if (++current_machine_index_ == declared_machine_count_) {
        current_machine_index_ = 0;
        if (++current_job_index_ == declared_job_count_) {
          current_job_index_ = -1;
          current_machine_index_ = 0;
          parser_state_ = JOBS_READ;
          transition_index_ = 0;
          for (int m = 0; m < declared_machine_count_; ++m) {
            Machine* const machine = problem_.mutable_machines(m);
            for (int i = 0; i < declared_job_count_ * declared_job_count_;
                 ++i) {
              machine->mutable_transition_time_matrix()->add_transition_time(0);
            }
          }
        }
      }
      break;
    }
    case JOBS_READ: {
      CHECK_EQ(1, words.size());
      const int index = transition_index_++;
      const int size = declared_job_count_ * declared_machine_count_ + 1;
      const int t1 = index / size;
      const int t2 = index % size;
      if (t1 == 0 || t2 == 0) {  // Dummy task.
        break;
      }
      const int item1 = t1 - 1;
      const int item2 = t2 - 1;
      const int job1 = item1 / declared_machine_count_;
      const int task1 = item1 % declared_machine_count_;
      const int m1 = problem_.jobs(job1).tasks(task1).machine(0);
      const int job2 = item2 / declared_machine_count_;
      const int task2 = item2 % declared_machine_count_;
      const int m2 = problem_.jobs(job2).tasks(task2).machine(0);
      if (m1 != m2) {  // We are only interested in same machine transitions.
        break;
      }
      const int transition = strtoint32(words[0]);
      Machine* const machine = problem_.mutable_machines(m1);
      machine->mutable_transition_time_matrix()->set_transition_time(
          job1 * declared_job_count_ + job2, transition);
      if (transition_index_ == size * size) {
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

void JsspParser::ProcessEarlyTardyLine(const std::string& line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, ' ', absl::SkipEmpty());
  switch (parser_state_) {
    case JOB_COUNT_READ: {
      CHECK_EQ(words.size(), declared_machine_count_ * 2 + 3);
      Job* const job = problem_.mutable_jobs(current_job_index_);
      for (int i = 0; i < declared_machine_count_; ++i) {
        const int machine_id = strtoint32(words[2 * i]);
        const int64_t duration = strtoint64(words[2 * i + 1]);
        Task* const task = job->add_tasks();
        task->add_machine(machine_id);
        task->add_duration(duration);
      }
      // Early Tardy problem in JET format.
      const int due_date = strtoint32(words[declared_machine_count_ * 2]);
      const int early_cost = strtoint32(words[declared_machine_count_ * 2 + 1]);
      const int late_cost = strtoint32(words[declared_machine_count_ * 2 + 2]);
      job->set_early_due_date(due_date);
      job->set_late_due_date(due_date);
      job->set_earliness_cost_per_time_unit(early_cost);
      job->set_lateness_cost_per_time_unit(late_cost);
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

int JsspParser::strtoint32(const std::string& word) {
  int result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}

int64_t JsspParser::strtoint64(const std::string& word) {
  int64_t result;
  CHECK(absl::SimpleAtoi(word, &result));
  return result;
}

}  // namespace jssp
}  // namespace scheduling
}  // namespace operations_research
