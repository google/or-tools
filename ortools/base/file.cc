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

#include "ortools/base/file.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cstdint>

#if defined(_MSC_VER)
#include <io.h>
#define access _access
#define F_OK 0
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace {

class CFile : public File {
 public:
  CFile(FILE* c_file, absl::string_view name) : File(name), f_(c_file) {}
  ~CFile() override = default;

  // Reads "size" bytes to buf from file, buf should be pre-allocated.
  size_t Read(void* buf, size_t size) override {
    return fread(buf, 1, size, f_);
  }

  // Writes "size" bytes of buf to file, buf should be pre-allocated.
  size_t Write(const void* buf, size_t size) override {
    return fwrite(buf, 1, size, f_);
  }

  // Closes the file and delete the underlying FILE* descriptor.
  absl::Status Close(int /*flags*/) override {
    absl::Status status;
    if (f_ == nullptr) {
      return status;
    }
    if (fclose(f_) == 0) {
      f_ = nullptr;
    } else {
      status.Update(
          absl::Status(absl::StatusCode::kInvalidArgument,
                       absl::StrCat("Could not close file '", name_, "'")));
    }
    delete this;
    return status;
  }

  // Flushes buffer.
  bool Flush() override { return fflush(f_) == 0; }

  // Returns file size.
  size_t Size() override {
    struct stat f_stat;
    stat(name_.c_str(), &f_stat);
    return f_stat.st_size;
  }

  bool Open() const override { return f_ != nullptr; }

 private:
  FILE* f_;
};

}  // namespace

File::File(absl::string_view name) : name_(name) {}

File* File::OpenOrDie(absl::string_view file_name, absl::string_view mode) {
  File* f = File::Open(file_name, mode);
  CHECK(f != nullptr) << absl::StrCat("Could not open '", file_name, "'");
  return f;
}

File* File::Open(absl::string_view file_name, absl::string_view mode) {
  const std::string filename = std::string(file_name);
  std::string mode_str(mode);
#if defined(_MSC_VER)
  if (mode_str == "r") {
    mode_str = "rb";
  } else if (mode_str == "w") {
    mode_str = "wb";
  }
#endif
  FILE* c_file = fopen(filename.c_str(), mode_str.c_str());
  if (c_file == nullptr) return nullptr;
  return new CFile(c_file, file_name);
}

int64_t File::ReadToString(std::string* line, uint64_t max_length) {
  CHECK(line != nullptr);
  line->clear();

  if (max_length == 0) return 0;

  int64_t needed = max_length;
  int bufsize = (needed < (2 << 20) ? needed : (2 << 20));

  std::unique_ptr<char[]> buf(new char[bufsize]);

  int64_t nread = 0;
  while (needed > 0) {
    nread = Read(buf.get(), (bufsize < needed ? bufsize : needed));
    if (nread > 0) {
      line->append(buf.get(), nread);
      needed -= nread;
    } else {
      break;
    }
  }
  return (nread >= 0 ? static_cast<int64_t>(line->size()) : -1);
}

size_t File::WriteString(absl::string_view str) {
  return Write(str.data(), str.size());
}

absl::string_view File::filename() const { return name_; }

namespace file {

absl::Status Open(absl::string_view file_name, absl::string_view mode, File** f,
                  Options options) {
  if (options == Defaults()) {
    *f = File::Open(file_name, mode);
    if (*f != nullptr) {
      return absl::OkStatus();
    }
  }
  return absl::Status(absl::StatusCode::kNotFound,
                      absl::StrCat("Could not open '", file_name, "'"));
}

File* OpenOrDie(absl::string_view file_name, absl::string_view mode,
                Options options) {
  File* f;
  CHECK_EQ(options, Defaults());
  f = File::Open(file_name, mode);
  CHECK(f != nullptr) << absl::StrCat("Could not open '", file_name, "'");
  return f;
}

absl::Status Delete(absl::string_view path, Options options) {
  if (options == Defaults()) {
    const std::string null_terminated_path = std::string(path);
    if (remove(null_terminated_path.c_str()) == 0) return absl::OkStatus();
  }
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("Could not delete '", path, "'."));
}

absl::Status Exists(absl::string_view path, Options options) {
  if (options == Defaults()) {
    const std::string null_terminated_path = std::string(path);
    if (access(null_terminated_path.c_str(), F_OK) == 0) {
      return absl::OkStatus();
    }
  }
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("File '", path, "' does not exist."));
}
}  // namespace file
