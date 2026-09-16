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

#include "ortools/base/gzipfile.h"

#include <zconf.h>
#include <zlib.h>

#include <cstdio>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/basictypes.h"
#include "ortools/base/file.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

TEST(GZipFileTest, ReadUncompressedData) {
  // 1. Create original content
  const std::string original =
      "Hello GZip! Hello GZip! Hello GZip! Hello GZip! Hello GZip!";

  // 2. Compress it using zlib compress
  uLongf dest_len = original.size() + 600;  // room for overhead
  std::vector<char> compressed(dest_len);

  int compress_status = compress(
      reinterpret_cast<Bytef*>(compressed.data()), &dest_len,
      reinterpret_cast<const Bytef*>(original.data()), original.size());
  ASSERT_EQ(compress_status, Z_OK);
  compressed.resize(dest_len);

  // 3. Write compressed data to a temp file using OR-Tools File API
  const std::string temp_filename =
      absl::StrCat(::testing::TempDir(), "/gziptest");
  File* out_file = File::Open(temp_filename, "w");
  ASSERT_NE(out_file, nullptr);
  size_t bytes_written = out_file->Write(compressed.data(), compressed.size());
  ASSERT_EQ(bytes_written, compressed.size());
  ASSERT_OK(out_file->Close(0));

  // 4. Open the file with File::Open
  File* raw_file = File::Open(temp_filename, "r");
  ASSERT_NE(raw_file, nullptr);

  // 5. Wrap it with GZipFileReader
  File* gzip_reader = GZipFileReader(temp_filename, raw_file, TAKE_OWNERSHIP);
  ASSERT_NE(gzip_reader, nullptr);

  // 6. Read the data back
  std::string decompressed;
  decompressed.resize(original.size());
  size_t bytes_read = gzip_reader->Read(&decompressed[0], decompressed.size());
  EXPECT_EQ(bytes_read, original.size());
  EXPECT_EQ(decompressed, original);

  // 7. Reading more should return 0 (EOF)
  char extra;
  size_t extra_read = gzip_reader->Read(&extra, 1);
  EXPECT_EQ(extra_read, 0);

  // 8. Close reader (should close raw_file and delete reader because of
  // TAKE_OWNERSHIP)
  EXPECT_OK(gzip_reader->Close(0));

  // 9. Clean up file
  std::remove(temp_filename.c_str());
}

TEST(GZipFileTest, ConcatenatedStreams) {
  // Test that concatenated compressed streams are decompressed as one
  // continuous stream.
  const std::string part1 = "Part 1 of the gzip data. ";
  const std::string part2 = "Part 2 of the gzip data!";

  // Compress part 1
  uLongf dest_len1 = part1.size() + 600;
  std::vector<char> compressed1(dest_len1);
  int compress_status =
      compress(reinterpret_cast<Bytef*>(compressed1.data()), &dest_len1,
               reinterpret_cast<const Bytef*>(part1.data()), part1.size());
  ASSERT_EQ(compress_status, Z_OK);
  compressed1.resize(dest_len1);

  // Compress part 2
  uLongf dest_len2 = part2.size() + 600;
  std::vector<char> compressed2(dest_len2);
  compress_status =
      compress(reinterpret_cast<Bytef*>(compressed2.data()), &dest_len2,
               reinterpret_cast<const Bytef*>(part2.data()), part2.size());
  ASSERT_EQ(compress_status, Z_OK);
  compressed2.resize(dest_len2);

  // Concatenate them
  std::string concatenated;
  concatenated.append(compressed1.data(), compressed1.size());
  concatenated.append(compressed2.data(), compressed2.size());

  // Write concatenated data to temp file
  const std::string temp_filename =
      absl::StrCat(::testing::TempDir(), "/gziptest_concat");
  File* out_file = File::Open(temp_filename, "w");
  ASSERT_NE(out_file, nullptr);
  size_t bytes_written =
      out_file->Write(concatenated.data(), concatenated.size());
  ASSERT_EQ(bytes_written, concatenated.size());
  ASSERT_OK(out_file->Close(0));

  // Open & wrap
  File* raw_file = File::Open(temp_filename, "r");
  ASSERT_NE(raw_file, nullptr);
  File* gzip_reader = GZipFileReader(temp_filename, raw_file, TAKE_OWNERSHIP);

  // Read back
  std::string decompressed;
  decompressed.resize(part1.size() + part2.size());
  size_t bytes_read = gzip_reader->Read(&decompressed[0], decompressed.size());
  EXPECT_EQ(bytes_read, decompressed.size());
  EXPECT_EQ(decompressed, part1 + part2);

  EXPECT_OK(gzip_reader->Close(0));

  // Clean up
  std::remove(temp_filename.c_str());
}

}  // namespace
}  // namespace operations_research
