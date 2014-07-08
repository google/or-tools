// Copyright 2010-2013 Google
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
// Driver to run MPSolver on MPModelProto text files.
//
// TODO(user): Move this under linear_solver/ and support more file formats and
// linear programming solvers.

#include <algorithm>
#include <cstdio>
#include "base/unique_ptr.h"
#include <string>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/timer.h"
#include "base/file.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "base/threadpool.h"
#include "util/gzip/gzipstring.h"
#include "glop/lp_solver.h"
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver2.pb.h"
#include "util/fp_utils.h"
#include "util/stats.h"
#include "base/status.h"

DEFINE_string(input, "", "File pattern for problems to be optimized.");
DEFINE_bool(use_clp, false,
            "Use clp. If both use_clp and use_glop are true, "
            "results from both are compared.");
DEFINE_bool(use_glop, true,
            "Use glop. If both use_clp and use_glop are true, "
            "results from both are compared.");
DEFINE_int64(max_time_in_ms, -1, "Maximum time allowed in milliseconds.");
DEFINE_int64(threads, 1, "Number of threads.");
DEFINE_double(variable_tolerance, 1e-7, "Tolerance on variable values.");
DEFINE_double(cost_tolerance, 1e-7, "Tolerance on cost value.");

namespace operations_research {
namespace glop {
using google::protobuf::TextFormat;

struct InstanceResult {
  InstanceResult()
      : parsing_time_in_sec(0),
        loading_time_in_sec(0),
        solving_time_in_sec(0),
        result_status(),
        objective_value(0),
        may_have_multiple_solutions(false),
        variable_values() {}
  double parsing_time_in_sec;
  double loading_time_in_sec;
  double solving_time_in_sec;
  MPSolver::ResultStatus result_status;
  double objective_value;
  bool may_have_multiple_solutions;
  std::vector<double> variable_values;
};

void Solve(MPSolver::OptimizationProblemType type, const std::string& file_name,
           InstanceResult* result) {
  std::string raw_data;
  CHECK_OK(file::GetContents(file_name, &raw_data, file::Defaults()));
  std::string uncompressed;
  if (!GunzipString(raw_data, &uncompressed)) {
    uncompressed = raw_data;
  }
  new_proto::MPModelProto proto;
  {
    ScopedWallTime timer(&(result->parsing_time_in_sec));
    if (!proto.ParseFromString(uncompressed)) {
      // We do not care about timing the parsing from a text proto, that's why
      // we try first to parse the proto as binary.
      CHECK(TextFormat::ParseFromString(uncompressed, &proto));
    }
  }
  MPSolver solver(file_name, type);
  if (FLAGS_max_time_in_ms >= 0) {
    solver.set_time_limit(FLAGS_max_time_in_ms);
  }
  MPSolverParameters param;
  param.SetIntegerParam(MPSolverParameters::SCALING,
                        MPSolverParameters::SCALING_OFF);
  {
    ScopedWallTime timer(&(result->loading_time_in_sec));
    const MPSolver::LoadStatus load_status = solver.LoadModelFromProto(proto);
    CHECK(load_status == MPSolver::NO_ERROR);
  }
  {
    ScopedWallTime timer(&(result->solving_time_in_sec));
    result->result_status = solver.Solve(param);
  }
  result->objective_value = (result->result_status == MPSolver::OPTIMAL)
                                ? solver.Objective().Value()
                                : 0;
  int num_variables = solver.NumVariables();
  result->variable_values.resize(num_variables);
  std::vector<MPVariable*> variables(solver.variables());
  if (result->result_status == MPSolver::OPTIMAL) {
    for (int j = 0; j < num_variables; ++j) {
      result->variable_values[j] = variables[j]->solution_value();
    }
  }
  if (type == MPSolver::GLOP_LINEAR_PROGRAMMING) {
    LPSolver* lp_solver = static_cast<LPSolver*>(solver.underlying_solver());
    CHECK_NOTNULL(lp_solver);
    result->may_have_multiple_solutions =
        lp_solver->MayHaveMultipleOptimalSolutions();
  }
}

void DisplayResults(const std::string& header, const std::vector<std::string>& file_list,
                    const std::vector<InstanceResult>& result) {
  printf("Results for %s:\n", header.c_str());
  TimeDistribution parsing_time_distribution("Parsing time summary");
  TimeDistribution loading_time_distribution("Loading time summary");
  TimeDistribution solving_time_distribution("Solving time summary");
  const int size = result.size();
  int num_solutions_on_facet = 0;
  for (int i = 0; i < size; ++i) {
    parsing_time_distribution.AddTimeInSec(result[i].parsing_time_in_sec);
    loading_time_distribution.AddTimeInSec(result[i].loading_time_in_sec);
    solving_time_distribution.AddTimeInSec(result[i].solving_time_in_sec);
    const std::string status =
        (result[i].result_status == MPSolver::OPTIMAL) ? "Optimal" : "Abnormal";
    num_solutions_on_facet += result[i].may_have_multiple_solutions;
    printf("%s,", file_list[i].c_str());
    printf("%s,", status.c_str());
    printf("%.15e,", result[i].objective_value);
    printf("%f,", result[i].solving_time_in_sec);
    printf("%d,", result[i].may_have_multiple_solutions);
    printf("\n");
  }
  printf("Number of solutions on a facet: %d\n", num_solutions_on_facet);
  printf("%s\n", parsing_time_distribution.StatString().c_str());
  printf("%s\n", loading_time_distribution.StatString().c_str());
  printf("%s\n", solving_time_distribution.StatString().c_str());
}

void Compare(const std::vector<std::string>& file_list,
             const std::vector<InstanceResult>& clp_result,
             const std::vector<InstanceResult>& glop_result) {
  const double cost_tolerance = FLAGS_cost_tolerance;
  const double variable_tolerance = FLAGS_variable_tolerance;
  const int num_instances = file_list.size();
  CHECK_EQ(num_instances, glop_result.size());
  CHECK_EQ(num_instances, clp_result.size());
  int num_differing_instances = 0;
  int num_cost_differences = 0;
  int num_detected_facets = 0;
  operations_research::DoubleDistribution cost_delta("Cost delta summary");
  for (int i = 0; i < num_instances; ++i) {
    const int num_variables = glop_result[i].variable_values.size();
    CHECK_EQ(num_variables, clp_result[i].variable_values.size());
    bool no_difference = true;
    const double glop_value = glop_result[i].objective_value;
    const double clp_value = clp_result[i].objective_value;
    cost_delta.Add(fabs(glop_value - clp_value) /
                   std::max(1.0, std::max(glop_value, clp_value)));
    if (!AreWithinAbsoluteOrRelativeTolerances(
             glop_result[i].objective_value, clp_result[i].objective_value,
             cost_tolerance, cost_tolerance)) {
      ++num_cost_differences;
    }
    for (int var = 0; var < num_variables; ++var) {
      const double glop_value = glop_result[i].variable_values[var];
      const double clp_value = clp_result[i].variable_values[var];
      if (no_difference &&
          !AreWithinAbsoluteOrRelativeTolerances(
               glop_value, clp_value, variable_tolerance, variable_tolerance)) {
        no_difference = false;
        ++num_differing_instances;
        bool multiple_solutions = glop_result[i].may_have_multiple_solutions;
        num_detected_facets += multiple_solutions;
      }
    }
  }
  printf("%d instances have cost with differences >= %E.\n",
         num_cost_differences, cost_tolerance);
  printf("%d instances have variables with differences >= %E.\n",
         num_differing_instances, variable_tolerance);
  printf("%d differing solutions detected to be on a facet.\n",
         num_detected_facets);
  printf("%s\n", cost_delta.StatString().c_str());
}
}  // namespace glop
}  // namespace operations_research

using operations_research::MPSolver;
using operations_research::glop::Compare;
using operations_research::glop::DisplayResults;
using operations_research::glop::InstanceResult;
using operations_research::glop::Solve;

int main(int argc, char* argv[]) {
  InitGoogle(
      "Runs Glop or Clp on a given pattern of files given by --input. "
      "The files must be in text proto format.",
      &argc, &argv, true);
  std::vector<std::string> file_list;
  File::Match(FLAGS_input, &file_list);
  const int size = file_list.size();
  std::vector<InstanceResult> clp_result(size);
  std::vector<InstanceResult> glop_result(size);
  {
    std::unique_ptr<ThreadPool> pool(
        new ThreadPool("LP_Solvers", FLAGS_threads));
    pool->StartWorkers();
    for (int i = 0; i < size; ++i) {
      if (FLAGS_use_clp) {
        pool->Add(NewCallback(&Solve, MPSolver::CLP_LINEAR_PROGRAMMING,
                              file_list[i], &(clp_result[i])));
      }
      if (FLAGS_use_glop) {
        pool->Add(NewCallback(&Solve, MPSolver::GLOP_LINEAR_PROGRAMMING,
                              file_list[i], &(glop_result[i])));
      }
    }
  }
  if (FLAGS_use_clp) {
    DisplayResults("CLP", file_list, clp_result);
  }
  if (FLAGS_use_glop) {
    DisplayResults("Glop", file_list, glop_result);
  }
  if (FLAGS_use_clp && FLAGS_use_glop) {
    Compare(file_list, clp_result, glop_result);
  }
  return EXIT_SUCCESS;
}
