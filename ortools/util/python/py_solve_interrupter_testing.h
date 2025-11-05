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

// Library to unit test PySolveInterrupter wrapper.
#ifndef ORTOOLS_UTIL_PYTHON_PY_SOLVE_INTERRUPTER_TESTING_H_
#define ORTOOLS_UTIL_PYTHON_PY_SOLVE_INTERRUPTER_TESTING_H_

#include <memory>
#include <optional>
#include <utility>

#include "absl/base/nullability.h"
#include "ortools/util/python/py_solve_interrupter.h"

namespace operations_research {

// Returns:
// * nullopt if `interrupter` is nullptr,
// * or false if `interrupter` is not nullptr and is not interrupted,
// * or true if `interrupted` is not nullptr and is interrupted.
//
// The Clif/pybind11 wrapper will return a `bool | None` value, with None for
// nullopt.
std::optional<bool> IsInterrupted(const PySolveInterrupter* interrupter);

// Class that keeps a reference on a std::shared_ptr<PySolveInterrupter> to
// test that the C++ object survive the cleanup of the Python reference.
class PySolveInterrupterReference {
 public:
  explicit PySolveInterrupterReference(
      absl_nonnull std::shared_ptr<PySolveInterrupter> interrupter)
      : interrupter_(std::move(interrupter)) {}

  PySolveInterrupterReference(const PySolveInterrupterReference&) = delete;
  PySolveInterrupterReference& operator=(const PySolveInterrupterReference&) =
      delete;

  // Returns the std::shared_ptr<PySolveInterrupter>::use_count().
  //
  // This is used to test that Python has stopped pointing to the object.
  int use_count() const { return interrupter_.use_count(); }

  // Returns true if the underlying interrupter is interrupted.
  bool is_interrupted() const { return interrupter_->IsInterrupted(); }

 private:
  const absl_nonnull std::shared_ptr<PySolveInterrupter> interrupter_;
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_PYTHON_PY_SOLVE_INTERRUPTER_TESTING_H_
