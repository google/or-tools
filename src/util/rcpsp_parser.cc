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
      deadline_(-1),
      load_status_(NOT_STARTED),
      declared_tasks_(-1),
      current_task_(-1),
      is_rcpsp_max_(false),
      is_resource_investment_(false),
      is_consumer_producer_(false),
      unreads_(0) {}

bool RcpspParser::LoadFile(const std::string& file_name) {
  if (load_status_ != NOT_STARTED) {
    return false;
  }

  is_rcpsp_max_ = strings::EndsWith(file_name, ".sch") ||
                  strings::EndsWith(file_name, ".SCH");
  const bool is_patterson = strings::EndsWith(file_name, ".rcp");
  load_status_ = HEADER_SECTION;

  for (const std::string& line : FileLines(file_name)) {
    if (is_rcpsp_max_) {
      ProcessRcpspMaxLine(line);
    } else if (is_patterson) {
      ProcessPattersonLine(line);
    } else {
      ProcessRcpspLine(line);
    }
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

void RcpspParser::ProcessRcpspLine(const std::string& line) {
  if (strings::StartsWith(line, "***")) return;
  if (strings::StartsWith(line, "---")) return;

  const std::vector<std::string> words =
      strings::Split(line, AnyOf(" :\t\r"), strings::SkipEmpty());

  if (words.empty()) return;

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
      } else if (words[0] == "jobs") {
        // Workaround for the mmlib files which has less headers.
        declared_tasks_ = atoi32(words[4]) - 2;
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
        // This declaration counts the 2 sentinels.
        declared_tasks_ = atoi32(words[4]) - 2;
      } else if (words[0] == "horizon") {
        horizon_ = atoi32(words[1]);
      } else if (words[0] == "RESOURCES") {
        // Nothing to do.
      } else if (words.size() > 1 && words[1] == "renewable") {
        for (int i = 0; i < atoi32(words[2]); ++i) {
          resources_.push_back({-1, -1, true, 0});
        }
      } else if (words.size() > 1 && words[1] == "nonrenewable") {
        for (int i = 0; i < atoi32(words[2]); ++i) {
          resources_.push_back({-1, -1, false, 0});
        }
      } else if (words.size() > 1 && words[1] == "doubly") {
        // Nothing to do.
      } else if (words.size() == 2 && words[0] == "PROJECT") {
        load_status_ = INFO_SECTION;
      } else if (words.size() == 2 && words[0] == "PRECEDENCE") {
        // mmlib files have no info section.
        load_status_ = PRECEDENCE_SECTION;
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
      } else if (words[0] == "RESOURCEAVAILABILITIES" ||
                 (words[0] == "RESOURCE" && words[1] == "AVAILABILITIES")) {
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
          resources_[i].max_capacity = atoi32(words[i]);
        }
        load_status_ = PARSING_FINISHED;
      } else {
        ReportError(line);
      }
      break;
    }
    case RESOURCE_MIN_SECTION: {
      LOG(FATAL) << "Should not be here";
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

void RcpspParser::ProcessRcpspMaxLine(const std::string& line) {
  const std::vector<std::string> words =
      strings::Split(line, AnyOf(" :\t[]\r"), strings::SkipEmpty());

  switch (load_status_) {
    case NOT_STARTED: {
      ReportError(line);
      break;
    }
    case HEADER_SECTION: {
      if (words.size() == 2) {
        is_consumer_producer_ = true;
      } else if (words.size() < 4 || atoi32(words[3]) != 0) {
        ReportError(line);
        break;
      }

      if (words.size() == 5) {
        deadline_ = atoi32(words[4]);
        is_resource_investment_ = true;
      }

      declared_tasks_ = atoi32(words[0]);
      tasks_.resize(declared_tasks_ + 2);  // 2 sentinels.
      temp_delays_.resize(declared_tasks_ + 2);

      // Creates resources.
      if (is_consumer_producer_) {
        const int num_nonrenewable_resources = atoi32(words[1]);
        resources_.resize(num_nonrenewable_resources, {-1, -1, false, 0});
      } else {
        const int num_renewable_resources = atoi32(words[1]);
        const int num_nonrenewable_resources = atoi32(words[2]);
        resources_.resize(num_renewable_resources, {-1, -1, true, 0});
        resources_.resize(num_renewable_resources + num_nonrenewable_resources,
                          {-1, -1, false, 0});
      }

      // Set up for the next section.
      load_status_ = PRECEDENCE_SECTION;
      current_task_ = 0;
      break;
    }
    case PROJECT_SECTION: {
      LOG(FATAL) << "Should not be here";
      break;
    }
    case INFO_SECTION: {
      LOG(FATAL) << "Should not be here";
      break;
    }
    case PRECEDENCE_SECTION: {
      if (words.size() < 3) {
        ReportError(line);
        break;
      }

      const int task_id = atoi32(words[0]);
      if (task_id != current_task_) {
        ReportError(line);
        break;
      } else {
        current_task_++;
      }

      const int num_modes = atoi32(words[1]);
      const int num_successors = atoi32(words[2]);

      RcpspParser::Task& task = tasks_[task_id];
      task.recipes.resize(num_modes);

      // Read successors.
      for (int i = 0; i < num_successors; ++i) {
        task.successors.push_back(atoi32(words[3 + i]));
      }

      // Read flattened delays into temp_delays_.
      for (int i = 3 + num_successors; i < words.size(); ++i) {
        temp_delays_[task_id].push_back(atoi32(words[i]));
      }

      if (task_id == declared_tasks_ + 1) {
        // Convert the flattened delays into structured delays (1 vector per
        // successor) in the task_size.
        for (int t = 1; t <= declared_tasks_; ++t) {
          const int num_modes = tasks_[t].recipes.size();
          const int num_successors = tasks_[t].successors.size();
          tasks_[t].delays.resize(num_successors);
          int count = 0;
          for (int s = 0; s < num_successors; ++s) {
            tasks_[t].delays[s].resize(num_modes);
            for (int m1 = 0; m1 < num_modes; ++m1) {
              const int other = tasks_[t].successors[s];
              const int num_other_modes = tasks_[other].recipes.size();
              for (int m2 = 0; m2 < num_other_modes; ++m2) {
                tasks_[t].delays[s][m1].push_back(temp_delays_[t][count++]);
              }
            }
          }
          CHECK_EQ(count, temp_delays_[t].size());
        }

        // Setup for next section.
        current_task_ = 0;
        load_status_ = REQUEST_SECTION;
      }
      break;
    }
    case REQUEST_SECTION: {
      if (words.size() == 3 + resources_.size()) {
        // Start of a new task.
        current_task_ = atoi32(words[0]);

        // 0 based indices for the mode.
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
      } else if (words.size() == 2 + resources_.size() &&
                 is_consumer_producer_) {
        // Start of a new task.
        current_task_ = atoi32(words[0]);

        // 0 based indices for the mode.
        const int current_mode = atoi32(words[1]) - 1;
        CHECK_LT(current_mode, tasks_[current_task_].recipes.size());
        if (current_mode != 0) {
          ReportError(line);
          break;
        }
        Recipe& recipe = tasks_[current_task_].recipes[current_mode];
        recipe.duration = 0;
        for (int i = 0; i < resources_.size(); ++i) {
          recipe.demands_per_resource.push_back(atoi32(words[2 + i]));
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
      }
      if (current_task_ == declared_tasks_ + 1) {
        load_status_ = RESOURCE_SECTION;
      }
      break;
    }
    case RESOURCE_SECTION: {
      if (words.size() == resources_.size()) {
        for (int i = 0; i < words.size(); ++i) {
          if (is_resource_investment_) {
            resources_[i].unit_cost = atoi32(words[i]);
          } else {
            resources_[i].max_capacity = atoi32(words[i]);
          }
        }
        if (is_consumer_producer_) {
          load_status_ = RESOURCE_MIN_SECTION;
        } else {
          load_status_ = PARSING_FINISHED;
        }
      } else {
        ReportError(line);
      }
      break;
    }
    case RESOURCE_MIN_SECTION: {
      if (words.size() == resources_.size()) {
        for (int i = 0; i < words.size(); ++i) {
          resources_[i].min_capacity = atoi32(words[i]);
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

void RcpspParser::ProcessPattersonLine(const std::string& line) {
  const std::vector<std::string> words =
      strings::Split(line, AnyOf(" :\t[]\r"), strings::SkipEmpty());

  if (words.empty()) return;

  switch (load_status_) {
    case NOT_STARTED: {
      ReportError(line);
      break;
    }
    case HEADER_SECTION: {
      if (words.size() != 2) {
        ReportError(line);
        break;
      }
      declared_tasks_ = atoi32(words[0]) - 2;  // Remove the 2 sentinels.
      tasks_.resize(declared_tasks_ + 2);      // 2 sentinels.

      // Creates resources.
      const int num_renewable_resources = atoi32(words[1]);
      resources_.resize(num_renewable_resources, {-1, -1, true, 0});

      // Set up for the next section.
      load_status_ = RESOURCE_SECTION;
      break;
    }
    case PROJECT_SECTION: {
      LOG(FATAL) << "Should not be here";
      break;
    }
    case INFO_SECTION: {
      LOG(FATAL) << "Should not be here";
      break;
    }
    case PRECEDENCE_SECTION: {
      if (unreads_ > 0) {
        for (int i = 0; i < words.size(); ++i) {
          tasks_[current_task_].successors.push_back(atoi32(words[i]) - 1);
          unreads_--;
          CHECK_GE(unreads_, 0);
        }
      } else {
        if (words.size() < 2 + resources_.size()) {
          ReportError(line);
          break;
        }

        RcpspParser::Task& task = tasks_[current_task_];
        task.recipes.resize(1);

        const int num_resources = resources_.size();

        RcpspParser::Recipe& recipe = task.recipes.front();
        recipe.duration = atoi32(words[0]);
        for (int i = 1; i <= num_resources; ++i) {
          recipe.demands_per_resource.push_back(atoi32(words[i]));
        }

        unreads_ = atoi32(words[1 + num_resources]);
        for (int i = 2 + num_resources; i < words.size(); ++i) {
          // Successors are 1 based in the data file.
          task.successors.push_back(atoi32(words[i]) - 1);
          unreads_--;
          CHECK_GE(unreads_, 0);
        }
      }

      if (unreads_ == 0 && ++current_task_ == declared_tasks_ + 2) {
        load_status_ = PARSING_FINISHED;
      }
      break;
    }
    case REQUEST_SECTION: {
      LOG(FATAL) << "Should not be here";
      break;
    }
    case RESOURCE_SECTION: {
      if (words.size() == resources_.size()) {
        for (int i = 0; i < words.size(); ++i) {
          resources_[i].max_capacity = atoi32(words[i]);
        }
        load_status_ = PRECEDENCE_SECTION;
        current_task_ = 0;
      } else {
        ReportError(line);
      }
      break;
    }
    case RESOURCE_MIN_SECTION: {
      LOG(FATAL) << "Should not be here";
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
