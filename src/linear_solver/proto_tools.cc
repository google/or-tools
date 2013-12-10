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
#include "base/map_util.h"
#include "base/hash.h"
#include "base/strutil.h"

using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::Reflection;
using google::protobuf::TextFormat;

namespace operations_research {

bool ConvertOldMPModelProtoToNew(const MPModelProto& src_proto,
                                 new_proto::MPModelProto* dest_proto) {
  dest_proto->set_maximize(src_proto.maximize());
  if (src_proto.has_objective_offset()) {
    dest_proto->set_objective_offset(src_proto.objective_offset());
  }
  dest_proto->set_name(src_proto.name());

  hash_map<string, int> var_id_to_index_map;
  for (int var_index = 0; var_index < src_proto.variables_size(); ++var_index) {
    const MPVariableProto& var_proto = src_proto.variables(var_index);
    const string& id = var_proto.id();
    if (ContainsKey(var_id_to_index_map, id)) {
      LOG(DFATAL) << "Duplicate variable id found: " << id;
      return false;
    }
    var_id_to_index_map[id] = var_index;
  }
  std::vector<double> objective(src_proto.variables_size(), 0.0);
  for (int k = 0; k < src_proto.objective_terms_size(); ++k) {
    const MPTermProto& term_proto = src_proto.objective_terms(k);
    const string& id = term_proto.variable_id();
    const int var_index  = FindWithDefault(var_id_to_index_map, id, -1);
    if (var_index == -1) {
      LOG(DFATAL) << "Non-existent variable with id " << id
                  << " used in objective.";
      return false;
    }
    objective[var_index] = term_proto.coefficient();
  }
  for (int var_index = 0; var_index < src_proto.variables_size(); ++var_index) {
    const MPVariableProto& var_proto = src_proto.variables(var_index);
    new_proto::MPVariableProto* const new_var = dest_proto->add_variable();
    new_var->set_lower_bound(var_proto.lb());
    new_var->set_upper_bound(var_proto.ub());
    new_var->set_name(var_proto.id());
    new_var->set_is_integer(var_proto.integer());
    new_var->set_objective_coefficient(objective[var_index]);
  }
  for (int cst_index = 0;
       cst_index < src_proto.constraints_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = src_proto.constraints(cst_index);
    new_proto::MPConstraintProto* new_cst = dest_proto->add_constraint();
    new_cst->set_lower_bound(ct_proto.lb());
    new_cst->set_upper_bound(ct_proto.ub());
    new_cst->set_name(ct_proto.id());
    for (int k = 0; k < ct_proto.terms_size(); ++k) {
      const MPTermProto& term_proto = ct_proto.terms(k);
      const string& id = term_proto.variable_id();
      const int var_index  = FindWithDefault(var_id_to_index_map, id, -1);
      if (var_index == -1) {
        LOG(DFATAL) << "Non-existent variable with id " << id
                    << " used in constraint # " << cst_index << ".";
        return false;
      }
      new_proto::MPConstraintProto::UnaryTerm* new_term =
          new_cst->add_linear_term();
      new_term->set_var_index(var_index);
      new_term->set_coefficient(term_proto.coefficient());
    }
  }
  return true;
}

bool ConvertNewMPModelProtoToOld(const new_proto::MPModelProto& src_proto,
                                 MPModelProto* dest_proto) {
  dest_proto->set_maximize(src_proto.maximize());
  if (src_proto.has_objective_offset()) {
    dest_proto->set_objective_offset(src_proto.objective_offset());
  }
  dest_proto->set_name(src_proto.name());

  // Note(user): we assume the name to be suitable as ids. If not, the generated
  // proto will be invalid (which will be detected when trying to solve it).
  // TODO(user): detect this now?
  std::vector<string> var_index_to_id;
  for (int i = 0; i < src_proto.variable_size(); ++i) {
    const new_proto::MPVariableProto& var_proto = src_proto.variable(i);
    const string& id = var_proto.name();
    var_index_to_id.push_back(id);

    // Create the variables.
    MPVariableProto* new_var = dest_proto->add_variables();
    new_var->set_lb(var_proto.lower_bound());
    new_var->set_ub(var_proto.upper_bound());
    new_var->set_id(id);
    new_var->set_integer(var_proto.is_integer());

    // Create the objective for this variable.
    if (var_proto.objective_coefficient() != 0.0) {
      MPTermProto* new_objective_term = dest_proto->add_objective_terms();
      new_objective_term->set_variable_id(id);
      new_objective_term->set_coefficient(var_proto.objective_coefficient());
    }
  }
  for (int i = 0; i < src_proto.constraint_size(); ++i) {
    const new_proto::MPConstraintProto& cst_proto = src_proto.constraint(i);

    // Create the constraint.
    MPConstraintProto* new_cst = dest_proto->add_constraints();
    new_cst->set_lb(cst_proto.lower_bound());
    new_cst->set_ub(cst_proto.upper_bound());
    new_cst->set_id(cst_proto.name());

    // Copy the linear terms.
    for (int k = 0; k < cst_proto.linear_term_size(); ++k) {
      const new_proto::MPConstraintProto::UnaryTerm& term_proto
          = cst_proto.linear_term(k);
      MPTermProto* new_term = new_cst->add_terms();
      if (term_proto.var_index() >= var_index_to_id.size()) {
        LOG(DFATAL) << "Variable index out of bound in constraint named "
                    << cst_proto.name() << ".";
        return false;
      }
      new_term->set_variable_id(var_index_to_id[term_proto.var_index()]);
      new_term->set_coefficient(term_proto.coefficient());
    }
  }
  return true;
}

bool ReadFileToProto(const string& file_name, google::protobuf::Message* proto) {
  string data;
  file::GetContents(file_name, &data, file::Defaults()).CheckSuccess();
// Note that gzipped files are currently not supported.
  // Try binary format first, then text format, then give up.
  if (proto->ParseFromString(data)) return true;
  if (google::protobuf::TextFormat::ParseFromString(data, proto)) return true;
  LOG(WARNING) << "Could not parse protocol buffer";
  return false;
}

bool WriteProtoToFile(
    const string& file_name, const google::protobuf::Message& proto,
    bool binary, bool gzipped) {
// Note that gzipped files are currently not supported.
  gzipped = false;

  string output_string;
  google::protobuf::io::StringOutputStream stream(&output_string);
  if (binary) {
    if (!proto.SerializeToZeroCopyStream(&stream)) {
      LOG(WARNING) << "Serialize to stream failed.";
      return false;
    }
  } else {
    if (!google::protobuf::TextFormat::PrintToString(proto, &output_string)) {
      LOG(WARNING) << "Printing to string failed.";
    }
  }
  const string output_file_name =
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
                              int indent_level, string* out) {
  string temp_string;
  const string indent(indent_level * 2, ' ');
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

string FullProtocolMessageAsString(
    const google::protobuf::Message& message, int indent_level) {
  string message_str;
  WriteFullProtocolMessage(message, indent_level, &message_str);
  return message_str;
}

}  // namespace operations_research
