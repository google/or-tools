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

#ifndef OR_TOOLS_MODEL_BUILDER_WRAPPERS_MODEL_BUILDER_HELPER_H_
#define OR_TOOLS_MODEL_BUILDER_WRAPPERS_MODEL_BUILDER_HELPER_H_

#include <atomic>
#include <functional>
#include <limits>
#include <optional>
#include <string>

#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/util/logging.h"

namespace operations_research {

// The arguments of the functions defined below must follow these rules
// to be wrapped by SWIG correctly:
// 1) Their types must include the full operations_research::
//    namespace.
// 2) Their names must correspond to the ones declared in the .i
//    file (see the java/ and csharp/ subdirectories).

// Helper for importing/exporting models and model protobufs.
//
// Wrapping global function is brittle with SWIG. It is much easier to
// wrap static class methods.
//
// Note: all these methods rely on c++ code that uses absl::Status or
// absl::StatusOr. Unfortunately, these are inconsistently wrapped in non C++
// languages. As a consequence, we need to provide an API that does not involve
// absl::Status or absl::StatusOr.
class ModelBuilderHelper {
 public:
  std::string ExportToMpsString(const operations_research::MPModelExportOptions&
                                    options = MPModelExportOptions());
  std::string ExportToLpString(const operations_research::MPModelExportOptions&
                                   options = MPModelExportOptions());
  bool WriteModelToFile(const std::string& filename);

  bool ImportFromMpsString(const std::string& mps_string);
  bool ImportFromMpsFile(const std::string& mps_file);
  bool ImportFromLpString(const std::string& lp_string);
  bool ImportFromLpFile(const std::string& lp_file);

  const MPModelProto& model() const;
  MPModelProto* mutable_model();

  // Direct low level model building API.
  int AddVar();
  void SetVarLowerBound(int var_index, double lb);
  void SetVarUpperBound(int var_index, double ub);
  void SetVarIntegrality(int var_index, bool is_integer);
  void SetVarObjectiveCoefficient(int var_index, double coeff);
  void SetVarName(int var_index, const std::string& name);

  int AddLinearConstraint();
  void SetConstraintLowerBound(int ct_index, double lb);
  void SetConstraintUpperBound(int ct_index, double ub);
  void AddConstraintTerm(int ct_index, int var_index, double coeff);
  void SetConstraintName(int ct_index, const std::string& name);

  int num_variables() const;
  double VarLowerBound(int var_index) const;
  double VarUpperBound(int var_index) const;
  bool VarIsIntegral(int var_index) const;
  double VarObjectiveCoefficient(int var_index) const;
  std::string VarName(int var_index) const;

  int num_constraints() const;
  double ConstraintLowerBound(int ct_index) const;
  double ConstraintUpperBound(int ct_index) const;
  std::string ConstraintName(int ct_index) const;
  std::vector<int> ConstraintVarIndices(int ct_index) const;
  std::vector<double> ConstraintCoefficients(int ct_index) const;

  std::string name() const;
  void SetName(const std::string& name);

  bool maximize() const;
  void SetMaximize(bool maximize);
  double ObjectiveOffset() const;
  void SetObjectiveOffset(double offset);

 private:
  MPModelProto model_;
};

// Simple director class for C#.
class LogCallback {
 public:
  virtual ~LogCallback() {}
  virtual void NewMessage(const std::string& message) = 0;
};

// Class used to solve a request. This class is not meant to be exposed to the
// public. Its responsibility is to bridge the MPModelProto in the non-C++
// languages with the C++ Solve method.
//
// It contains 2 helper objects: a logger, and an atomic bool to interrupt
// search.
class ModelSolverHelper {
 public:
  void Solve(const ModelBuilderHelper& model);

  // Only used by the CVXPY interface. Does not store the response internally.
  // interrupt_solve_ is passed to the solve method.
  std::optional<MPSolutionResponse> SolveRequest(const MPModelRequest& request);

  // Returns true if the interrupt signal was correctly sent, that is if the
  // underlying solver supports it.
  bool InterruptSolve();

  void SetLogCallback(std::function<void(const std::string&)> log_callback);
  void SetLogCallbackFromDirectorClass(LogCallback* log_callback);

  bool has_response() const;
  const MPSolutionResponse& response() const;
  MPSolverResponseStatus status() const;

  // If not defined, or no solution, they will silently return 0.
  double objective_value() const;
  double best_objective_bound() const;
  double variable_value(int var_index) const;
  double reduced_cost(int var_index) const;
  double dual_value(int ct_index) const;

  std::string status_string() const;

  // Solve parameters.
  bool SetSolverName(const std::string& solver_name);
  void SetTimeLimitInSeconds(double limit);
  void SetSolverSpecificParameters(
      const std::string& solver_specific_parameters);

  // TODO(user): set parameters.

 private:
  std::atomic<bool> interrupt_solve_;
  std::function<void(const std::string&)> log_callback_;
  std::optional<MPSolutionResponse> response_;
  MPModelRequest::SolverType solver_type_ =
      MPModelRequest::GLOP_LINEAR_PROGRAMMING;
  std::optional<double> time_limit_in_second_;
  std::string solver_specific_parameters_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_MODEL_BUILDER_WRAPPERS_MODEL_BUILDER_HELPER_H_
