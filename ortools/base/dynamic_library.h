// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_DYNAMIC_LIBRARY_H_
#define OR_TOOLS_BASE_DYNAMIC_LIBRARY_H_

#include <functional>
#include <stdexcept>
#include <string>

#include "ortools/base/logging.h"

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN  // disables several conflicting macros
#include <windows.h>
#elif defined(__GNUC__)
#include <dlfcn.h>
#endif

#define NAMEOF(x) #x

class DynamicLibrary {
 public:
  DynamicLibrary() : library_handle_(nullptr) {}

  ~DynamicLibrary() {
    if (library_handle_ == nullptr) {
      return;
    }

#if defined(_MSC_VER)
    FreeLibrary(static_cast<HINSTANCE>(library_handle_));
#elif defined(__GNUC__)
    dlclose(library_handle_);
#endif
  }

  bool TryToLoad(const std::string& library_name) {
    library_name_ = std::string(library_name);
#if defined(_MSC_VER)
    library_handle_ = static_cast<void*>(LoadLibrary(library_name.c_str()));
#elif defined(__GNUC__)
    library_handle_ = dlopen(library_name.c_str(), RTLD_NOW);
#endif
    return library_handle_ != nullptr;
  }

  bool LibraryIsLoaded() const { return library_handle_ != nullptr; }

  template <typename T>
  std::function<T> GetFunction(const char* function_name) {
    const void* function_address =
#if defined(_MSC_VER)
        static_cast<void*>(GetProcAddress(
            static_cast<HINSTANCE>(library_handle_), function_name));
#else
        dlsym(library_handle_, function_name);
#endif

    CHECK(function_address != nullptr)
        << "Error: could not find function " << std::string(function_name)
        << " in " << library_name_;

    return TypeParser<T>::CreateFunction(function_address);
  }

  template <typename T>
  std::function<T> GetFunction(const std::string& function_name) {
    return GetFunction<T>(function_name.c_str());
  }

  template <typename T>
  void GetFunction(std::function<T>* function, const char* function_name) {
    *function = GetFunction<T>(function_name);
  }

  template <typename T>
  void GetFunction(std::function<T>* function,
                   const std::string function_name) {
    GetFunction<T>(function, function_name.c_str());
  }

 private:
  void* library_handle_ = nullptr;
  std::string library_name_;

  template <typename T>
  struct TypeParser {};

  template <typename Ret, typename... Args>
  struct TypeParser<Ret(Args...)> {
    static std::function<Ret(Args...)> CreateFunction(
        const void* function_address) {
      return std::function<Ret(Args...)>(reinterpret_cast<Ret (*)(Args...)>(
          const_cast<void*>(function_address)));
    }
  };
};

#endif  // OR_TOOLS_BASE_DYNAMIC_LIBRARY_H_
