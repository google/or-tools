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

#include "ortools/sat/recordio.h"

#include <cstdint>
#include <istream>
#include <ostream>

#include "google/protobuf/message_lite.h"

namespace operations_research {
namespace sat {

RecordReader::RecordReader(std::istream* istream)
    : istream_(istream), coded_istream_(&istream_) {}

bool RecordReader::ReadRecord(google::protobuf::MessageLite* record) {
  uint32_t size;
  if (!coded_istream_.ReadVarint32(&size)) return false;
  auto limit = coded_istream_.PushLimit(size);
  if (!record->ParseFromCodedStream(&coded_istream_)) return false;
  coded_istream_.PopLimit(limit);
  return true;
}

void RecordReader::Close() {}

RecordWriter::RecordWriter(std::ostream* ostream)
    : ostream_(ostream), coded_ostream_(&ostream_) {}

bool RecordWriter::WriteRecord(const google::protobuf::MessageLite& record) {
  coded_ostream_.WriteVarint32(record.ByteSizeLong());
  return record.SerializeToCodedStream(&coded_ostream_);
}

void RecordWriter::Close() { coded_ostream_.Trim(); }

}  // namespace sat
}  // namespace operations_research
