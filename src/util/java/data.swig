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
// TODO(user): make this SWIG file comply with the SWIG style guide.
%include base/base.swig

%{
#include <vector>
#include "base/integral_types.h"
using std::string;
%}

%{
#include "base/jniutil.h"
%}

// Typemaps to represent const std::vector<CType>& arguments as arrays of JavaType.
%define VECTOR_AS_JAVA_ARRAY(CType, JavaType, JavaTypeName)
%typemap(jni) const std::vector<CType>& "j" #JavaType "Array"
%typemap(jtype) const std::vector<CType>& #JavaType "[]"
%typemap(jstype) const std::vector<CType>& #JavaType "[]"
%typemap(javain) const std::vector<CType>& "$javainput"
%typemap(in) const std::vector<CType>& %{
  if($input) {
    $1 = new std::vector<CType>;
    const int size = jenv->GetArrayLength($input);
    $1->reserve(size);
    j ## JavaType *values = jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, NULL);
    for (int i = 0; i < size; ++i) {
      JavaType value = values[i];
      $1->emplace_back(value);
    }
    jenv->Release ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, values, JNI_ABORT);
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const std::vector<CType>& {
  delete $1;
}
%typemap(out) const std::vector<CType>& %{
  $result = jenv->New ## JavaTypeName ## Array($1->size());
  jenv->Set ## JavaTypeName ## ArrayRegion(
      $result, 0, $1->size(), reinterpret_cast<j ## CType*>(&(*$1)[0]));
%}
%typemap(javaout) const std::vector<CType> & {
  return $jnicall;
}
%enddef
VECTOR_AS_JAVA_ARRAY(int, int, Int);
VECTOR_AS_JAVA_ARRAY(int64, long, Long);

// Convert long[][] to std::vector<std::vector<int64> >
%typemap(jni) const std::vector<std::vector<int64> >& "jobjectArray"
%typemap(jtype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(jstype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(javain) const std::vector<std::vector<int64> >& "$javainput"
%typemap(in) const std::vector<std::vector<int64> >& {
  const int size = jenv->GetArrayLength($input);
  $1 = new std::vector<std::vector<int64> >(size);
  for (int index1 = 0; index1 < size; ++index1) {
    jlongArray inner_array =
        (jlongArray)jenv->GetObjectArrayElement($input, index1);
    const int inner_size = jenv->GetArrayLength(inner_array);
    (*$1)[index1].resize(inner_size);
    jlong* values = jenv->GetLongArrayElements(inner_array, nullptr);
    for (int index2 = 0; index2 < inner_size; ++index2) {
      (*$1)[index1][index2] = values[index2];
    }
    jenv->ReleaseLongArrayElements((jlongArray)inner_array, values, JNI_ABORT);
    jenv->DeleteLocalRef(inner_array);
  }
}
%typemap(freearg) std::vector<std::vector<int64> >& {
  delete $1;
}


// SWIG macros to be used in generating Java wrappers for C++ protocol
// message parameters.  Each protocol message is serialized into
// byte[] before passing into (or returning from) C++ code.

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

// Passing each protocol message from Java to C++ by value. Each ProtocolMessage
// is serialized into byte[] when it is passed from Java to C++, the C++ code
// deserializes into C++ native protocol message.
//
// @param CppProtoType the fully qualified C++ protocol message type
// @param JavaProtoType the corresponding fully qualified Java protocol message
//        type
// @param param_name the parameter name
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

%enddef // end PROTO_INPUT

%define PROTO2_RETURN(CppProtoType, JavaProtoType, giveOwnership)
%typemap(jni) CppProtoType* "jbyteArray"
%typemap(jtype) CppProtoType* "byte[]"
%typemap(jstype) CppProtoType* "JavaProtoType"
%typemap(javaout) CppProtoType* {
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
%typemap(out) CppProtoType* {
  std::unique_ptr<char[]> buf(new char[$1->ByteSize()]);
  $1->SerializeWithCachedSizesToArray(reinterpret_cast<uint8*>(buf.get()));
  $result = JNIUtil::MakeJByteArray(jenv, buf.get(), $1->ByteSize());
  if (giveOwnership) {
    // To prevent a memory leak.
    delete $1;
    $1 = NULL;
  }
}
%enddef // end PROTO2_RETURN
