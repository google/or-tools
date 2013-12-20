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
#ifndef OR_TOOLS_LINEAR_SOLVER_PROTO_TOOLS_H_
#define OR_TOOLS_LINEAR_SOLVER_PROTO_TOOLS_H_

#include <string>
#include "google/protobuf/message.h"

namespace operations_research {

// Exactly like file::ReadFileToProto() but also supports GZipped files.
// TODO(user): move this to ../util ?
bool ReadFileToProto(const std::string& file_name, google::protobuf::Message* proto);

// Like file::WriteProtoToFile() or file::WriteProtoToASCIIFile(), but also
// supports GZipped output.
// If 'binary'is true, ".bin" is appended to file_name.
// If 'gzipped'is true, ".gz" is appended to file_name.
// TODO(user): move this to ../util ?
bool WriteProtoToFile(const std::string& file_name, const google::protobuf::Message& proto,
                      bool binary, bool gzipped);

// Prints a proto2 message as a std::string, it behaves like TextFormat::Print()
// but also prints the default values of unset fields which is useful for
// printing parameters.
std::string FullProtocolMessageAsString(
    const google::protobuf::Message& message, int indent_level);

}  // namespace operations_research
#endif  // OR_TOOLS_LINEAR_SOLVER_PROTO_TOOLS_H_
