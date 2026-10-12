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

#include "ortools/util/monoid_operation_tree.h"

#include <array>
#include <cmath>
#include <string>

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"

namespace {

using operations_research::MonoidOperationTree;

// Double, with addition
class DoublePlus {
 public:
  static const double kIdentity;
  DoublePlus() : value_(kIdentity) {}
  explicit DoublePlus(double value) : value_(value) {}
  void Set(const DoublePlus& dp) { value_ = dp.value_; }
  void Compute(const DoublePlus& left, const DoublePlus& right) {
    value_ = left.value_ + right.value_;
  }
  double value() const { return value_; }
  std::string DebugString() const { return absl::StrFormat("%g", value_); }

 private:
  double value_;
};

const double DoublePlus::kIdentity = 0.0;

typedef MonoidOperationTree<DoublePlus> SumTree;

TEST(MonoidOperationTreeTest, doubleAddition0) {
  const int kSize = 0;
  SumTree sumTree(kSize);
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
  EXPECT_FALSE(sumTree.DebugString().empty());
}

TEST(MonoidOperationTreeTest, doubleAddition1) {
  const int kSize = 1;
  SumTree sumTree(kSize);
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
  const double kSomeValue = 3.14;
  const DoublePlus kDP(kSomeValue);
  sumTree.Set(0, kDP);
  EXPECT_EQ(kSomeValue, sumTree.result().value());
  EXPECT_EQ(kSomeValue, sumTree.GetOperand(0).value());
  sumTree.Reset(0);
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.GetOperand(0).value());
}

TEST(MonoidOperationTreeTest, doubleAddition2) {
  const int kSize = 2;
  SumTree sumTree(kSize);
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
  const double kSomeValue = 3.14;
  const DoublePlus kDP(kSomeValue);
  sumTree.Set(0, kDP);
  EXPECT_EQ(kSomeValue, sumTree.result().value());
  const double kHugeValue = std::pow(2.0, 100);
  const DoublePlus kHugeDP(kHugeValue);
  sumTree.Set(1, kHugeDP);
  EXPECT_EQ(kHugeValue, sumTree.result().value());
  sumTree.Reset(1);
  EXPECT_EQ(kSomeValue, sumTree.result().value());
}

TEST(MonoidOperationTreeTest, doubleAddition3) {
  const auto values = std::to_array<double>({0.123, 0.456, 0.789});
  SumTree sumTree(values.size());
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
  double sum = 0.0;
  for (int i = 0; i < values.size(); ++i) {
    const DoublePlus dp(values[i]);
    sumTree.Set(i, dp);
    sum += values[i];
    EXPECT_EQ(sum, sumTree.result().value());
  }
  sumTree.Clear();
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
}

// A diver than finds the smallest sum of left terms greater than a quantity
class FindSmallestLeftSumGreaterThan {
 public:
  explicit FindSmallestLeftSumGreaterThan(double threshold)
      : sumLeftTerms_(0.0), threshold_(threshold) {}
  double sumLeftTerms() const { return sumLeftTerms_; }
  void OnArgumentReached(int index, const DoublePlus& argument) {
    sumLeftTerms_ += argument.value();
  }
  bool ChooseGoLeft(const DoublePlus& current, const DoublePlus& left_child,
                    const DoublePlus& right_child) {
    if (sumLeftTerms_ + left_child.value() >= threshold_) {
      // There's enough on the left.
      return true;
    } else {
      // Not enough on the left: go right.
      sumLeftTerms_ += left_child.value();
      return false;
    }
  }
  void OnComeBackFromLeft(const DoublePlus& current,
                          const DoublePlus& left_child,
                          const DoublePlus& right_child) {}
  void OnComeBackFromRight(const DoublePlus& current,
                           const DoublePlus& left_child,
                           const DoublePlus& right_child) {}

 private:
  double sumLeftTerms_;
  double threshold_;
};

TEST(MonoidOperationTreeTest, dive) {
  const auto values = std::to_array<double>({0.5, 0.2, 0.8, 0.9});
  SumTree sumTree(values.size());
  EXPECT_EQ(DoublePlus::kIdentity, sumTree.result().value());
  double sum = 0.0;
  for (int i = 0; i < values.size(); ++i) {
    const DoublePlus dp(values[i]);
    sumTree.Set(i, dp);
    sum += values[i];
    EXPECT_NEAR(sum, sumTree.result().value(), 1.0e-6);
  }
  // Now dive.
  const auto thresholds =
      std::to_array<double>({0.4, 0.5, 0.6, 0.7, 1.5, 1.6, 2.5});
  const auto left_terms =
      std::to_array<double>({0.5, 0.5, 0.7, 0.7, 1.5, 2.4, 2.4});
  ASSERT_EQ(left_terms.size(), thresholds.size());
  for (int i = 0; i < left_terms.size(); ++i) {
    FindSmallestLeftSumGreaterThan diver(thresholds[i]);
    sumTree.DiveInTree(&diver);
    EXPECT_EQ(left_terms[i], diver.sumLeftTerms());
  }
}

}  // namespace
