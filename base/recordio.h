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

#ifndef OR_TOOLS_BASE_RECORDIO_H_
#define OR_TOOLS_BASE_RECORDIO_H_

#include <string>
#include "base/file.h"
#include "base/scoped_ptr.h"

// This file defines some IO interfaces to compatible with Google
// IO specifications.
namespace operations_research {
// This class appends a protocol buffer to a file in a binary format.
class RecordWriter {
 public:
  // Magic number when writing and reading protocol buffers.
  static const int kMagicNumber;

  explicit RecordWriter(File* const file);

  template <class P> bool WriteProtocolMessage(const P& proto) {
    std::string uncompressed_buffer;
    proto.SerializeToString(&uncompressed_buffer);
    const unsigned long uncompressed_size = uncompressed_buffer.size();
    const std::string compressed_buffer = Compress(uncompressed_buffer);
    const unsigned long compressed_size = compressed_buffer.size();
    if (file_->Write(&kMagicNumber, sizeof(kMagicNumber)) !=
        sizeof(kMagicNumber)) {
      return false;
    }
    if (file_->Write(&uncompressed_size, sizeof(uncompressed_size)) !=
        sizeof(uncompressed_size)) {
      return false;
    }
    if (file_->Write(&compressed_size, sizeof(compressed_size)) !=
        sizeof(compressed_size)) {
      return false;
    }
    if (file_->Write(compressed_buffer.c_str(), compressed_size) !=
        compressed_size) {
      return false;
    }
    return true;
  }
  // Closes the underlying file.
  bool Close();

 private:
  std::string Compress(const std::string& input) const;
  File* const file_;
};

// This class reads a protocol buffer from a file.
class RecordReader {
 public:
  explicit RecordReader(File* const file);

  template <class P> bool ReadProtocolMessage(P* const proto) {
    unsigned long usize = 0;
    unsigned long csize = 0;
    int magic_number = 0;
    if (file_->Read(&magic_number, sizeof(magic_number)) !=
        sizeof(magic_number)) {
      return false;
    }
    if (magic_number != RecordWriter::kMagicNumber) {
      return false;
    }
    if (file_->Read(&usize, sizeof(usize)) != sizeof(usize)) {
      return false;
    }
    if (file_->Read(&csize, sizeof(csize)) != sizeof(csize)) {
      return false;
    }
    scoped_array<char> compressed_buffer(new char[csize + 1]);
    if (file_->Read(compressed_buffer.get(), csize) != csize) {
      return false;
    }
    compressed_buffer[csize] = '\0';
    scoped_array<char> buffer(new char[usize + 1]);
    Uncompress(compressed_buffer.get(), usize, buffer.get(), usize);
    proto->ParseFromArray(buffer.get(), usize);
    return true;
  }

  // Closes the underlying file.
  bool Close();

 private:
  void Uncompress(const char* const source,
                  unsigned long source_size,
                  char* const output_buffer,
                  unsigned long output_size) const;

  File* const file_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_RECORDIO_H_
