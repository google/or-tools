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

#include <memory>
#include <random>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

// RandomTriangularMatrix() generates a random sparse triangular matrix
// with specified size (number of rows / columns) and density (approximate
// fraction of non-zero entries).  If diagonal_of_ones is true, the generated
// matrix has a diagonal of all ones.  If upper_triangular is true, the matrix
// is upper triangular, otherwise it is lower triangular.
template <bool upper_triangular>
void RandomTriangularMatrix(absl::BitGenRef randomizer, RowIndex size,
                            double density, TriangularMatrix* matrix,
                            bool diagonal_of_ones) {
  SparseMatrix sparse_matrix;
  if (diagonal_of_ones) {
    FillSparseMatrixWithRandomLowerTriangularMatrix(size, density, randomizer,
                                                    &sparse_matrix);
  } else {
    // We use the fact that FillSparseMatricWithRandomUpperTriangularMatrix()
    // does not have unit diagonal, in contrast with
    // FillSparseMatrixWithRandomLowerTriangularMatrix().
    FillSparseMatrixWithRandomUpperTriangularMatrix(size, density, randomizer,
                                                    &sparse_matrix);
  }
  // We must transpose the matrix if we requested both an upper_triangular
  // matrix and a diagonal_of_ones, or if we requested neither.
  if (upper_triangular == diagonal_of_ones) {
    TriangularMatrix transpose_matrix;
    transpose_matrix.PopulateFromTriangularSparseMatrix(sparse_matrix);
    matrix->PopulateFromTranspose(transpose_matrix);
  } else {
    matrix->PopulateFromTriangularSparseMatrix(sparse_matrix);
  }
}

// TODO(user): Migrate to use go/absl-random after cl/243894758.
void RandomSparseVector(absl::BitGenRef randomizer, RowIndex size,
                        double density, DenseColumn* vector) {
  vector->resize(size, 0.0);
  for (RowIndex row(0); row < size; ++row) {
    if (absl::Bernoulli(randomizer, density)) {
      (*vector)[row] = absl::Uniform<double>(randomizer, -10.0, 10.0);
    }
  }
}

// SolveInternal() provides a common template function to be instantiated
// for each benchmark of triangular solves.  Note that, while the solves
// are typically fast, generating the random matrix with
// RandomTriangularMatrix() can be time-consuming, which restricts the size of
// benchmarks.
template <bool upper_triangular, bool transpose>
void SolveInternal(benchmark::State& state, bool diagonal_of_ones) {
  std::mt19937 randomizer(12345);
  const RowIndex problem_size(state.range(0));
  const double matrix_density = 1.0 / 500;

  TriangularMatrix matrix;
  RandomTriangularMatrix<upper_triangular ^ transpose>(
      randomizer, problem_size, matrix_density, &matrix, diagonal_of_ones);
  DenseColumn rhs;
  RandomSparseVector(randomizer, problem_size, /* density= */ 1.0 / 4000, &rhs);
  // Record the number of nonzeros (not bytes) in the matrix.
  state.SetBytesProcessed(matrix.num_entries().value());

  // Only the code inside the loop below will be benchmarked, not the creation
  // code above.
  for (auto _ : state) {
    // Solves overwrite the input, so to run the same test multiple times
    // we must copy the input.  This should be negligible compared to the solve.
    DenseColumn rhs_copy(rhs);
    if (upper_triangular) {
      if (transpose) {
        matrix.TransposeLowerSolve(&rhs_copy);
      } else {
        matrix.UpperSolve(&rhs_copy);
      }
    } else {
      if (transpose) {
        matrix.TransposeUpperSolve(&rhs_copy);
      } else {
        matrix.LowerSolve(&rhs_copy);
      }
    }
  }
}

// HyperSparseSolveInternal() provides a common template function to be
// instantiated for each benchmark of triangular solves.  Note that, while the
// solves are typically fast, generating the random matrix with
// RandomTriangularMatrix() can be time-consuming, which restricts the size of
// benchmarks.
void HyperSparseSolveInternal(benchmark::State& state, bool diagonal_of_ones) {
  std::mt19937 randomizer(12345);
  const RowIndex problem_size(state.range(0));
  const double matrix_density = 1.0 / 500000;

  std::unique_ptr<TriangularMatrix> matrix;
  DenseColumn rhs;
  RowIndexVector sorted_non_zeros;
  do {
    matrix = std::make_unique<TriangularMatrix>();
    rhs.clear();
    sorted_non_zeros.clear();
    RandomTriangularMatrix<false>(randomizer, problem_size, matrix_density,
                                  matrix.get(), diagonal_of_ones);
    RandomSparseVector(randomizer, problem_size, /* density= */ 1.0 / 4000,
                       &rhs);
    // Compute the non-zero position for the hyper-sparse solves.
    ComputeNonZeros(rhs, &sorted_non_zeros);
    matrix->ComputeRowsToConsiderInSortedOrder(&sorted_non_zeros);
    // It is possible for parameters to be set such that internal tolerances
    // decide not to allow hyper-sparse solves for performance reasons.
  } while (sorted_non_zeros.empty());

  // Record the number of nonzeros (not bytes) in the matrix.
  state.SetBytesProcessed(matrix->num_entries().value());

  // Only the code inside the loop below will be benchmarked, not the creation
  // code above.
  for (auto _ : state) {
    // Solves overwrite the input, so to run the same test multiple times
    // we must copy the input.  This should be negligible compared to the solve.
    DenseColumn rhs_copy(rhs);
    RowIndexVector sorted_non_zeros_copy(sorted_non_zeros);
    matrix->HyperSparseSolveWithReversedNonZeros(&rhs_copy,
                                                 &sorted_non_zeros_copy);
  }
}

enum : bool { kLower = false, kUpper = true };

enum : bool { kNoTranspose = false, kTranspose = true };

template <bool upper_triangular, bool transpose>
void BM_Solve(benchmark::State& state) {
  SolveInternal<upper_triangular, transpose>(state, false);
}

BENCHMARK(BM_Solve<kUpper, kNoTranspose>)->Range(1024, 16384);
BENCHMARK(BM_Solve<kUpper, kTranspose>)->Range(1024, 16384);
BENCHMARK(BM_Solve<kLower, kNoTranspose>)->Range(1024, 16384);
BENCHMARK(BM_Solve<kLower, kTranspose>)->Range(1024, 16384);

template <bool upper_triangular, bool transpose>
void BM_SolveDiagonalOfOnes(benchmark::State& state) {
  SolveInternal<upper_triangular, transpose>(state, true);
}
BENCHMARK(BM_SolveDiagonalOfOnes<kUpper, kNoTranspose>)->Range(1024, 16384);
BENCHMARK(BM_SolveDiagonalOfOnes<kUpper, kTranspose>)->Range(1024, 16384);
BENCHMARK(BM_SolveDiagonalOfOnes<kLower, kNoTranspose>)->Range(1024, 16384);
BENCHMARK(BM_SolveDiagonalOfOnes<kLower, kTranspose>)->Range(1024, 16384);

void BM_HyperSparseSolve(benchmark::State& state) {
  HyperSparseSolveInternal(state, false);
}

BENCHMARK(BM_HyperSparseSolve)->Range(8192, 16384);

void BM_HyperSparseSolveDiagonalOfOnes(benchmark::State& state) {
  HyperSparseSolveInternal(state, true);
}

BENCHMARK(BM_HyperSparseSolveDiagonalOfOnes)->Range(8192, 16384);

}  // namespace
}  // namespace glop
}  // namespace operations_research
