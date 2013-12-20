// Copyright 2010-2013 Google
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
#include "linear_solver/proto_tools.h"

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/file.h"
#include "base/file.h"
#include "base/file.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/io/gzip_stream.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "base/join.h"
#include "base/map_util.h"
#include "base/hash.h"
#include "base/strutil.h"

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Reflection;
using google::protobuf::TextFormat;

namespace operations_research {

bool ReadFileToProto(const std::string& file_name, google::protobuf::Message* proto) {
  std::string data;
  file::GetContents(file_name, &data, file::Defaults()).CheckSuccess();
// Note that gzipped files are currently not supported.
  // Try binary format first, then text format, then give up.
  if (proto->ParseFromString(data)) return true;
  if (google::protobuf::TextFormat::ParseFromString(data, proto)) return true;
  LOG(WARNING) << "Could not parse protocol buffer";
  return false;
}

bool WriteProtoToFile(
    const std::string& file_name, const google::protobuf::Message& proto,
    bool binary, bool gzipped) {
// Note that gzipped files are currently not supported.
  gzipped = false;

  std::string output_string;
  google::protobuf::io::StringOutputStream stream(&output_string);
  if (binary) {
    if (!proto.SerializeToZeroCopyStream(&stream)) {
      LOG(WARNING) << "Serialize to stream failed.";
      return false;
    }
  } else {
    if (!google::protobuf::TextFormat::PrintToString(proto, &output_string)) {
      LOG(WARNING) << "Printing to std::string failed.";
    }
  }
  const std::string output_file_name =
      StrCat(file_name, binary ? ".bin" : "", gzipped ? ".gz" : "");
  VLOG(1) << "Writing " << output_string.size() << " bytes to "
          << output_file_name;
  if (!file::SetContents(output_file_name, output_string, file::Defaults())
      .ok()) {
    LOG(WARNING) << "Writing to file failed.";
    return false;
  }
  return true;
}

namespace {
void WriteFullProtocolMessage(const google::protobuf::Message& message,
                              int indent_level, std::string* out) {
  std::string temp_string;
  const std::string indent(indent_level * 2, ' ');
  const Descriptor* desc = message.GetDescriptor();
  const Reflection* refl = message.GetReflection();
  for (int i = 0; i < desc->field_count(); ++i) {
    const FieldDescriptor* fd = desc->field(i);
    const bool repeated = fd->is_repeated();
    const int start = repeated ? 0 : -1;
    const int limit = repeated ? refl->FieldSize(message, fd) : 0;
    for (int j = start; j < limit; ++j) {
      StrAppend(out, indent, fd->name());
      if (fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
        StrAppend(out, " {\n");
        const google::protobuf::Message& nested_message = repeated
            ? refl->GetRepeatedMessage(message, fd, j)
            : refl->GetMessage(message, fd);
        WriteFullProtocolMessage(nested_message, indent_level + 1, out);
        StrAppend(out, indent, "}\n");
      } else {
        TextFormat::PrintFieldValueToString(message, fd, j, &temp_string);
        StrAppend(out, ": ", temp_string, "\n");
      }
    }
  }
}
}  // namespace

std::string FullProtocolMessageAsString(
    const google::protobuf::Message& message, int indent_level) {
  std::string message_str;
  WriteFullProtocolMessage(message, indent_level, &message_str);
  return message_str;
}

}  // namespace operations_research
