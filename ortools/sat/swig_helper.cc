// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/swig_helper.h"

#include <stdint.h>

#include <functional>
#include <string>

#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

SolutionCallback::~SolutionCallback() = default;

void SolutionCallback::Run(
    const operations_research::sat::CpSolverResponse& response) const {
  response_ = response;
  has_response_ = true;
  OnSolutionCallback();
}

int64_t SolutionCallback::NumBooleans() const {
  return response_.num_booleans();
}

int64_t SolutionCallback::NumBranches() const {
  return response_.num_branches();
}

int64_t SolutionCallback::NumConflicts() const {
  return response_.num_conflicts();
}

int64_t SolutionCallback::NumBinaryPropagations() const {
  return response_.num_binary_propagations();
}

int64_t SolutionCallback::NumIntegerPropagations() const {
  return response_.num_integer_propagations();
}

double SolutionCallback::WallTime() const { return response_.wall_time(); }

double SolutionCallback::UserTime() const { return response_.user_time(); }

double SolutionCallback::DeterministicTime() const {
  return response_.deterministic_time();
}

double SolutionCallback::ObjectiveValue() const {
  return response_.objective_value();
}

double SolutionCallback::BestObjectiveBound() const {
  return response_.best_objective_bound();
}

int64_t SolutionCallback::SolutionIntegerValue(int index) const {
  return index >= 0 ? response_.solution(index)
                    : -response_.solution(-index - 1);
}

bool SolutionCallback::SolutionBooleanValue(int index) const {
  return index >= 0 ? response_.solution(index) != 0
                    : response_.solution(-index - 1) == 0;
}

void SolutionCallback::StopSearch() {
  if (wrapper_ != nullptr) wrapper_->StopSearch();
}

const operations_research::sat::CpSolverResponse& SolutionCallback::Response()
    const {
  return response_;
}

void SolutionCallback::SetWrapperClass(SolveWrapper* wrapper) const {
  wrapper_ = wrapper;
}

bool SolutionCallback::HasResponse() const { return has_response_; }

void SolveWrapper::SetParameters(
    const operations_research::sat::SatParameters& parameters) {
  model_.Add(NewSatParameters(parameters));
}

void SolveWrapper::SetStringParameters(const std::string& string_parameters) {
  model_.Add(NewSatParameters(string_parameters));
}

void SolveWrapper::AddSolutionCallback(const SolutionCallback& callback) {
  callback.SetWrapperClass(this);
  model_.Add(NewFeasibleSolutionObserver(
      [&callback](const CpSolverResponse& r) { return callback.Run(r); }));
}

void SolveWrapper::ClearSolutionCallback(const SolutionCallback& callback) {
  callback.SetWrapperClass(nullptr);  // Detach the wrapper class.
}

void SolveWrapper::AddLogCallback(
    std::function<void(const std::string&)> log_callback) {
  if (log_callback != nullptr) {
    model_.GetOrCreate<SolverLogger>()->AddInfoLoggingCallback(log_callback);
  }
}

void SolveWrapper::AddLogCallbackFromClass(LogCallback* log_callback) {
  model_.GetOrCreate<SolverLogger>()->AddInfoLoggingCallback(
      [log_callback](const std::string& message) {
        log_callback->NewMessage(message);
      });
}

void SolveWrapper::AddBestBoundCallback(
    std::function<void(double)> best_bound_callback) {
  if (best_bound_callback != nullptr) {
    model_.Add(NewBestBoundCallback(best_bound_callback));
  }
}

void SolveWrapper::AddBestBoundCallbackFromClass(BestBoundCallback* callback) {
  DCHECK(callback != nullptr);
  model_.Add(NewBestBoundCallback(
      [callback](double bound) { callback->NewBestBound(bound); }));
}

operations_research::sat::CpSolverResponse SolveWrapper::Solve(
    const operations_research::sat::CpModelProto& model_proto) {
  FixFlagsAndEnvironmentForSwig();
  return operations_research::sat::SolveCpModel(model_proto, &model_);
}

void SolveWrapper::StopSearch() {
  model_.GetOrCreate<ModelSharedTimeLimit>()->Stop();
}

std::string CpSatHelper::ModelStats(
    const operations_research::sat::CpModelProto& model_proto) {
  return CpModelStats(model_proto);
}

std::string CpSatHelper::SolverResponseStats(
    const operations_research::sat::CpSolverResponse& response) {
  return CpSolverResponseStats(response);
}

std::string CpSatHelper::ValidateModel(
    const operations_research::sat::CpModelProto& model_proto) {
  return ValidateCpModel(model_proto);
}

operations_research::Domain CpSatHelper::VariableDomain(
    const operations_research::sat::IntegerVariableProto& variable_proto) {
  return ReadDomainFromProto(variable_proto);
}

bool CpSatHelper::WriteModelToFile(
    const operations_research::sat::CpModelProto& model_proto,
    const std::string& filename) {
  return WriteModelProtoToFile(model_proto, filename);
}

}  // namespace sat
}  // namespace operations_research
