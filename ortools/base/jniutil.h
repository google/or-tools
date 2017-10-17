// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_JNIUTIL_H_
#define OR_TOOLS_BASE_JNIUTIL_H_

#include <jni.h>
#include <string>
#include "ortools/base/logging.h"

class JNIUtil {
 public:
  // Creates a Java jstring from a null-terminated UTF-8 encoded C String.
  // The caller must delete the jstring reference.
  static jstring MakeJString(JNIEnv* env, const char* cstr) {
    if (cstr == NULL) return NULL;
    return env->NewStringUTF(cstr);
  }

  // Creates a null-terminated UTF-8 encoded C std::string from a jstring.
  // The returned std::string should be "delete[]"-ed when no longer needed.
  static char* MakeCString(JNIEnv* env, jstring str) {
    if (str == NULL) return NULL;
    jsize length = env->GetStringUTFLength(str);
    const char* src = env->GetStringUTFChars(str, NULL);
    char* dst = new char[length + 1];
    memcpy(dst, src, length);
    dst[length] = '\0';
    env->ReleaseStringUTFChars(str, src);
    return dst;
  }

  // Creates a new char array from a jbyteArray.
  // The caller must delete[] the returned array.
  static char* MakeCharArray(JNIEnv* env, jbyteArray a, int* size) {
    jsize n = env->GetArrayLength(a);
    *size = n;
    jbyte* jba = new jbyte[n];

    env->GetByteArrayRegion(a, 0, n, jba);
    // We make use of the fact that jbyte's are really just chars.
    // If this changes (different VM, etc.) things will break.
    return reinterpret_cast<char*>(jba);
  }

  // Produces a jbyteArray from a char array.
  static jbyteArray MakeJByteArray(JNIEnv* env, const char* a, int size) {
    // Create empty array object
    jbyteArray output = env->NewByteArray(size);
    // Fill it
    env->SetByteArrayRegion(output, 0, size, reinterpret_cast<const jbyte*>(a));
    return output;
  }
};

#endif  // OR_TOOLS_BASE_JNIUTIL_H_
