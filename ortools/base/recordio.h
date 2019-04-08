// Copyright 2010-2018 Google LLC
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

#include <memory>
#include <string>

#include "ortools/base/file.h"

// This file defines some IO interfaces to compatible with Google
// IO specifications.
namespace recordio {
// This class appends a protocol buffer to a file in a binary format.
// The data written in the file follows the following format (sequentially):
// - MagicNumber (32 bits) to recognize this format.
// - Uncompressed data payload size (64 bits).
// - Compressed data payload size (64 bits), or 0 if the
//   data is not compressed.
// - Payload, possibly compressed. See RecordWriter::Compress()
//   and RecordReader::Uncompress.
class RecordWriter {
 public:
  // Magic number when reading and writing protocol buffers.
  static const int kMagicNumber;

  explicit RecordWriter(File* const file);

  template <class P>
  bool WriteProtocolMessage(const P& proto) {
    std::string uncompressed_buffer;
    proto.SerializeToString(&uncompressed_buffer);
    const uint64 uncompressed_size = uncompressed_buffer.size();
    const std::string compressed_buffer =
        use_compression_ ? Compress(uncompressed_buffer) : "";
    const uint64 compressed_size = compressed_buffer.size();
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
    if (use_compression_) {
      if (file_->Write(compressed_buffer.c_str(), compressed_size) !=
          compressed_size) {
        return false;
      }
    } else {
      if (file_->Write(uncompressed_buffer.c_str(), uncompressed_size) !=
          uncompressed_size) {
        return false;
      }
    }
    return true;
  }
  // Closes the underlying file.
  bool Close();

  void set_use_compression(bool use_compression);

 private:
  std::string Compress(const std::string& input) const;
  File* const file_;
  bool use_compression_;
};

// This class reads a protocol buffer from a file.
// The format must be the one described in RecordWriter, above.
class RecordReader {
 public:
  explicit RecordReader(File* const file);

  template <class P>
  bool ReadProtocolMessage(P* const proto) {
    uint64 usize = 0;
    uint64 csize = 0;
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
    std::unique_ptr<char[]> buffer(new char[usize + 1]);
    if (csize != 0) {  // The data is compressed.
      std::unique_ptr<char[]> compressed_buffer(new char[csize + 1]);
      if (file_->Read(compressed_buffer.get(), csize) != csize) {
        return false;
      }
      compressed_buffer[csize] = '\0';
      Uncompress(compressed_buffer.get(), csize, buffer.get(), usize);
    } else {
      if (file_->Read(buffer.get(), usize) != usize) {
        return false;
      }
    }
    proto->ParseFromArray(buffer.get(), usize);
    return true;
  }

  // Closes the underlying file.
  bool Close();

 private:
  void Uncompress(const char* const source, uint64 source_size,
                  char* const output_buffer, uint64 output_size) const;

  File* const file_;
};
}  // namespace recordio

#endif  // OR_TOOLS_BASE_RECORDIO_H_
