// Copyright 2010-2025 Google LLC
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

#include "ortools/math_opt/core/c_api/solver.h"

#include <stddef.h>

#include <cstdlib>
#include <cstring>
#include <ios>
#include <limits>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/solver.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/util/solve_interrupter.h"

struct MathOptInterrupter {
  operations_research::SolveInterrupter cpp_interrupter;
};

namespace operations_research::math_opt {
namespace {

// Returns a serialized SolveResultProto and its size. The caller is responsible
// for freeing the result.
absl::StatusOr<std::pair<void*, size_t>> SolveImpl(
    const void* model_bytes, const size_t model_size, const int solver_type,
    MathOptInterrupter* const interrupter, const bool build_result) {
  if (model_bytes == nullptr && model_size != 0) {
    return absl::InvalidArgumentError(
        "model cannot be null unless model_size is zero");
  }
  if (model_size > std::numeric_limits<int>::max()) {
    return util::InvalidArgumentErrorBuilder()
           << "model_size must be at most max int, was: " << model_size;
  }
  ModelProto model;
  if (model_size > 0) {
    if (!model.ParseFromArray(model_bytes, static_cast<int>(model_size))) {
      return absl::InvalidArgumentError("bad model proto");
    }
  }
  Solver::InitArgs init_args;
  Solver::SolveArgs solve_args;
  if (interrupter != nullptr) {
    solve_args.interrupter = &interrupter->cpp_interrupter;
  }
  ASSIGN_OR_RETURN(const SolveResultProto result,
                   Solver::NonIncrementalSolve(
                       model, static_cast<SolverTypeProto>(solver_type),
                       init_args, solve_args));
  const size_t result_size_bytes = result.ByteSizeLong();
  if (result_size_bytes > std::numeric_limits<int>::max()) {
    return util::InvalidArgumentErrorBuilder()
           << "cannot serialize a SolveResultProto with more than INT_MAX = "
           << std::numeric_limits<int>::max() << "(0x" << std::hex
           << std::numeric_limits<int>::max() << std::dec
           << ") bytes, but solve result proto needed " << result_size_bytes
           << " bytes in binary format";
  }
  void* result_bin = nullptr;
  if (build_result) {
    result_bin = malloc(result_size_bytes);
    // For current implementation, only fails on proto3 when the size is bigger
    // than 2 gigs.
    const bool serialize_ok = result.SerializeToArray(
        result_bin, static_cast<int>(result_size_bytes));
    if (!serialize_ok) {
      free(result_bin);
      return absl::InternalError("fail to serialize SolveResultProto");
    }
  }
  return std::make_pair(result_bin, result_size_bytes);
}

}  // namespace
}  // namespace operations_research::math_opt

MathOptInterrupter* MathOptNewInterrupter() { return new MathOptInterrupter(); }

void MathOptFreeInterrupter(MathOptInterrupter* interrupter) {
  delete interrupter;
}

void MathOptInterrupt(MathOptInterrupter* interrupter) {
  CHECK(interrupter != nullptr);
  interrupter->cpp_interrupter.Interrupt();
}
int MathOptIsInterrupted(const MathOptInterrupter* interrupter) {
  CHECK(interrupter != nullptr);
  return static_cast<int>(interrupter->cpp_interrupter.IsInterrupted());
}

int MathOptSolve(const void* model, const size_t model_size,
                 const int solver_type, MathOptInterrupter* const interrupter,
                 void** solve_result, size_t* solve_result_size,
                 char** status_msg) {
  const absl::StatusOr<std::pair<void*, size_t>> result =
      operations_research::math_opt::SolveImpl(
          model, model_size, solver_type, interrupter,
          /*build_result=*/solve_result != nullptr);
  if (result.ok()) {
    if (solve_result_size != nullptr) {
      *solve_result_size = result->second;
    }
    if (solve_result != nullptr) {
      *solve_result = result->first;
    }
    if (status_msg != nullptr) {
      *status_msg = nullptr;
    }
    return 0;
  }
  // WARNING: failure could be caused by null arguments!
  if (status_msg != nullptr) {
    const size_t num_bytes = result.status().message().size() + 1;
    *status_msg = static_cast<char*>(malloc(num_bytes));
    std::memcpy(*status_msg, result.status().message().data(), num_bytes);
  }
  if (solve_result != nullptr) {
    *solve_result = nullptr;
  }
  if (solve_result_size != nullptr) {
    *solve_result_size = 0;
  }
  return result.status().raw_code();
}

void MathOptFree(void* ptr) { free(ptr); }
