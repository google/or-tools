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

#include "ortools/util/random_engine.h"

#include <random>

#include "gtest/gtest.h"

// This test is just here to make sure that a type has been selected that
// models UniformRandomBitGenerator.
TEST(RandomEngineCompileTest, TestIt) {
  operations_research::random_engine_t engine;
  engine.seed(10);
  ASSERT_LT(engine.min(), engine.max());
  EXPECT_LT(-1, std::uniform_int_distribution<int>(0, 1)(engine));
}
