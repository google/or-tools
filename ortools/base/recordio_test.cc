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

#include "ortools/base/recordio.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/base/file.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/recordio_test.pb.h"
#include "ortools/base/temp_file.h"

namespace ORTOOLS_RECORDIO_NAMESPACE {
namespace {

// Parameters for RecordReaderInvalidPayloadTest.
struct RecordReaderInvalidPayloadTestParams {
  // Name of the test; must be snake_case.
  std::string name ABSL_REQUIRE_EXPLICIT_INIT;

  // All tests should fail, but we also want to make sure the test code itself
  // is correct enough so that valid data don't fail as well.%
  bool expect_failure = true;

  // Change the value of RecordWriter::set_use_compression() used to create the
  // valid data.
  bool use_compression ABSL_REQUIRE_EXPLICIT_INIT;

  // Function to change the valid data and make them invalid (unless
  // expect_failure is false).
  //
  // The content of `data` is the result of calling
  // RecordWriter::WriteProtocolMessage() with
  // ortools_recordio_test::SomeMessage{x=1}.
  std::function<void(std::string& data)> update_data;
};

class RecordReaderInvalidPayloadTest
    : public testing::TestWithParam<RecordReaderInvalidPayloadTestParams> {};

TEST_P(RecordReaderInvalidPayloadTest, Read) {
  // TODO: b/584406860 - Enable those when fixed.
  if (GetParam().name == "csize_max" ||
      GetParam().name == "usize_max_with_compression" ||
      GetParam().name == "usize_max_without_compression") {
    GTEST_SKIP() << "Skipping until b/584406860 is fixed.";
  }
  ASSERT_OK_AND_ASSIGN(
      const std::string valid_file_name,
      file::MakeTempFilename(::testing::TempDir(), "valid_payload"));

  // Write valid data to the temp file; we use this to get a valid compressed
  // stream.
  {
    File* temp_file = nullptr;
    ASSERT_OK(file::Open(valid_file_name, "wb", &temp_file, file::Defaults()));
    ASSERT_NE(temp_file, nullptr);

    RecordWriter writer(temp_file);
    writer.set_use_compression(GetParam().use_compression);
    {
      ortools_recordio_test::SomeMessage msg;
      msg.set_x(1);
      ASSERT_TRUE(writer.WriteProtocolMessage(msg));
    }
    ASSERT_TRUE(writer.Close());
  }

  // Read this data back, alter it and overwrite the file.
  ASSERT_OK_AND_ASSIGN(
      const std::string invalid_file_name,
      file::MakeTempFilename(::testing::TempDir(), "invalid_payload"));
  {
    ASSERT_OK_AND_ASSIGN(std::string contents,
                         file::GetContents(valid_file_name, file::Defaults()));
    GetParam().update_data(contents);
    ASSERT_OK(file::SetContents(invalid_file_name, contents, file::Defaults()));
  }

  // Finally try to read the file.
  File* temp_file = nullptr;
  ASSERT_OK(file::Open(invalid_file_name, "rb", &temp_file, file::Defaults()));
  ASSERT_NE(temp_file, nullptr);

  RecordReader reader(temp_file);
  ortools_recordio_test::SomeMessage msg;
  ASSERT_NE(reader.ReadProtocolMessage(&msg), GetParam().expect_failure);
}

INSTANTIATE_TEST_SUITE_P(
    All, RecordReaderInvalidPayloadTest,
    testing::Values(
        RecordReaderInvalidPayloadTestParams{
            .name = "valid_with_compression",
            .expect_failure = false,
            .use_compression = true,
            .update_data = [](std::string&) {},
        },
        RecordReaderInvalidPayloadTestParams{
            .name = "valid_without_compression",
            .expect_failure = false,
            .use_compression = false,
            .update_data = [](std::string&) {},
        },
        RecordReaderInvalidPayloadTestParams{
            .name = "bad_magic",
            .use_compression = false,
            .update_data =
                [](std::string& contents) {
                  CHECK_GE(contents.size(), sizeof(int));  // kMagicNumber
                  static_assert(sizeof(int) >= 4);
                  // Replace the magic number by the first 4 bytes
                  // of a PNG file
                  // (https://en.wikipedia.org/wiki/PNG).
                  contents[0] = '\x89';
                  contents[1] = 'P';
                  contents[2] = 'N';
                  contents[3] = 'G';
                },
        },
        RecordReaderInvalidPayloadTestParams{
            .name = "usize_max_with_compression",
            .use_compression = true,
            .update_data =
                [](std::string& contents) {
                  CHECK_GE(contents.size(),
                           sizeof(int) +               // kMagicNumber
                               2 * sizeof(uint64_t));  // usize + csize
                  // Change usize to 0xFF...
                  for (size_t i = sizeof(int);
                       i < sizeof(int) + sizeof(uint64_t); ++i) {
                    contents[i] = '\xff';
                  }
                },
        },
        RecordReaderInvalidPayloadTestParams{
            .name = "usize_max_without_compression",
            .use_compression = false,
            .update_data =
                [](std::string& contents) {
                  CHECK_GE(contents.size(),
                           sizeof(int) +               // kMagicNumber
                               2 * sizeof(uint64_t));  // usize + csize
                  // Change usize to 0xFF...
                  for (size_t i = sizeof(int);
                       i < sizeof(int) + sizeof(uint64_t); ++i) {
                    contents[i] = '\xff';
                  }
                },
        },
        RecordReaderInvalidPayloadTestParams{
            .name = "csize_max",
            .use_compression = true,
            .update_data =
                [](std::string& contents) {
                  CHECK_GE(contents.size(),
                           sizeof(int) +               // kMagicNumber
                               2 * sizeof(uint64_t));  // usize + csize
                  // Change csize to 0xFF...
                  for (size_t i = sizeof(int) + sizeof(uint64_t);
                       i < sizeof(int) + 2 * sizeof(uint64_t); ++i) {
                    contents[i] = '\xff';
                  }
                },
        }),
    [](const testing::TestParamInfo<RecordReaderInvalidPayloadTest::ParamType>&
           info) { return info.param.name; });

}  // namespace
}  // namespace ORTOOLS_RECORDIO_NAMESPACE
