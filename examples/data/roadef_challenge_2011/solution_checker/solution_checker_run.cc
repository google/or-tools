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

#include <fstream>
#include <iostream>
#include <vector>

#include "ortools/base/status.h"
#include "solution_checker.h"

namespace {

using ::roadef_challenge::DataParser;
using ::roadef_challenge::SolutionChecker;

void FileToVector(const char* const filename, std::vector<int>* values) {
  CHECK_NOTNULL(filename);
  CHECK_NOTNULL(values);
  values->clear();

  ifstream file(filename);
  CHECK(file);

  int value = -1;
  while (file >> value) {
    values->push_back(value);
  }

  CHECK(file.eof());
  file.close();
}
}  // anonymous namespace

int main(int argc, char** argv) {
  const int kExpectedArgc = 4;
  if (argc != kExpectedArgc) {
    LOG(INFO) << "Wrong number of files to read." << std::endl
              << "The syntax should be:" << std::endl
              << "solution_checker instance_filename "
              << "initial_solution_filename new_solution_filename" << std::endl
              << "Current is:" << std::endl;

    for (int i = 0; i < argc; ++i) {
      LOG(INFO) << " " << argv[i];
    }
    LOG(INFO) << std::endl;
    return 0;
  }

  std::vector<int> model;
  std::vector<int> initial_assignments;
  std::vector<int> new_assignments;

  FileToVector(argv[1], &model);
  FileToVector(argv[2], &initial_assignments);
  FileToVector(argv[3], &new_assignments);

  DataParser data(model, initial_assignments, new_assignments);

  SolutionChecker solution_checker(
      data.machines(), data.services(), data.processes(), data.balance_costs(),
      data.process_move_cost_weight(), data.service_move_cost_weight(),
      data.machine_move_cost_weight(), data.initial_assignments(),
      data.new_assignments());

  if (solution_checker.Check()) {
    const int64_t objective_cost = solution_checker.GetObjectiveCost();
    LOG(INFO) << "Solution is valid. Total objective cost is " << objective_cost
              << std::endl;
  } else {
    LOG(INFO) << "Solution is invalid." << std::endl;
  }

  solution_checker.PrintStats();

  return 0;
}
