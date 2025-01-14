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

#include "ortools/sat/combine_solutions.h"

#include <cstdint>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::Optional;

TEST(CombineSolutionsTest, BasicTest) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 1, 1, 1, 1 ]
    }
  )pb");
  const std::vector<int64_t> existing_solution = {1, 0, 0, 0};
  const std::vector<int64_t> base_solution = {0, 1, 0, 0};
  const std::vector<int64_t> new_solution = {0, 1, 1, 0};
  Model m;
  SharedResponseManager response_manager(&m);
  response_manager.NewSolution(existing_solution, "existing", nullptr);
  response_manager.NewSolution(base_solution, "base", nullptr);
  response_manager.NewSolution(new_solution, "new", nullptr);
  std::string solution_info;
  const auto combined_solution = FindCombinedSolution(
      model, new_solution, base_solution, &response_manager, &solution_info);
  EXPECT_THAT(combined_solution, Optional(ElementsAre(1, 0, 1, 0)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
