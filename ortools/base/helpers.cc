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

#include "ortools/base/helpers.h"

#include <cstdint>
#include <filesystem>  // NOLINT
#include <string>
#include <system_error>  // NOLINT

#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/file.h"

#ifdef ABSL_HAVE_EXCEPTIONS
#include <exception>
#endif

namespace file {

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

  return absl::Status(absl::StatusCode::kNotFound,
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

absl::Status SetContents(absl::string_view file_name,
                         absl::string_view contents, Options options) {
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

    // Attempt to decode ASCII before deciding binary. Do it in this order
    // because it is much harder for a binary encoding to happen to be a valid
    // ASCII encoding than the other way around. For instance "index: 1\n" is a
    // valid (but nonsensical) binary encoding. We want to avoid printing errors
    // for valid binary encodings if the ASCII parsing fails, and so specify a
    // no-op error collector.
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

absl::StatusOr<int64_t> GetSize(std::string_view path,
                                const file::Options& options) {
  (void)options;
#ifdef ABSL_HAVE_EXCEPTIONS
  try {
#endif
    std::filesystem::path p(path);
    std::error_code ec;
    const bool is_dir = std::filesystem::is_directory(p, ec);
    if (ec) {
      if (ec == std::errc::no_such_file_or_directory) {
        return absl::NotFoundError(ec.message());
      }
      return absl::InvalidArgumentError(ec.message());
    }
    if (is_dir) {
      return absl::FailedPreconditionError(
          absl::StrCat(path, " is a directory."));
    }
    const uint64_t size = std::filesystem::file_size(p, ec);
    if (ec) {
      if (ec == std::errc::no_such_file_or_directory) {
        return absl::NotFoundError(ec.message());
      }
      return absl::InvalidArgumentError(ec.message());
    }
    return static_cast<int64_t>(size);
#ifdef ABSL_HAVE_EXCEPTIONS
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(e.what());
  }
#endif
}

}  // namespace file
