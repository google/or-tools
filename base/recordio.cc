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

#include "base/recordio.h"

namespace operations_research {
const int RecordWriter::kMagicNumber = 0x3ed7230a;

RecordWriter::RecordWriter(File* const file) : file_(file) {}

bool RecordWriter::Close() {
  return file_->Close();
}

RecordReader::RecordReader(File* const file) : file_(file) {}

bool RecordReader::Close() {
  return file_->Close();
}
}  // namespace operations_research
