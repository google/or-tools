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

#include "ortools/util/qap_reader.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/util/data_path_resolver.h"

namespace operations_research {
namespace {

TEST(QapReader, ReadsQapFromFile) {
  const QapProblem expected_problem = {
      .weights = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}},
      .distances = {{1, 2, 3}, {4, 5, 6}, {7, 8, 10}}};
  const std::string filepath = ortools::GetDataDependencyFilepath(
      "ortools/util/testdata/qap_example1.txt");
  const QapProblem observed_problem = ReadQapProblemOrDie(filepath);
  EXPECT_EQ(observed_problem, expected_problem);
}

}  // namespace
}  // namespace operations_research
