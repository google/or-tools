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

#ifndef OR_TOOLS_BASE_OPTIONAL_H_
#define OR_TOOLS_BASE_OPTIONAL_H_

namespace absl {

template <class T>
class optional {
 public:
  optional() : has_(false) {}
  optional& operator=(const T& t) {
    has_ = true;
    data_ = t;
    return *this;
  }
  const T& data() const { return data_; }
  constexpr const T& operator*() const& { return reference(); }
  T& operator*() & {
    assert(this->has_);
    return reference();
  }
  constexpr explicit operator bool() const noexcept { return has_; }
  const T* operator->() const {
    assert(this->has_);
    return std::addressof(this->data_);
  }
  T* operator->() {
    assert(this->has_);
    return std::addressof(this->data_);
  }

 private:
  // Private accessors for internal storage viewed as reference to T.
  constexpr const T& reference() const { return this->data_; }
  T& reference() { return this->data_; }
  bool has_;
  T data_;
};

}  // namespace absl

#endif  // OR_TOOLS_BASE_OPTIONAL_H_
