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

//

#include "examples/cpp/fap_utilities.h"

#include <map>
#include <set>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/map_util.h"

namespace operations_research {

bool CheckConstraintSatisfaction(
    const std::vector<FapConstraint>& data_constraints,
    const std::vector<int>& variables,
    const std::map<int, int>& index_from_key) {
  bool status = true;
  for (const FapConstraint& ct : data_constraints) {
    const int index1 = FindOrDie(index_from_key, ct.variable1);
    const int index2 = FindOrDie(index_from_key, ct.variable2);
    CHECK_LT(index1, variables.size());
    CHECK_LT(index2, variables.size());
    const int var1 = variables[index1];
    const int var2 = variables[index2];
    const int absolute_difference = abs(var1 - var2);

    if ((ct.operation == ">") && (absolute_difference <= ct.value)) {
      LOG(INFO) << "  Violation of contraint between variable " << ct.variable1
                << " and variable " << ct.variable2 << ".";
      LOG(INFO) << "  Expected |" << var1 << " - " << var2
                << "| (= " << absolute_difference << ") >  " << ct.value
                << ".";
      status = false;
    } else if ((ct.operation == "=") && (absolute_difference != ct.value)) {
      LOG(INFO) << "  Violation of contraint between variable " << ct.variable1
                << " and variable " << ct.variable2 << ".";
      LOG(INFO) << "  Expected |" << var1 << " - " << var2
                << "| (= " << absolute_difference << ") =  " << ct.value
                << ".";
      status = false;
    }
  }
  return status;
}

bool CheckVariablePosition(const std::map<int, FapVariable>& data_variables,
                           const std::vector<int>& variables,
                           const std::map<int, int>& index_from_key) {
  bool status = true;
  for (const auto& it : data_variables) {
    const int index = FindOrDie(index_from_key, it.first);
    CHECK_LT(index, variables.size());
    const int var = variables[index];

    if (it.second.hard && (it.second.initial_position != -1) &&
        (var != it.second.initial_position)) {
      LOG(INFO) << "  Change of position of hard variable " << it.first << ".";
      LOG(INFO) << "  Expected " << it.second.initial_position
                << " instead of given " << var << ".";
      status = false;
    }
  }
  return status;
}

int NumberOfAssignedValues(const std::vector<int>& variables) {
  std::set<int> assigned(variables.begin(), variables.end());
  return static_cast<int>(assigned.size());
}

void PrintElapsedTime(const int64 time1, const int64 time2) {
  LOG(INFO) << "End of solving process.";
  LOG(INFO) << "The Solve method took " << (time2 - time1) / 1000.0
            << " seconds.";
}

void PrintResultsHard(SolutionCollector* const collector,
                      const std::vector<IntVar*>& variables,
                      IntVar* const objective_var,
                      const std::map<int, FapVariable>& data_variables,
                      const std::vector<FapConstraint>& data_constraints,
                      const std::map<int, int>& index_from_key,
                      const std::vector<int>& key_from_index) {
  LOG(INFO) << "Printing...";
  LOG(INFO) << "Number of Solutions: " << collector->solution_count();
  for (int solution_index = 0; solution_index < collector->solution_count();
       ++solution_index) {
    Assignment* const solution = collector->solution(solution_index);
    std::vector<int> results(variables.size());
    LOG(INFO) << "------------------------------------------------------------";
    LOG(INFO) << "Solution " << solution_index + 1;
    LOG(INFO) << "Cost: " << solution->Value(objective_var);
    for (int i = 0; i < variables.size(); ++i) {
      results[i] = solution->Value(variables[i]);
      LOG(INFO) << "  Variable " << key_from_index[i] << ": " << results[i];
    }
    if (CheckConstraintSatisfaction(data_constraints, results,
                                    index_from_key)) {
      LOG(INFO) << "All hard constraints satisfied.";
    } else {
      LOG(INFO) << "Warning! Hard constraint violation detected.";
    }
    if (CheckVariablePosition(data_variables, results, index_from_key)) {
      LOG(INFO) << "All hard variables stayed unharmed.";
    } else {
      LOG(INFO) << "Warning! Hard variable modification detected.";
    }

    LOG(INFO) << "Values used: " << NumberOfAssignedValues(results);
    LOG(INFO) << "Maximum value used: "
              << *std::max_element(results.begin(), results.end());
    LOG(INFO) << "  Failures: " << collector->failures(solution_index);
  }
  LOG(INFO) << "  ============================================================";
}

void PrintResultsSoft(SolutionCollector* const collector,
                      const std::vector<IntVar*>& variables,
                      IntVar* const total_cost,
                      const std::map<int, FapVariable>& hard_variables,
                      const std::vector<FapConstraint>& hard_constraints,
                      const std::map<int, FapVariable>& soft_variables,
                      const std::vector<FapConstraint>& soft_constraints,
                      const std::map<int, int>& index_from_key,
                      const std::vector<int>& key_from_index) {
  LOG(INFO) << "Printing...";
  LOG(INFO) << "Number of Solutions: " << collector->solution_count();
  for (int solution_index = 0; solution_index < collector->solution_count();
       ++solution_index) {
    Assignment* const solution = collector->solution(solution_index);
    std::vector<int> results(variables.size());
    LOG(INFO) << "------------------------------------------------------------";
    LOG(INFO) << "Solution";
    for (int i = 0; i < variables.size(); ++i) {
      results[i] = solution->Value(variables[i]);
      LOG(INFO) << "  Variable " << key_from_index[i] << ": " << results[i];
    }
    if (CheckConstraintSatisfaction(hard_constraints, results,
                                    index_from_key)) {
      LOG(INFO) << "All hard constraints satisfied.";
    } else {
      LOG(INFO) << "Warning! Hard constraint violation detected.";
    }
    if (CheckVariablePosition(hard_variables, results, index_from_key)) {
      LOG(INFO) << "All hard variables stayed unharmed.";
    } else {
      LOG(INFO) << "Warning! Hard constraint violation detected.";
    }

    if (CheckConstraintSatisfaction(soft_constraints, results,
                                    index_from_key) &&
        CheckVariablePosition(soft_variables, results, index_from_key)) {
      LOG(INFO) << "Problem feasible: "
                   "Soft constraints and soft variables satisfied.";
      LOG(INFO) << "  Weighted Sum: " << solution->Value(total_cost);
    } else {
      LOG(INFO) << "Problem unfeasible. Optimized weighted sum of violations.";
      LOG(INFO) << "  Weighted Sum: " << solution->Value(total_cost);
    }

    LOG(INFO) << "Values used: " << NumberOfAssignedValues(results);
    LOG(INFO) << "Maximum value used: "
              << *std::max_element(results.begin(), results.end());
    LOG(INFO) << "  Failures: " << collector->failures(solution_index);
  }
  LOG(INFO) << "  ============================================================";
}

}  // namespace operations_research
