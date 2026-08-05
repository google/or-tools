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

#include <random>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse_vector.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace glop {

namespace {

// We use our own specialization of SparseVector<> with a custom integral type.
DEFINE_STRONG_INDEX_TYPE(TestIndexType);

// Fills sparse_vector with entries of indices from [0, vector_size); each entry
// is set to 1.0 with probability non_zero_probability and to 0.0 (omitted in
// the sparse vector) otherwise; the random values are generated using random.
void MakeRandomSparseVector(int vector_size, double non_zero_probability,
                            absl::BitGenRef random,
                            SparseVector<TestIndexType>* sparse_vector) {
  CHECK(sparse_vector != nullptr);
  sparse_vector->Clear();
  // We sample from a geometric distribution with the exponent parameter equal
  // to non_zero_probability. This is equivalent to iterating over all indices
  // in [0, vector_size) and adding a non-zero entry with probability
  // non_zero_probability, but there is only one iteration of the loop per
  // non-zero entry.
  std::geometric_distribution<> geo_distrib(non_zero_probability);
  for (TestIndexType index(geo_distrib(random)); index < vector_size;
       index += 1 + geo_distrib(random)) {
    sparse_vector->SetCoefficient(index, 1.0);
  }
}

// Measures the speed of the method SparseVector::CleanUp() on vectors with
// vector_size random indices and values. The time needed to populate the vector
// is not counted in the benchmark.
void BM_CleanUp(benchmark::State& state) {
  int vector_size = state.range(0);
  static const int kMaxIndex = 1 << 24;
  std::mt19937 random(12345);
  for (auto _ : state) {
    state.PauseTiming();
    SparseVector<TestIndexType> a;
    a.Reserve(EntryIndex(vector_size));
    const TestIndexType index(absl::Uniform(random, 0, kMaxIndex));
    for (int entry = 0; entry < vector_size; ++entry) {
      a.SetCoefficient(index, absl::Uniform<double>(random, 0.0, 1.0));
    }

    state.ResumeTiming();
    a.CleanUp();
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_CleanUp)->Range(1, 1 << 16);

// Measures the speed of filling the vector by consecutive calls to
// SparseVector::SetCoefficient(). The method SparseVector::Reserve() is not
// used, and the vector grows using the standard growth mechanism.
void BM_FillWithOrganicGrowth(benchmark::State& state) {
  int vector_size = state.range(0);
  static const Fractional kValue = 1.23456;
  for (auto _ : state) {
    SparseVector<TestIndexType> a;
    for (TestIndexType index(0); index < vector_size; ++index) {
      a.SetCoefficient(index, kValue);
    }
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_FillWithOrganicGrowth)->Range(1, 1 << 16);

// Measures the speed of filling the vector by consecutive calls to
// SparseVector::SetCoefficient(). The method SparseVector::Reserve() is used to
// allocate memory for exactly the right number of entries.
void BM_FillWithReserve(benchmark::State& state) {
  int vector_size = state.range(0);
  static const Fractional kValue = 1.23456;
  for (auto _ : state) {
    SparseVector<TestIndexType> a;
    a.Reserve(EntryIndex(vector_size));
    for (TestIndexType index(0); index < vector_size; ++index) {
      a.SetCoefficient(index, kValue);
    }
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_FillWithReserve)->Range(1, 1 << 16);

// Measures the speed of filling the sparse vector with the contents of another
// sparse vector.
void BM_PopulateFromSparseVector(benchmark::State& state) {
  int vector_size = state.range(0);
  static const Fractional kValue = 123.456;
  SparseVector<TestIndexType> a;
  for (TestIndexType index(0); index < vector_size; ++index) {
    a.SetCoefficient(index, kValue);
  }

  for (auto _ : state) {
    SparseVector<TestIndexType> b;
    b.PopulateFromSparseVector(a);
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_PopulateFromSparseVector)->Range(1, 1 << 20);

// Measures the speed of comparing two sparse vectors. In practice, the speed of
// the comparisons depends heavily on whether (and on which entry) they differ.
// In this benchmark, we measure the speed on comparing two equal vectors.
void BM_IsEqualTo(benchmark::State& state) {
  int vector_size = state.range(0);
  static const Fractional kValue = 1234.56;
  SparseVector<TestIndexType> a;
  SparseVector<TestIndexType> b;
  a.Reserve(EntryIndex(vector_size));
  b.Reserve(EntryIndex(vector_size));
  for (TestIndexType index(0); index < vector_size; ++index) {
    a.SetCoefficient(index, kValue);
    b.SetCoefficient(index, kValue);
  }

  int num_equalities = 0;
  for (auto _ : state) {
    num_equalities += a.IsEqualTo(b);
  }
  CHECK_EQ(num_equalities, state.max_iterations);
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_IsEqualTo)->Range(1, 1 << 20);

// Measures the speed of populating a sparse vector from a dense vector. The
// dense vector is initialized randomly; each entry of the dense vector has a
// non-zero value with probability 0.1.
void BM_PopulateFromDenseVector(benchmark::State& state) {
  int vector_size = state.range(0);
  static const double kNonZeroProbability = 0.1;
  StrictITIVector<TestIndexType, Fractional> input_vector;
  input_vector.resize(TestIndexType(vector_size), 0.0);
  std::mt19937 random(12345);
  std::geometric_distribution<> geo_distrib(kNonZeroProbability);
  // We sample from a geometric distribution with the exponent parameter equal
  // to non_zero_probability. This is equivalent to iterating over all indices
  // in [0, vector_size) and setting the value at each index to 1.0 with a
  // non-zero entry with probability non_zero_probability, but there is only one
  // iteration of the loop per non-zero entry.
  for (TestIndexType index(geo_distrib(random)); index < vector_size;
       index += 1 + geo_distrib(random)) {
    input_vector[index] = 1.0;
  }

  for (auto _ : state) {
    SparseVector<TestIndexType> sparse;
    sparse.PopulateFromDenseVector(input_vector);
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}

// Measures the efficiency of iterating over all entries of the sparse vector
// using a range for loop. The body of the loop either computes the sum of all
// indices, the sum of all values, or both, depending on the template arguments
// use_indices and use_values.
template <bool use_indices, bool use_values>
void BM_RangeForLoop(benchmark::State& state) {
  int vector_size = state.range(0);
  SparseVector<TestIndexType> a;
  for (TestIndexType index(0); index < vector_size; ++index) {
    a.SetCoefficient(index, 1.0);
  }

  for (auto _ : state) {
    TestIndexType index_sum(0);
    Fractional coefficient_sum = 0.0;
    for (const SparseVector<TestIndexType>::Entry& entry : a) {
      if (use_indices) index_sum += entry.index();
      if (use_values) coefficient_sum += entry.coefficient();
    }
    if (use_indices) {
      CHECK_EQ(index_sum.value(), vector_size * (vector_size - 1) / 2);
    }
    if (use_values) {
      CHECK_EQ(coefficient_sum, static_cast<Fractional>(vector_size));
    }
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK_TEMPLATE(BM_RangeForLoop, false, true)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_RangeForLoop, true, false)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_RangeForLoop, true, true)->Range(1, 1 << 20);

// Measures the speed of populating a dense vector from a sparse vector with a
// 10% fill in.
void BM_CopyToDenseVector(benchmark::State& state) {
  int vector_size = state.range(0);
  static const double kNonZeroProbability = 0.1;
  std::mt19937 random(12345);
  SparseVector<TestIndexType> a;
  MakeRandomSparseVector(vector_size, kNonZeroProbability, random, &a);

  for (auto _ : state) {
    StrictITIVector<TestIndexType, Fractional> output_vector;
    a.CopyToDenseVector(TestIndexType(vector_size), &output_vector);
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_CopyToDenseVector)->Range(1, 1 << 20);

// Measures the speed of multiplying all values by a scalar constant.
void BM_MultiplyByConstant(benchmark::State& state) {
  int vector_size = state.range(0);
  // The term subtracted from 1.0 must be big enough so that the compiler does
  // not optimize the whole for loop away.
  static const Fractional kFactor = 1.0 - 1e-15;
  SparseVector<TestIndexType> a;
  for (TestIndexType index(0); index < vector_size; ++index) {
    a.SetCoefficient(index, 1.0);
  }

  for (auto _ : state) {
    a.MultiplyByConstant(kFactor);
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_MultiplyByConstant)->Range(1, 1 << 20);

// Measures the speed of the AddMultipleToSparseVector methods. The benchmark
// adds two vectors with 30% fill-in, to have at least a certain probability of
// overlap.
void BM_AddMultipleToSparseVector(benchmark::State& state) {
  int vector_size = state.range(0);
  static const double kNonZeroProbability = 0.3;
  static const Fractional kMultiplier = 123.456;
  const TestIndexType common_index(vector_size / 2);

  for (auto _ : state) {
    state.PauseTiming();
    std::mt19937 random(12345);
    SparseVector<TestIndexType> a;
    SparseVector<TestIndexType> b;
    MakeRandomSparseVector(vector_size, kNonZeroProbability, random, &a);
    MakeRandomSparseVector(vector_size, kNonZeroProbability, random, &b);
    // Make sure that all vectors have a non-zero value at the common index;
    // since we add the entries "out of order", we need to clean up the vectors
    // afterwards.
    a.SetCoefficient(common_index, 1.0);
    b.SetCoefficient(common_index, 1.0);
    a.CleanUp();
    b.CleanUp();
    state.ResumeTiming();

    a.AddMultipleToSparseVectorAndIgnoreCommonIndex(kMultiplier, common_index,
                                                    1e-12, &b);
  }
  state.SetBytesProcessed(sizeof(Fractional) * state.max_iterations *
                          vector_size);
}
BENCHMARK(BM_AddMultipleToSparseVector)->Range(1, 1 << 20);

}  // namespace
}  // namespace glop
}  // namespace operations_research
