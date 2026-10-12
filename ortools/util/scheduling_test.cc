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

#include "ortools/util/scheduling.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

DEFINE_STRONG_INT64_TYPE(IntegerValue);

template <typename T>
class ThetaTreeTest : public ::testing::Test {};

using IntegerTypes = ::testing::Types<IntegerValue, int64_t>;
TYPED_TEST_SUITE(ThetaTreeTest, IntegerTypes);

TYPED_TEST(ThetaTreeTest, EnvelopeOfEmptySet) {
  ThetaLambdaTree<TypeParam> tree;
  tree.Reset(0);
  EXPECT_EQ(std::numeric_limits<TypeParam>::min(), tree.GetEnvelope());
}

template <typename IntegerType>
std::vector<IntegerType> IntegerTypeVector(std::vector<int> arg) {
  return std::vector<IntegerType>(arg.begin(), arg.end());
}

TYPED_TEST(ThetaTreeTest, Envelope) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({-10, -7, -6, -4, -2});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 2, 2});
  tree.Reset(5);

  for (int i = 0; i < 5; i++) {
    tree.AddOrUpdateEvent(i, envelope[i], energy[i], energy[i]);
  }
  EXPECT_EQ(1, tree.GetEnvelope());  // (-7) + (1+3+2+2) or (-6) + (3+2+2)
  EXPECT_EQ(2, tree.GetMaxEventWithEnvelopeGreaterThan(TypeParam(0)));
  EXPECT_EQ(4, tree.GetMaxEventWithEnvelopeGreaterThan(TypeParam(-1)));
  EXPECT_EQ(0, tree.GetEnvelopeOf(0));
  EXPECT_EQ(1, tree.GetEnvelopeOf(1));
  EXPECT_EQ(1, tree.GetEnvelopeOf(2));
  EXPECT_EQ(0, tree.GetEnvelopeOf(3));
  EXPECT_EQ(0, tree.GetEnvelopeOf(4));
}

TYPED_TEST(ThetaTreeTest, EnvelopeOpt) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({-10, -7, -6, -4, -2});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 3, 2});
  tree.Reset(5);

  int event, optional_event;
  TypeParam energy_max;

  tree.AddOrUpdateEvent(0, envelope[0], energy[0], energy[0]);
  tree.AddOrUpdateEvent(1, envelope[1], energy[1], energy[1]);
  tree.AddOrUpdateEvent(3, envelope[3], TypeParam(0), energy[3]);
  tree.AddOrUpdateEvent(4, envelope[4], energy[4], energy[4]);
  EXPECT_EQ(1, tree.GetOptionalEnvelope());

  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(0), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(3, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);

  tree.RemoveEvent(4);
  tree.AddOrUpdateEvent(2, envelope[2], energy[2], energy[2]);
  EXPECT_EQ(0, tree.GetOptionalEnvelope());
  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(-1), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(2, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);
  EXPECT_EQ(-4, tree.GetEnvelopeOf(0));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(1));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(2));
}

TYPED_TEST(ThetaTreeTest, EnvelopeOptWithAddOptional) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({-10, -7, -6, -4, -2});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 3, 2});
  tree.Reset(5);

  int event, optional_event;
  TypeParam energy_max;

  tree.AddOrUpdateEvent(0, envelope[0], energy[0], energy[0]);
  tree.AddOrUpdateEvent(1, envelope[1], energy[1], energy[1]);
  tree.AddOrUpdateOptionalEvent(3, envelope[3], energy[3]);
  tree.AddOrUpdateEvent(4, envelope[4], energy[4], energy[4]);
  EXPECT_EQ(1, tree.GetOptionalEnvelope());

  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(0), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(3, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);

  tree.RemoveEvent(4);
  tree.AddOrUpdateEvent(2, envelope[2], energy[2], energy[2]);
  EXPECT_EQ(0, tree.GetOptionalEnvelope());
  tree.GetEventsWithOptionalEnvelopeGreaterThan(TypeParam(-1), &event,
                                                &optional_event, &energy_max);
  EXPECT_EQ(2, event);
  EXPECT_EQ(3, optional_event);
  EXPECT_EQ(2, energy_max);
  EXPECT_EQ(-4, tree.GetEnvelopeOf(0));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(1));
  EXPECT_EQ(-3, tree.GetEnvelopeOf(2));
}

TYPED_TEST(ThetaTreeTest, AddingAndGettingOptionalEvents) {
  ThetaLambdaTree<TypeParam> tree;
  std::vector<TypeParam> envelope =
      IntegerTypeVector<TypeParam>({0, 3, 4, 6, 8});
  std::vector<TypeParam> energy = IntegerTypeVector<TypeParam>({2, 1, 3, 3, 2});
  tree.Reset(5);

  tree.AddOrUpdateEvent(0, envelope[0], energy[0], energy[0]);
  tree.AddOrUpdateEvent(1, envelope[1], energy[1], energy[1]);
  EXPECT_EQ(4, tree.GetEnvelope());

  // Even with 0 energy, standard update takes task 3's envelope into account.
  tree.AddOrUpdateEvent(3, envelope[3], TypeParam(0), energy[3]);
  EXPECT_EQ(6, tree.GetEnvelope());
  EXPECT_EQ(9, tree.GetOptionalEnvelope());
  tree.RemoveEvent(3);

  // Changing task 3 to optional makes it disappear from GetEnvelope().
  tree.AddOrUpdateOptionalEvent(3, envelope[3], energy[3]);
  EXPECT_EQ(4, tree.GetEnvelope());  // Same as before adding task 3.
  EXPECT_EQ(9, tree.GetOptionalEnvelope());

  // Changing task 3 to optional changes its optional values.
  tree.AddOrUpdateEvent(3, envelope[3], TypeParam(1), TypeParam(9));
  tree.AddOrUpdateOptionalEvent(3, envelope[3], energy[3]);
  EXPECT_EQ(4, tree.GetEnvelope());
  EXPECT_EQ(9, tree.GetOptionalEnvelope());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
