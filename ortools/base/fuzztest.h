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

#ifndef OR_TOOLS_BASE_FUZZTEST_H_
#define OR_TOOLS_BASE_FUZZTEST_H_

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "fuzztest/domain.h"
#include "fuzztest/fuzztest.h"
#include "fuzztest/googletest_fixture_adapter.h"
#include "fuzztest/init_fuzztest.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/logging.h"

namespace fuzztest {

// Reads protos from directory and returns a vector usable by the .WithSeeds()
// function to aid in fuzz test migrations. `is_text_format` should be true iff
// the protos are in text format.
template <class ProtoType>
std::vector<std::tuple<ProtoType>> ReadFilesFromDirectory(
    std::string_view dir) {
  std::vector<std::tuple<ProtoType>> corpus;

  for (std::tuple<std::string>& proto_tuple : ReadFilesFromDirectory(dir)) {
    std::string text_proto = std::get<0>(proto_tuple);
    ProtoType proto;
    bool was_parsed =
        google::protobuf::TextFormat::ParseFromString(text_proto, &proto);
    if (was_parsed) {
      corpus.push_back(std::make_tuple(proto));
    }
  }
  return corpus;
}

}  // namespace fuzztest

#endif  // OR_TOOLS_BASE_FUZZTEST_H_
