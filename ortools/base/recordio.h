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

// This file defines some IO interfaces to compatible with Google
// IO specifications.
#ifndef ORTOOLS_BASE_RECORDIO_H_
#define ORTOOLS_BASE_RECORDIO_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/log/log.h"
#include "ortools/base/base_export.h"
#include "ortools/base/file.h"

// Enable switching the namespace in the unit tests at Google.
//
#define ORTOOLS_RECORDIO_NAMESPACE recordio

namespace ORTOOLS_RECORDIO_NAMESPACE {
// This class appends a protocol buffer to a file in a binary format.
// The data written in the file follows the following format (sequentially):
// - MagicNumber (32 bits) to recognize this format.
// - Uncompressed data payload size (64 bits).
// - Compressed data payload size (64 bits), or 0 if the
//   data is not compressed.
// - Payload, possibly compressed. See RecordWriter::Compress()
//   and RecordReader::Uncompress.
class OR_DLL RecordWriter {
 public:
  // Magic number when reading and writing protocol buffers.
  static const int kMagicNumber;

  // Maximum size of a record.
  //
  // Protos are limited to an `int` size. We assume here that compressed protos
  // should fit this size as well.
  static constexpr uint64_t kMaxRecordSize = std::numeric_limits<int>::max();

  explicit RecordWriter(File* file);
  ~RecordWriter();

  template <class P>
  bool WriteProtocolMessage(const P& proto) {
    if (file_ == nullptr) {
      LOG(DFATAL) << "Close() already called";
      return false;
    }
    std::string uncompressed_buffer;
    if (!proto.SerializeToString(&uncompressed_buffer)) {
      return false;
    }
    const uint64_t uncompressed_size = uncompressed_buffer.size();
    if (uncompressed_size > kMaxRecordSize) {
      LOG(DFATAL) << "SerializeToString() should have failed if ByteSizeLong() "
                     "> INT_MAX but it returned a buffer of "
                  << uncompressed_size << " bytes.";
      return false;
    }
    const std::string compressed_buffer =
        use_compression_ ? Compress(uncompressed_buffer) : "";
    const uint64_t compressed_size = compressed_buffer.size();
    if (compressed_size > kMaxRecordSize) {
      // We know that uncompressed_size <= kMaxRecordSize.
      LOG(DFATAL) << "compressed_size = " << compressed_size
                  << " does not fit in INT_MAX.";
      return false;
    }
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
  File* absl_nullable file_;
  bool use_compression_;
};

// This class reads a protocol buffer from a file.
// The format must be the one described in RecordWriter, above.
class RecordReader {
 public:
  explicit RecordReader(File* const file);

  ~RecordReader();

  template <class P>
  bool ReadProtocolMessage(P* const proto) {
    if (file_ == nullptr) {
      LOG(DFATAL) << "Close() already called";
      return false;
    }

    uint64_t usize = 0;
    uint64_t csize = 0;
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
    if (usize > RecordWriter::kMaxRecordSize) {
      return false;
    }
    if (file_->Read(&csize, sizeof(csize)) != sizeof(csize)) {
      return false;
    }
    if (csize > RecordWriter::kMaxRecordSize) {
      return false;
    }
    std::unique_ptr<char[]> buffer(new char[usize]);
    if (csize != 0) {  // The data is compressed.
      std::unique_ptr<char[]> compressed_buffer(new char[csize]);
      if (file_->Read(compressed_buffer.get(), csize) != csize) {
        return false;
      }
      if (!Uncompress(compressed_buffer.get(), csize, buffer.get(), usize)) {
        return false;
      }
    } else {
      if (file_->Read(buffer.get(), usize) != usize) {
        return false;
      }
    }
    return proto->ParseFromArray(buffer.get(), usize);
  }

  // Closes the underlying file.
  bool Close();

 private:
  bool Uncompress(const char* const source, uint64_t source_size,
                  char* const output_buffer, uint64_t output_size) const;

  File* absl_nullable file_;
};
}  // namespace ORTOOLS_RECORDIO_NAMESPACE

#endif  // ORTOOLS_BASE_RECORDIO_H_
