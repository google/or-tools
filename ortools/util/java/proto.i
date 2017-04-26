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
//   PROTO2_RETURN(MyProto, com.google.proto.protos.test.MyProto, giveOwnership)
//   -> the 'giveOwnership' parameter should be true iff the C++ function
//      returns a new proto which should be deleted by the client.
//
// Passing each protocol message from Java to C++ by value. Each ProtocolMessage
// is serialized into byte[] when it is passed from Java to C++, the C++ code
// deserializes into C++ native protocol message.
//
// @param CppProtoType the fully qualified C++ protocol message type
// @param JavaProtoType the corresponding fully qualified Java protocol message
//        type
// @param param_name the parameter name
//
// TODO(user): move this file to base/swig/java

%include "ortools/base/base.i"

%{
#include "ortools/base/integral_types.h"
%}

%{
#include "ortools/base/jniutil.h"
%}

%define PROTO_INPUT(CppProtoType, JavaProtoType, param_name)
%typemap(jni) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "jbyteArray"
%typemap(jtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "byte[]"
%typemap(jstype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "JavaProtoType"
%typemap(javain) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "$javainput.toByteArray()"
%typemap(in) PROTO_TYPE* INPUT (CppProtoType temp),
             PROTO_TYPE& INPUT (CppProtoType temp) {
  int proto_size = 0;
  std::unique_ptr<char[]> proto_buffer(
    JNIUtil::MakeCharArray(jenv, $input, &proto_size));
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
  std::unique_ptr<char[]> buf(new char[$1.ByteSize()]);
  $1.SerializeWithCachedSizesToArray(reinterpret_cast<uint8*>(buf.get()));
  $result = JNIUtil::MakeJByteArray(jenv, buf.get(), $1.ByteSize());
}
%enddef // PROTO2_RETURN
