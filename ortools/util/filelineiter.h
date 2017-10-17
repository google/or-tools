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

// Allows to read a text file line by line with:
//   for (const std::string& line : FileLines("myfile.txt")) { ... }
//
// More details:
// * The lines are separated by '\n' (which is removed by default) and have no
//   size limits.
// * Consecutive '\n' result in empty lines being produced.
// * If not empty, the std::string after the last '\n' is produced as the last line.
// * Options are available to keep the trailing '\n' for each line, to remove
//   carriage-return chararters ('\r'), and to remove blank lines.
//
#ifndef OR_TOOLS_UTIL_FILELINEITER_H_
#define OR_TOOLS_UTIL_FILELINEITER_H_

#include "ortools/base/logging.h"
#include "ortools/base/file.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/base/strutil.h"

namespace operations_research {

// Implements the minimum interface for a range-based for loop iterator.
class FileLineIterator {
 public:
  enum {
    DEFAULT = 0x0000,
    KEEP_LINEFEED = 0x0001,       // Terminating \n in result.
    REMOVE_INLINE_CR = 0x0002,    // Remove \r characters.
    REMOVE_BLANK_LINES = 0x0004,  // Remove empty or \n-only lines.
  };

  FileLineIterator(File* file, int options)
      : next_position_after_eol_(0),
        buffer_size_(0),
        file_(file),
        options_(options) {
    ReadNextLine();
  }
  const std::string& operator*() const { return line_; }
  bool operator!=(const FileLineIterator& other) const {
    return file_ != other.file_;
  }
  void operator++() { ReadNextLine(); }

 private:
  bool HasOption(int option) const { return options_ & option; }

  void ReadNextLine() {
    line_.clear();
    if (file_ == nullptr) return;
    do {
      while (true) {
        int i = next_position_after_eol_;
        for (; i < buffer_size_; ++i) {
          if (buffer_[i] == '\n') break;
        }
        if (i == buffer_size_) {
          line_.append(&buffer_[next_position_after_eol_],
                       i - next_position_after_eol_);
          buffer_size_ = file_->Read(&buffer_, kBufferSize);
          if (buffer_size_ < 0) {
            LOG(WARNING) << "Error while reading file.";
            file_ = nullptr;
            break;
          }
          next_position_after_eol_ = 0;
          if (buffer_size_ == 0) {
            if (line_.empty()) {
              file_ = nullptr;
            }
            break;
          }
        } else {
          line_.append(&buffer_[next_position_after_eol_],
                       i - next_position_after_eol_ + 1);
          next_position_after_eol_ = i + 1;
          break;
        }
      }
      PostProcessLine();
    } while (file_ != nullptr && HasOption(REMOVE_BLANK_LINES) &&
             (line_.empty() || line_ == "\n"));
  }

  void PostProcessLine() {
    if (HasOption(REMOVE_INLINE_CR)) {
      line_.erase(std::remove(line_.begin(), line_.end(), '\r'), line_.end());
    }
    const auto eol = std::find(line_.begin(), line_.end(), '\n');
    if (!HasOption(KEEP_LINEFEED) && eol != line_.end()) {
      line_.erase(eol);
    }
  }

  static const int kBufferSize = 5 * 1024;
  char buffer_[kBufferSize];
  int next_position_after_eol_;
  int64 buffer_size_;
  File* file_;
  std::string line_;
  const int options_;
};

class FileLines {
 public:
  FileLines(const std::string& filename, int options) : options_(options) {
    if (!file::Open(filename, "r", &file_, file::Defaults()).ok()) return;

  }
  explicit FileLines(const std::string& filename)
      : FileLines(filename, FileLineIterator::DEFAULT) {}
  ~FileLines() {
    if (file_ != nullptr) file_->Close(file::Defaults()).IgnoreError();
  }
  FileLineIterator begin() { return FileLineIterator(file_, options_); }
  FileLineIterator end() const { return FileLineIterator(nullptr, options_); }

 private:
  File* file_;
  const int options_;
  DISALLOW_COPY_AND_ASSIGN(FileLines);
};

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_FILELINEITER_H_
