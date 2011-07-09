// Copyright 2011 Google Inc. All Rights Reserved.

#include <fstream>
#include <iostream>
#include <vector>

#include "solution_checker.h"

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

int main(int argc, char **argv) {

  const int kExpectedArgc = 4;
  if (argc != kExpectedArgc) {
    LG << "Wrong number of files to read." << endl
       << "The syntax should be:" << endl
       << "solution_checker instance_filename "
       << "initial_solution_filename new_solution_filename" << endl
       << "Current is:" << endl;

    for (int i = 0; i < argc; ++i) {
      LG << " " << argv[i];
    }
    LG << endl;
    return 0;
  }

  vector<int> model;
  vector<int> initial_assignments;
  vector<int> new_assignments;

  FileToVector(argv[1], &model);
  FileToVector(argv[2], &initial_assignments);
  FileToVector(argv[3], &new_assignments);

  DataParser data(model,
                  initial_assignments,
                  new_assignments);

  SolutionChecker solution_checker(data.machines(),
                                   data.services(),
                                   data.processes(),
                                   data.balance_costs(),
                                   data.process_move_cost_weight(),
                                   data.service_move_cost_weight(),
                                   data.machine_move_cost_weight(),
                                   data.initial_assignments(),
                                   data.new_assignments());

  if (solution_checker.Check()) {
    const int64 objective_cost = solution_checker.GetObjectiveCost();
    LG << "Solution is valid. Total objective cost is " << objective_cost
       << endl;
  } else {
    LG << "Solution is invalid." << endl;
  }

  return 0;
}
