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

#include "ortools/util/java/wrappers.h"

#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/span.h"
#include "google/protobuf/descriptor.h"

namespace operations_research::util::java {

namespace {

std::string ToPascalCase(absl::string_view input) {
  std::string result;
  bool capitalize_next = true;
  for (char c : input) {
    if (c == '_') {
      capitalize_next = true;
    } else {
      if (capitalize_next) {
        result.push_back(toupper(c));
      } else {
        result.push_back(c);
      }
      capitalize_next = isdigit(c);
    }
  }
  return result;
}

std::string GetCppType(google::protobuf::FieldDescriptor::CppType type,
                       const google::protobuf::FieldDescriptor& field) {
  switch (type) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return "::int32_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return "::int64_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return "::uint32_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return "::uint64_t";
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return "double";
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return "float";
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return "bool";
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      return absl::StrReplaceAll(std::string(field.enum_type()->full_name()),
                                 {{".", "::"}});
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return "std::string";
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return absl::StrReplaceAll(std::string(field.message_type()->full_name()),
                                 {{".", "::"}});
  }
  return "";
}

std::string GetJavaBoxedType(absl::string_view clean_name) {
  if (clean_name == "int") return "Integer";
  if (clean_name == "long") return "Long";
  if (clean_name == "uint32") return "Long";
  if (clean_name == "uint64") return "Long";
  if (clean_name == "double") return "Double";
  if (clean_name == "float") return "Float";
  if (clean_name == "bool") return "Boolean";
  return std::string(clean_name);
}

std::string GetJavaUnboxedType(absl::string_view clean_name) {
  if (clean_name == "::int32_t") return "int";
  if (clean_name == "::int64_t") return "long";
  if (clean_name == "double" || clean_name == "float" || clean_name == "bool") {
    return std::string(clean_name);
  }
  LOG(FATAL) << "Unsupported unboxed type: " << clean_name;
}

std::string GetJavaName(const google::protobuf::Descriptor& msg) {
  std::string name(msg.name());
  const google::protobuf::Descriptor* parent = msg.containing_type();
  while (parent != nullptr) {
    name = absl::StrCat(parent->name(), ".", name);
    parent = parent->containing_type();
  }
  return name;
}

}  // namespace

// A class that generates SWIG code for a proto message.
class Generator {
 public:
  explicit Generator(
      absl::Span<const google::protobuf::Descriptor* absl_nonnull const> roots,
      absl::Span<const google::protobuf::EnumDescriptor* absl_nonnull const>
          enums) {
    // Collect all messages and enums.
    std::vector<const google::protobuf::Descriptor*> stack(roots.begin(),
                                                           roots.end());
    while (!stack.empty()) {
      const google::protobuf::Descriptor* msg = stack.back();
      stack.pop_back();
      if (!all_messages_.insert(msg).second) continue;

      for (int i = 0; i < msg->nested_type_count(); ++i) {
        stack.push_back(msg->nested_type(i));
      }
      for (int i = 0; i < msg->enum_type_count(); ++i) {
        all_enums_.insert(msg->enum_type(i));
      }
      for (int i = 0; i < msg->field_count(); ++i) {
        const auto* field = msg->field(i);
        if (field->message_type()) {
          stack.push_back(field->message_type());
          if (field->is_repeated()) {
            repeated_ptr_types_.insert(field->message_type());
          }
        } else if (field->enum_type()) {
          all_enums_.insert(field->enum_type());
        }
        if (field->is_repeated() && !field->message_type() &&
            field->cpp_type() !=
                google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
          repeated_scalar_types_.insert(GetCppType(field->cpp_type(), *field));
        }
      }
    }
    for (const auto* pb_enum : enums) {
      all_enums_.insert(pb_enum);
    }

    absl::StrAppend(&out_,
                    "%{ \n#include "
                    "\"ortools/util/java/jni_helper.h\"\n%} \n");

    absl::StrAppend(&out_, "%typemap(javafinalize) SWIGTYPE \"\"\n");

    absl::StrAppend(&out_, "%include \"enums.swg\"\n");

    for (const auto* msg : all_messages_) {
      const std::string cpp_name = GetQualifiedCppName(*msg);
      const std::string java_name = GetJavaName(*msg);

      absl::SubstituteAndAppend(&out_, "%typemap(javacode) $0 %{\n", cpp_name);
      absl::SubstituteAndAppend(
          &out_, "  public static class Builder extends $0 {\n", java_name);
      absl::StrAppend(&out_,
                      "    public Builder(long cPtr, boolean cMemoryOwn) {\n");
      absl::StrAppend(&out_, "      super(cPtr, cMemoryOwn);\n");
      absl::StrAppend(&out_, "    }\n");
      absl::SubstituteAndAppend(&out_, "    public $0 build() {\n", java_name);
      absl::StrAppend(&out_, "      return this;\n");
      absl::StrAppend(&out_, "    }\n");
      absl::StrAppend(&out_, "  }\n");
      absl::StrAppend(&out_, "  public Builder toBuilder() {\n");
      absl::StrAppend(&out_, "    return new Builder(getCPtr(this), false);\n");
      absl::StrAppend(&out_, "  }\n");

      // Add equals and hashCode.
      absl::StrAppend(&out_, "  @Override\n");
      absl::StrAppend(&out_, "  public boolean equals(Object obj) {\n");
      absl::SubstituteAndAppend(&out_, "    if (obj instanceof $0) {\n",
                                java_name);
      absl::SubstituteAndAppend(
          &out_, "      return getCPtr(this) == getCPtr(($0)obj);\n",
          java_name);
      absl::StrAppend(&out_, "    }\n");
      absl::StrAppend(&out_, "    return false;\n");
      absl::StrAppend(&out_, "  }\n");
      absl::StrAppend(&out_, "  @Override\n");
      absl::StrAppend(&out_, "  public int hashCode() {\n");
      absl::StrAppend(&out_, "    return (int)getCPtr(this);\n");
      absl::StrAppend(&out_, "  }\n");

      absl::StrAppend(&out_, "%}\n");

      absl::SubstituteAndAppend(
          &out_, "%typemap(jstype) $0* newBuilder \"$1.Builder\"\n", cpp_name,
          java_name);
      absl::SubstituteAndAppend(&out_, "%typemap(javaout) $0* newBuilder {\n",
                                cpp_name);
      absl::SubstituteAndAppend(
          &out_, "    return new $0.Builder($$jnicall, true);\n", java_name);
      absl::StrAppend(&out_, "  }\n");

      for (int i = 0; i < msg->field_count(); ++i) {
        const auto* field = msg->field(i);
        if (field->message_type()) {
          const std::string field_java_name =
              GetJavaName(*field->message_type());
          const std::string field_cpp_name =
              GetQualifiedCppName(*field->message_type());
          const std::string camel_name = ToPascalCase(field->name());

          if (field->is_repeated()) {
            absl::SubstituteAndAppend(
                &out_, "%typemap(jstype) $0* add$1Builder \"$2.Builder\"\n",
                field_cpp_name, camel_name, field_java_name);
            absl::SubstituteAndAppend(&out_,
                                      "%typemap(javaout) $0* add$1Builder {\n",
                                      field_cpp_name, camel_name);
            absl::SubstituteAndAppend(
                &out_, "    return new $0.Builder($$jnicall, false);\n",
                field_java_name);
            absl::StrAppend(&out_, "  }\n");

            absl::SubstituteAndAppend(
                &out_, "%typemap(jstype) $0* get$1Builder \"$2.Builder\"\n",
                field_cpp_name, camel_name, field_java_name);
            absl::SubstituteAndAppend(&out_,
                                      "%typemap(javaout) $0* get$1Builder {\n",
                                      field_cpp_name, camel_name);
            absl::SubstituteAndAppend(
                &out_, "    return new $0.Builder($$jnicall, false);\n",
                field_java_name);
            absl::StrAppend(&out_, "  }\n");
          } else {
            absl::SubstituteAndAppend(
                &out_, "%typemap(jstype) $0* get$1Builder \"$2.Builder\"\n",
                field_cpp_name, camel_name, field_java_name);
            absl::SubstituteAndAppend(&out_,
                                      "%typemap(javaout) $0* get$1Builder {\n",
                                      field_cpp_name, camel_name);
            absl::SubstituteAndAppend(
                &out_, "    return new $0.Builder($$jnicall, false);\n",
                field_java_name);
            absl::StrAppend(&out_, "  }\n");

            absl::SubstituteAndAppend(
                &out_, "%typemap(jstype) $0* mutable$1 \"$2.Builder\"\n",
                field_cpp_name, camel_name, field_java_name);
            absl::SubstituteAndAppend(&out_,
                                      "%typemap(javaout) $0* mutable$1 {\n",
                                      field_cpp_name, camel_name);
            absl::SubstituteAndAppend(
                &out_, "    return new $0.Builder($$jnicall, false);\n",
                field_java_name);
            absl::StrAppend(&out_, "  }\n");
          }
        }
      }
    }

    for (const auto* e : all_enums_) {
      const std::string cpp_enum_name = GetQualifiedCppName(*e);
      absl::SubstituteAndAppend(&out_, "%typemap(javacode) $0 %{\n",
                                cpp_enum_name);
      absl::StrAppend(&out_, "  public final int getNumber() {\n");
      absl::StrAppend(&out_, "    return swigValue();\n");
      absl::StrAppend(&out_, "  }\n");
      absl::SubstituteAndAppend(
          &out_, "  public static $0 forNumber(int value) {\n", e->name());
      absl::SubstituteAndAppend(&out_, "    return swigToEnum(value);\n");
      absl::StrAppend(&out_, "  }\n");
      absl::StrAppend(&out_, "%}\n");
    }

    // Grouping by namespace/package for declarations.
    absl::flat_hash_set<std::string> packages;
    for (const auto* msg : all_messages_)
      packages.insert(std::string(msg->file()->package()));
    for (const auto* e : all_enums_)
      packages.insert(std::string(e->file()->package()));

    // 1. Forward declarations for classes.
    for (const auto& pkg : packages) {
      std::vector<std::string> parts = absl::StrSplit(pkg, '.');
      for (const auto& part : parts)
        absl::SubstituteAndAppend(&out_, "namespace $0 {\n", part);
      for (const auto* msg : all_messages_) {
        if (msg->file()->package() == pkg &&
            msg->containing_type() == nullptr) {
          absl::SubstituteAndAppend(&out_, "class $0;\n", msg->name());
        }
      }
      for (int i = 0; i < parts.size(); ++i) absl::StrAppend(&out_, "}\n");
    }

    for (const auto* msg : repeated_ptr_types_) {
      const std::string clean_name = GetEscapedName(*msg);
      const std::string java_name = GetJavaName(*msg);

      absl::SubstituteAndAppend(
          &out_, "%rename(getInternal) RepeatedPtrField_$0::get;\n",
          clean_name);
      absl::SubstituteAndAppend(
          &out_, "%rename(setInternal) RepeatedPtrField_$0::set;\n",
          clean_name);
      absl::SubstituteAndAppend(
          &out_, "%rename(removeInternal) RepeatedPtrField_$0::remove;\n",
          clean_name);
      absl::SubstituteAndAppend(
          &out_,
          "%rename(addInternal) RepeatedPtrField_$0::add(int, const $1&);\n",
          clean_name, GetQualifiedCppName(*msg));

      absl::SubstituteAndAppend(&out_,
                                "%typemap(javabase) RepeatedPtrField_$0 "
                                "\"java.util.AbstractList<$1>\";\n",
                                clean_name, java_name);
      absl::SubstituteAndAppend(
          &out_, "%typemap(javacode) RepeatedPtrField_$0 %{\n", clean_name);
      absl::SubstituteAndAppend(&out_, R"(
  @Override
  public $0 get(int index) {
    return getInternal(index);
  }

  @Override
  public boolean add($0 e) {
    append(e);
    return true;
  }

  @Override
  public void add(int index, $0 element) {
    addInternal(index, element);
  }

  @Override
  public $0 set(int index, $0 element) {
    $0 old = get(index);
    setInternal(index, element);
    return old;
  }

  @Override
  public $0 remove(int index) {
    $0 old = get(index);
    removeInternal(index);
    return old;
  }
%})",
                                java_name);
    }

    absl::SubstituteAndAppend(
        &out_, "%rename(getInternal) RepeatedPtrField_string::get;\n");
    absl::SubstituteAndAppend(
        &out_, "%rename(setInternal) RepeatedPtrField_string::set;\n");
    absl::SubstituteAndAppend(
        &out_, "%rename(removeInternal) RepeatedPtrField_string::remove;\n");
    absl::SubstituteAndAppend(&out_,
                              "%rename(addInternal) "
                              "RepeatedPtrField_string::add(int, const "
                              "std::string&);\n");

    absl::StrAppend(&out_,
                    "%typemap(javabase) RepeatedPtrField_string "
                    "\"java.util.AbstractList<String>\";\n");
    absl::StrAppend(&out_, "%typemap(javacode) RepeatedPtrField_string %{\n");
    absl::StrAppend(&out_, R"(
  @Override
  public String get(int index) {
    return getInternal(index);
  }

  @Override
  public boolean add(String e) {
    append(e);
    return true;
  }

  @Override
  public void add(int index, String element) {
    addInternal(index, element);
  }

  @Override
  public String set(int index, String element) {
    String old = get(index);
    Object unused = setInternal(index, element);
    return old;
  }

  @Override
  public String remove(int index) {
    String old = get(index);
    removeInternal(index);
    return old;
  }
%})");

    for (const auto& scalar : repeated_scalar_types_) {
      std::string clean_name = GetJavaUnboxedType(scalar);
      const std::string boxed_type = GetJavaBoxedType(clean_name);

      absl::SubstituteAndAppend(
          &out_, "%rename(getInternal) RepeatedField_$0::get;\n", clean_name);
      absl::SubstituteAndAppend(
          &out_, "%rename(setInternal) RepeatedField_$0::set;\n", clean_name);
      absl::SubstituteAndAppend(
          &out_, "%rename(removeInternal) RepeatedField_$0::remove;\n",
          clean_name);
      absl::SubstituteAndAppend(
          &out_, "%rename(addInternal) RepeatedField_$0::add(int, $1);\n",
          clean_name, scalar);

      absl::SubstituteAndAppend(
          &out_,
          "%javamethodmodifiers RepeatedField_$0::append($1 value) "
          "\"  "
          "public\";\n",
          clean_name, scalar);

      absl::SubstituteAndAppend(&out_,
                                "%typemap(javabase) RepeatedField_$0 "
                                "\"java.util.AbstractList<$1>\";\n",
                                clean_name, boxed_type);
      absl::SubstituteAndAppend(
          &out_, "%typemap(javacode) RepeatedField_$0 %{\n", clean_name);
      absl::SubstituteAndAppend(&out_, R"(
  @Override
  public $0 get(int index) {
    return getInternal(index);
  }

  @Override
  public boolean add($0 e) {
    append(e);
    return true;
  }

  @Override
  public void add(int index, $0 element) {
    addInternal(index, element);
  }

  @Override
  public $0 set(int index, $0 element) {
    $0 old = get(index);
    Object unused = setInternal(index, element);
    return old;
  }

  @Override
  public $0 remove(int index) {
    $0 old = get(index);
    removeInternal(index);
    return old;
  }
%})",
                                boxed_type);
    }
    absl::StrAppend(
        &out_,
        "%javamethodmodifiers RepeatedPtrField_string::append(const "
        "std::string& value) "
        "\"  "
        "public\";\n");

    // 2. Non-template classes for repeated fields (for SWIG).
    absl::StrAppend(&out_, "%inline %{\n");
    for (const auto* msg : repeated_ptr_types_) {
      std::string full_name(msg->full_name());
      absl::StrReplaceAll({{".", "::"}}, &full_name);
      std::string clean_name = GetEscapedName(*msg);
      absl::SubstituteAndAppend(&out_, R"(
class RepeatedPtrField_$0 {
 public:
  int size() const { return reinterpret_cast<const google::protobuf::RepeatedPtrField<$1>*>(this)->size(); }
  void clear() { reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this)->Clear(); }
  $1* add() { return reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this)->Add(); }
  void append(const $1& value) { *reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this)->Add() = value; }
  void add(int index, const $1& value) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this);
    *field->Add() = value;
    for (int i = field->size() - 1; i > index; --i) {
      field->SwapElements(i, i - 1);
    }
  }
  $1* get(int index) { return reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this)->Mutable(index); }
  void set(int index, const $1& value) { *reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this)->Mutable(index) = value; }
  void remove(int index) { reinterpret_cast<google::protobuf::RepeatedPtrField<$1>*>(this)->DeleteSubrange(index, 1); }
};
)",
                                clean_name, full_name);
    }
    for (const auto& scalar : repeated_scalar_types_) {
      std::string clean_name = GetJavaUnboxedType(scalar);
      absl::SubstituteAndAppend(&out_, R"(
class RepeatedField_$0 {
 public:
  int size() const { return reinterpret_cast<const google::protobuf::RepeatedField<$1>*>(this)->size(); }
  void clear() { reinterpret_cast<google::protobuf::RepeatedField<$1>*>(this)->Clear(); }
  RepeatedField_$0* append($1 value) { reinterpret_cast<google::protobuf::RepeatedField<$1>*>(this)->Add(value); return this; }
  void add(int index, $1 value) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedField<$1>*>(this);
    field->Add(value);
    for (int i = field->size() - 1; i > index; --i) {
      field->SwapElements(i, i - 1);
    }
  }
  $1 get(int index) const { return reinterpret_cast<const google::protobuf::RepeatedField<$1>*>(this)->Get(index); }
  RepeatedField_$0* set(int index, $1 value) { reinterpret_cast<google::protobuf::RepeatedField<$1>*>(this)->Set(index, value); return this; }
  void remove(int index) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedField<$1>*>(this);
    field->erase(field->begin() + index);
  }
};
)",
                                clean_name, scalar);
    }
    absl::StrAppend(&out_, R"(
class RepeatedPtrField_string {
 public:
  int size() const { return reinterpret_cast<const google::protobuf::RepeatedPtrField<std::string>*>(this)->size(); }
  void clear() { reinterpret_cast<google::protobuf::RepeatedPtrField<std::string>*>(this)->Clear(); }
  RepeatedPtrField_string* append(const std::string& value) { reinterpret_cast<google::protobuf::RepeatedPtrField<std::string>*>(this)->Add(std::string(value)); return this; }
  void add(int index, const std::string& value) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedPtrField<std::string>*>(this);
    *field->Add() = value;
    for (int i = field->size() - 1; i > index; --i) {
      field->SwapElements(i, i - 1);
    }
  }
  std::string get(int index) const { return reinterpret_cast<const google::protobuf::RepeatedPtrField<std::string>*>(this)->Get(index); }
  RepeatedPtrField_string* set(int index, const std::string& value) { *reinterpret_cast<google::protobuf::RepeatedPtrField<std::string>*>(this)->Mutable(index) = std::string(value); return this; }
  void remove(int index) { reinterpret_cast<google::protobuf::RepeatedPtrField<std::string>*>(this)->DeleteSubrange(index, 1); }
};
)");
    absl::StrAppend(&out_, "%}\n");

    for (const auto& pkg : packages) {
      std::vector<std::string> parts = absl::StrSplit(pkg, '.');
      for (const auto& part : parts)
        absl::SubstituteAndAppend(&out_, "namespace $0 {\n", part);
      for (const auto* msg : all_messages_) {
        if (msg->file()->package() == pkg &&
            msg->containing_type() == nullptr) {
          GenerateMinimalDecl(*msg);
        }
      }
      for (const auto* e : all_enums_) {
        if (e->file()->package() == pkg && e->containing_type() == nullptr) {
          GenerateEnumDecl(*e);
        }
      }
      for (int i = 0; i < parts.size(); ++i) absl::StrAppend(&out_, "}\n");
    }

    // Now generate %extend for each message
    for (const auto* msg : all_messages_) {
      GenerateMessageExtend(*msg, GetJavaName(*msg));
    }
  }

  std::string Result() && { return std::move(out_); }

 private:
  void GenerateMinimalDecl(const google::protobuf::Descriptor& msg) {
    absl::SubstituteAndAppend(&out_, "class $0 {\n public:\n", msg.name());
    for (int i = 0; i < msg.nested_type_count(); ++i) {
      GenerateMinimalDecl(*msg.nested_type(i));
    }
    for (int i = 0; i < msg.enum_type_count(); ++i) {
      GenerateEnumDecl(*msg.enum_type(i));
    }
    absl::StrAppend(&out_, "};\n");
  }

  void GenerateEnumDecl(const google::protobuf::EnumDescriptor& pb_enum) {
    absl::SubstituteAndAppend(&out_, "enum $0 {\n", pb_enum.name());
    for (int i = 0; i < pb_enum.value_count(); ++i) {
      const google::protobuf::EnumValueDescriptor& value = *pb_enum.value(i);
      absl::SubstituteAndAppend(&out_, "  $0 = $1,\n", value.name(),
                                value.number());
    }
    absl::StrAppend(&out_, "};\n");
  }

  template <typename DescriptorT>
  static std::string GetQualifiedCppName(const DescriptorT& descriptor) {
    return absl::StrReplaceAll(std::string(descriptor.full_name()),
                               {{".", "::"}});
  }

  template <typename DescriptorT>
  static std::string GetEscapedName(const DescriptorT& descriptor) {
    return absl::StrReplaceAll(std::string(descriptor.full_name()),
                               {{".", "_"}, {"::", "_"}});
  }

  void GenerateMessageExtend(const google::protobuf::Descriptor& msg,
                             absl::string_view java_name) {
    const std::string cpp_name = GetQualifiedCppName(msg);
    const std::string unqualified_name(msg.name());

    absl::SubstituteAndAppend(&out_, "%extend $0 {\n", cpp_name);

    // Default constructor for mirror classes. MUST be unqualified name in SWIG.
    absl::SubstituteAndAppend(&out_, "  $0() { return new $1(); }\n",
                              unqualified_name, cpp_name);
    absl::SubstituteAndAppend(
        &out_, "  static $0* create() { return new $0(); }\n", cpp_name);

    // parseFrom and toByteArray for bridge.
    absl::SubstituteAndAppend(&out_, R"(
  static $0* newBuilder() { return new $0(); }
  $0* getBuilder() { return $$self; }
  $0* build() { return $$self; }
  void mergeFrom(jbyteArray data) {
    JNIEnv* env = operations_research::util::java::GetThreadLocalJniEnv();
    int len = env->GetArrayLength(data);
    jbyte* buffer = env->GetByteArrayElements(data, nullptr);
    $$self->ParsePartialFromArray(buffer, len);
    env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
  }
  void mergeFrom(const $0& other) {
    $$self->MergeFrom(other);
  }
  void copyFrom(const $0& other) {
    $$self->CopyFrom(other);
  }
  static $0* parseFrom(jbyteArray data) {
    JNIEnv* env = operations_research::util::java::GetThreadLocalJniEnv();
    int len = env->GetArrayLength(data);
    jbyte* buffer = env->GetByteArrayElements(data, nullptr);
    $0* proto = new $0;
    bool success = proto->ParseFromArray(buffer, len);
    env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
    if (!success) {
      delete proto;
      return nullptr;
    }
    return proto;
  }

  bool parseTextFormat(const std::string& text) {
    return google::protobuf::TextFormat::ParseFromString(text, $$self);
  }

  jbyteArray toByteArray() const {
    int len = $$self->ByteSizeLong();
    JNIEnv* env = operations_research::util::java::GetThreadLocalJniEnv();
    jbyteArray data = env->NewByteArray(len);
    jbyte* buffer = env->GetByteArrayElements(data, nullptr);
    $$self->SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer));
    env->ReleaseByteArrayElements(data, buffer, 0);
    return data;
  }

  std::string toString() const {
    return operations_research::ProtobufDebugString(*$$self);
  }

  void clear() {
    $$self->Clear();
  }
)",
                              cpp_name);

    for (int i = 0; i < msg.field_count(); ++i) {
      const google::protobuf::FieldDescriptor& field =
          *ABSL_DIE_IF_NULL(msg.field(i));
      const std::string camel_name = ToPascalCase(field.name());
      if (field.is_repeated()) {
        const google::protobuf::Descriptor* msg_type = field.message_type();
        if (msg_type != nullptr) {
          const std::string escaped_name = GetEscapedName(*msg_type);
          absl::SubstituteAndAppend(&out_, R"(
  RepeatedPtrField_$0* get$1List() {
    return (RepeatedPtrField_$0*)$$self->mutable_$2();
  }
  RepeatedPtrField_$0* mutable$1() {
    return (RepeatedPtrField_$0*)$$self->mutable_$2();
  }
  int get$1Count() const {
    return $$self->$2_size();
  }
  const $3& get$1(int index) const {
    return $$self->$2(index);
  }
  $3* add$1() {
    return $$self->add_$2();
  }
  $3* add$1Builder() {
    return $$self->add_$2();
  }
  $3* get$1Builder(int index) {
    return $$self->mutable_$2(index);
  }
  void set$1(int index, const $3& value) {
    *$$self->mutable_$2(index) = value;
  }
  %typemap(jstype) $4* add$1 "$5.Builder"
  %typemap(javaout) $4* add$1 {
    long cPtr = $$jnicall;
    return this instanceof $5.Builder ? ($5.Builder)this : new $5.Builder(cPtr, false);
  }
  %javamethodmodifiers add$1(const $3& value) "public";
  $4* add$1(const $3& value) {
    *$$self->add_$2() = value;
    return $$self;
  }
)",
                                    escaped_name, camel_name, field.name(),
                                    GetQualifiedCppName(*msg_type),
                                    unqualified_name, java_name);
        } else {
          const std::string cpp_type = GetCppType(field.cpp_type(), field);
          if (field.cpp_type() ==
              google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
            absl::SubstituteAndAppend(&out_, R"(
  RepeatedPtrField_string* get$0List() {
    return (RepeatedPtrField_string*)$$self->mutable_$1();
  }
  RepeatedPtrField_string* mutable$0() {
    return (RepeatedPtrField_string*)$$self->mutable_$1();
  }
  int get$0Count() const {
    return $$self->$1_size();
  }
  const std::string& get$0(int index) const {
    return $$self->$1(index);
  }
  void set$0(int index, const std::string& value) {
    *$$self->mutable_$1(index) = value;
  }
  %typemap(jstype) $2* add$0 "$3.Builder"
  %typemap(javaout) $2* add$0 {
    long cPtr = $$jnicall;
    return this instanceof $3.Builder ? ($3.Builder)this : new $3.Builder(cPtr, false);
  }
  %javamethodmodifiers add$0(const std::string& value) "public";
  $2* add$0(const std::string& value) {
    $$self->add_$1(value);
    return $$self;
  }
)",
                                      camel_name, field.name(),
                                      unqualified_name, java_name);
          } else {
            std::string clean_scalar_type = GetJavaUnboxedType(cpp_type);
            absl::SubstituteAndAppend(&out_, R"(
  RepeatedField_$0* get$1List() {
    return (RepeatedField_$0*)$$self->mutable_$2();
  }
  RepeatedField_$0* mutable$1() {
    return (RepeatedField_$0*)$$self->mutable_$2();
  }
  int get$1Count() const {
    return $$self->$2_size();
  }
  $3 get$1(int index) const {
    return $$self->$2(index);
  }
  void set$1(int index, $3 value) {
    $$self->set_$2(index, value);
  }
  %typemap(jstype) $4* add$1 "$5.Builder"
  %typemap(javaout) $4* add$1 {
    long cPtr = $$jnicall;
    return this instanceof $5.Builder ? ($5.Builder)this : new $5.Builder(cPtr, false);
  }
  %javamethodmodifiers add$1($3 value) "public";
  $4* add$1($3 value) {
    $$self->add_$2(value);
    return $$self;
  }
)",
                                      clean_scalar_type, camel_name,
                                      field.name(), cpp_type, unqualified_name,
                                      java_name);
          }
        }
      } else {
        const std::string cpp_type = GetCppType(field.cpp_type(), field);
        if (field.message_type() != nullptr) {
          absl::SubstituteAndAppend(&out_, R"(
  $0* get$1() {
    return $$self->mutable_$2();
  }
  $0* get$1Builder() {
    return $$self->mutable_$2();
  }
  %javamethodmodifiers mutable$1() "public";
  $0* mutable$1() {
    return $$self->mutable_$2();
  }
  %typemap(jstype) $3* set$1 "$4.Builder"
  %typemap(javaout) $3* set$1 {
    long cPtr = $$jnicall;
    return this instanceof $4.Builder ? ($4.Builder)this : new $4.Builder(cPtr, false);
  }
  %javamethodmodifiers set$1(const $0& value) "public";
  $3* set$1(const $0& value) {
    *$$self->mutable_$2() = value;
    return $$self;
  }
  bool has$1() const {
    return $$self->has_$2();
  }
)",
                                    cpp_type, camel_name, field.name(),
                                    unqualified_name, java_name);
        } else {
          absl::SubstituteAndAppend(&out_, R"(
  %typemap(jstype) $3* set$1 "$4.Builder"
  %typemap(javaout) $3* set$1 {
    long cPtr = $$jnicall;
    return this instanceof $4.Builder ? ($4.Builder)this : new $4.Builder(cPtr, false);
  }
  %javamethodmodifiers set$1($0 val) "public";
  $3* set$1($0 val) {
    $$self->set_$2(val);
    return $$self;
  }
  $0 get$1() const {
    return $$self->$2();
  }
)",
                                    cpp_type, camel_name, field.name(),
                                    unqualified_name, java_name);
        }
        if (field.has_presence() && field.message_type() == nullptr) {
          absl::SubstituteAndAppend(&out_, R"(
  bool has$0() const {
    return $$self->has_$1();
  }
)",
                                    camel_name, field.name());
        }
      }
      absl::SubstituteAndAppend(&out_, R"(
  void clear$0() {
    $$self->clear_$1();
  }
)",
                                camel_name, field.name());
    }

    for (int i = 0; i < msg.oneof_decl_count(); ++i) {
      const google::protobuf::OneofDescriptor& oneof =
          *ABSL_DIE_IF_NULL(msg.oneof_decl(i));
      absl::SubstituteAndAppend(&out_, R"(
  void clear$0() {
    $$self->clear_$1();
  }
)",
                                ToPascalCase(oneof.name()), oneof.name());
    }

    absl::StrAppend(&out_, "}\n");
  }

  // Output buffer.
  std::string out_;

  absl::flat_hash_set<const google::protobuf::Descriptor*> all_messages_;
  absl::flat_hash_set<const google::protobuf::EnumDescriptor*> all_enums_;

  // A list of repeated ptr wrappers to generate.
  absl::flat_hash_set<const google::protobuf::Descriptor*> repeated_ptr_types_;
  // A list of repeated scalar wrappers to generate.
  absl::flat_hash_set<std::string> repeated_scalar_types_;
};

std::string GenerateJavaSwigCode(
    absl::Span<const google::protobuf::Descriptor* absl_nonnull const> roots,
    absl::Span<const google::protobuf::EnumDescriptor* absl_nonnull const>
        enums) {
  return Generator(roots, enums).Result();
}

}  // namespace operations_research::util::java
