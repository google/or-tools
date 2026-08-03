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

#include "ortools/util/java/jni_helper.h"

#include <jni.h>

namespace operations_research::util::java {

// Returns the JNIEnv* for the current thread.
//
// In a SWIG/JNI context, this uses the standard JNI_GetCreatedJavaVMs
// to find the VM and caches the JNIEnv* in thread-local storage.
//
JNIEnv* GetThreadLocalJniEnv() {
  static thread_local JNIEnv* tls_env = nullptr;

  // Return cached env if available
  if (tls_env) return tls_env;

  JavaVM* vm = nullptr;
  jsize count = 0;

  // Directly call the JNI standard function to get the existing VM.
  // This symbol is available when linked/loaded in a JNI context.
  if (JNI_GetCreatedJavaVMs(&vm, 1, &count) == JNI_OK && count > 0) {
    // GetEnv returns JNI_OK if the current thread is already attached.
    if (vm->GetEnv(reinterpret_cast<void**>(&tls_env), JNI_VERSION_1_6) ==
        JNI_OK) {
      return tls_env;
    }
  }

  return nullptr;  // Thread is not attached or VM not found.
}

}  // namespace operations_research::util::java
