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

#include "ortools/util/file_util.h"

#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/json/json.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "ortools/base/file.h"
#include "ortools/base/gzipstring.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/base/status_macros.h"

namespace operations_research {

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::JsonStringToMessage;

absl::StatusOr<std::string> ReadFileToString(absl::string_view filename) {
  std::string contents;
  RETURN_IF_ERROR(file::GetContents(filename, &contents, file::Defaults()));
  // Try decompressing it.
  {
    std::string uncompressed;
    if (GunzipString(contents, &uncompressed)) contents.swap(uncompressed);
  }
  return contents;
}

absl::Status ReadFileToProto(absl::string_view filename,
                             google::protobuf::Message* proto,
                             bool allow_partial) {
  std::string data;
  RETURN_IF_ERROR(file::GetContents(filename, &data, file::Defaults()));
  return util::StatusBuilder(StringToProto(data, proto, allow_partial))
         << " in file '" << filename << "'";
}

absl::Status StringToProto(absl::string_view data,
                           google::protobuf::Message* proto,
                           bool allow_partial) {
  // Try decompressing it.
  std::string uncompressed;
  if (GunzipString(data, &uncompressed)) {
    VLOG(1) << "ReadFileToProto(): input is gzipped";
    data = uncompressed;
  }
  // Try binary format first, then text format, then JSON, then proto3 JSON,
  // then give up.
  // For some of those, like binary format and proto3 JSON, we perform
  // additional checks to verify that we have the right proto: it can happen
  // to try to read a proto of type Foo as a proto of type Bar, by mistake, and
  // we'd rather have this function fail rather than silently accept it, because
  // the proto parser is too lenient with unknown fields.
  // We don't require ByteSizeLong(parsed) == input.size(), because it may be
  // the case that the proto version changed and some fields are dropped.
  // We just fail when the difference is too large.
  constexpr double kMaxBinaryProtoParseShrinkFactor = 2;
  std::string binary_format_error;
  if (proto->ParsePartialFromString(data) &&
      (allow_partial || proto->IsInitialized())) {
    // NOTE(user): When using ParseFromString() from a generic
    // google::protobuf::Message, like we do here, all fields are stored, even
    // if they are unknown in the underlying proto type. Unless we explicitly
    // discard those 'unknown fields' here, our call to ByteSizeLong() will
    // still count the unknown payload.
    proto->DiscardUnknownFields();
    if (proto->ByteSizeLong() <
        data.size() / kMaxBinaryProtoParseShrinkFactor) {
      binary_format_error =
          "The input may be a binary protobuf payload, but it"
          " probably is from a different proto class";
    } else {
      VLOG(1) << "StringToProto(): input seems to be a binary proto";
      return absl::OkStatus();
    }
  }

  struct : public google::protobuf::io::ErrorCollector {
    void RecordError(int line, google::protobuf::io::ColumnNumber column,
                     absl::string_view error_message) override {
      if (!message.empty()) message += ", ";
      absl::StrAppendFormat(&message, "%d:%d: %s",
                            // We convert the 0-indexed line to 1-indexed.
                            line + 1, column, error_message);
    }
    std::string message;
  } text_format_error;
  google::protobuf::TextFormat::Parser text_parser;
  text_parser.RecordErrorsTo(&text_format_error);
  text_parser.AllowPartialMessage(allow_partial);
  if (text_parser.ParseFromString(data, proto)) {
    VLOG(1) << "StringToProto(): input is a text proto";
    return absl::OkStatus();
  }
  std::string json_error;
  absl::Status json_status =
      JsonStringToMessage(data, proto, JsonParseOptions());
  if (json_status.ok()) {
    // NOTE(user): We protect against the JSON proto3 parser being very lenient
    // and easily accepting any JSON as a valid JSON for our proto: if the
    // parsed proto's size is too small compared to the JSON, we probably parsed
    // a JSON that wasn't representing a valid proto.
    constexpr int kMaxJsonToBinaryShrinkFactor = 30;
    if (proto->ByteSizeLong() < data.size() / kMaxJsonToBinaryShrinkFactor) {
      json_status = absl::InvalidArgumentError(
          "The input looks like valid JSON, but probably not"
          " of the right proto class");
    } else {
      VLOG(1) << "StringToProto(): input is a proto JSON";
      return absl::OkStatus();
    }
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "binary format error: '%s', text format error: '%s', json error: '%s'",
      binary_format_error, text_format_error.message, json_status.message()));
}

absl::Status WriteProtoToFile(absl::string_view filename,
                              const google::protobuf::Message& proto,
                              ProtoWriteFormat proto_write_format, bool gzipped,
                              bool append_extension_to_file_name) {
  std::string file_type_suffix;
  std::string output_string;
  google::protobuf::io::StringOutputStream stream(&output_string);
  auto make_error = [filename](absl::string_view error_message) {
    return absl::InternalError(absl::StrFormat(
        "WriteProtoToFile('%s') failed: %s", filename, error_message));
  };
  switch (proto_write_format) {
    case ProtoWriteFormat::kProtoBinary:
      if (!proto.SerializeToZeroCopyStream(&stream)) {
        return make_error("SerializeToZeroCopyStream()");
      }
      file_type_suffix = ".bin";
      break;
    case ProtoWriteFormat::kProtoText:
      if (!google::protobuf::TextFormat::PrintToString(proto, &output_string)) {
        return make_error("TextFormat::PrintToString()");
      }
      break;
    case ProtoWriteFormat::kJson: {
      google::protobuf::util::JsonPrintOptions options;
      options.add_whitespace = true;
      options.always_print_fields_with_no_presence = true;
      options.preserve_proto_field_names = true;
      if (!google::protobuf::util::MessageToJsonString(proto, &output_string,
                                                       options)
               .ok()) {
        LOG(WARNING) << "Printing to stream failed.";
        return make_error("google::protobuf::util::MessageToJsonString()");
      }
      file_type_suffix = ".json";
      break;
    }
    case ProtoWriteFormat::kCanonicalJson:
      google::protobuf::util::JsonPrintOptions options;
      options.add_whitespace = true;
      if (!google::protobuf::util::MessageToJsonString(proto, &output_string,
                                                       options)
               .ok()) {
        LOG(WARNING) << "Printing to stream failed.";
        return make_error("google::protobuf::util::MessageToJsonString()");
      }
      file_type_suffix = ".json";
      break;
  }
  if (gzipped) {
    std::string gzip_string;
    GzipString(output_string, &gzip_string);
    output_string.swap(gzip_string);
    file_type_suffix += ".gz";
  }
  std::string output_filename(filename);
  if (append_extension_to_file_name) output_filename += file_type_suffix;
  VLOG(1) << "Writing " << output_string.size() << " bytes to '"
          << output_filename << "'";
  return file::SetContents(output_filename, output_string, file::Defaults());
}

}  // namespace operations_research
