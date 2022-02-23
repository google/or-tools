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
  static std::string ExportModelProtoToMpsString(
      const operations_research::MPModelProto& input_model,
      const operations_research::MPModelExportOptions& options =
          MPModelExportOptions());

  static std::string ExportModelProtoToLpString(
      const operations_research::MPModelProto& input_model,
      const operations_research::MPModelExportOptions& options =
          MPModelExportOptions());

  static operations_research::MPModelProto ImportFromMpsString(
      const std::string& mps_string);
  static operations_research::MPModelProto ImportFromMpsFile(
      const std::string& mps_file);
  static operations_research::MPModelProto ImportFromLpString(
      const std::string& lp_string);
  static operations_research::MPModelProto ImportFromLpFile(
      const std::string& lp_file);
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
  operations_research::MPSolutionResponse Solve(
      const operations_research::MPModelRequest& request);

  // Returns true if the interrupt signal was correctly sent, that is if the
  // underlying solver supports it.
  bool InterruptSolve();

  void SetLogCallback(std::function<void(const std::string&)> log_callback);
  void SetLogCallbackFromDirectorClass(LogCallback* log_callback);

 private:
  std::atomic<bool> interrupt_solve_;
  std::function<void(const std::string&)> log_callback_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_MODEL_BUILDER_WRAPPERS_MODEL_BUILDER_HELPER_H_
