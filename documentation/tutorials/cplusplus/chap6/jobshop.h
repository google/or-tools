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
//  The JobshopData class is a simple container for job-shop problem instances.
//  It can read the JSSP and professor Taillard's instances formats.
//
//  Note that the format is only partially checked: bad inputs might
//  cause undefined behavior.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_H_
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_H_

#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strtoint.h"
#include "base/join.h"
#include "base/file.h"
#include "base/filelinereader.h"
#include "base/split.h"

namespace operations_research {

class JobShopData {
 public:
  struct Task {
    Task(int j, int m, int d) : job_id(j), machine_id(m), duration(d) {}
    int job_id;
    int machine_id;
    int duration;
  };

  enum ProblemType {
    UNDEFINED,
    JSSP,
    TAILLARD
  };

  enum TaillardState {
    START,
    JOBS_READ,
    MACHINES_READ,
    SEED_READ,
    JOB_ID_READ,
    JOB_LENGTH_READ,
    JOB_READ
  };

  explicit JobShopData(const std::string& filename) :
        name_(""),
        filename_(filename),
        machine_count_(0),
        job_count_(0),
        horizon_(0),
        current_job_index_(0),
        current_line_nbr_(0),
        problem_type_(UNDEFINED),
        taillard_state_(START),
        kWordDelimiters(" "),
        problem_numbers_defined(false) {
    FileLineReader reader(filename_.c_str());
    reader.set_line_callback(NewPermanentCallback(
        this,
        &JobShopData::ProcessNewLine));
    reader.Reload();
    if (!reader.loaded_successfully()) {
      LOG(FATAL) << "Could not open job-shop file " << filename_;
    }
  }

  ~JobShopData() {}

  int machine_count() const { return machine_count_; }

  int job_count() const { return job_count_; }

  const std::string& name() const { return name_; }

  int horizon() const { return horizon_; }

  // Returns the tasks of a job, ordered by precedence.
  const std::vector<Task>& TasksOfJob(int job_id) const {
    return all_tasks_[job_id];
  }

  void Report(std::ostream & out) {
    out << "Job-shop problem instance ";
    if (!problem_numbers_defined) {
      out << "not defined yet!" << std::endl;
      return;
    }
    out << "in " << (problem_type_ == JSSP ? "JSSP" : "TAILLARD's")
        << " format read from file " << filename_ << std::endl;
    out << "Name: " << name() << std::endl;
    out << "Jobs: " << job_count() << std::endl;
    out << "Machines: " << machine_count() << std::endl;
  }

  void ReportAll(std::ostream & out) {
    Report(out);

    out << "==========================================" << std::endl;
    for (int i = 0; i < all_tasks_.size(); ++i) {
      out << "Job: " << i << std::endl;
      for (int j = 0; j < all_tasks_[i].size(); ++j) {
        Task t = all_tasks_[i][j];
        out << "(" << t.machine_id << "," << t.duration << ") ";
      }
      out << std::endl;
    }
  }


 private:
  void ProcessNewLine(char* const line) {
    ++current_line_nbr_;
    VLOG(3) << "Line number " << current_line_nbr_;

    std::vector<std::string> words;
    words = strings::Split(line, kWordDelimiters, strings::SkipEmpty());
    //SplitStringUsing(line, kWordDelimiters, &words);
    switch (problem_type_) {
      case UNDEFINED: {
        if (words.size() == 2 && words[0] == "instance") {
          problem_type_ = JSSP;
          VLOG(1) << "Reading jssp instance " << words[1];
          name_ = words[1];
        } else if (words.size() == 1 && atoi32(words[0]) > 0) {
          problem_type_ = TAILLARD;
          VLOG(1) << "Reading Taillard instance from file " << filename_;
          name_ = StrCat("Taillard instance from file ", filename_);
          taillard_state_ = JOBS_READ;
          job_count_ = atoi32(words[0]);
          CHECK_GT(job_count_, 0);
          all_tasks_.resize(job_count_);
          problem_numbers_defined = true;
        }
        break;
      }
      case JSSP: {
        if (words.size() == 2 && !problem_numbers_defined) {
          job_count_ = atoi32(words[0]);
          machine_count_ = atoi32(words[1]);
          CHECK_GT(machine_count_, 0)
          << "Number of machines must be greater than 0 on line "
          << current_line_nbr_;
          CHECK_GT(job_count_, 0)
          << "Number of jobs must be greater than 0 on line "
          << current_line_nbr_;
          VLOG(1) << machine_count_ << " machines and "
                  << job_count_ << " jobs";
          all_tasks_.resize(job_count_);
          problem_numbers_defined = true;
          break;
        }

        if (words.size() >= 2 && problem_numbers_defined) {
          CHECK_EQ(words.size() % 2, 0)
          << "Odd number of token on line "
          << current_line_nbr_;
          VLOG(3) << "job index " << current_job_index_;
          const int task_count = words.size() / 2;
          for (int i = 0; i < task_count; ++i) {
            VLOG(4) <<  "Task " << i;
            const int machine_id = atoi32(words[2 * i]);
            const int duration = atoi32(words[2 * i + 1]);
            VLOG(4)  << "Machine id " << machine_id
                     << ", duration " << duration;
            AddTask(current_job_index_, machine_id, duration);
          }
          current_job_index_++;
          break;
        }
        break;
      }
      case TAILLARD: {
        switch (taillard_state_) {
          case START: {
            LOG(FATAL) << "Should not be here";
            break;
          }
          case JOBS_READ: {
            CHECK_EQ(1, words.size());
            machine_count_ = atoi32(words[0]);
            CHECK_GT(machine_count_, 0);
            taillard_state_ = MACHINES_READ;
            break;
          }
          case MACHINES_READ: {
            CHECK_EQ(1, words.size());
            const int seed = atoi32(words[0]);
            VLOG(1) << "Taillard instance with " << job_count_
                    << " jobs, and " << machine_count_
                    << " machines, generated with a seed of " << seed;
            taillard_state_ = SEED_READ;
            break;
          }
          case SEED_READ:
          case JOB_READ: {
            CHECK_EQ(1, words.size())
            << "Only one token allowed on line " << current_line_nbr_;
            current_job_index_ = atoi32(words[0]);
            VLOG(3) << "job index " << current_job_index_;
            taillard_state_ = JOB_ID_READ;
            break;
          }
          case JOB_ID_READ: {
            CHECK_EQ(1, words.size());
            taillard_state_ = JOB_LENGTH_READ;
            break;
          }
          case JOB_LENGTH_READ: {
            CHECK_EQ(machine_count_, words.size())
            << "Number of tasks must be equal to number of machines on line "
            << current_line_nbr_;
            for (int i = 0; i < machine_count_; ++i) {
              const int duration = atoi32(words[i]);
              VLOG(4)  << "Machine id " << i << ", duration " << duration;
              AddTask(current_job_index_, i, duration);
            }
            taillard_state_ = JOB_READ;
            break;
          }
        }
        break;
      }
    }
  }

  void AddTask(int job_id, int machine_id, int duration) {
    all_tasks_[job_id].push_back(Task(job_id, machine_id, duration));
    horizon_ += duration;
  }

  std::string name_;
  std::string filename_;
  int machine_count_;
  int job_count_;
  int horizon_;
  std::vector<std::vector<Task> > all_tasks_;
  int current_job_index_;
  int current_line_nbr_;
  ProblemType problem_type_;
  TaillardState taillard_state_;
  const char* kWordDelimiters;
  bool problem_numbers_defined;
};
}  // namespace operations_research

#endif  // OR_TOOLS_TUTORIALS_CPLUSPLUS_JOBSHOP_H_
