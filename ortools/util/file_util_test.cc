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

#include "ortools/util/file_util.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/base/filesystem.h"
#include "ortools/base/gmock.h"
#include "ortools/base/gzipstring.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/temp_file.h"
#include "ortools/util/file_util_test.pb.h"

namespace operations_research {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::AllOf;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

class FileUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(file_name_,
                         file::MakeTempFilename(::testing::TempDir(), "file"));
    ASSERT_OK_AND_ASSIGN(empty_file_name_,
                         file::MakeTempFilename(::testing::TempDir(), "empty"));
    unknown_file_name_ = file_name_ + "_does_not_exist";

    ASSERT_OK(file::SetContents(empty_file_name_, "", file::Defaults()));
  }

  std::string file_name_;
  std::string empty_file_name_;
  std::string unknown_file_name_;
};

typedef FileUtilsTest ReadFileToStringTest;

TEST_F(ReadFileToStringTest, SmallTextFile) {
  constexpr absl::string_view contents = "line 1\nline 2";
  ASSERT_OK(file::SetContents(file_name_, contents, file::Defaults()));

  EXPECT_THAT(ReadFileToString(file_name_), IsOkAndHolds(contents));
}

TEST_F(ReadFileToStringTest, EmptyFile) {
  EXPECT_THAT(ReadFileToString(empty_file_name_), IsOkAndHolds(""));
}

TEST_F(ReadFileToStringTest, UnknownFile) {
  EXPECT_THAT(ReadFileToString(unknown_file_name_),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(ReadFileToStringTest, GzippedTextFile) {
  constexpr absl::string_view contents = "line 1\nline 2";
  std::string compressed;
  GzipString(contents, &compressed);
  ASSERT_OK(file::SetContents(file_name_, compressed, file::Defaults()));

  EXPECT_THAT(ReadFileToString(file_name_), IsOkAndHolds(contents));
}

TEST(StringToProtoTest, Binary) {
  const FileUtilTestProto1 original_proto = ParseTestProto("some_field:42");
  FileUtilTestProto1 parsed_proto;
  ASSERT_OK(StringToProto(original_proto.SerializeAsString(), &parsed_proto));
  EXPECT_THAT(parsed_proto, EqualsProto(original_proto));
}

TEST(StringToProtoTest, Text) {
  FileUtilTestProto1 proto;
  ASSERT_OK(StringToProto("some_field:42", &proto));
  EXPECT_THAT(proto, EqualsProto("some_field:42"));
}

constexpr char kTestTextProto[] = R"pb(
  optional_field: 123
  repeated_fields { some_field: 42 }
  repeated_fields { some_field: 43 }
  repeated_fields { some_field: 44 }
  repeated_fields { some_field: 45 }
  repeated_fields { some_field: 46 }
)pb";

constexpr char kTestJson[] = R"json({
 "optional_field": 123,
 "repeated_fields": [
  {
   "some_field": 42
  },
  {
   "some_field": 43
  },
  {
   "some_field": 44
  },
  {
   "some_field": 45
  },
  {
   "some_field": 46
  }
 ]
}
)json";

constexpr char kTestJsonProto3[] = R"json({
 "optionalField": 123,
 "repeatedFields": [
  {
    "someField": 42
  },
  {
    "someField": 43
  },
  {
    "someField": 44
  },
  {
    "someField": 45
  },
  {
    "someField": 46
  }
 ]
}
)json";

std::string RemoveWhitespaceFromString(const std::string input) {
  return absl::StrReplaceAll(input, {{"\n", ""}, {" ", ""}});
}

TEST(StringToProtoTest, JSon) {
  FileUtilTestProto2 proto;
  ASSERT_OK(StringToProto(kTestJson, &proto));
  EXPECT_THAT(proto, EqualsProto(kTestTextProto));
}

TEST(StringToProtoTest, JSonProto3) {
  FileUtilTestProto2 proto;
  ASSERT_OK(StringToProto(kTestJsonProto3, &proto));
  EXPECT_THAT(proto, EqualsProto(kTestTextProto));
}

TEST(StringToProtoTest, JSonWithSingleValueInRepeatedField) {
  absl::string_view contents = R"json({
  "optional_field": 123,
  "repeated_fields": {
      "some_field": 42
  }
}
)json";
  FileUtilTestProto2 proto;
  ASSERT_OK(StringToProto(contents, &proto));
  EXPECT_THAT(proto, EqualsProto(R"pb(
                optional_field: 123
                repeated_fields { some_field: 42 }
              )pb"));
}

TEST(StringToProtoTest, EmptyFile) {
  FileUtilTestProto2 proto;
  ASSERT_OK(StringToProto("", &proto));
  EXPECT_THAT(proto, EqualsProto(""));
}

TEST(StringToProtoTest, WrongContents) {
  FileUtilTestProto2 proto;
  EXPECT_THAT(StringToProto("Not a proto.", &proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("binary format error: "),
                             HasSubstr("text format error: "),
                             HasSubstr("json error: "))));
}

TEST(StringToProtoTest, WrongProtoText) {
  FileUtilTestProto2 proto;
  // This is a valid text proto, but not a FileUtilTestProto2.
  EXPECT_THAT(StringToProto("foobar: 42", &proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("1:6"),
                             HasSubstr("no field named \"foobar\""))));
  EXPECT_THAT(proto, EqualsProto(""));
}

TEST(StringToProtoTest, WrongProtoJson) {
  FileUtilTestProto1 proto;
  EXPECT_THAT(StringToProto(kTestJson, &proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'optional_field'")));
  EXPECT_THAT(proto, EqualsProto(""));
}

TEST(StringToProtoTest, WrongProtoJsonProto3) {
  FileUtilTestProto1 proto;
  EXPECT_THAT(StringToProto(kTestJsonProto3, &proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'optionalField'")));
  EXPECT_THAT(proto, EqualsProto(""));
}

TEST(StringToProtoTest, WrongProtoBinary) {
  // To make the proto verification catch a wrong proto in binary format, we
  // have to populate a reasonable proto -- those that are too trivial are
  // likely to be parsed as a payload of almost similar size, even as a wrong
  // proto.
  FileUtilTestProto2 proto2;
  for (int i = 0; i < 10; ++i) {
    proto2.add_repeated_fields()->set_some_field(i + 42);
  }
  FileUtilTestProto1 proto1;
  EXPECT_THAT(StringToProto(proto2.SerializeAsString(), &proto1),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("binary format error")));
  EXPECT_THAT(proto1, EqualsProto(""));
}

typedef FileUtilsTest ReadFileToProtoTest;

TEST_F(ReadFileToProtoTest, FileError) {
  FileUtilTestProto2 proto;
  EXPECT_THAT(
      ReadFileToProto(unknown_file_name_, &proto),
      StatusIs(absl::StatusCode::kNotFound, HasSubstr(unknown_file_name_)));
}

TEST_F(ReadFileToProtoTest, OutputsFileNameInErrorStatus) {
  FileUtilTestProto2 proto;
  ASSERT_OK(file::SetContents(file_name_, "gloubi-boulga", file::Defaults()));
  EXPECT_THAT(
      ReadFileToProto(file_name_, &proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr(file_name_)));
}

TEST(WriteProtoToFileTest, TextProtoOnRealFile) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kProtoText));
  FileUtilTestProto2 written_proto;
  ASSERT_OK(file::GetTextProto(filename, &written_proto, file::Defaults()));
  EXPECT_THAT(written_proto, EqualsProto(proto));
}

TEST(WriteProtoToFileTest, JsonProtoOnRealFile) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kJson));
  std::string written_json;
  ASSERT_OK(
      file::GetContents(filename + ".json", &written_json, file::Defaults()));
  EXPECT_EQ(written_json, kTestJson);
}

TEST(WriteProtoToFileTest, CanonicalJsonProtoOnRealFile) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(
      WriteProtoToFile(filename, proto, ProtoWriteFormat::kCanonicalJson));
  std::string written_json;
  ASSERT_OK(
      file::GetContents(filename + ".json", &written_json, file::Defaults()));
  EXPECT_EQ(RemoveWhitespaceFromString(written_json),
            RemoveWhitespaceFromString(kTestJsonProto3));
}

TEST(WriteProtoToFileTest, BinaryProtoOnRealFile) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kProtoBinary));
  FileUtilTestProto2 written_proto;
  ASSERT_OK(file::GetBinaryProto(filename + ".bin", &written_proto,
                                 file::Defaults()));
  EXPECT_THAT(written_proto, EqualsProto(proto));
}

TEST(WriteProtoToFileTest, TextProtoOnRealFileNoAppendExtension) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kProtoText,
                             /*gzipped=*/false,
                             /*append_extension_to_file_name=*/false));
  FileUtilTestProto2 written_proto;
  ASSERT_OK(file::GetTextProto(filename, &written_proto, file::Defaults()));
  EXPECT_THAT(written_proto, EqualsProto(proto));
}

TEST(WriteProtoToFileTest, JsonProtoOnRealFileNoAppendExtension) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kJson,
                             /*gzipped=*/false,
                             /*append_extension_to_file_name=*/false));
  std::string written_json;
  ASSERT_OK(file::GetContents(filename, &written_json, file::Defaults()));
  EXPECT_EQ(written_json, kTestJson);
}

TEST(WriteProtoToFileTest, BinaryProtoOnRealFileNoAppendExtension) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kProtoBinary,
                             /*gzipped=*/false,
                             /*append_extension_to_file_name=*/false));
  FileUtilTestProto2 written_proto;
  ASSERT_OK(file::GetBinaryProto(filename, &written_proto, file::Defaults()));
  EXPECT_THAT(written_proto, EqualsProto(proto));
}

TEST(ReadAndWriteProtoToFileTest, Gzipped) {
  ASSERT_OK_AND_ASSIGN(const std::string filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  ASSERT_OK_AND_ASSIGN(const std::string gzipped_filename,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  ASSERT_OK_AND_ASSIGN(const std::string gzipped_filename2,
                       file::MakeTempFilename(::testing::TempDir(), ""));
  FileUtilTestProto2 proto = ParseTestProto(kTestTextProto);
  ASSERT_OK(WriteProtoToFile(filename, proto, ProtoWriteFormat::kProtoText));
  ASSERT_OK(WriteProtoToFile(gzipped_filename, proto,
                             ProtoWriteFormat::kProtoText,
                             /*gzipped=*/true));
  ASSERT_OK(WriteProtoToFile(
      gzipped_filename2, proto, ProtoWriteFormat::kProtoText,
      /*gzipped=*/true, /*append_extension_to_file_name=*/false));
  FileUtilTestProto2 written_proto;
  ASSERT_OK(file::GetTextProto(filename, &written_proto, file::Defaults()));
  EXPECT_THAT(written_proto, EqualsProto(proto));
  written_proto.Clear();
  ASSERT_OK(ReadFileToProto(gzipped_filename + ".gz", &written_proto));
  EXPECT_THAT(written_proto, EqualsProto(proto));
  written_proto.Clear();
  ASSERT_OK(ReadFileToProto(gzipped_filename2, &written_proto));
  EXPECT_THAT(written_proto, EqualsProto(proto));

  // Verify that the gzipped file size is significantly smaller.
  ASSERT_OK_AND_ASSIGN(
      const int64_t gzipped_size,
      file::GetSize(gzipped_filename + ".gz", file::Defaults()));
  ASSERT_OK_AND_ASSIGN(const int64_t regular_size,
                       file::GetSize(filename, file::Defaults()));
  ASSERT_OK_AND_ASSIGN(const int64_t gzipped_size2,
                       file::GetSize(gzipped_filename2, file::Defaults()));

  EXPECT_LE(gzipped_size, 0.8 * regular_size);
  EXPECT_EQ(gzipped_size, gzipped_size2);
}

typedef FileUtilsTest ReadFileToProtoStatusOrTest;

TEST_F(ReadFileToProtoStatusOrTest, Success) {
  const FileUtilTestProto1 model = ParseTestProto("some_field:42");
  std::string model_text;
  google::protobuf::TextFormat::PrintToString(model, &model_text);
  ASSERT_OK(file::SetContents(file_name_, model_text, file::Defaults()));
  EXPECT_THAT(ReadFileToProto<FileUtilTestProto1>(file_name_),
              IsOkAndHolds(EqualsProto(model)));
}

TEST_F(ReadFileToProtoStatusOrTest, FileError) {
  EXPECT_THAT(
      ReadFileToProto<FileUtilTestProto1>(unknown_file_name_).status(),
      StatusIs(absl::StatusCode::kNotFound, HasSubstr(unknown_file_name_)));
}

}  // namespace
}  // namespace operations_research
