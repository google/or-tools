// Copyright 2010-2014 Google
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

#include "ortools/base/logging.h"
#include "ortools/base/file.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace operations_research {

using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Reflection;
using ::google::protobuf::TextFormat;

bool ReadFileToProto(string_view filename, google::protobuf::Message* proto) {
  std::string data;
  CHECK_OK(file::GetContents(filename, &data, file::Defaults()));
  // Note that gzipped files are currently not supported.
  // Try binary format first, then text format, then JSON, then give up.
  if (proto->ParseFromString(data)) return true;
  if (google::protobuf::TextFormat::ParseFromString(data, proto)) return true;
  LOG(WARNING) << "Could not parse protocol buffer";
  return false;
}

bool WriteProtoToFile(string_view filename, const google::protobuf::Message& proto,
                      ProtoWriteFormat proto_write_format, bool gzipped) {
  // Note that gzipped files are currently not supported.
    gzipped = false;

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
        LOG(WARNING) << "Printing to std::string failed.";
        return false;
      }
      break;
  }
  const std::string output_filename = StrCat(filename, file_type_suffix);
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
