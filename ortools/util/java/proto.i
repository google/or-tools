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

// SWIG macros to be used in generating Java wrappers for C++ protocol
// message parameters.  Each protocol message is serialized into
// byte[] before passing into (or returning from) C++ code.
//
// If the C++ function expects an input protocol message:
//   foo(const MyProto* message,...)
// Use PROTO_INPUT macro:
//   PROTO_INPUT(MyProto, com.google.proto.protos.test.MyProto, message)
//
// if the C++ function returns a protocol message:
//   MyProto* foo();
// Use PROTO2_RETURN macro:
//   PROTO2_RETURN(MyProto, com.google.proto.protos.test.MyProto)
//
// Passing each protocol message from Java to C++ by value. Each ProtocolMessage
// is serialized into byte[] when it is passed from Java to C++, the C++ code
// deserializes into C++ native protocol message.
//
// @param CppProtoType the fully qualified C++ protocol message type
// @param JavaProtoType the corresponding fully qualified Java protocol message
//        type
// @param param_name the parameter name

%{
#include <cstdint>
%}

%define PROTO_INPUT(CppProtoType, JavaProtoType, param_name)
%typemap(jni) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "jbyteArray"
%typemap(jtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "byte[]"
%typemap(jstype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "JavaProtoType"
%typemap(javain) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "$javainput.toByteArray()"
%typemap(in) PROTO_TYPE* INPUT (CppProtoType temp),
             PROTO_TYPE& INPUT (CppProtoType temp) {
  const int proto_size = jenv->GetArrayLength($input);
  std::unique_ptr<jbyte[]> proto_buffer(new jbyte[proto_size]);
  jenv->GetByteArrayRegion($input, 0, proto_size, proto_buffer.get());
  bool parsed_ok = temp.ParseFromArray(proto_buffer.get(), proto_size);
  if (!parsed_ok) {
    SWIG_JavaThrowException(jenv,
                            SWIG_JavaRuntimeException,
                            "Unable to parse CppProtoType protocol message.");
  }
  $1 = &temp;
}

%apply PROTO_TYPE& INPUT { const CppProtoType& param_name }
%apply PROTO_TYPE& INPUT { CppProtoType& param_name }
%apply PROTO_TYPE* INPUT { const CppProtoType* param_name }
%apply PROTO_TYPE* INPUT { CppProtoType* param_name }

%enddef // PROTO_INPUT

%define PROTO2_RETURN(CppProtoType, JavaProtoType)
%typemap(jni) CppProtoType "jbyteArray"
%typemap(jtype) CppProtoType "byte[]"
%typemap(jstype) CppProtoType "JavaProtoType"
%typemap(javaout) CppProtoType {
  byte[] buf = $jnicall;
  if (buf == null || buf.length == 0) {
    return null;
  }
  try {
    return JavaProtoType.parseFrom(buf);
  } catch (com.google.protobuf.InvalidProtocolBufferException e) {
    throw new RuntimeException(
        "Unable to parse JavaProtoType protocol message.");
  }
}
%typemap(out) CppProtoType {
  const long size = $1.ByteSizeLong();
  std::unique_ptr<jbyte[]> buf(new jbyte[size]);
  $1.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buf.get()));
  $result = jenv->NewByteArray(size);
  jenv->SetByteArrayRegion($result, 0, size, buf.get());
}
%enddef // PROTO2_RETURN

%define PROTO2_CALLBACK_ARGUMENT(CppProtoType, JavaProtoType, JavaProtoTypeWithSlashes, param_name)
%typemap(directorin, descriptor="L"JavaProtoTypeWithSlashes";") const CppProtoType& %{
  const size_t size = $1.ByteSizeLong();
  const std::unique_ptr<jbyte[]> buf(new jbyte[size]);
  $1.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buf.get()));
  $input = jenv->NewByteArray(size);
  jenv->SetByteArrayRegion($input, 0, size, buf.get());
  Swig::LocalRefGuard $1_refguard(jenv, $input);
%}

%typemap(javadirectorin) const CppProtoType& %{
  // This code is injected as a parameter in a function call. Thus it must be an
  // expression.
  new Object() {
    JavaProtoType run() {
      byte[] buf = $jniinput;
      if (buf == null) {
        return null;
      }
      try {
        return JavaProtoType.parseFrom(buf);
      } catch (com.google.protobuf.InvalidProtocolBufferException e) {
        throw new RuntimeException(
            "Unable to parse JavaProtoType protocol message.");
      }
    }
  }.run()
%}

PROTO_INPUT(CppProtoType, JavaProtoType, param_name);
%enddef // PROTO2_CALLBACK_ARGUMENT

%define PROTO2_CALLBACK_RETURN(CppProtoType, JavaProtoType, JavaProtoTypeWithSlashes)
%typemap(directorout) CppProtoType %{
  if ($input != nullptr) {
    const int proto_size = jenv->GetArrayLength($input);
    const std::unique_ptr<jbyte[]> proto_buffer(new jbyte[proto_size]);
    jenv->GetByteArrayRegion($input, 0, proto_size, proto_buffer.get());
    if (!$result.ParseFromArray(proto_buffer.get(), proto_size)) {
      SWIG_JavaThrowException(jenv,
                              SWIG_JavaRuntimeException,
                              "Unable to parse CppProtoType protocol message.");
    }
  }
%}

%typemap(javadirectorout) CppProtoType %{
  $javacall == null ? null : $javacall.toByteArray()
%}

%typemap(directorin, descriptor="L"JavaProtoTypeWithSlashes";") CppProtoType {}

PROTO2_RETURN(CppProtoType, JavaProtoType);
%enddef // PROTO2_CALLBACK_RETURN

// SWIG Macro for mapping protocol message enum type.
// @param CppEnumProto the C++ protocol message enum type
// @param JavaEnumProto the corresponding Java protocol message enum type
%define PROTO_ENUM_RETURN(CppEnumProto, JavaEnumProto)
%typemap(jni) CppEnumProto "jint"
%typemap(jtype) CppEnumProto "int"
%typemap(jstype) CppEnumProto "JavaEnumProto"

// From CppEnumProto to jni (in wrap.cxx code)
%typemap(out) CppEnumProto %{ $result = $1; %}

// From jtype to jstype (in .java code)
%typemap(javaout) CppEnumProto {
  return JavaEnumProto.forNumber($jnicall);
}

%enddef // end PROTO_ENUM_RETURN

%define PROTO2_ENUM_INPUT_COMMON(CppEnumProto, JavaEnumProto, param_name)
%typemap(jni) CppEnumProto INPUT "jint"
%typemap(jtype) CppEnumProto INPUT "int"
%typemap(jstype) CppEnumProto INPUT "JavaEnumProto"
%typemap(javain) CppEnumProto INPUT "$javainput.getNumber()"
%typemap(in) CppEnumProto {
  $1 = (CppEnumProto) $input;
}

%apply CppEnumProto INPUT { CppEnumProto param_name }
%apply CppEnumProto INPUT { const CppEnumProto& param_name }
%apply CppEnumProto INPUT { CppEnumProto& param_name }
%enddef // end PROTO2_ENUM_INPUT_COMMON
