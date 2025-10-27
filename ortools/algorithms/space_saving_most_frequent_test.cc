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

#include "ortools/algorithms/space_saving_most_frequent.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

using ssmf_internal::BoundedAllocator;
using ssmf_internal::DoubleLinkedList;
using ssmf_internal::Ptr;
using ::testing::ElementsAre;
using ::testing::Pair;

TEST(BoundedAllocator, Alloc) {
  BoundedAllocator<int> allocator(1);
  EXPECT_TRUE(allocator.empty());
  EXPECT_FALSE(allocator.full());
  auto p = allocator.New();
  EXPECT_FALSE(allocator.empty());
  EXPECT_TRUE(allocator.full());
  *p = 42;
  allocator.Return(std::move(p));
  EXPECT_TRUE(allocator.empty());
  EXPECT_FALSE(allocator.full());
}

TEST(BoundedAllocator, FromFreeList) {
  BoundedAllocator<int> allocator(1);
  auto p = allocator.New();
  *p = 42;
  allocator.Return(std::move(p));
  auto q = allocator.New();
  EXPECT_EQ(*q, 0);
  allocator.Return(std::move(q));
}

TEST(BoundedAllocator, UnReturnedItems) {
  ASSERT_DEATH(
      {
        BoundedAllocator<int> allocator(1);
        allocator.New();  // Allocated item not Return-ed
      },
      "");
}

TEST(BoundedAllocator, Disposed) {
  BoundedAllocator<int> allocator(1);
  EXPECT_TRUE(allocator.empty());
  EXPECT_FALSE(allocator.full());
  allocator.DisposeAll();
  // Allocator becomes unusable.
  EXPECT_TRUE(allocator.empty());
  EXPECT_TRUE(allocator.full());
}

struct Node {
  int value = 0;
  Node* absl_nullable next = nullptr;
  Node* absl_nullable prev = nullptr;
};

class DoublyLinkedListTest : public ::testing::Test {
 public:
  DoublyLinkedListTest() : allocator_(10) {}

  void TearDown() override {
    while (!list_.empty()) {
      allocator_.Return(list_.pop_front());
    }
  }

  static std::vector<int> AsVector(const DoubleLinkedList<Node>& list) {
    std::vector<int> values;
    if (!list.empty()) {
      for (Node* node = list.front(); node != nullptr; node = node->next) {
        values.push_back(node->value);
      }
    }
    return values;
  }

 protected:
  BoundedAllocator<Node> allocator_;
  DoubleLinkedList<Node> list_;
};

TEST_F(DoublyLinkedListTest, EmptyList) {
  EXPECT_TRUE(list_.empty());
  EXPECT_FALSE(list_.single());
}

TEST_F(DoublyLinkedListTest, InsertFront) {
  Ptr<Node> node1 = allocator_.New();
  node1->value = 1;
  list_.insert_front(std::move(node1));
  EXPECT_FALSE(list_.empty());
  EXPECT_TRUE(list_.single());
  EXPECT_THAT(AsVector(list_), ElementsAre(1));

  Ptr<Node> node2 = allocator_.New();
  node2->value = 2;
  list_.insert_front(std::move(node2));
  EXPECT_FALSE(list_.empty());
  EXPECT_FALSE(list_.single());
  EXPECT_THAT(AsVector(list_), ElementsAre(2, 1));
}

TEST_F(DoublyLinkedListTest, InsertBack) {
  auto* node1 = list_.insert_back(allocator_.New());
  node1->value = 1;
  EXPECT_FALSE(list_.empty());
  EXPECT_TRUE(list_.single());
  EXPECT_THAT(AsVector(list_), ElementsAre(1));

  auto* node2 = list_.insert_back(allocator_.New());
  node2->value = 2;
  EXPECT_FALSE(list_.empty());
  EXPECT_FALSE(list_.single());
  EXPECT_THAT(AsVector(list_), ElementsAre(1, 2));
}

TEST_F(DoublyLinkedListTest, InsertAfter) {
  Node* node1 = list_.insert_back(allocator_.New());
  node1->value = 1;

  Node* node2 = list_.insert_back(allocator_.New());
  node2->value = 2;

  Node* node3 = list_.insert_after(node1, allocator_.New());
  node3->value = 3;
  EXPECT_THAT(AsVector(list_), ElementsAre(1, 3, 2));

  Node* node4 = list_.insert_after(node2, allocator_.New());
  node4->value = 4;
  EXPECT_THAT(AsVector(list_), ElementsAre(1, 3, 2, 4));
}

TEST_F(DoublyLinkedListTest, InsertBefore) {
  auto* node1 = list_.insert_back(allocator_.New());
  node1->value = 1;

  auto* node2 = list_.insert_back(allocator_.New());
  node2->value = 2;

  auto* node3 = list_.insert_before(node2, allocator_.New());
  node3->value = 3;
  EXPECT_THAT(AsVector(list_), ElementsAre(1, 3, 2));

  auto* node4 = list_.insert_before(node1, allocator_.New());
  node4->value = 4;
  EXPECT_THAT(AsVector(list_), ElementsAre(4, 1, 3, 2));
}

TEST_F(DoublyLinkedListTest, Erase) {
  auto* node1 = list_.insert_back(allocator_.New());
  node1->value = 1;

  auto* node2 = list_.insert_back(allocator_.New());
  node2->value = 2;

  auto* node3 = list_.insert_back(allocator_.New());
  node3->value = 3;
  EXPECT_THAT(AsVector(list_), ElementsAre(1, 2, 3));

  allocator_.Return(list_.erase(node2));
  EXPECT_THAT(AsVector(list_), ElementsAre(1, 3));

  allocator_.Return(list_.erase(node1));
  EXPECT_THAT(AsVector(list_), ElementsAre(3));
  EXPECT_TRUE(list_.single());

  allocator_.Return(list_.erase(node3));
  EXPECT_THAT(AsVector(list_), ElementsAre());
  EXPECT_TRUE(list_.empty());
}

// Very inefficient but very simple implementation of Space-Saving. Should
// return the same results as SpaceSavingMostFrequent.
template <typename T>
class SpaceSavingMostFrequentNaive {
 public:
  explicit SpaceSavingMostFrequentNaive(int storage_size)
      : storage_size_(storage_size), contents_(storage_size) {
    CHECK_GT(storage_size, 0);
  }

  void Add(T value) {
    ++current_timestamp_;

    // If the value is already in the list, update its count and timestamp.
    for (auto& item : contents_) {
      if (item.value == value) {
        item.IncrementAndUpdate(current_timestamp_);
        return;
      }
    }

    // Otherwise, replace the least frequent item with the new one.
    auto it2 = absl::c_min_element(contents_);
    *it2 = ItemCountAndTimestamp{
        .count = 1, .timestamp = current_timestamp_, .value = value};
  }

  void FullyRemove(T value) {
    for (auto& item : contents_) {
      if (item.value == value) {
        item.Clear();
        return;
      }
    }
  }

  std::vector<std::pair<T, int64_t>> GetMostFrequent(int num_samples) {
    std::vector<std::pair<T, int64_t>> result;

    absl::c_sort(contents_, std::greater<ItemCountAndTimestamp>());
    for (const auto& [count, timestamp, value] : contents_) {
      if (count < 0) break;
      if (result.size() == num_samples) break;
      result.push_back({*value, count});
    }
    return result;
  }

 private:
  int storage_size_;
  int64_t current_timestamp_ = 0;

  struct ItemCountAndTimestamp {
    int64_t count = -1;
    int64_t timestamp = -1;
    std::optional<T> value;

    void IncrementAndUpdate(int64_t new_timestamp) {
      ++count;
      timestamp = new_timestamp;
    }

    void Clear() {
      count = -1;
      timestamp = -1;
      value = std::nullopt;
    }

    auto AsTuple() const { return std::tie(count, timestamp, value); }

    friend bool operator<(const ItemCountAndTimestamp& a,
                          const ItemCountAndTimestamp& b) {
      return a.AsTuple() < b.AsTuple();
    }

    friend bool operator>(const ItemCountAndTimestamp& a,
                          const ItemCountAndTimestamp& b) {
      return a.AsTuple() > b.AsTuple();
    }
  };

  std::vector<ItemCountAndTimestamp> contents_;
};

template <typename T>
struct Implementations {
  explicit Implementations(int storage_size)
      : impl(storage_size), naive(storage_size) {}

  void Add(T value) {
    impl.Add(value);
    naive.Add(value);
  }
  void FullyRemove(T value) {
    impl.FullyRemove(value);
    naive.FullyRemove(value);
  }

  std::vector<std::pair<T, int64_t>> GetMostFrequent(int num_samples) {
    const std::vector<std::pair<T, int64_t>> impl_result =
        impl.GetMostFrequent(num_samples);
    const std::vector<std::pair<T, int64_t>> naive_result =
        naive.GetMostFrequent(num_samples);
    EXPECT_THAT(impl_result, naive_result);
    return impl_result;
  }

  void CheckIdenticalResults(int num_samples) {
    CHECK_EQ(impl.GetMostFrequent(num_samples),
             naive.GetMostFrequent(num_samples));
  }

  SpaceSavingMostFrequent<T> impl;
  SpaceSavingMostFrequentNaive<T> naive;
};

TEST(SpaceSavingMostFrequent, SimpleExamples) {
  Implementations<std::string> most_frequent(5);

  most_frequent.Add("a");  // 1 : a
  most_frequent.Add("b");  // 1 : a, b
  most_frequent.Add("c");  // 1 : a, b, c
  most_frequent.Add("d");  // 1 : a, b, c, d
  most_frequent.Add("e");  // 1 : a, b, c, d, e
  most_frequent.Add("a");  // 2 : a | 1 : b, c, d, e
  most_frequent.Add("a");  // 3 : a | 1 : b, c, d, e
  most_frequent.Add("a");  // 4 : a | 1 : b, c, d, e
  most_frequent.Add("b");  // 4 : a | 2 : b | 1 : c, d, e
  most_frequent.Add("c");  // 4 : a | 2 : b, c | 1 : d, e
  most_frequent.Add("d");  // 4 : a | 2 : b, c, d | 1 : e
  most_frequent.Add("e");  // 4 : a | 2 : b, c, d, e

  // Eviction starts.
  most_frequent.Add("f");  // 4 : a | 2 : c, d, e | 1 : f (b was evicted)
  most_frequent.Add("g");  // 4 : a | 2 : c, d, e | 1 : g (f was evicted)
  most_frequent.Add("h");  // 4 : a | 2 : c, d, e | 1 : h (g was evicted)
  most_frequent.Add("i");  // 4 : a | 2 : c, d, e | 1 : i (h was evicted)
  most_frequent.Add("j");  // 4 : a | 2 : c, d, e | 1 : j (i was evicted)
  most_frequent.Add("k");  // 4 : a | 2 : c, d, e | 1 : k (j was evicted)
  most_frequent.Add("l");  // 4 : a | 2 : c, d, e | 1 : l (k was evicted)
  most_frequent.Add("m");  // 4 : a | 2 : c, d, e | 1 : m (l was evicted)
  most_frequent.Add("n");  // 4 : a | 2 : c, d, e | 1 : n (m was evicted)
  most_frequent.Add("o");  // 4 : a | 2 : c, d, e | 1 : o (n was evicted)
  most_frequent.Add("p");  // 4 : a | 2 : c, d, e | 1 : p (o was evicted)
  most_frequent.Add("p");  // 4 : a | 2 : c, d, e, p
  most_frequent.Add("p");  // 4 : a | 3 : p | 2 : c, d, e

  EXPECT_THAT(most_frequent.GetMostFrequent(10),
              ElementsAre(Pair("a", 4), Pair("p", 3), Pair("e", 2),
                          Pair("d", 2), Pair("c", 2)));

  most_frequent.FullyRemove("c");  // 4 : a | 3 : p | 2 : d, e
  most_frequent.Add("f");          // 4 : a | 3 : p | 2 : d, e | 1 : f

  EXPECT_THAT(most_frequent.GetMostFrequent(10),
              ElementsAre(Pair("a", 4), Pair("p", 3), Pair("e", 2),
                          Pair("d", 2), Pair("f", 1)));
}

TEST(SpaceSavingMostFrequent, CornerCase) {
  Implementations<std::string> most_frequent(5);

  most_frequent.Add("a");  // 1 : a
  most_frequent.Add("b");  // 1 : a, b
  most_frequent.Add("c");  // 1 : a, b, c
  most_frequent.Add("d");  // 1 : a, b, c, d
  most_frequent.Add("e");  // 1 : a, b, c, d, e
  most_frequent.Add("f");  // 1 : b, c, d, e, f
  most_frequent.Add("g");  // 1 : c, d, e, f, g

  // Eviction starts.
  most_frequent.Add("x");  // 1 : d, e, f, g, x (a was evicted)
  most_frequent.Add("y");  // 1 : e, f, g, x, y (d was evicted)

  // Here's an example of why we should remove the oldest item in case of a
  // tie on the frequency count: we don't want "y" to remove the "x".
  most_frequent.Add("x");  // 2 : x | 1 : e, f, g, y
  most_frequent.Add("y");  // 2 : x, y | 1 : e, f, g
  most_frequent.Add("x");  // 3 : x | 2 : y | 1 : e, f, g
  most_frequent.Add("y");  // 3 : x, y | 1 : e, f, g

  EXPECT_THAT(most_frequent.GetMostFrequent(10),
              ElementsAre(Pair("y", 3), Pair("x", 3), Pair("g", 1),
                          Pair("f", 1), Pair("e", 1)));
}

TEST(SpaceSavingMostFrequent, RandomInstances) {
  absl::BitGen gen;
  static constexpr int kNumTests = 379;
  for (int test = 0; test < kNumTests; ++test) {
    const int num_items = absl::Uniform(gen, 0, 1000);
    const int num_samples = absl::Uniform(gen, 0, 100);
    const int storage_size = absl::Uniform(gen, 1, 100);

    Implementations<int> most_frequent(storage_size);
    std::vector<int> values;
    values.reserve(num_items);
    for (int i = 0; i < num_items; ++i) {
      const int value = absl::Uniform(gen, 0, 1000);
      most_frequent.Add(value);
      if (absl::Bernoulli(gen, 0.1)) {
        auto vec = most_frequent.GetMostFrequent(num_samples);
        if (!vec.empty()) {
          const int to_remove = absl::Uniform(gen, 0u, vec.size());
          most_frequent.FullyRemove(vec[to_remove].first);
        }
      }
      values.push_back(value);
    }
    most_frequent.CheckIdenticalResults(num_samples);
  }
}

TEST(SpaceSavingMostFrequent, WorksWithUniquePtr) {
  SpaceSavingMostFrequentNaive<std::string> naive_most_frequent(5);

  struct StringPtrHash {
    std::size_t operator()(const std::unique_ptr<std::string>& s) const {
      return absl::Hash<std::string>()(*s);
    }
  };
  struct StringPtrEq {
    bool operator()(const std::unique_ptr<std::string>& a,
                    const std::unique_ptr<std::string>& b) const {
      return *a == *b;
    }
  };
  SpaceSavingMostFrequent<std::unique_ptr<std::string>, StringPtrHash,
                          StringPtrEq>
      most_frequent(5);

  auto add = [&](const std::string& value) {
    most_frequent.Add(std::make_unique<std::string>(value));
    naive_most_frequent.Add(value);
  };

  add("a");
  add("b");
  add("c");
  add("d");
  add("e");
  add("a");
  add("a");
  add("a");

  add("b");
  add("c");
  add("d");
  add("e");
  add("f");
  add("g");

  std::vector<std::pair<std::string, int64_t>> res;
  for (int i = 0; i < 10; ++i) {
    const int64_t count = most_frequent.CountOfMostFrequent();
    if (count == 0) break;
    res.push_back({*most_frequent.PopMostFrequent(), count});
  }

  EXPECT_EQ(res, naive_most_frequent.GetMostFrequent(10));
}

template <int kElementSize>
struct Element {
  Element() = default;
  explicit Element(int value) : value(value) {}
  int value;
  std::array<int, kElementSize> zeros;
  template <typename H>
  friend H AbslHashValue(H h, const Element& e) {
    return H::combine(std::move(h), e.value);
  }
  friend bool operator==(const Element& a, const Element& b) {
    return a.value == b.value;
  }
};

template <int kSize, int kCapacity, int kElementSize>
void BM_Add_GeometricDistributed(benchmark::State& state) {
  using Element = Element<kElementSize>;
  static constexpr int kNumInputs = 100;
  absl::BitGen random;
  std::vector<std::vector<Element>> inputs;
  inputs.reserve(kNumInputs);
  std::geometric_distribution<int> distribution(1.0 / kCapacity);
  for (int i = 0; i < kNumInputs; ++i) {
    std::vector<Element>& input = inputs.emplace_back();
    input.reserve(kSize);
    for (int j = 0; j < kSize; ++j) {
      input.push_back(Element(distribution(random)));
    }
  }

  // Start the benchmark.
  for (auto _ : state) {
    for (const std::vector<Element>& input : inputs) {
      SpaceSavingMostFrequent<Element> most_frequent(kCapacity);
      for (const Element& value : input) {
        most_frequent.Add(value);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * kNumInputs * kSize);
}

BENCHMARK(BM_Add_GeometricDistributed<30, 10, 0>);
BENCHMARK(BM_Add_GeometricDistributed<100, 30, 0>);
BENCHMARK(BM_Add_GeometricDistributed<1000, 100, 0>);
BENCHMARK(BM_Add_GeometricDistributed<10000, 1000, 0>);
BENCHMARK(BM_Add_GeometricDistributed<100000, 10000, 0>);

BENCHMARK(BM_Add_GeometricDistributed<30, 10, 4>);
BENCHMARK(BM_Add_GeometricDistributed<100, 30, 4>);
BENCHMARK(BM_Add_GeometricDistributed<1000, 100, 4>);
BENCHMARK(BM_Add_GeometricDistributed<10000, 1000, 4>);
BENCHMARK(BM_Add_GeometricDistributed<100000, 10000, 4>);

}  // namespace
}  // namespace operations_research
