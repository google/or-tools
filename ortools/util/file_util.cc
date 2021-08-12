// Copyright 2010-2021 Google LLC
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

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "ortools/base/file.h"
#include "ortools/base/gzipstring.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"

namespace operations_research {

using ::google::protobuf::TextFormat;
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

bool ReadFileToProto(absl::string_view filename,
                     google::protobuf::Message* proto) {
  std::string data;
  CHECK_OK(file::GetContents(filename, &data, file::Defaults()));
  // Try decompressing it.
  {
    std::string uncompressed;
    if (GunzipString(data, &uncompressed)) {
      VLOG(1) << "ReadFileToProto(): input is gzipped";
      data.swap(uncompressed);
    }
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
  if (proto->ParseFromString(data)) {
    // NOTE(user): When using ParseFromString() from a generic
    // google::protobuf::Message, like we do here, all fields are stored, even
    // if they are unknown in the underlying proto type. Unless we explicitly
    // discard those 'unknown fields' here, our call to ByteSizeLong() will
    // still count the unknown payload.
    proto->DiscardUnknownFields();
    if (proto->ByteSizeLong() <
        data.size() / kMaxBinaryProtoParseShrinkFactor) {
      VLOG(1) << "ReadFileToProto(): input may be a binary proto, but of a "
                 "different proto";
    } else {
      VLOG(1) << "ReadFileToProto(): input seems to be a binary proto";
      return true;
    }
  }
  if (google::protobuf::TextFormat::ParseFromString(data, proto)) {
    VLOG(1) << "ReadFileToProto(): input is a text proto";
    return true;
  }
  if (JsonStringToMessage(data, proto, JsonParseOptions()).ok()) {
    // NOTE(user): We protect against the JSON proto3 parser being very lenient
    // and easily accepting any JSON as a valid JSON for our proto: if the
    // parsed proto's size is too small compared to the JSON, we probably parsed
    // a JSON that wasn't representing a valid proto.
    constexpr int kMaxJsonToBinaryShrinkFactor = 30;
    if (proto->ByteSizeLong() < data.size() / kMaxJsonToBinaryShrinkFactor) {
      VLOG(1) << "ReadFileToProto(): input is probably JSON, but probably not"
                 " of the right proto";
    } else {
      VLOG(1) << "ReadFileToProto(): input is a proto JSON";
      return true;
    }
  }
  LOG(WARNING) << "Could not parse protocol buffer";
  return false;
}

bool WriteProtoToFile(absl::string_view filename,
                      const google::protobuf::Message& proto,
                      ProtoWriteFormat proto_write_format, bool gzipped,
                      bool append_extension_to_file_name) {
  std::string file_type_suffix;
  std::string output_string;
  google::protobuf::io::StringOutputStream stream(&output_string);
  switch (proto_write_format) {
    case ProtoWriteFormat::kProtoBinary:
      if (!proto.SerializeToZeroCopyStream(&stream)) {
        LOG(WARNING) << "Serialize to stream failed.";
        return false;
      }
      file_type_suffix = ".bin";
      break;
    case ProtoWriteFormat::kProtoText:
      if (!google::protobuf::TextFormat::PrintToString(proto, &output_string)) {
        LOG(WARNING) << "Printing to string failed.";
        return false;
      }
      break;
    case ProtoWriteFormat::kJson: {
      google::protobuf::util::JsonPrintOptions options;
      options.add_whitespace = true;
      options.always_print_primitive_fields = true;
      options.preserve_proto_field_names = true;
      if (!google::protobuf::util::MessageToJsonString(proto, &output_string,
                                                       options)
               .ok()) {
        LOG(WARNING) << "Printing to stream failed.";
        return false;
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
        return false;
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
  VLOG(1) << "Writing " << output_string.size() << " bytes to "
          << output_filename;
  if (!file::SetContents(output_filename, output_string, file::Defaults())
           .ok()) {
    LOG(WARNING) << "Writing to file failed.";
    return false;
  }
  return true;
}

}  // namespace operations_research
