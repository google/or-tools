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

#include "ortools/sat/recordio.h"

#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/path.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::EqualsProto;

TEST(RecordIoTest, WriteAndReadBack) {
  std::string filename =
      file::JoinPath(testing::TempDir(), "recordio_test.bin");

  std::vector<CpModelProto> cp_models;
  std::ofstream output_stream(filename, std::ios::binary);
  RecordWriter writer(&output_stream);
  for (int i = 0; i < 128; ++i) {
    CpModelProto cp_model;
    for (int j = 0; j < i % 11; ++j) {
      cp_model.add_variables()->add_domain(j);
    }
    for (int j = 0; j < i % 17; ++j) {
      cp_model.add_constraints()->add_enforcement_literal(j);
    }
    cp_models.push_back(cp_model);
    EXPECT_TRUE(writer.WriteRecord(cp_model));
  }
  writer.Close();
  output_stream.close();

  std::ifstream input_stream(filename, std::ios::binary);
  RecordReader reader(&input_stream);
  CpModelProto cp_model;
  int index = 0;
  while (reader.ReadRecord(&cp_model)) {
    EXPECT_THAT(cp_model, EqualsProto(cp_models[index++]));
  }
  reader.Close();
  input_stream.close();
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
