// Copyright 2011 Google Inc. All Rights Reserved.

#include <fstream>
#include <iostream>
#include <vector>

#include "solution_checker.h"
#include "util/task/status.h"

using roadef_challenge::DataParser;
using roadef_challenge::SolutionChecker;

namespace {
void FileToVector(const char* const filename, vector<int>* values) {
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

  vector<int> model;
  vector<int> initial_assignments;
  vector<int> new_assignments;

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
    const int64 objective_cost = solution_checker.GetObjectiveCost();
    LOG(INFO) << "Solution is valid. Total objective cost is " << objective_cost
              << std::endl;
  } else {
    LOG(INFO) << "Solution is invalid." << std::endl;
  }

  solution_checker.PrintStats();

  return 0;
}
