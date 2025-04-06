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
#include <zlib.h>

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
#include <iostream>  // NOLINT
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace {
enum class Format {
  NORMAL_FILE,
  GZIP_FILE,
};

static Format GetFormatFromName(absl::string_view name) {
  const int size = name.size();
  if (size > 4 && name.substr(size - 3) == ".gz") {
    return Format::GZIP_FILE;
  } else {
    return Format::NORMAL_FILE;
  }
}

class CFile : public File {
 public:
  CFile(FILE* c_file, absl::string_view name) : File(name), f_(c_file) {}
  virtual ~CFile() = default;

  // Reads "size" bytes to buf from file, buf should be pre-allocated.
  size_t Read(void* buf, size_t size) override {
    return fread(buf, 1, size, f_);
  }

  // Writes "size" bytes of buf to file, buf should be pre-allocated.
  size_t Write(const void* buf, size_t size) override {
    return fwrite(buf, 1, size, f_);
  }

  // Closes the file and delete the underlying FILE* descriptor.
  absl::Status Close(int flags) override {
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

class GzFile : public File {
  public:
  GzFile(gzFile gz_file, absl::string_view name) : File(name), f_(gz_file) {}
   virtual ~GzFile() = default;

   // Reads "size" bytes to buf from file, buf should be pre-allocated.
   size_t Read(void* buf, size_t size) override {
     return gzread(f_, buf, size);
   }

   // Writes "size" bytes of buf to file, buf should be pre-allocated.
   size_t Write(const void* buf, size_t size) override {
     return gzwrite(f_, buf, size);
   }

   // Closes the file and delete the underlying FILE* descriptor.
   absl::Status Close(int flags) override {
     absl::Status status;
     if (f_ == nullptr) {
       return status;
     }
     if (gzclose(f_) == 0) {
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
   bool Flush() override { return gzflush(f_, Z_FINISH) == Z_OK; }
 
   // Returns file size.
   size_t Size() override {
    gzFile file;
    std::string null_terminated_name = std::string(name_);
    #if defined(_MSC_VER)
    file = gzopen (null_terminated_name.c_str(), "rb");
    #else
    file = gzopen (null_terminated_name.c_str(), "r");
    #endif
    if (! file) {
      LOG(FATAL) << "Cannot get the size of '" << name_
                 << "': " << strerror(errno);
    }

    const int kLength = 5 * 1024;
    unsigned char buffer[kLength];
    size_t uncompressed_size = 0;
    while (1) {
      int err;
      int bytes_read;
      bytes_read = gzread(file, buffer, kLength - 1);
      uncompressed_size += bytes_read;
      if (bytes_read < kLength - 1) {
        if (gzeof(file)) {
          break;
        } else {
          const char* error_string;
          error_string = gzerror(file, &err);
          if (err) {
            LOG(FATAL) << "Error " << error_string;
          }
        }
      }
    }
    gzclose(file);
    return uncompressed_size;
   }

   bool Open() const override { return f_ != nullptr; }
 
  private:
   gzFile f_;
 };
 
}  // namespace

File::File(absl::string_view name) : name_(name) {}

File* File::OpenOrDie(absl::string_view file_name, absl::string_view mode) {
  File* f = File::Open(file_name, mode);
  CHECK(f != nullptr) << absl::StrCat("Could not open '", file_name, "'");
  return f;
}

File* File::Open(absl::string_view file_name, absl::string_view mode) {
  std::string null_terminated_name = std::string(file_name);
  std::string null_terminated_mode = std::string(mode);
#if defined(_MSC_VER)
  if (null_terminated_mode == "r") {
    null_terminated_mode = "rb";
  } else if (null_terminated_mode == "w") {
    null_terminated_mode = "wb";
  }
#endif
  const Format format = GetFormatFromName(file_name);
  switch (format) {
    case Format::NORMAL_FILE: {
      FILE* c_file =
          fopen(null_terminated_name.c_str(), null_terminated_mode.c_str());
      if (c_file == nullptr) return nullptr;
      return new CFile(c_file, file_name);
    }
    case Format::GZIP_FILE: {
      gzFile gz_file =
          gzopen(null_terminated_name.c_str(), null_terminated_mode.c_str());
      if (!gz_file) {
        return nullptr;
      }
      return new GzFile(gz_file, file_name);
    }
  }
  return nullptr;
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

void File::Init() {}

namespace file {
absl::Status Open(absl::string_view file_name, absl::string_view mode, File** f,
                  Options options) {
  if (options == Defaults()) {
    *f = File::Open(file_name, mode);
    if (*f != nullptr) {
      return absl::OkStatus();
    }
  }
  return absl::Status(absl::StatusCode::kInvalidArgument,
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

absl::StatusOr<std::string> GetContents(absl::string_view path,
                                        Options options) {
  std::string contents;
  absl::Status status = GetContents(path, &contents, options);
  if (!status.ok()) {
    return status;
  }
  return contents;
}

absl::Status GetContents(absl::string_view file_name, std::string* output,
                         Options options) {
  File* file;
  // For windows, the "b" is added in file::Open.
  auto status = file::Open(file_name, "r", &file, options);
  if (!status.ok()) return status;

  const int64_t size = file->Size();
  if (file->ReadToString(output, size) == size) {
    status.Update(file->Close(options));
    return status;
  }

  file->Close(options).IgnoreError();  // Even if ReadToString() fails!

  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("Could not read from '", file_name, "'."));
}

absl::Status WriteString(File* file, absl::string_view contents,
                         Options options) {
  if (options == Defaults() && file != nullptr &&
      file->Write(contents.data(), contents.size()) == contents.size()) {
    return absl::OkStatus();
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not write ", contents.size(), " bytes"));
}

absl::Status SetContents(absl::string_view file_name, absl::string_view contents,
                         Options options) {
  File* file;
  // For windows, the "b" is added in file::Open.
  auto status = file::Open(file_name, "w", &file, options);
  if (!status.ok()) return status;
  status = file::WriteString(file, contents, options);
  status.Update(file->Close(options));  // Even if WriteString() fails!
  return status;
}

namespace {
class NoOpErrorCollector : public google::protobuf::io::ErrorCollector {
 public:
  ~NoOpErrorCollector() override = default;
  void RecordError(int /*line*/, int /*column*/,
                   absl::string_view /*message*/) override {}
};
}  // namespace

absl::Status GetTextProto(absl::string_view file_name,
                          google::protobuf::Message* proto, Options options) {
  if (options == Defaults()) {
    std::string str;
    if (!GetContents(file_name, &str, file::Defaults()).ok()) {
      VLOG(1) << "Could not read '" << file_name << "'";
      return absl::Status(
          absl::StatusCode::kInvalidArgument,
          absl::StrCat("Could not read proto from '", file_name, "'."));
    }

    // Attempt to decode ASCII before deciding binary. Do it in this order because
    // it is much harder for a binary encoding to happen to be a valid ASCII
    // encoding than the other way around. For instance "index: 1\n" is a valid
    // (but nonsensical) binary encoding. We want to avoid printing errors for
    // valid binary encodings if the ASCII parsing fails, and so specify a no-op
    // error collector.
    NoOpErrorCollector error_collector;
    google::protobuf::TextFormat::Parser parser;
    parser.RecordErrorsTo(&error_collector);

    if (parser.ParseFromString(str, proto)) {  // Text format.
      return absl::OkStatus();
    }

    if (proto->ParseFromString(str)) {  // Binary format.
      return absl::OkStatus();
    }

    // Re-parse the ASCII, just to show the diagnostics (we could also get them
    // out of the ErrorCollector but this way is easier).
    google::protobuf::TextFormat::ParseFromString(str, proto);
    VLOG(1) << "Could not parse contents of '" << file_name << "'";
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not read proto from '", file_name, "'."));
}

absl::Status SetTextProto(absl::string_view file_name,
                          const google::protobuf::Message& proto,
                          Options options) {
  if (options == Defaults()) {
    std::string proto_string;
    if (google::protobuf::TextFormat::PrintToString(proto, &proto_string) &&
        file::SetContents(file_name, proto_string, file::Defaults()).ok()) {
      return absl::OkStatus();
    }
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not write proto to '", file_name, "'."));
}

absl::Status GetBinaryProto(const absl::string_view file_name,
                            google::protobuf::Message* proto, Options options) {
  std::string str;
  if (options == Defaults() &&
      GetContents(file_name, &str, file::Defaults()).ok() &&
      proto->ParseFromString(str)) {
    return absl::OkStatus();
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not read proto from '", file_name, "'."));
}

absl::Status SetBinaryProto(absl::string_view file_name,
                            const google::protobuf::Message& proto,
                            Options options) {
  if (options == Defaults()) {
    std::string proto_string;
    if (proto.AppendToString(&proto_string) &&
        file::SetContents(file_name, proto_string, file::Defaults()).ok()) {
      return absl::OkStatus();
    }
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not write proto to '", file_name, "'."));
}

absl::Status Delete(absl::string_view path, Options options) {
  if (options == Defaults()) {
    std::string null_terminated_path = std::string(path);
    if (remove(null_terminated_path.c_str()) == 0) return absl::OkStatus();
  }
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("Could not delete '", path, "'."));
}

absl::Status Exists(absl::string_view path, Options options) {
  if (options == Defaults()) {
    std::string null_terminated_path = std::string(path);
    if (access(null_terminated_path.c_str(), F_OK) == 0) {
      return absl::OkStatus();
    }
  }
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("File '", path, "' does not exist."));
}
}  // namespace file
