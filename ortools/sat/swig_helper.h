// Copyright 2010-2022 Google LLC
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
#include <cstdint>
#include <functional>
#include <string>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

// Base class for SWIG director based on solution callbacks.
// See http://www.swig.org/Doc4.0/SWIGDocumentation.html#CSharp_directors.
class SolutionCallback {
 public:
  virtual ~SolutionCallback();

  virtual void OnSolutionCallback() const = 0;

  void Run(const operations_research::sat::CpSolverResponse& response) const;

  int64_t NumBooleans() const;

  int64_t NumBranches() const;

  int64_t NumConflicts() const;

  int64_t NumBinaryPropagations() const;

  int64_t NumIntegerPropagations() const;

  double WallTime() const;

  double UserTime() const;

  double DeterministicTime() const;

  double ObjectiveValue() const;

  double BestObjectiveBound() const;

  int64_t SolutionIntegerValue(int index);

  bool SolutionBooleanValue(int index);

  // Stops the search.
  void StopSearch();

  operations_research::sat::CpSolverResponse Response() const;

  // We use mutable and non const methods to overcome SWIG difficulties.
  void SetAtomicBooleanToStopTheSearch(std::atomic<bool>* stopped_ptr) const;

  bool HasResponse() const;

 private:
  mutable CpSolverResponse response_;
  mutable bool has_response_ = false;
  mutable std::atomic<bool>* stopped_ptr_;
};

// Simple director class for C#.
class LogCallback {
 public:
  virtual ~LogCallback() {}
  virtual void NewMessage(const std::string& message) = 0;
};

// This class is not meant to be reused after one solve.
class SolveWrapper {
 public:
  // The arguments of the functions defined below must follow these rules
  // to be wrapped by swig correctly:
  // 1) Their types must include the full operations_research::sat::
  //    namespace.
  // 2) Their names must correspond to the ones declared in the .i
  //    file (see the python/ and java/ subdirectories).
  // 3) String variations of the parameters have been added for C# as
  //    C# protobufs do not support proto2.

  void SetParameters(const operations_research::sat::SatParameters& parameters);

  void SetStringParameters(const std::string& string_parameters);

  void AddSolutionCallback(const SolutionCallback& callback);
  void ClearSolutionCallback(const SolutionCallback& callback);
  void AddLogCallback(std::function<void(const std::string&)> log_callback);

  // Workaround for C#.
  void AddLogCallbackFromClass(LogCallback* log_callback);

  operations_research::sat::CpSolverResponse Solve(
      const operations_research::sat::CpModelProto& model_proto);

  void StopSearch();

 private:
  Model model_;
  std::atomic<bool> stopped_ = false;
};

// Static methods are stored in a module which name can vary.
// To avoid this issue, we put all global functions as a static method of the
// CpSatHelper class.
struct CpSatHelper {
 public:
  // Returns a string with some statistics on the given CpModelProto.
  static std::string ModelStats(
      const operations_research::sat::CpModelProto& model_proto);

  // Returns a string with some statistics on the solver response.
  static std::string SolverResponseStats(
      const operations_research::sat::CpSolverResponse& response);

  // Returns a non empty string explaining the issue if the model is not valid.
  static std::string ValidateModel(
      const operations_research::sat::CpModelProto& model_proto);

  // Rebuilds a domain from an integer variable proto.
  static operations_research::Domain VariableDomain(
      const operations_research::sat::IntegerVariableProto& variable_proto);

  // Write the model proto to file. If the filename ends with 'txt', the model
  // will be written as a text file, otherwise, the binary format will be used.
  // The functions returns true if the model was correctly written.
  static bool WriteModelToFile(
      const operations_research::sat::CpModelProto& model_proto,
      const std::string& filename);
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SWIG_HELPER_H_
