// Copyright 2010-2017 Google
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

#include "ortools/base/filelinereader.h"

#include <cstring>
#include <memory>
#include <string>

#include "ortools/base/file.h"
#include "ortools/base/logging.h"

namespace operations_research {
FileLineReader::FileLineReader(const char* const filename)
    : filename_(filename),
      line_callback_(nullptr),
      loaded_successfully_(false) {}

FileLineReader::~FileLineReader() {}

void FileLineReader::set_line_callback(Callback1<char*>* const callback) {
  line_callback_.reset(callback);
}

void FileLineReader::Reload() {
  const int kMaxLineLength = 60 * 1024;
  File* const data_file = File::Open(filename_, "r");
  if (data_file == NULL) {
    loaded_successfully_ = false;
    return;
  }

  std::unique_ptr<char[]> line(new char[kMaxLineLength]);
  for (;;) {
    char* const result = data_file->ReadLine(line.get(), kMaxLineLength);
    if (result == NULL) {
      data_file->Close();
      loaded_successfully_ = true;
      return;
    }
    // Chop the last linefeed if present.
    int len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {  // Linefeed.
      result[--len] = '\0';
    }
    if (len > 0 && result[len - 1] == '\r') {  // Carriage return.
      result[--len] = '\0';
    }
    if (line_callback_.get() != NULL) {
      line_callback_->Run(result);
    }
  }
  data_file->Close();
}

bool FileLineReader::loaded_successfully() const {
  return loaded_successfully_;
}
}  // namespace operations_research
