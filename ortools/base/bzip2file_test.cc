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

#include "ortools/base/bzip2file.h"

#include <cstdio>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "bzlib.h"
#include "gtest/gtest.h"
#include "ortools/base/basictypes.h"
#include "ortools/base/file.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

TEST(BZip2FileTest, ReadUncompressedData) {
  // 1. Create original content
  const std::string original =
      "Hello BZip2! Hello BZip2! Hello BZip2! Hello BZip2! Hello BZip2!";

  // 2. Compress it using BZip2
  unsigned int dest_len = original.size() + 600;  // room for overhead
  std::vector<char> compressed(dest_len);

  int compress_status = BZ2_bzBuffToBuffCompress(
      compressed.data(), &dest_len, const_cast<char*>(original.data()),
      original.size(), 9, 0, 30);
  ASSERT_EQ(compress_status, BZ_OK);
  compressed.resize(dest_len);

  // 3. Write compressed data to a temp file using OR-Tools File API
  const std::string temp_filename =
      absl::StrCat(::testing::TempDir(), "/bz2test");
  File* out_file = File::Open(temp_filename, "w");
  ASSERT_NE(out_file, nullptr);
  size_t bytes_written = out_file->Write(compressed.data(), compressed.size());
  ASSERT_EQ(bytes_written, compressed.size());
  ASSERT_OK(out_file->Close(0));

  // 4. Open the file with File::Open
  File* raw_file = File::Open(temp_filename, "r");
  ASSERT_NE(raw_file, nullptr);

  // 5. Wrap it with BZip2FileReader
  File* bz2_reader = BZip2FileReader(temp_filename, raw_file, TAKE_OWNERSHIP);
  ASSERT_NE(bz2_reader, nullptr);

  // 6. Read the data back
  std::string decompressed;
  decompressed.resize(original.size());
  size_t bytes_read = bz2_reader->Read(&decompressed[0], decompressed.size());
  EXPECT_EQ(bytes_read, original.size());
  EXPECT_EQ(decompressed, original);

  // 7. Reading more should return 0 (EOF)
  char extra;
  size_t extra_read = bz2_reader->Read(&extra, 1);
  EXPECT_EQ(extra_read, 0);

  // 8. Close reader (should close raw_file and delete reader because of
  // TAKE_OWNERSHIP)
  EXPECT_OK(bz2_reader->Close(0));

  // 9. Clean up file
  std::remove(temp_filename.c_str());
}

TEST(BZip2FileTest, ConcatenatedStreams) {
  // Test that concatenated BZip2 streams are decompressed as one continuous
  // stream.
  const std::string part1 = "Part 1 of the data. ";
  const std::string part2 = "Part 2 of the data!";

  // Compress part 1
  unsigned int dest_len1 = part1.size() + 600;
  std::vector<char> compressed1(dest_len1);
  int compress_status = BZ2_bzBuffToBuffCompress(
      compressed1.data(), &dest_len1, const_cast<char*>(part1.data()),
      part1.size(), 9, 0, 30);
  ASSERT_EQ(compress_status, BZ_OK);
  compressed1.resize(dest_len1);

  // Compress part 2
  unsigned int dest_len2 = part2.size() + 600;
  std::vector<char> compressed2(dest_len2);
  compress_status = BZ2_bzBuffToBuffCompress(compressed2.data(), &dest_len2,
                                             const_cast<char*>(part2.data()),
                                             part2.size(), 9, 0, 30);
  ASSERT_EQ(compress_status, BZ_OK);
  compressed2.resize(dest_len2);

  // Concatenate them
  std::string concatenated;
  concatenated.append(compressed1.data(), compressed1.size());
  concatenated.append(compressed2.data(), compressed2.size());

  // Write concatenated data to temp file
  const std::string temp_filename =
      absl::StrCat(::testing::TempDir(), "/bz2test_concat");
  File* out_file = File::Open(temp_filename, "w");
  ASSERT_NE(out_file, nullptr);
  size_t bytes_written =
      out_file->Write(concatenated.data(), concatenated.size());
  ASSERT_EQ(bytes_written, concatenated.size());
  ASSERT_OK(out_file->Close(0));

  // Open & wrap
  File* raw_file = File::Open(temp_filename, "r");
  ASSERT_NE(raw_file, nullptr);
  File* bz2_reader = BZip2FileReader(temp_filename, raw_file, TAKE_OWNERSHIP);

  // Read back
  std::string decompressed;
  decompressed.resize(part1.size() + part2.size());
  size_t bytes_read = bz2_reader->Read(&decompressed[0], decompressed.size());
  EXPECT_EQ(bytes_read, decompressed.size());
  EXPECT_EQ(decompressed, part1 + part2);

  EXPECT_OK(bz2_reader->Close(0));

  // Clean up
  std::remove(temp_filename.c_str());
}

}  // namespace
}  // namespace operations_research
