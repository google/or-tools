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

#include "ortools/math_opt/cpp/solver_resources.h"

#include <ostream>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

std::ostream& operator<<(std::ostream& out, const SolverResources& resources) {
  out << '{' << AbslUnparseFlag(resources) << '}';
  return out;
}

SolverResourcesProto SolverResources::Proto() const {
  SolverResourcesProto ret;
  if (cpu.has_value()) {
    ret.set_cpu(cpu.value());
  }
  if (ram.has_value()) {
    ret.set_ram(ram.value());
  }
  return ret;
}

absl::StatusOr<SolverResources> SolverResources::FromProto(
    const SolverResourcesProto& proto) {
  SolverResources ret;
  if (proto.has_cpu()) {
    ret.cpu = proto.cpu();
  }
  if (proto.has_ram()) {
    ret.ram = proto.ram();
  }
  return ret;
}

bool AbslParseFlag(const absl::string_view text,
                   SolverResources* const solver_resources,
                   std::string* const error) {
  SolverResourcesProto proto;
  if (!ProtobufParseTextProtoForFlag(text, &proto, error)) {
    return false;
  }
  absl::StatusOr<SolverResources> resources = SolverResources::FromProto(proto);
  if (!resources.ok()) {
    *error = absl::StrCat(
        "SolverResourcesProto was invalid and could not convert to "
        "SolverResources: ",
        resources.status().ToString());
    return false;
  }
  *solver_resources = *std::move(resources);
  return true;
}

std::string AbslUnparseFlag(const SolverResources& solver_resources) {
  return ProtobufTextFormatPrintToStringForFlag(solver_resources.Proto());
}

}  // namespace operations_research::math_opt
