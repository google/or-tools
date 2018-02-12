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

#ifndef OR_TOOLS_SAT_SWIG_HELPER_H_
#define OR_TOOLS_SAT_SWIG_HELPER_H_

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

// Useful for director based callbacks.
class SolutionCallback {
 public:
  virtual ~SolutionCallback() {}
  virtual void OnSolutionCallback() = 0;
  void Run(const operations_research::sat::CpSolverResponse& response) {
    response_ = response;
    OnSolutionCallback();
  }

  int64 ObjectiveValue() const {
    return response_.objective_value();
  }

  int64 NumBranches() const {
    return response_.num_branches();
  }

  int64 NumConflicts() const {
    return response_.num_conflicts();
  }

  double WallTime() const {
    return response_.wall_time();
  }

  int64 SolutionIntegerValue(int index) {
    return index >= 0 ? response_.solution(index)
                      : -response_.solution(-index - 1);
  }

  bool SolutionBooleanValue(int index) {
    if (index >= 0) {
      return response_.solution(index) != 0;
    } else {
      return response_.solution(-index - 1) == 0;
    }
  }

 private:
  CpSolverResponse response_;
};

class SatHelper {
 public:
  // The arguments of the functions defined below must follow these rules
  // to be wrapped by swig correctly:
  // 1) Their types must include the full operations_research::sat::
  //    namespace.
  // 2) Their names must correspond to the ones declared in the .i
  //    file (see the python/ and java/ subdirectories).
  static operations_research::sat::CpSolverResponse Solve(
      const operations_research::sat::CpModelProto& model_proto) {
    Model model;
    return operations_research::sat::SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse SolveWithParameters(
      const operations_research::sat::CpModelProto& model_proto,
      const operations_research::sat::SatParameters& parameters) {
    Model model;
    model.Add(NewSatParameters(parameters));
    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse SolveWithStringParameters(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& parameters) {
    Model model;
    model.Add(NewSatParameters(parameters));
    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SolveWithParametersAndSolutionObserver(
      const operations_research::sat::CpModelProto& model_proto,
      const operations_research::sat::SatParameters& parameters,
      std::function<
          void(const operations_research::sat::CpSolverResponse& response)>
          observer) {
    Model model;
    model.Add(NewSatParameters(parameters));
    model.Add(NewFeasibleSolutionObserver(observer));
    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SolveWithStringParametersAndSolutionObserver(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& parameters,
      std::function<
          void(const operations_research::sat::CpSolverResponse& response)>
          observer) {
    Model model;
    model.Add(NewSatParameters(parameters));
    model.Add(NewFeasibleSolutionObserver(observer));
    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SolveWithStringParametersAndSolutionCallback(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& parameters,
      SolutionCallback* callback) {
    Model model;
    model.Add(NewSatParameters(parameters));
    model.Add(NewFeasibleSolutionObserver(
        [callback](const CpSolverResponse& r) { return callback->Run(r); }));
    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SearchAllSolutionsWithStringParametersAndSolutionCallback(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& parameters,
      SolutionCallback* callback) {
    Model model;
    SatParameters sat_parameters;
    sat_parameters.ParseFromString(parameters);
    sat_parameters.set_enumerate_all_solutions(true);
    sat_parameters.set_cp_model_presolve(false);
    model.Add(NewSatParameters(sat_parameters));
    model.Add(NewFeasibleSolutionObserver(
        [callback](const CpSolverResponse& r) { return callback->Run(r); }));
    return SolveCpModel(model_proto, &model);
  }
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SWIG_HELPER_H_
