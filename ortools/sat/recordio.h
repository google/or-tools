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

#ifndef ORTOOLS_SAT_RECORDIO_H_
#define ORTOOLS_SAT_RECORDIO_H_

#include <istream>
#include <ostream>

#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/message_lite.h"

namespace operations_research {
namespace sat {

// Reads a sequence of serialized protos from a stream, written by a
// RecordWriter.
class RecordReader {
 public:
  explicit RecordReader(std::istream* istream);
  // This class is neither copyable nor movable.
  RecordReader(const RecordReader&) = delete;
  RecordReader& operator=(const RecordReader&) = delete;

  bool ReadRecord(google::protobuf::MessageLite* record);
  void Close();

 private:
  google::protobuf::io::IstreamInputStream istream_;
};

// Writes a sequence of serialized protos to a stream.
class RecordWriter {
 public:
  explicit RecordWriter(std::ostream* ostream);
  // This class is neither copyable nor movable.
  RecordWriter(const RecordWriter&) = delete;
  RecordWriter& operator=(const RecordWriter&) = delete;

  bool WriteRecord(const google::protobuf::MessageLite& record);
  void Close();

 private:
  google::protobuf::io::OstreamOutputStream ostream_;
  google::protobuf::io::CodedOutputStream coded_ostream_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_RECORDIO_H_
