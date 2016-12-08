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

#include "util/rcpsp_parser.h"

#include "base/strtoint.h"
#include "base/numbers.h"
#include "base/split.h"
#include "base/stringpiece_utils.h"
#include "util/filelineiter.h"

namespace operations_research {

using ::strings::delimiter::AnyOf;

RcpspParser::RcpspParser()
    : seed_(-1),
      horizon_(-1),
      release_date_(-1),
      due_date_(-1),
      tardiness_cost_(-1),
      mpm_time_(-1),
      load_status_(NOT_STARTED),
      declared_tasks_(-1),
      current_task_(-1) {}

bool RcpspParser::LoadFile(const std::string& file_name) {
  if (load_status_ != NOT_STARTED) {
    return false;
  }

  load_status_ = HEADER_SECTION;

  for (const std::string& line : FileLines(file_name)) {
    ProcessLine(line);
    if (load_status_ == ERROR_FOUND) {
      return false;
    }
  }
  // Count the extra start and end tasks.
  return declared_tasks_ + 2 == tasks_.size() &&
         load_status_ == PARSING_FINISHED;
}

void RcpspParser::ReportError(const std::string& line) {
  LOG(ERROR) << "Error: status = " << load_status_ << ", line = " << line;
  load_status_ = ERROR_FOUND;
}

void RcpspParser::ProcessLine(const std::string& line) {
  if (strings::StartsWith(line, "***")) return;
  if (strings::StartsWith(line, "---")) return;

  const std::vector<std::string> words =
      strings::Split(line, AnyOf(" :\t"), strings::SkipEmpty());

  switch (load_status_) {
    case NOT_STARTED: {
      ReportError(line);
      break;
    }
    case HEADER_SECTION: {
      if (words[0] == "file") {
        basedata_ = words[3];
      } else if (words[0] == "initial") {
        seed_ = atoi64(words[4]);
        load_status_ = PROJECT_SECTION;
      } else {
        ReportError(line);
      }
      break;
    }
    case PROJECT_SECTION: {
      if (words[0] == "projects") {
        // Nothing to do.
      } else if (words[0] == "jobs") {
        declared_tasks_ = atoi32(words[4]);
      } else if (words[0] == "horizon") {
        horizon_ = atoi32(words[1]);
      } else if (words[0] == "RESOURCES") {
        // Nothing to do.
      } else if (words.size() > 1 && words[1] == "renewable") {
        for (int i = 0; i < atoi32(words[2]); ++i) {
          resources_.push_back({-1, true});
        }
      } else if (words.size() > 1 && words[1] == "nonrenewable") {
        for (int i = 0; i < atoi32(words[2]); ++i) {
          resources_.push_back({-1, false});
        }
      } else if (words.size() > 1 && words[1] == "doubly") {
        // Nothing to do.
      } else if (words.size() == 2 && words[0] == "PROJECT") {
        load_status_ = INFO_SECTION;
      } else {
        ReportError(line);
      }
      break;
    }
    case INFO_SECTION: {
      if (words[0] == "pronr.") {
        // Nothing to do.
      } else if (words.size() == 6) {
        declared_tasks_ = atoi32(words[1]);
        release_date_ = atoi32(words[2]);
        due_date_ = atoi32(words[3]);
        tardiness_cost_ = atoi32(words[4]);
        mpm_time_ = atoi32(words[5]);
      } else if (words.size() == 2 && words[0] == "PRECEDENCE") {
        load_status_ = PRECEDENCE_SECTION;
      } else {
        ReportError(line);
      }
      break;
    }
    case PRECEDENCE_SECTION: {
      if (words[0] == "jobnr.") {
        // Nothing to do.
      } else if (words.size() >= 3) {
        const int task_index = atoi32(words[0]);
        CHECK_EQ(task_index, tasks_.size() + 1);
        tasks_.resize(task_index);
        tasks_.back().recipes.resize(atoi32(words[1]));
        const int num_successors = atoi32(words[2]);
        if (words.size() != 3 + num_successors) {
          ReportError(line);
          break;
        }
        for (int i = 0; i < num_successors; ++i) {
          // The array of tasks is 0-based for us.
          tasks_.back().successors.push_back(atoi32(words[3 + i]) - 1);
        }
      } else if (words[0] == "REQUESTS/DURATIONS") {
        load_status_ = REQUEST_SECTION;
      } else {
        ReportError(line);
      }
      break;
    }
    case REQUEST_SECTION: {
      if (words[0] == "jobnr.") {
        // Nothing to do.
      } else if (words.size() == 3 + resources_.size()) {
        // Start of a new task (index is 0-based for us).
        current_task_ = atoi32(words[0]) - 1;
        const int current_mode = atoi32(words[1]) - 1;
        CHECK_LT(current_mode, tasks_[current_task_].recipes.size());
        if (current_mode != 0) {
          ReportError(line);
          break;
        }
        Recipe& recipe = tasks_[current_task_].recipes[current_mode];
        recipe.duration = atoi32(words[2]);
        for (int i = 0; i < resources_.size(); ++i) {
          recipe.demands_per_resource.push_back(atoi32(words[3 + i]));
        }
      } else if (words.size() == 2 + resources_.size()) {
        // New mode for a current task.
        const int current_mode = atoi32(words[0]) - 1;
        CHECK_LT(current_mode, tasks_[current_task_].recipes.size());
        Recipe& recipe = tasks_[current_task_].recipes[current_mode];
        recipe.duration = atoi32(words[1]);
        for (int i = 0; i < resources_.size(); ++i) {
          recipe.demands_per_resource.push_back(atoi32(words[2 + i]));
        }
      } else if (words[0] == "RESOURCEAVAILABILITIES") {
        load_status_ = RESOURCE_SECTION;
      } else {
        ReportError(line);
      }
      break;
    }
    case RESOURCE_SECTION: {
      if (words.size() == 2 * resources_.size()) {
        // Nothing to do.
      } else if (words.size() == resources_.size()) {
        for (int i = 0; i < words.size(); ++i) {
          resources_[i].capacity = atoi32(words[i]);
        }
        load_status_ = PARSING_FINISHED;
      } else {
        ReportError(line);
      }
      break;
    }
    case PARSING_FINISHED: {
      break;
    }
    case ERROR_FOUND: {
      break;
    }
  }
}

}  // namespace operations_research
