// Copyright 2011 Google
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

#include <zlib.h>
#include <string>
#include "base/logging.h"
#include "base/recordio.h"
#include "base/scoped_ptr.h"

namespace operations_research {
const int RecordWriter::kMagicNumber = 0x3ed7230a;

RecordWriter::RecordWriter(File* const file) : file_(file) {}

bool RecordWriter::Close() {
  return file_->Close();
}

std::string RecordWriter::Compress(std::string const& s) const {
  const unsigned long source_size = s.size();
  const char * source = s.c_str();

  unsigned long dsize = source_size + (source_size * 0.1f) + 16;
  scoped_ptr<char> destination(new char[dsize]);

  const int result = compress((unsigned char *)destination.get(),
                              &dsize,
                              (const unsigned char *)source,
                              source_size);

  if (result != Z_OK) {
    LOG(FATAL) << "Compress error occured! Error code: " << result;
  }
  return std::string(destination.get(), dsize);
}

RecordReader::RecordReader(File* const file) : file_(file) {}

bool RecordReader::Close() {
  return file_->Close();
}

void RecordReader::Uncompress(const char* const source,
                             unsigned long source_size,
                             char* const output_buffer,
                             unsigned long output_size) const {
  unsigned long result_size = output_size;
  const int result = uncompress((unsigned char *)output_buffer,
                                &result_size,
                                (const unsigned char *)source,
                                source_size);
  if (result != Z_OK) {
    LOG(FATAL) << "Uncompress error occured! Error code: " << result;
  }
  CHECK_LE(result_size, output_size);
}
}  // namespace operations_research
