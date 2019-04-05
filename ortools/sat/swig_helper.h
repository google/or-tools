// Copyright 2010-2018 Google LLC
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

#include <atomic>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sigint.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Base class for SWIG director based on solution callbacks.
// See http://www.swig.org/Doc3.0/SWIGDocumentation.html#CSharp_directors.
class SolutionCallback {
 public:
  virtual ~SolutionCallback() {}

  virtual void OnSolutionCallback() const = 0;

  void Run(const operations_research::sat::CpSolverResponse& response) const {
    response_ = response;
    has_response_ = true;
    OnSolutionCallback();
  }

  int64 NumBooleans() const { return response_.num_booleans(); }

  int64 NumBranches() const { return response_.num_branches(); }

  int64 NumConflicts() const { return response_.num_conflicts(); }

  int64 NumBinaryPropagations() const {
    return response_.num_binary_propagations();
  }

  int64 NumIntegerPropagations() const {
    return response_.num_integer_propagations();
  }

  double WallTime() const { return response_.wall_time(); }

  double UserTime() const { return response_.user_time(); }

  double DeterministicTime() const { return response_.deterministic_time(); }

  double ObjectiveValue() const { return response_.objective_value(); }

  double BestObjectiveBound() const { return response_.best_objective_bound(); }

  int64 SolutionIntegerValue(int index) {
    return index >= 0 ? response_.solution(index)
                      : -response_.solution(-index - 1);
  }

  bool SolutionBooleanValue(int index) {
    return index >= 0 ? response_.solution(index) != 0
                      : response_.solution(-index - 1) == 0;
  }

  void StopSearch() { stopped_ = true; }

  std::atomic<bool>* stopped() const { return &stopped_; }

  operations_research::sat::CpSolverResponse Response() const {
    return response_;
  }

  bool HasResponse() const { return has_response_; }

 private:
  mutable CpSolverResponse response_;
  mutable bool has_response_ = false;
  mutable std::atomic<bool> stopped_;
};

class SatHelper {
 public:
  // The arguments of the functions defined below must follow these rules
  // to be wrapped by swig correctly:
  // 1) Their types must include the full operations_research::sat::
  //    namespace.
  // 2) Their names must correspond to the ones declared in the .i
  //    file (see the python/ and java/ subdirectories).
  // 3) String variations of the parameters have been added for C# as
  //    C# protobufs do not support proto2.
  static operations_research::sat::CpSolverResponse Solve(
      const operations_research::sat::CpModelProto& model_proto) {
    FixFlagsAndEnvironmentForSwig();
    Model model;
    std::atomic<bool> stopped(false);
    model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stopped);
    model.GetOrCreate<SigintHandler>()->Register(
        [&stopped]() { stopped = true; });

    return operations_research::sat::SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse SolveWithParameters(
      const operations_research::sat::CpModelProto& model_proto,
      const operations_research::sat::SatParameters& parameters) {
    FixFlagsAndEnvironmentForSwig();
    Model model;
    model.Add(NewSatParameters(parameters));
    std::atomic<bool> stopped(false);
    model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stopped);
    model.GetOrCreate<SigintHandler>()->Register(
        [&stopped]() { stopped = true; });

    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse SolveWithStringParameters(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& parameters) {
    FixFlagsAndEnvironmentForSwig();
    Model model;
    model.Add(NewSatParameters(parameters));
    std::atomic<bool> stopped(false);
    model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stopped);
    model.GetOrCreate<SigintHandler>()->Register(
        [&stopped]() { stopped = true; });

    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SolveWithParametersAndSolutionCallback(
      const operations_research::sat::CpModelProto& model_proto,
      const operations_research::sat::SatParameters& parameters,
      const SolutionCallback& callback) {
    FixFlagsAndEnvironmentForSwig();
    Model model;
    model.Add(NewSatParameters(parameters));
    model.Add(NewFeasibleSolutionObserver(
        [&callback](const CpSolverResponse& r) { return callback.Run(r); }));
    model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
        callback.stopped());
    model.GetOrCreate<SigintHandler>()->Register(
        [&callback]() { *callback.stopped() = true; });

    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SolveWithStringParametersAndSolutionCallback(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& parameters, const SolutionCallback& callback) {
    FixFlagsAndEnvironmentForSwig();
    Model model;
    model.Add(NewSatParameters(parameters));
    model.Add(NewFeasibleSolutionObserver(
        [&callback](const CpSolverResponse& r) { return callback.Run(r); }));
    model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
        callback.stopped());
    model.GetOrCreate<SigintHandler>()->Register(
        [&callback]() { *callback.stopped() = true; });

    return SolveCpModel(model_proto, &model);
  }

  // Returns a std::string with some statistics on the given CpModelProto.
  static std::string ModelStats(
      const operations_research::sat::CpModelProto& model_proto) {
    return CpModelStats(model_proto);
  }

  // Returns a std::string with some statistics on the solver response.
  static std::string SolverResponseStats(
      const operations_research::sat::CpSolverResponse& response) {
    return CpSolverResponseStats(response);
  }

  // Returns a non empty std::string explaining the issue if the model is not
  // valid.
  static std::string ValidateModel(
      const operations_research::sat::CpModelProto& model_proto) {
    return ValidateCpModel(model_proto);
  }
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SWIG_HELPER_H_
