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
#include <initializer_list>
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

    SubstituteAndAppend(R"(%{ 
#include "ortools/util/java/jni_helper.h"
%} 
%typemap(javafinalize) SWIGTYPE ""
%include "enums.swg"
)");

    for (const auto* msg : all_messages_) {
      const std::string cpp_name = GetQualifiedCppName(*msg);
      const std::string java_name = GetJavaName(*msg);

      SubstituteAndAppend(
          R"(%typemap(javacode) ${cpp_name} %{
  public static class Builder extends ${java_name} {
    public Builder(long cPtr, boolean cMemoryOwn) {
      super(cPtr, cMemoryOwn);
    }
    public ${java_name} build() {
      return this;
    }
  }
  public Builder toBuilder() {
    return new Builder(getCPtr(this), false);
  }
  @Override
  public boolean equals(Object obj) {
    if (obj instanceof ${java_name}) {
      return getCPtr(this) == getCPtr((${java_name})obj);
    }
    return false;
  }
  @Override
  public int hashCode() {
    return (int)getCPtr(this);
  }
%}
%typemap(jstype) ${cpp_name}* newBuilder "${java_name}.Builder"
%typemap(javaout) ${cpp_name}* newBuilder {
    return new ${java_name}.Builder($jnicall, true);
  }
)",
          {{"${cpp_name}", cpp_name}, {"${java_name}", java_name}});

      for (int i = 0; i < msg->field_count(); ++i) {
        const auto* field = msg->field(i);
        if (field->message_type()) {
          const std::string field_java_name =
              GetJavaName(*field->message_type());
          const std::string field_cpp_name =
              GetQualifiedCppName(*field->message_type());
          const std::string camel_name = ToPascalCase(field->name());

          if (field->is_repeated()) {
            SubstituteAndAppend(
                R"(%typemap(jstype) ${field_cpp_name}* add${camel_name}Builder "${field_java_name}.Builder"
%typemap(javaout) ${field_cpp_name}* add${camel_name}Builder {
    return new ${field_java_name}.Builder($jnicall, false);
  }
%typemap(jstype) ${field_cpp_name}* get${camel_name}Builder "${field_java_name}.Builder"
%typemap(javaout) ${field_cpp_name}* get${camel_name}Builder {
    return new ${field_java_name}.Builder($jnicall, false);
  }
)",
                {{"${field_cpp_name}", field_cpp_name},
                 {"${camel_name}", camel_name},
                 {"${field_java_name}", field_java_name}});
          } else {
            SubstituteAndAppend(
                R"(%typemap(jstype) ${field_cpp_name}* get${camel_name}Builder "${field_java_name}.Builder"
%typemap(javaout) ${field_cpp_name}* get${camel_name}Builder {
    return new ${field_java_name}.Builder($jnicall, false);
  }
%typemap(jstype) ${field_cpp_name}* mutable${camel_name} "${field_java_name}.Builder"
%typemap(javaout) ${field_cpp_name}* mutable${camel_name} {
    return new ${field_java_name}.Builder($jnicall, false);
  }
)",
                {{"${field_cpp_name}", field_cpp_name},
                 {"${camel_name}", camel_name},
                 {"${field_java_name}", field_java_name}});
          }
        }
      }
    }

    for (const auto* e : all_enums_) {
      const std::string cpp_enum_name = GetQualifiedCppName(*e);
      SubstituteAndAppend(
          R"(%typemap(javacode) ${cpp_enum_name} %{
  public final int getNumber() {
    return swigValue();
  }
  public static ${enum_name} forNumber(int value) {
    return swigToEnum(value);
  }
%}
)",
          {{"${cpp_enum_name}", cpp_enum_name}, {"${enum_name}", e->name()}});
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
        SubstituteAndAppend("namespace ${part} {\n", {{"${part}", part}});
      for (const auto* msg : all_messages_) {
        if (msg->file()->package() == pkg &&
            msg->containing_type() == nullptr) {
          SubstituteAndAppend("class ${name};\n", {{"${name}", msg->name()}});
        }
      }
      for (int i = 0; i < parts.size(); ++i) SubstituteAndAppend("}\n");
    }

    for (const auto* msg : repeated_ptr_types_) {
      const std::string clean_name = GetEscapedName(*msg);
      const std::string java_name = GetJavaName(*msg);

      SubstituteAndAppend(
          R"(%rename(getInternal) RepeatedPtrField_${clean_name}::get;
%rename(setInternal) RepeatedPtrField_${clean_name}::set;
%rename(removeInternal) RepeatedPtrField_${clean_name}::remove;
%rename(addInternal) RepeatedPtrField_${clean_name}::add(int, const ${cpp_name}&);
%typemap(javabase) RepeatedPtrField_${clean_name} "java.util.AbstractList<${java_name}>";
%typemap(javacode) RepeatedPtrField_${clean_name} %{

  @Override
  public ${java_name} get(int index) {
    return getInternal(index);
  }

  @Override
  public boolean add(${java_name} e) {
    append(e);
    return true;
  }

  @Override
  public void add(int index, ${java_name} element) {
    addInternal(index, element);
  }

  @Override
  public ${java_name} set(int index, ${java_name} element) {
    ${java_name} old = get(index);
    setInternal(index, element);
    return old;
  }

  @Override
  public ${java_name} remove(int index) {
    ${java_name} old = get(index);
    removeInternal(index);
    return old;
  }
%})",
          {{"${clean_name}", clean_name},
           {"${cpp_name}", GetQualifiedCppName(*msg)},
           {"${java_name}", java_name}});
    }

    SubstituteAndAppend(R"(%rename(getInternal) RepeatedPtrField_string::get;
%rename(setInternal) RepeatedPtrField_string::set;
%rename(removeInternal) RepeatedPtrField_string::remove;
%rename(addInternal) RepeatedPtrField_string::add(int, const std::string&);
%typemap(javabase) RepeatedPtrField_string "java.util.AbstractList<String>";
%typemap(javacode) RepeatedPtrField_string %{

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

      SubstituteAndAppend(
          R"(%rename(getInternal) RepeatedField_${clean_name}::get;
%rename(setInternal) RepeatedField_${clean_name}::set;
%rename(removeInternal) RepeatedField_${clean_name}::remove;
%rename(addInternal) RepeatedField_${clean_name}::add(int, ${scalar});
%javamethodmodifiers RepeatedField_${clean_name}::append(${scalar} value) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
%typemap(javabase) RepeatedField_${clean_name} "java.util.AbstractList<${boxed_type}>";
%typemap(javacode) RepeatedField_${clean_name} %{

  @Override
  public ${boxed_type} get(int index) {
    return getInternal(index);
  }

  @Override
  public boolean add(${boxed_type} e) {
    append(e);
    return true;
  }

  @Override
  public void add(int index, ${boxed_type} element) {
    addInternal(index, element);
  }

  @Override
  public ${boxed_type} set(int index, ${boxed_type} element) {
    ${boxed_type} old = get(index);
    Object unused = setInternal(index, element);
    return old;
  }

  @Override
  public ${boxed_type} remove(int index) {
    ${boxed_type} old = get(index);
    removeInternal(index);
    return old;
  }
%})",
          {{"${clean_name}", clean_name},
           {"${scalar}", scalar},
           {"${boxed_type}", boxed_type}});
    }

    // 2. Non-template classes for repeated fields (for SWIG).
    SubstituteAndAppend(
        R"(%javamethodmodifiers RepeatedPtrField_string::append(const std::string& value) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
%inline %{
)");
    for (const auto* msg : repeated_ptr_types_) {
      std::string full_name = GetQualifiedCppName(*msg);
      std::string clean_name = GetEscapedName(*msg);
      SubstituteAndAppend(
          R"(
class RepeatedPtrField_${clean_name} {
 public:
  int size() const { return reinterpret_cast<const google::protobuf::RepeatedPtrField<${full_name}>*>(this)->size(); }
  void clear() { reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this)->Clear(); }
  ${full_name}* add() { return reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this)->Add(); }
  void append(const ${full_name}& value) { *reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this)->Add() = value; }
  void add(int index, const ${full_name}& value) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this);
    *field->Add() = value;
    for (int i = field->size() - 1; i > index; --i) {
      field->SwapElements(i, i - 1);
    }
  }
  ${full_name}* get(int index) { return reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this)->Mutable(index); }
  void set(int index, const ${full_name}& value) { *reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this)->Mutable(index) = value; }
  void remove(int index) { reinterpret_cast<google::protobuf::RepeatedPtrField<${full_name}>*>(this)->DeleteSubrange(index, 1); }
};
)",
          {{"${clean_name}", clean_name}, {"${full_name}", full_name}});
    }
    for (const auto& scalar : repeated_scalar_types_) {
      std::string clean_name = GetJavaUnboxedType(scalar);
      SubstituteAndAppend(
          R"(
class RepeatedField_${clean_name} {
 public:
  int size() const { return reinterpret_cast<const google::protobuf::RepeatedField<${scalar}>*>(this)->size(); }
  void clear() { reinterpret_cast<google::protobuf::RepeatedField<${scalar}>*>(this)->Clear(); }
  RepeatedField_${clean_name}* append(${scalar} value) { reinterpret_cast<google::protobuf::RepeatedField<${scalar}>*>(this)->Add(value); return this; }
  void add(int index, ${scalar} value) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedField<${scalar}>*>(this);
    field->Add(value);
    for (int i = field->size() - 1; i > index; --i) {
      field->SwapElements(i, i - 1);
    }
  }
  ${scalar} get(int index) const { return reinterpret_cast<const google::protobuf::RepeatedField<${scalar}>*>(this)->Get(index); }
  RepeatedField_${clean_name}* set(int index, ${scalar} value) { reinterpret_cast<google::protobuf::RepeatedField<${scalar}>*>(this)->Set(index, value); return this; }
  void remove(int index) {
    auto* field = reinterpret_cast<google::protobuf::RepeatedField<${scalar}>*>(this);
    field->erase(field->begin() + index);
  }
};
)",
          {{"${clean_name}", clean_name}, {"${scalar}", scalar}});
    }
    SubstituteAndAppend(R"(
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
%}
)");

    for (const auto& pkg : packages) {
      std::vector<std::string> parts = absl::StrSplit(pkg, '.');
      for (const auto& part : parts)
        SubstituteAndAppend("namespace ${part} {\n", {{"${part}", part}});
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
      for (int i = 0; i < parts.size(); ++i) SubstituteAndAppend("}\n");
    }

    // Now generate %extend for each message
    for (const auto* msg : all_messages_) {
      GenerateMessageExtend(*msg, GetJavaName(*msg));
    }
  }

  std::string Result() && { return std::move(out_); }

 private:
  void SubstituteAndAppend(
      absl::string_view format,
      std::initializer_list<std::pair<absl::string_view, absl::string_view>>
          replacements = {}) {
    absl::StrAppend(&out_, absl::StrReplaceAll(format, replacements));
  }

  void GenerateMinimalDecl(const google::protobuf::Descriptor& msg) {
    SubstituteAndAppend("class ${name} {\n public:\n",
                        {{"${name}", msg.name()}});
    for (int i = 0; i < msg.nested_type_count(); ++i) {
      GenerateMinimalDecl(*msg.nested_type(i));
    }
    for (int i = 0; i < msg.enum_type_count(); ++i) {
      GenerateEnumDecl(*msg.enum_type(i));
    }
    SubstituteAndAppend("};\n");
  }

  void GenerateEnumDecl(const google::protobuf::EnumDescriptor& pb_enum) {
    SubstituteAndAppend("enum ${name} {\n", {{"${name}", pb_enum.name()}});
    for (int i = 0; i < pb_enum.value_count(); ++i) {
      const google::protobuf::EnumValueDescriptor& value = *pb_enum.value(i);
      SubstituteAndAppend("  ${name} = ${number},\n",
                          {{"${name}", value.name()},
                           {"${number}", absl::StrCat(value.number())}});
    }
    SubstituteAndAppend("};\n");
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

    // Default constructor for mirror classes. MUST be unqualified name in SWIG.
    // Plus parseFrom and toByteArray for bridge.
    SubstituteAndAppend(
        R"(%extend ${cpp_name} {
  ${unqualified_name}() { return new ${cpp_name}(); }
  static ${cpp_name}* create() { return new ${cpp_name}(); }

  static ${cpp_name}* newBuilder() { return new ${cpp_name}(); }
  ${cpp_name}* getBuilder() { return $self; }
  ${cpp_name}* build() { return $self; }
  void mergeFrom(jbyteArray data) {
    JNIEnv* env = operations_research::util::java::GetThreadLocalJniEnv();
    int len = env->GetArrayLength(data);
    jbyte* buffer = env->GetByteArrayElements(data, nullptr);
    if (!($self->ParsePartialFromArray(buffer, len))) {
      env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "parse failure");
      return;
    }
    env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
  }
  void mergeFrom(const ${cpp_name}& other) {
    $self->MergeFrom(other);
  }
  void copyFrom(const ${cpp_name}& other) {
    $self->CopyFrom(other);
  }
  static ${cpp_name}* parseFrom(jbyteArray data) {
    JNIEnv* env = operations_research::util::java::GetThreadLocalJniEnv();
    int len = env->GetArrayLength(data);
    jbyte* buffer = env->GetByteArrayElements(data, nullptr);
    ${cpp_name}* proto = new ${cpp_name};
    bool success = proto->ParseFromArray(buffer, len);
    env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
    if (!success) {
      delete proto;
      return nullptr;
    }
    return proto;
  }

  bool parseTextFormat(const std::string& text) {
    return google::protobuf::TextFormat::ParseFromString(text, $self);
  }

  jbyteArray toByteArray() const {
    int len = $self->ByteSizeLong();
    JNIEnv* env = operations_research::util::java::GetThreadLocalJniEnv();
    jbyteArray data = env->NewByteArray(len);
    jbyte* buffer = env->GetByteArrayElements(data, nullptr);
    (void)$self->SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer));
    env->ReleaseByteArrayElements(data, buffer, 0);
    return data;
  }

  std::string toString() const {
    return operations_research::ProtobufDebugString(*$self);
  }

  void clear() {
    $self->Clear();
  }
)",
        {{"${cpp_name}", cpp_name}, {"${unqualified_name}", unqualified_name}});

    for (int i = 0; i < msg.field_count(); ++i) {
      const google::protobuf::FieldDescriptor& field =
          *ABSL_DIE_IF_NULL(msg.field(i));
      const std::string camel_name = ToPascalCase(field.name());
      if (field.is_repeated()) {
        const google::protobuf::Descriptor* msg_type = field.message_type();
        if (msg_type != nullptr) {
          const std::string escaped_name = GetEscapedName(*msg_type);
          SubstituteAndAppend(
              R"(
  RepeatedPtrField_${escaped_name}* get${camel_name}List() {
    return (RepeatedPtrField_${escaped_name}*)$self->mutable_${field_name}();
  }
  RepeatedPtrField_${escaped_name}* mutable${camel_name}() {
    return (RepeatedPtrField_${escaped_name}*)$self->mutable_${field_name}();
  }
  int get${camel_name}Count() const {
    return $self->${field_name}_size();
  }
  const ${field_cpp_name}& get${camel_name}(int index) const {
    return $self->${field_name}(index);
  }
  ${field_cpp_name}* add${camel_name}() {
    return $self->add_${field_name}();
  }
  ${field_cpp_name}* add${camel_name}Builder() {
    return $self->add_${field_name}();
  }
  ${field_cpp_name}* get${camel_name}Builder(int index) {
    return $self->mutable_${field_name}(index);
  }
  void set${camel_name}(int index, const ${field_cpp_name}& value) {
    *$self->mutable_${field_name}(index) = value;
  }
  %typemap(jstype) ${unqualified_name}* add${camel_name} "${java_name}.Builder"
  %typemap(javaout) ${unqualified_name}* add${camel_name} {
    long cPtr = $jnicall;
    return this instanceof ${java_name}.Builder ? (${java_name}.Builder)this : new ${java_name}.Builder(cPtr, false);
  }
  %javamethodmodifiers add${camel_name}(const ${field_cpp_name}& value) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
  ${unqualified_name}* add${camel_name}(const ${field_cpp_name}& value) {
    *$self->add_${field_name}() = value;
    return $self;
  }
)",
              {{"${escaped_name}", escaped_name},
               {"${camel_name}", camel_name},
               {"${field_name}", field.name()},
               {"${field_cpp_name}", GetQualifiedCppName(*msg_type)},
               {"${unqualified_name}", unqualified_name},
               {"${java_name}", java_name}});
        } else {
          const std::string cpp_type = GetCppType(field.cpp_type(), field);
          if (field.cpp_type() ==
              google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
            SubstituteAndAppend(
                R"(
  RepeatedPtrField_string* get${camel_name}List() {
    return (RepeatedPtrField_string*)$self->mutable_${field_name}();
  }
  RepeatedPtrField_string* mutable${camel_name}() {
    return (RepeatedPtrField_string*)$self->mutable_${field_name}();
  }
  int get${camel_name}Count() const {
    return $self->${field_name}_size();
  }
  const std::string& get${camel_name}(int index) const {
    return $self->${field_name}(index);
  }
  void set${camel_name}(int index, const std::string& value) {
    *$self->mutable_${field_name}(index) = value;
  }
  %typemap(jstype) ${unqualified_name}* add${camel_name} "${java_name}.Builder"
  %typemap(javaout) ${unqualified_name}* add${camel_name} {
    long cPtr = $jnicall;
    return this instanceof ${java_name}.Builder ? (${java_name}.Builder)this : new ${java_name}.Builder(cPtr, false);
  }
  %javamethodmodifiers add${camel_name}(const std::string& value) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
  ${unqualified_name}* add${camel_name}(const std::string& value) {
    $self->add_${field_name}(value);
    return $self;
  }
)",
                {{"${camel_name}", camel_name},
                 {"${field_name}", field.name()},
                 {"${unqualified_name}", unqualified_name},
                 {"${java_name}", java_name}});
          } else {
            std::string clean_scalar_type = GetJavaUnboxedType(cpp_type);
            SubstituteAndAppend(
                R"(
  RepeatedField_${clean_scalar_type}* get${camel_name}List() {
    return (RepeatedField_${clean_scalar_type}*)$self->mutable_${field_name}();
  }
  RepeatedField_${clean_scalar_type}* mutable${camel_name}() {
    return (RepeatedField_${clean_scalar_type}*)$self->mutable_${field_name}();
  }
  int get${camel_name}Count() const {
    return $self->${field_name}_size();
  }
  ${cpp_type} get${camel_name}(int index) const {
    return $self->${field_name}(index);
  }
  void set${camel_name}(int index, ${cpp_type} value) {
    $self->set_${field_name}(index, value);
  }
  %typemap(jstype) ${unqualified_name}* add${camel_name} "${java_name}.Builder"
  %typemap(javaout) ${unqualified_name}* add${camel_name} {
    long cPtr = $jnicall;
    return this instanceof ${java_name}.Builder ? (${java_name}.Builder)this : new ${java_name}.Builder(cPtr, false);
  }
  %javamethodmodifiers add${camel_name}(${cpp_type} value) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
  ${unqualified_name}* add${camel_name}(${cpp_type} value) {
    $self->add_${field_name}(value);
    return $self;
  }
)",
                {{"${clean_scalar_type}", clean_scalar_type},
                 {"${camel_name}", camel_name},
                 {"${field_name}", field.name()},
                 {"${cpp_type}", cpp_type},
                 {"${unqualified_name}", unqualified_name},
                 {"${java_name}", java_name}});
          }
        }
      } else {
        const std::string cpp_type = GetCppType(field.cpp_type(), field);
        if (field.message_type() != nullptr) {
          SubstituteAndAppend(
              R"(
  ${cpp_type}* get${camel_name}() {
    return $self->mutable_${field_name}();
  }
  ${cpp_type}* get${camel_name}Builder() {
    return $self->mutable_${field_name}();
  }
  %javamethodmodifiers mutable${camel_name}() "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
  ${cpp_type}* mutable${camel_name}() {
    return $self->mutable_${field_name}();
  }
  %typemap(jstype) ${unqualified_name}* set${camel_name} "${java_name}.Builder"
  %typemap(javaout) ${unqualified_name}* set${camel_name} {
    long cPtr = $jnicall;
    return this instanceof ${java_name}.Builder ? (${java_name}.Builder)this : new ${java_name}.Builder(cPtr, false);
  }
  %javamethodmodifiers set${camel_name}(const ${cpp_type}& value) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
  ${unqualified_name}* set${camel_name}(const ${cpp_type}& value) {
    *$self->mutable_${field_name}() = value;
    return $self;
  }
  bool has${camel_name}() const {
    return $self->has_${field_name}();
  }
)",
              {{"${cpp_type}", cpp_type},
               {"${camel_name}", camel_name},
               {"${field_name}", field.name()},
               {"${unqualified_name}", unqualified_name},
               {"${java_name}", java_name}});
        } else {
          SubstituteAndAppend(
              R"(
  %typemap(jstype) ${unqualified_name}* set${camel_name} "${java_name}.Builder"
  %typemap(javaout) ${unqualified_name}* set${camel_name} {
    long cPtr = $jnicall;
    return this instanceof ${java_name}.Builder ? (${java_name}.Builder)this : new ${java_name}.Builder(cPtr, false);
  }
  %javamethodmodifiers set${camel_name}(${cpp_type} val) "@com.google.errorprone.annotations.CanIgnoreReturnValue\n  public";
  ${unqualified_name}* set${camel_name}(${cpp_type} val) {
    $self->set_${field_name}(val);
    return $self;
  }
  ${cpp_type} get${camel_name}() const {
    return $self->${field_name}();
  }
)",
              {{"${cpp_type}", cpp_type},
               {"${camel_name}", camel_name},
               {"${field_name}", field.name()},
               {"${unqualified_name}", unqualified_name},
               {"${java_name}", java_name}});
        }
        if (field.has_presence() && field.message_type() == nullptr) {
          SubstituteAndAppend(
              R"(
  bool has${camel_name}() const {
    return $self->has_${field_name}();
  }
)",
              {{"${camel_name}", camel_name}, {"${field_name}", field.name()}});
        }
      }
      SubstituteAndAppend(
          R"(
  void clear${camel_name}() {
    $self->clear_${field_name}();
  }
)",
          {{"${camel_name}", camel_name}, {"${field_name}", field.name()}});
    }

    for (int i = 0; i < msg.oneof_decl_count(); ++i) {
      const google::protobuf::OneofDescriptor& oneof =
          *ABSL_DIE_IF_NULL(msg.oneof_decl(i));
      SubstituteAndAppend(
          R"(
  void clear${camel_name}() {
    $self->clear_${oneof_name}();
  }
)",
          {{"${camel_name}", ToPascalCase(oneof.name())},
           {"${oneof_name}", oneof.name()}});
    }

    SubstituteAndAppend("}\n");
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
