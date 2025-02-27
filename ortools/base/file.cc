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

File::File(FILE* descriptor, absl::string_view name)
    : f_(descriptor), name_(name) {}

bool File::Delete(absl::string_view filename) {
  std::string null_terminated_name = std::string(filename);
  return remove(null_terminated_name.c_str()) == 0;
}

bool File::Exists(absl::string_view filename) {
  std::string null_terminated_name = std::string(filename);
  return access(null_terminated_name.c_str(), F_OK) == 0;
}

size_t File::Size() {
  struct stat f_stat;
  stat(name_.c_str(), &f_stat);
  return f_stat.st_size;
}

bool File::Flush() { return fflush(f_) == 0; }

// Deletes "this" on closing.
bool File::Close() {
  bool ok = true;
  if (f_ == nullptr) {
    return ok;
  }
  if (fclose(f_) == 0) {
    f_ = nullptr;
  } else {
    ok = false;
  }
  delete this;
  return ok;
}

// Deletes "this" on closing.
absl::Status File::Close(int /*flags*/) {
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

void File::ReadOrDie(void* buf, size_t size) {
  CHECK_EQ(fread(buf, 1, size, f_), size);
}

size_t File::Read(void* buf, size_t size) { return fread(buf, 1, size, f_); }

void File::WriteOrDie(const void* buf, size_t size) {
  CHECK_EQ(fwrite(buf, 1, size, f_), size);
}
size_t File::Write(const void* buf, size_t size) {
  return fwrite(buf, 1, size, f_);
}

File* File::OpenOrDie(absl::string_view filename, absl::string_view mode) {
  File* f = File::Open(filename, mode);
  CHECK(f != nullptr) << absl::StrCat("Could not open '", filename, "'");
  return f;
}

File* File::Open(absl::string_view filename, absl::string_view mode) {
  std::string null_terminated_name = std::string(filename);
  std::string null_terminated_mode = std::string(mode);
  FILE* f_des =
      fopen(null_terminated_name.c_str(), null_terminated_mode.c_str());
  if (f_des == nullptr) return nullptr;
  File* f = new File(f_des, filename);
  return f;
}

char* File::ReadLine(char* output, uint64_t max_length) {
  return fgets(output, max_length, f_);
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

bool File::WriteLine(absl::string_view line) {
  if (Write(line.data(), line.size()) != line.size()) return false;
  return Write("\n", 1) == 1;
}

absl::string_view File::filename() const { return name_; }

bool File::Open() const { return f_ != nullptr; }

void File::Init() {}

namespace file {
absl::Status Open(absl::string_view filename, absl::string_view mode, File** f,
                  Options options) {
  if (options == Defaults()) {
    *f = File::Open(filename, mode);
    if (*f != nullptr) {
      return absl::OkStatus();
    }
  }
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("Could not open '", filename, "'"));
}

File* OpenOrDie(absl::string_view filename, absl::string_view mode,
                Options options) {
  File* f;
  CHECK_EQ(options, Defaults());
  f = File::Open(filename, mode);
  CHECK(f != nullptr) << absl::StrCat("Could not open '", filename, "'");
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

absl::Status GetContents(absl::string_view filename, std::string* output,
                         Options options) {
  File* file;
  auto status = file::Open(filename, "r", &file, options);
  if (!status.ok()) return status;

  const int64_t size = file->Size();
  if (file->ReadToString(output, size) == size) {
    status.Update(file->Close(options));
    return status;
  }
#if defined(_MSC_VER)
  // On windows, binary files needs to be opened with the "rb" flags.
  file->Close();
  // Retry in binary mode.
  status = file::Open(filename, "rb", &file, options);
  if (!status.ok()) return status;

  const int64_t b_size = file->Size();
  if (file->ReadToString(output, b_size) == b_size) {
    status.Update(file->Close(options));
    return status;
  }
#endif  // _MSC_VER

  file->Close(options).IgnoreError();  // Even if ReadToString() fails!
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      absl::StrCat("Could not read from '", filename, "'."));
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

absl::Status SetContents(absl::string_view filename, absl::string_view contents,
                         Options options) {
  File* file;
#if defined(_MSC_VER)
  auto status = file::Open(filename, "wb", &file, options);
#else
  auto status = file::Open(filename, "w", &file, options);
#endif
  if (!status.ok()) return status;
  status = file::WriteString(file, contents, options);
  status.Update(file->Close(options));  // Even if WriteString() fails!
  return status;
}

bool ReadFileToString(absl::string_view file_name, std::string* output) {
  return GetContents(file_name, output, file::Defaults()).ok();
}

bool WriteStringToFile(absl::string_view data, absl::string_view file_name) {
  return SetContents(file_name, data, file::Defaults()).ok();
}

namespace {
class NoOpErrorCollector : public google::protobuf::io::ErrorCollector {
 public:
  ~NoOpErrorCollector() override = default;
  void RecordError(int /*line*/, int /*column*/,
                   absl::string_view /*message*/) override {}
};
}  // namespace

bool ReadFileToProto(absl::string_view file_name,
                     google::protobuf::Message* proto) {
  std::string str;
  if (!ReadFileToString(file_name, &str)) {
    LOG(INFO) << "Could not read " << file_name;
    return false;
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
  if (parser.ParseFromString(str, proto)) {
    return true;
  }
  if (proto->ParseFromString(str)) {
    return true;
  }
  // Re-parse the ASCII, just to show the diagnostics (we could also get them
  // out of the ErrorCollector but this way is easier).
  google::protobuf::TextFormat::ParseFromString(str, proto);
  LOG(INFO) << "Could not parse contents of " << file_name;
  return false;
}

void ReadFileToProtoOrDie(absl::string_view file_name,
                          google::protobuf::Message* proto) {
  CHECK(ReadFileToProto(file_name, proto)) << "file_name: " << file_name;
}

bool WriteProtoToASCIIFile(const google::protobuf::Message& proto,
                           absl::string_view file_name) {
  std::string proto_string;
  return google::protobuf::TextFormat::PrintToString(proto, &proto_string) &&
         WriteStringToFile(proto_string, file_name);
}

void WriteProtoToASCIIFileOrDie(const google::protobuf::Message& proto,
                                absl::string_view file_name) {
  CHECK(WriteProtoToASCIIFile(proto, file_name)) << "file_name: " << file_name;
}

bool WriteProtoToFile(const google::protobuf::Message& proto,
                      absl::string_view file_name) {
  std::string proto_string;
  return proto.AppendToString(&proto_string) &&
         WriteStringToFile(proto_string, file_name);
}

void WriteProtoToFileOrDie(const google::protobuf::Message& proto,
                           absl::string_view file_name) {
  CHECK(WriteProtoToFile(proto, file_name)) << "file_name: " << file_name;
}

absl::Status GetTextProto(absl::string_view filename,
                          google::protobuf::Message* proto, Options options) {
  if (options == Defaults()) {
    if (ReadFileToProto(filename, proto)) return absl::OkStatus();
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not read proto from '", filename, "'."));
}

absl::Status SetTextProto(absl::string_view filename,
                          const google::protobuf::Message& proto,
                          Options options) {
  if (options == Defaults()) {
    if (WriteProtoToASCIIFile(proto, filename)) return absl::OkStatus();
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not write proto to '", filename, "'."));
}

absl::Status GetBinaryProto(const absl::string_view filename,
                            google::protobuf::Message* proto, Options options) {
  std::string str;
  if (options == Defaults() && ReadFileToString(filename, &str) &&
      proto->ParseFromString(str)) {
    return absl::OkStatus();
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not read proto from '", filename, "'."));
}

absl::Status SetBinaryProto(absl::string_view filename,
                            const google::protobuf::Message& proto,
                            Options options) {
  if (options == Defaults()) {
    if (WriteProtoToFile(proto, filename)) return absl::OkStatus();
  }
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Could not write proto to '", filename, "'."));
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
