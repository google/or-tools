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

#include <cstdint>
#include <memory>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {
namespace {

TEST(DomainTest, RemoveValue) {
  std::unique_ptr<Solver> solver(new Solver("DomainTest"));
  const int64_t bound = 10000000;
  IntVar* var = solver->MakeIntVar(-bound, bound);
  for (int i = 0; i < bound; ++i) {
    var->RemoveValue(i);
    EXPECT_FALSE(var->Contains(i));
    var->RemoveValue(-i);
    EXPECT_FALSE(var->Contains(-i));
  }
}

}  // namespace
}  // namespace operations_research
