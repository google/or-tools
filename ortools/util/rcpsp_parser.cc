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

#include "ortools/util/rcpsp_parser.h"

#include "ortools/base/strtoint.h"
#include "ortools/base/numbers.h"
#include "ortools/base/split.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/util/filelineiter.h"

namespace operations_research {
namespace util {
namespace rcpsp {

using ::strings::delimiter::AnyOf;

RcpspParser::RcpspParser()
    : seed_(-1),
      load_status_(NOT_STARTED),
      declared_tasks_(-1),
      current_task_(-1),
      unreads_(0) {
  rcpsp_.set_deadline(-1);
  rcpsp_.set_horizon(-1);
}

bool RcpspParser::LoadFile(const std::string& file_name) {
  if (load_status_ != NOT_STARTED) {
    return false;
  }

  const bool is_rcpsp_max = strings::EndsWith(file_name, ".sch") ||
                            strings::EndsWith(file_name, ".SCH");
  const bool is_patterson = strings::EndsWith(file_name, ".rcp");
  load_status_ = HEADER_SECTION;

  for (const std::string& line : FileLines(file_name)) {
    if (is_rcpsp_max) {
      ProcessRcpspMaxLine(line);
    } else if (is_patterson) {
      ProcessPattersonLine(line);
    } else {
      ProcessRcpspLine(line);
    }
    if (load_status_ == ERROR_FOUND) {
      LOG(INFO) << rcpsp_.DebugString();
      return false;
    }
  }
  // Count the extra start and end tasks.
  return declared_tasks_ + 2 == rcpsp_.tasks_size() &&
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
        rcpsp_.set_basedata(words[3]);
      } else if (words[0] == "initial") {
        rcpsp_.set_seed(atoi64(words[4]));
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
        rcpsp_.set_horizon(atoi32(words[1]));
      } else if (words[0] == "RESOURCES") {
        // Nothing to do.
      } else if (words.size() > 1 && words[1] == "renewable") {
        for (int i = 0; i < atoi32(words[2]); ++i) {
          util::rcpsp::Resource* const res = rcpsp_.add_resources();
          res->set_max_capacity(-1);
          res->set_renewable(true);
          res->set_unit_cost(0);
        }
      } else if (words.size() > 1 && words[1] == "nonrenewable") {
        for (int i = 0; i < atoi32(words[2]); ++i) {
          util::rcpsp::Resource* const res = rcpsp_.add_resources();
          res->set_max_capacity(-1);
          res->set_min_capacity(-1);
          res->set_renewable(false);
          res->set_unit_cost(0);
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
        rcpsp_.set_release_date(atoi32(words[2]));
        rcpsp_.set_due_date(atoi32(words[3]));
        rcpsp_.set_tardiness_cost(atoi32(words[4]));
        rcpsp_.set_mpm_time(atoi32(words[5]));
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
        CHECK_EQ(task_index, rcpsp_.tasks_size() + 1);
        util::rcpsp::Task* const task = rcpsp_.add_tasks();
        const int num_recipes = atoi32(words[1]);
        for (int i = 0; i < num_recipes; ++i) {
          task->add_recipes();
        }
        const int num_successors = atoi32(words[2]);
        if (words.size() != 3 + num_successors) {
          ReportError(line);
          break;
        }
        for (int i = 0; i < num_successors; ++i) {
          // The array of tasks is 0-based for us.
          task->add_successors(atoi32(words[3 + i]) - 1);
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
      } else if (words.size() == 3 + rcpsp_.resources_size()) {
        // Start of a new task (index is 0-based for us).
        current_task_ = atoi32(words[0]) - 1;
        const int current_mode = atoi32(words[1]) - 1;
        CHECK_LT(current_mode, rcpsp_.tasks(current_task_).recipes_size());
        if (current_mode != 0) {
          ReportError(line);
          break;
        }
        util::rcpsp::Recipe* const recipe =
            rcpsp_.mutable_tasks(current_task_)->mutable_recipes(current_mode);
        recipe->set_duration(atoi32(words[2]));
        for (int i = 0; i < rcpsp_.resources_size(); ++i) {
          const int demand = atoi32(words[3 + i]);
          if (demand != 0) {
            recipe->add_demands(demand);
            recipe->add_resources(i);
          }
        }
      } else if (words.size() == 2 + rcpsp_.resources_size()) {
        // New mode for a current task.
        const int current_mode = atoi32(words[0]) - 1;
        CHECK_LT(current_mode, rcpsp_.tasks(current_task_).recipes_size());
        util::rcpsp::Recipe* const recipe =
            rcpsp_.mutable_tasks(current_task_)->mutable_recipes(current_mode);
        recipe->set_duration(atoi32(words[1]));
        for (int i = 0; i < rcpsp_.resources_size(); ++i) {
          const int demand = atoi32(words[2 + i]);
          if (demand != 0) {
            recipe->add_demands(demand);
            recipe->add_resources(i);
          }
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
      if (words.size() == 2 * rcpsp_.resources_size()) {
        // Nothing to do.
      } else if (words.size() == rcpsp_.resources_size()) {
        for (int i = 0; i < words.size(); ++i) {
          rcpsp_.mutable_resources(i)->set_max_capacity(atoi32(words[i]));
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
      rcpsp_.set_is_rcpsp_max(true);
      if (words.size() == 2) {
        rcpsp_.set_is_consumer_producer(true);
      } else if (words.size() < 4 || atoi32(words[3]) != 0) {
        ReportError(line);
        break;
      }

      if (words.size() == 5) {
        rcpsp_.set_deadline(atoi32(words[4]));
        rcpsp_.set_is_resource_investment(true);
      }

      declared_tasks_ = atoi32(words[0]);
      temp_delays_.resize(declared_tasks_ + 2);
      recipe_sizes_.resize(declared_tasks_ + 2, 0);

      // Creates resources.
      if (rcpsp_.is_consumer_producer()) {
        const int num_nonrenewable_resources = atoi32(words[1]);
        for (int i = 0; i < num_nonrenewable_resources; ++i) {
          util::rcpsp::Resource* const res = rcpsp_.add_resources();
          res->set_max_capacity(-1);
          res->set_min_capacity(-1);
          res->set_renewable(false);
          res->set_unit_cost(0);
        }
      } else {
        const int num_renewable_resources = atoi32(words[1]);
        const int num_nonrenewable_resources = atoi32(words[2]);
        for (int i = 0; i < num_renewable_resources; ++i) {
          util::rcpsp::Resource* const res = rcpsp_.add_resources();
          res->set_max_capacity(-1);
          res->set_renewable(true);
          res->set_unit_cost(0);
        }
        for (int i = 0; i < num_nonrenewable_resources; ++i) {
          util::rcpsp::Resource* const res = rcpsp_.add_resources();
          res->set_max_capacity(-1);
          res->set_min_capacity(-1);
          res->set_renewable(false);
          res->set_unit_cost(0);
        }
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
      recipe_sizes_[task_id] = num_modes;
      const int num_successors = atoi32(words[2]);

      util::rcpsp::Task* const task = rcpsp_.add_tasks();

      // Read successors.
      for (int i = 0; i < num_successors; ++i) {
        task->add_successors(atoi32(words[3 + i]));
      }

      // Read flattened delays into temp_delays_.
      for (int i = 3 + num_successors; i < words.size(); ++i) {
        temp_delays_[task_id].push_back(atoi32(words[i]));
      }

      if (task_id == declared_tasks_ + 1) {
        // Convert the flattened delays into structured delays (1 vector per
        // successor) in the task_size.
        for (int t = 1; t <= declared_tasks_; ++t) {
          const int num_modes = recipe_sizes_[t];
          const int num_successors = rcpsp_.tasks(t).successors_size();
          int count = 0;
          for (int s = 0; s < num_successors; ++s) {
            util::rcpsp::PerSuccessorDelays* const succ_delays =
                rcpsp_.mutable_tasks(t)->add_successor_delays();
            for (int m1 = 0; m1 < num_modes; ++m1) {
              util::rcpsp::PerRecipeDelays* const recipe_delays =
                  succ_delays->add_recipe_delays();
              const int other = rcpsp_.tasks(t).successors(s);
              const int num_other_modes = recipe_sizes_[other];
              for (int m2 = 0; m2 < num_other_modes; ++m2) {
                recipe_delays->add_min_delays(temp_delays_[t][count++]);
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
      if (words.size() == 3 + rcpsp_.resources_size()) {
        // Start of a new task.
        current_task_ = atoi32(words[0]);

        // 0 based indices for the mode.
        const int current_mode = atoi32(words[1]) - 1;
        if (current_mode != 0) {
          ReportError(line);
          break;
        }
        util::rcpsp::Recipe* const recipe =
            rcpsp_.mutable_tasks(current_task_)->add_recipes();
        recipe->set_duration(atoi32(words[2]));
        for (int i = 0; i < rcpsp_.resources_size(); ++i) {
          const int demand = atoi32(words[3 + i]);
          if (demand != 0) {
            recipe->add_demands(demand);
            recipe->add_resources(i);
          }
        }
      } else if (words.size() == 2 + rcpsp_.resources_size() &&
                 rcpsp_.is_consumer_producer()) {
        // Start of a new task.
        current_task_ = atoi32(words[0]);

        // 0 based indices for the mode.
        const int current_mode = atoi32(words[1]) - 1;
        if (current_mode != 0) {
          ReportError(line);
          break;
        }
        util::rcpsp::Recipe* const recipe =
            rcpsp_.mutable_tasks(current_task_)->add_recipes();
        recipe->set_duration(0);
        for (int i = 0; i < rcpsp_.resources_size(); ++i) {
          const int demand = atoi32(words[2 + i]);
          if (demand != 0) {
            recipe->add_demands(demand);
            recipe->add_resources(i);
          }
        }
      } else if (words.size() == 2 + rcpsp_.resources_size()) {
        // New mode for a current task.
        const int current_mode = atoi32(words[0]) - 1;
        util::rcpsp::Recipe* const recipe =
            rcpsp_.mutable_tasks(current_task_)->add_recipes();
        recipe->set_duration(atoi32(words[1]));
        for (int i = 0; i < rcpsp_.resources_size(); ++i) {
          const int demand = atoi32(words[2 + i]);
          if (demand != 0) {
            recipe->add_demands(demand);
            recipe->add_resources(i);
          }
        }
      }
      if (current_task_ == declared_tasks_ + 1) {
        load_status_ = RESOURCE_SECTION;
      }
      break;
    }
    case RESOURCE_SECTION: {
      if (words.size() == rcpsp_.resources_size()) {
        for (int i = 0; i < words.size(); ++i) {
          if (rcpsp_.is_resource_investment()) {
            rcpsp_.mutable_resources(i)->set_unit_cost(atoi32(words[i]));
          } else {
            rcpsp_.mutable_resources(i)->set_max_capacity(atoi32(words[i]));
          }
        }
        if (rcpsp_.is_consumer_producer()) {
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
      if (words.size() == rcpsp_.resources_size()) {
        for (int i = 0; i < words.size(); ++i) {
          rcpsp_.mutable_resources(i)->set_min_capacity(atoi32(words[i]));
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
      for (int i = 0; i < declared_tasks_ + 2; ++i) {  // Add back 2 sentinels.
        rcpsp_.add_tasks();
      }

      // Creates resources.
      const int num_renewable_resources = atoi32(words[1]);
      for (int i = 0; i < num_renewable_resources; ++i) {
        util::rcpsp::Resource* const res = rcpsp_.add_resources();
        res->set_max_capacity(-1);
        res->set_min_capacity(-1);
        res->set_renewable(true);
        res->set_unit_cost(0);
      }

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
          rcpsp_.mutable_tasks(current_task_)->add_successors(
              atoi32(words[i]) - 1);
          unreads_--;
          CHECK_GE(unreads_, 0);
        }
      } else {
        if (words.size() < 2 + rcpsp_.resources_size()) {
          ReportError(line);
          break;
        }

        util::rcpsp::Task* const task = rcpsp_.mutable_tasks(current_task_);
        util::rcpsp::Recipe* const recipe = task->add_recipes();

        const int num_resources = rcpsp_.resources_size();

        recipe->set_duration(atoi32(words[0]));
        for (int i = 1; i <= num_resources; ++i) {
          const int demand = atoi32(words[i]);
          if (demand != 0) {
            recipe->add_demands(demand);
            recipe->add_resources(i - 1);
          }
        }

        unreads_ = atoi32(words[1 + num_resources]);
        for (int i = 2 + num_resources; i < words.size(); ++i) {
          // Successors are 1 based in the data file.
          task->add_successors(atoi32(words[i]) - 1);
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
      if (words.size() == rcpsp_.resources_size()) {
        for (int i = 0; i < words.size(); ++i) {
          rcpsp_.mutable_resources(i)->set_max_capacity(atoi32(words[i]));
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

}  // namespace rcpsp
}  // namespace util
}  // namespace operations_research
