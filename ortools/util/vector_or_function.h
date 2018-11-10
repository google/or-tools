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

#ifndef OR_TOOLS_UTIL_VECTOR_OR_FUNCTION_H_
#define OR_TOOLS_UTIL_VECTOR_OR_FUNCTION_H_

#include <algorithm>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"

namespace operations_research {

// Template to abstract the access to STL functions or vector values.
template <typename ScalarType, typename Evaluator>
class VectorOrFunction {
 public:
  explicit VectorOrFunction(Evaluator evaluator)
      : evaluator_(std::move(evaluator)) {}
  void Reset(Evaluator evaluator) { evaluator_ = std::move(evaluator); }
  ScalarType operator()(int i) const { return evaluator_(i); }

 private:
  Evaluator evaluator_;
};

// Specialization for vectors.
template <typename ScalarType>
class VectorOrFunction<ScalarType, std::vector<ScalarType>> {
 public:
  explicit VectorOrFunction(std::vector<ScalarType> values)
      : values_(std::move(values)) {}
  void Reset(std::vector<ScalarType> values) { values_ = std::move(values); }
  ScalarType operator()(int i) const { return values_[i]; }

 private:
  std::vector<ScalarType> values_;
};

// Template to abstract the access to STL functions or vector-base matrix
// values.
template <typename ScalarType, typename Evaluator, bool square = false>
class MatrixOrFunction {
 public:
  explicit MatrixOrFunction(Evaluator evaluator)
      : evaluator_(std::move(evaluator)) {}
  void Reset(Evaluator evaluator) { evaluator_ = std::move(evaluator); }
  ScalarType operator()(int i, int j) const { return evaluator_(i, j); }
  bool Check() const { return true; }

 private:
  Evaluator evaluator_;
};

// Specialization for vector-based matrices.
template <typename ScalarType, bool square>
class MatrixOrFunction<ScalarType, std::vector<std::vector<ScalarType>>,
                       square> {
 public:
  explicit MatrixOrFunction(std::vector<std::vector<ScalarType>> matrix)
      : matrix_(std::move(matrix)) {}
  void Reset(std::vector<std::vector<ScalarType>> matrix) {
    matrix_ = std::move(matrix);
  }
  ScalarType operator()(int i, int j) const { return matrix_[i][j]; }
  // Returns true if the matrix is square or rectangular.
  // Intended to be used in a CHECK.
  bool Check() const {
    if (matrix_.empty()) return true;
    const int size = square ? matrix_.size() : matrix_[0].size();
    const char* msg =
        square ? "Matrix must be square." : "Matrix must be rectangular.";
    for (const std::vector<ScalarType>& row : matrix_) {
      CHECK_EQ(size, row.size()) << msg;
    }
    return true;
  }

 private:
  std::vector<std::vector<ScalarType>> matrix_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_VECTOR_OR_FUNCTION_H_
