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
    std::string compressed_buffer;
    proto.SerializeToString(&compressed_buffer);
    const int size = compressed_buffer.length();
    if (file_->Write(&kMagicNumber, sizeof(kMagicNumber)) !=
        sizeof(kMagicNumber)) {
      return false;
    }
    if (file_->Write(&size, sizeof(size)) != sizeof(size)) {
      return false;
    }
    if (file_->Write(compressed_buffer.c_str(), size) != size) {
      return false;
    }
    return true;
  }
  // Closes the underlying file.
  bool Close();

 private:
  File* const file_;
};

// This class reads a protocol buffer from a file.
class RecordReader {
 public:
  explicit RecordReader(File* const file);
  template <class P> bool ReadProtocolMessage(P* const proto) {
    int size = 0;
    int magic_number = 0;
    if (file_->Read(&magic_number, sizeof(magic_number)) !=
        sizeof(magic_number)) {
      return false;
    }
    if (magic_number != RecordWriter::kMagicNumber) {
      return false;
    }
    if (file_->Read(&size, sizeof(size)) != sizeof(size)) {
      return false;
    }
    scoped_array<char> buffer(new char[size + 1]);
    if (file_->Read(buffer.get(), size) != size) {
      return false;
    }
    buffer[size] = 0;
    proto->ParseFromArray(buffer.get(), size);
    return true;
  }
  // Closes the underlying file.
  bool Close();

 private:
  File* const file_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_RECORDIO_H_
