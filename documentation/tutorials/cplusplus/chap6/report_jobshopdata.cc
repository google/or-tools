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
//  Simple program to report the content of a job-shop problem instance
//  in JSSP or Taillard formats.

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "constraint_solver/constraint_solver.h"

#include "jobshop.h"

DEFINE_string(
  data_file,
  "",
  "Input file with a description of the job-shop problem instance to solve "
  "in JSSP or Taillard's format.\n");

DEFINE_bool(
  complete_report,
  false,
  "Complete report?\n");

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_data_file.empty()) {
    LOG(FATAL) << "Please supply a data file with --data_file=";
  }
  operations_research::JobShopData data(FLAGS_data_file);
  if (FLAGS_complete_report) {
    data.ReportAll(std::cout);
  } else {
    data.Report(std::cout);
  }

  return 0;
}
