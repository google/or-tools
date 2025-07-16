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

#include "ortools/sat/python/wrappers.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/span.h"
#include "google/protobuf/descriptor.h"

namespace operations_research::sat::python {

// A class that generates pybind11 code for a proto message.
class Generator {
 public:
  struct Context {
    static Context TopLevel(const google::protobuf::Descriptor& msg) {
      const std::string cpp_name = GetQualifiedCppName(msg);
      const std::string shared_name =
          absl::StrCat("std::shared_ptr<", cpp_name, ">");
      return {.cpp_name = cpp_name, .self_mutable_name = shared_name};
    }

    static Context Nested(const google::protobuf::Descriptor& msg) {
      const std::string cpp_name = GetQualifiedCppName(msg);
      return {.cpp_name = cpp_name,
              .self_mutable_name = absl::StrCat(cpp_name, "*")};
    }

    std::string cpp_name;
    std::string self_mutable_name;
  };

  explicit Generator(
      absl::Span<const google::protobuf::Descriptor* absl_nonnull const> roots)
      : message_stack_(roots.begin(), roots.end()) {
    // DFS on root.
    while (!message_stack_.empty()) {
      const google::protobuf::Descriptor* const msg = message_stack_.back();
      message_stack_.pop_back();
      if (!visited_messages_.insert(msg).second) continue;
      const bool is_top_level = absl::c_linear_search(roots, msg);
      current_context_ =
          is_top_level ? Context::TopLevel(*msg) : Context::Nested(*msg);
      if (is_top_level) {
        GenerateTopLevelMessageDecl(*msg);
      } else {
        GenerateMessageDecl(*msg);
      }
      GenerateMessageFields(*msg);
      absl::StrAppend(&out_, ";\n");
    }

    // Now generate wrappers for enums, repeated and repeated ptr fields that
    // were encountered along the way.
    for (const google::protobuf::EnumDescriptor* pb_enum : enum_types_) {
      GenerateEnumDecl(*pb_enum);
    }
    for (const google::protobuf::Descriptor* msg : repeated_ptr_types_) {
      GenerateRepeatedPtrDecl(*msg);
    }
    for (const absl::string_view scalar_type : repeated_scalar_types_) {
      GenerateRepeatedScalarDecl(scalar_type);
    }
  }

  std::string Result() && { return std::move(out_); }

 private:
  template <typename DescriptorT>
  static std::string GetQualifiedCppName(const DescriptorT& descriptor) {
    return absl::StrReplaceAll(descriptor.full_name(), {{".", "::"}});
  }

  template <typename DescriptorT>
  static std::string GetEscapedName(const DescriptorT& descriptor) {
    return absl::StrReplaceAll(descriptor.full_name(), {{".", "_"}});
  }

  static std::string GetCppType(
      const google::protobuf::FieldDescriptor::CppType cpp_type,
      const google::protobuf::FieldDescriptor& field) {
    switch (cpp_type) {
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        return "int32_t";
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        return "int64_t";
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        return "uint32_t";
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        return "uint64_t";
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        return "double";
      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        return "float";
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        return "bool";
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
        return GetQualifiedCppName(*field.enum_type());
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        return "std::string";
      default:
        LOG(FATAL) << "Unsupported type: " << cpp_type;
    }
  }

  // Generates a pybind11 wrapper class declaration for a top level message.
  void GenerateTopLevelMessageDecl(const google::protobuf::Descriptor& msg) {
    CHECK(wrapper_id_.emplace(&msg, wrapper_id_.size()).second)
        << "duplicate message: " << msg.full_name();
    absl::SubstituteAndAppend(&out_, R"(
  const auto $0 = py::class_<$1, std::shared_ptr<$1>>($2, "$3"))",
                              GetWrapperName(&msg), current_context_.cpp_name,
                              GetWrapperName(msg.containing_type()),
                              msg.name());
    // Add constructor and utilities.
    absl::SubstituteAndAppend(&out_, R"(
    .def(py::init<>())
    .def("copy_from",
          [](std::shared_ptr<$0> self, std::shared_ptr<$0> other) {
            self->CopyFrom(*other); 
          })
    .def("merge_from",
          [](std::shared_ptr<$0> self, std::shared_ptr<$0> other) {
            self->MergeFrom(*other); 
          })
    .def("merge_text_format",
          [](std::shared_ptr<$0> self, const std::string& text) {
            return google::protobuf::TextFormat::MergeFromString(text, self.get());
          })
    .def("parse_text_format",
          [](std::shared_ptr<$0> self, const std::string& text) {
            return google::protobuf::TextFormat::ParseFromString(text, self.get());
          })
    .def("__copy__",
          [](std::shared_ptr<$0> self) {
            return self;
          })
    .def("__deepcopy__",
         [](std::shared_ptr<$0> self, py::dict) {
            std::shared_ptr<$0> result = std::make_shared<$0>();
            result->CopyFrom(*self);
            return result;
          })
    .def("__str__",
          [](std::shared_ptr<$0> self) {
              return operations_research::ProtobufDebugString(*self);
          }))",
                              current_context_.cpp_name);
  }

  // Generates a pybind11 wrapper class declaration for a message.
  void GenerateMessageDecl(const google::protobuf::Descriptor& msg) {
    CHECK(wrapper_id_.emplace(&msg, wrapper_id_.size()).second)
        << "duplicate message: " << msg.full_name();
    absl::SubstituteAndAppend(&out_, R"(
  const auto $0 = py::class_<$1>($2, "$3"))",
                              GetWrapperName(&msg), current_context_.cpp_name,
                              GetWrapperName(msg.containing_type()),
                              msg.name());
    // Add constructor and utilities.
    absl::SubstituteAndAppend(&out_, R"(
    .def(py::init<>())
    .def("copy_from",
          []($0* self, const $0& other) { self->CopyFrom(other); })
    .def("merge_from",
          []($0* self, const $0& other) { self->MergeFrom(other); })
    .def("merge_text_format",
          []($0* self, const std::string& text) {
            return google::protobuf::TextFormat::MergeFromString(text, self);
          })
    .def("parse_text_format",
          []($0* self, const std::string& text) {
            return google::protobuf::TextFormat::ParseFromString(text, self);
          })
    .def("__copy__",
          []($0 self) {
            return $0(self); 
          })
    .def("__deepcopy__",
         []($0 self, py::dict) {
            return $0(self); 
          })
    .def("__str__",
          []($0 self) {
              return operations_research::ProtobufDebugString(self);
          }))",
                              current_context_.cpp_name);
  }

  // Generates a pybind11 wrapper class declaration for an enum.
  void GenerateEnumDecl(const google::protobuf::EnumDescriptor& pb_enum) {
    absl::SubstituteAndAppend(&out_, R"(
  py::enum_<$0>($1, "$2"))",
                              GetQualifiedCppName(pb_enum),
                              GetWrapperName(pb_enum.containing_type()),
                              pb_enum.name());
    for (int i = 0; i < pb_enum.value_count(); ++i) {
      const google::protobuf::EnumValueDescriptor& value = *pb_enum.value(i);
      absl::SubstituteAndAppend(&out_, R"(
    .value("$0", $1))",
                                value.name(), GetQualifiedCppName(value));
    }
    absl::SubstituteAndAppend(&out_, R"(
    .export_values();)");
  }

  // Generates a pybind11 wrapper class declaration & definitions for a repeated
  // ptr.
  void GenerateRepeatedPtrDecl(const google::protobuf::Descriptor& msg) {
    absl::SubstituteAndAppend(&out_, R"(
  py::class_<google::protobuf::RepeatedPtrField<$0>>(py_module, "repeated_$1")
    .def("add",
         [](google::protobuf::RepeatedPtrField<$0>* self) {
            return self->Add();
         },
         py::return_value_policy::reference, py::keep_alive<0, 1>())
    .def("append", [](google::protobuf::RepeatedPtrField<$0>* self, const $0& value) {
            *self->Add() = value;
    })
    .def("extend",
         [](google::protobuf::RepeatedPtrField<$0>* self, const std::vector<$0>& values) {
            for (const $0& value : values) {
                  *self->Add() = value;
            }
    })
    .def("__len__", &google::protobuf::RepeatedPtrField<$0>::size)
    .def("__getitem__",
         [](google::protobuf::RepeatedPtrField<$0>* self, int index) {
            if (index >= self->size()) {
              PyErr_SetString(PyExc_IndexError, "Index out of range");
              throw py::error_already_set();
            }
            return self->Mutable(index);
         },
         py::return_value_policy::reference, py::keep_alive<0, 1>());)",
                              GetQualifiedCppName(msg), msg.name());
  }

  // Generates a pybind11 wrapper class declaration & definitions for a repeated
  // scalar.
  void GenerateRepeatedScalarDecl(absl::string_view scalar_type) {
    if (scalar_type == "std::string") {
      absl::StrAppend(&out_, R"(
  py::class_<google::protobuf::RepeatedPtrField<std::string>>(py_module, "repeated_scalar_std_string")
    .def("append",
         [](google::protobuf::RepeatedPtrField<std::string>* self, std::string str) {
            self->Add(std::move(str));
          })
    .def("extend",
         [](google::protobuf::RepeatedPtrField<std::string>* self,
            const std::vector<std::string>& values) {
            self->Add(values.begin(), values.end());
          })
    .def("__len__", [](const google::protobuf::RepeatedPtrField<std::string>& self) {
            return self.size();
         })
    .def("__getitem__",
         [](const google::protobuf::RepeatedPtrField<std::string>& self, int index) {
            if (index >= self.size()) {
              PyErr_SetString(PyExc_IndexError, "Index out of range");
              throw py::error_already_set();
            }

            return self.Get(index);
          },
         py::return_value_policy::copy)
    .def("__setitem__",
         [](google::protobuf::RepeatedPtrField<std::string>* self,
            int index, const std::string& value) {
            self->at(index) = value;
        })
    .def("__str__", [](const google::protobuf::RepeatedPtrField<std::string>& self) {
            return absl::StrCat("[", absl::StrJoin(self, ", "), "]");
    });)");
    } else {
      absl::SubstituteAndAppend(
          &out_, R"(
  py::class_<google::protobuf::RepeatedField<$0>>(py_module, "repeated_scalar_$1")
    .def("append", [](google::protobuf::RepeatedField<$0>* self, $0 value) {
          self->Add(value);
        })
    .def("extend", [](google::protobuf::RepeatedField<$0>* self,
                      const std::vector<$0>& values) {
          self->Add(values.begin(), values.end());
        })
    .def("__len__", [](const google::protobuf::RepeatedField<$0>& self) {
             return self.size();
         })
    .def("__getitem__", [](const google::protobuf::RepeatedField<$0>& self, int index) {
      if (index >= self.size()) {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        throw py::error_already_set();
      }

      return self.Get(index);
    })
    .def("__setitem__", &google::protobuf::RepeatedField<$0>::Set)
    .def("__str__", [](const google::protobuf::RepeatedField<$0>& self) {
            return absl::StrCat("[", absl::StrJoin(self, ", "), "]");
    });)",
          scalar_type, absl::StrReplaceAll(scalar_type, {{"::", "_"}}));
    }
  }

  void GenerateRepeatedField(const google::protobuf::FieldDescriptor& field) {
    const google::protobuf::Descriptor* msg_type = field.message_type();
    if (msg_type != nullptr) {
      // Repeated message.
      absl::SubstituteAndAppend(
          &out_, R"(
    .def_property_readonly(
        "$0",
        []($1 self) { return self->mutable_$2(); },
        py::return_value_policy::reference, py::keep_alive<0, 1>()))",
          field.name(), current_context_.self_mutable_name, field.name());
      // We'll need to generate the wrapping for `proto2::RepeatedPtrField<$3>`.
      repeated_ptr_types_.insert(msg_type);
      // We'll need to generate the wrapping for this message type.
      message_stack_.push_back(ABSL_DIE_IF_NULL(field.message_type()));
    } else {
      // Repeated scalar field.
      absl::SubstituteAndAppend(&out_, R"(
    .def_property_readonly(
        "$0",
        []($1 self) { return self->mutable_$0(); },
        py::return_value_policy::reference, py::keep_alive<0, 1>()))",
                                field.name(),
                                current_context_.self_mutable_name);
      // We'll need to generate the wrapping for `proto2::RepeatedField<$2>`.
      repeated_scalar_types_.insert(GetCppType(field.cpp_type(), field));
    }
  }

  void GenerateSingularField(const google::protobuf::FieldDescriptor& field) {
    if (const google::protobuf::Descriptor* msg_type = field.message_type()) {
      // Singular message.
      absl::SubstituteAndAppend(&out_, R"(
    .def_property(
        "$0",
        []($1 self) { return self->mutable_$0(); },
        []($1 self, $2 arg) { *self->mutable_$0() = arg; },
        py::return_value_policy::reference_internal)
    .def("clear_$0", []($1 self) { self->clear_$0(); })
    .def("has_$0", []($1 self) { return self->has_$0(); }))",
                                field.name(),
                                current_context_.self_mutable_name,
                                GetQualifiedCppName(*msg_type));
      // We'll need to generate the wrapping for this message type.
      message_stack_.push_back(ABSL_DIE_IF_NULL(field.message_type()));
    } else {
      if (const google::protobuf::EnumDescriptor* enum_type =
              field.enum_type()) {
        enum_types_.insert(enum_type);
      }
      // Singular scalar (int, string, ...).
      absl::SubstituteAndAppend(&out_, R"(
    .def_property(
        "$0",
        []($1 msg) { return msg->$0(); },
        []($1 msg, $2 arg) { return msg->set_$0(arg); })
    .def("clear_$0", []($1 self) { self->clear_$0(); }))",
                                field.name(),
                                current_context_.self_mutable_name,
                                GetCppType(field.cpp_type(), field));
    }
  }

  // Generates definitions for accessing fields of a message.
  void GenerateMessageFields(const google::protobuf::Descriptor& msg) {
    const std::string msg_name = GetQualifiedCppName(msg);

    for (int i = 0; i < msg.field_count(); ++i) {
      const google::protobuf::FieldDescriptor& field =
          *ABSL_DIE_IF_NULL(msg.field(i));
      if (field.is_repeated()) {
        GenerateRepeatedField(field);
      } else {
        GenerateSingularField(field);
      }
    }
  }

  // Returns the wrapper name for a message (or "py_module" if `msg` is null).
  // Dies if the scope is not found.
  std::string GetWrapperName(const google::protobuf::Descriptor* msg) {
    const auto it = wrapper_id_.find(msg);
    CHECK(it != wrapper_id_.end())
        << "wrapper id not found: " << msg->full_name();
    if (msg == nullptr) return "py_module";
    return absl::StrCat("gen_", it->second);
  }

  // This identifies the pybind11 wrapper variable for a `_class` declaration in
  // the generated code. These are used to generate enums in the correct
  // scope.
  static constexpr int kNoScope = 0;
  absl::flat_hash_map<const google::protobuf::Descriptor*, int> wrapper_id_ = {
      {nullptr, kNoScope}};

  // Output buffer.
  std::string out_;

  // Our DFS stack.
  std::vector<const google::protobuf::Descriptor* absl_nonnull> message_stack_;
  absl::flat_hash_set<const google::protobuf::Descriptor* absl_nonnull>
      visited_messages_;

  // A list of enum wrappers to generate.
  absl::flat_hash_set<const google::protobuf::EnumDescriptor* absl_nonnull>
      enum_types_;
  // A list of repeated ptr wrappers to generate.
  absl::flat_hash_set<const google::protobuf::Descriptor* absl_nonnull>
      repeated_ptr_types_;
  // A list of repeated scalar wrappers to generate.
  absl::flat_hash_set<std::string> repeated_scalar_types_;

  // Context for the current message being generated.
  Context current_context_;
};

std::string GeneratePybindCode(
    absl::Span<const google::protobuf::Descriptor* absl_nonnull const> roots) {
  return Generator(roots).Result();
}

}  // namespace operations_research::sat::python
