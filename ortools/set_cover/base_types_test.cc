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

#include "ortools/set_cover/base_types.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

template <typename IndexType>
std::vector<IndexType> GenerateIncreasingVector(int start_value, int size) {
  std::vector<IndexType> data(size);
  std::iota(data.begin(), data.end(), IndexType(start_value));
  return data;
}

template <typename ContainerType, typename IndexType>
class IterableContainerTest : public ::testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}

  void CompareVectors(const ContainerType& data_source,
                      const std::vector<IndexType>& expected_data) {
    IterableContainer<ContainerType> container(data_source);
    std::vector<IndexType> actual_data;
    for (const IndexType x : container) {
      actual_data.push_back(x);
    }
    ASSERT_EQ(actual_data.size(), expected_data.size());
    for (size_t i = 0; i < actual_data.size(); ++i) {
      ASSERT_EQ(actual_data[i].value(), expected_data[i].value());
    }
  }
};

using VectorSubsetIndexTest =
    IterableContainerTest<std::vector<SubsetIndex>, SubsetIndex>;
using VectorElementIndexTest =
    IterableContainerTest<std::vector<ElementIndex>, ElementIndex>;

using SpanSubsetIndexTest =
    IterableContainerTest<absl::Span<SubsetIndex>, SubsetIndex>;
using SpanElementIndexTest =
    IterableContainerTest<absl::Span<ElementIndex>, ElementIndex>;

using IndexRangeSubsetIndexTest =
    IterableContainerTest<IndexRange<SubsetIndex>, SubsetIndex>;
using IndexRangeElementIndexTest =
    IterableContainerTest<IndexRange<ElementIndex>, ElementIndex>;

using SparseRowTest = IterableContainerTest<SparseRow, SubsetIndex>;
using SparseColumnTest = IterableContainerTest<SparseColumn, ElementIndex>;

using CompressedRowTest = IterableContainerTest<CompressedRow, SubsetIndex>;
using CompressedColumnTest =
    IterableContainerTest<CompressedColumn, ElementIndex>;

TEST_F(VectorSubsetIndexTest, StdVectorSubsetIndex) {
  std::vector<SubsetIndex> vec = {SubsetIndex(1), SubsetIndex(2),
                                  SubsetIndex(3)};
  std::vector<SubsetIndex> expected_data = vec;
  CompareVectors(vec, expected_data);
}

TEST_F(VectorElementIndexTest, StdVectorElementIndex) {
  std::vector<ElementIndex> vec = {ElementIndex(10), ElementIndex(20),
                                   ElementIndex(30)};
  std::vector<ElementIndex> expected_data = vec;
  CompareVectors(vec, expected_data);
}

TEST_F(SpanSubsetIndexTest, AbslSpanSubsetIndex) {
  std::vector<SubsetIndex> vec = {SubsetIndex(5), SubsetIndex(6),
                                  SubsetIndex(7), SubsetIndex(8)};
  absl::Span<SubsetIndex> span(vec);
  CompareVectors(span, vec);
}

TEST_F(SpanElementIndexTest, AbslSpanElementIndex) {
  std::vector<ElementIndex> vec = {ElementIndex(50), ElementIndex(60),
                                   ElementIndex(70)};
  absl::Span<ElementIndex> span(vec);
  CompareVectors(span, vec);
}

TEST_F(IndexRangeSubsetIndexTest, IndexRangeSubsetIndex) {
  IndexRange range(SubsetIndex(10), SubsetIndex(20));
  std::vector<SubsetIndex> expected_data =
      GenerateIncreasingVector<SubsetIndex>(10, 10);
  CompareVectors(range, expected_data);
}

TEST_F(IndexRangeElementIndexTest, IndexRangeElementIndex) {
  IndexRange range(ElementIndex(100), ElementIndex(110));
  std::vector<ElementIndex> expected_data =
      GenerateIncreasingVector<ElementIndex>(100, 10);
  CompareVectors(range, expected_data);
}

TEST_F(SparseRowTest, StrongVectorSubsetIndex) {
  SparseRow strong_vector{SubsetIndex(1), SubsetIndex(5), SubsetIndex(7)};
  std::vector<SubsetIndex> expected_data = {SubsetIndex(1), SubsetIndex(5),
                                            SubsetIndex(7)};
  CompareVectors(strong_vector, expected_data);
}

TEST_F(SparseColumnTest, StrongVectorElementIndex) {
  SparseColumn strong_vector{ElementIndex(10), ElementIndex(20),
                             ElementIndex(30)};
  std::vector<ElementIndex> expected_data = {ElementIndex(10), ElementIndex(20),
                                             ElementIndex(30)};
  CompareVectors(strong_vector, expected_data);
}

TEST_F(CompressedRowTest, CompressedStrongVectorSubsetIndex) {
  SparseRow original_vector{SubsetIndex(2), SubsetIndex(5), SubsetIndex(8),
                            SubsetIndex(12)};
  CompressedRow compressed_vector(original_vector);
  std::vector<SubsetIndex> expected_data = {SubsetIndex(2), SubsetIndex(5),
                                            SubsetIndex(8), SubsetIndex(12)};
  CompareVectors(compressed_vector, expected_data);
}

TEST_F(CompressedColumnTest, CompressedStrongVectorElementIndex) {
  SparseColumn original_vector{ElementIndex(100), ElementIndex(105),
                               ElementIndex(111)};
  CompressedStrongList<ColumnEntryIndex, ElementIndex> compressed_vector(
      original_vector);
  std::vector<ElementIndex> expected_data = {
      ElementIndex(100), ElementIndex(105), ElementIndex(111)};
  CompareVectors(compressed_vector, expected_data);
}

SparseRow GenerateRandomSparseRow(size_t size, int64_t max_value) {
  SparseRow sparse_row;
  sparse_row.reserve(size);
  absl::BitGen gen;
  std::uniform_int_distribution<int64_t> dist(0, max_value);
  SubsetIndex current_value(0);
  for (size_t i = 0; i < size; ++i) {
    current_value += SubsetIndex(dist(gen));
    sparse_row.push_back(current_value);
  }
  return sparse_row;
}
static void BM_StrongVectorIteration(benchmark::State& state) {
  const size_t size = state.range(0);
  const int64_t delta_range = state.range(1);
  SparseRow strong_vector = GenerateRandomSparseRow(size, delta_range);
  for (auto _ : state) {
    int64_t sum = 0;
    for (const auto& x : strong_vector) {
      sum += x.value();
    }
    benchmark::DoNotOptimize(sum);  // Prevent optimization
  }
}
BENCHMARK(BM_StrongVectorIteration)
    ->ArgsProduct({{100'000, 100'000'000}, {1 << 8, 1 << 16}});

static void BM_CompressedStrongVectorIteration(benchmark::State& state) {
  const size_t size = state.range(0);
  const int64_t delta_range = state.range(1);
  CompressedRow compressed_vector(GenerateRandomSparseRow(size, delta_range));
  for (auto _ : state) {
    int64_t sum = 0;
    for (const auto& x : compressed_vector) {
      sum += x.value();
    }
    benchmark::DoNotOptimize(sum);  // Prevent optimization
  }
}
BENCHMARK(BM_CompressedStrongVectorIteration)
    ->ArgsProduct({
        {100'000, 100'000'000},
        {1 << 8, 1 << 16},
    });

}  // namespace
}  // namespace operations_research
