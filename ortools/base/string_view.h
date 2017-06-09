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

// A std::string-like object that points to a sized piece of memory.
//
// Functions or methods may use const string_view& parameters to accept either
// a "const char*" or a "std::string" value that will be implicitly converted to
// a string_view.  The implicit conversion means that it is often appropriate
// to include this .h file in other files rather than forward-declaring
// string_view as would be appropriate for most other Google classes.
//
// Systematic usage of string_view is encouraged as it will reduce unnecessary
// conversions from "const char*" to "std::string" and back again.

#ifndef OR_TOOLS_BASE_STRING_VIEW_H_
#define OR_TOOLS_BASE_STRING_VIEW_H_

#include <string.h>

#include <algorithm>
#include <cstddef>
#include <iosfwd>
#include <string>

namespace operations_research {

class string_view {
 private:
  const char* ptr_;
  int length_;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "std::string" wherever a "string_view" is
  // expected.
  string_view() : ptr_(NULL), length_(0) {}
  string_view(const char* str)  // NOLINT
      : ptr_(str), length_((str == NULL) ? 0 : static_cast<int>(strlen(str))) {}
  string_view(const std::string& str)  // NOLINT
      : ptr_(str.data()), length_(static_cast<int>(str.size())) {}
  string_view(const char* offset, int len) : ptr_(offset), length_(len) {}

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated std::string.
  const char* data() const { return ptr_; }
  int size() const { return length_; }
  int length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() {
    ptr_ = NULL;
    length_ = 0;
  }
  void set(const char* data, int len) {
    ptr_ = data;
    length_ = len;
  }
  void set(const char* str) {
    ptr_ = str;
    if (str != NULL)
      length_ = static_cast<int>(strlen(str));
    else
      length_ = 0;
  }
  void set(const void* data, int len) {
    ptr_ = reinterpret_cast<const char*>(data);
    length_ = len;
  }

  char operator[](int i) const { return ptr_[i]; }

  void remove_prefix(int n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(int n) { length_ -= n; }

  int compare(const string_view& x) const;

  std::string as_string() const { return std::string(data(), size()); }
  // We also define ToString() here, since many other std::string-like
  // interfaces name the routine that converts to a C++ std::string
  // "ToString", and it's confusing to have the method that does that
  // for a string_view be called "as_string()".  We also leave the
  // "as_string()" method defined here for existing code.
  std::string ToString() const { return std::string(data(), size()); }

  void CopyToString(std::string* target) const;
  void AppendToString(std::string* target) const;

  // Does "this" start with "x"?
  bool starts_with(const string_view& x) const {
    return ((length_ >= x.length_) && (memcmp(ptr_, x.ptr_, x.length_) == 0));
  }

  // Does "this" end with "x"?
  bool ends_with(const string_view& x) const {
    return ((length_ >= x.length_) &&
            (memcmp(ptr_ + (length_ - x.length_), x.ptr_, x.length_) == 0));
  }

  // Standard STL container boilerplate.
  typedef char value_type;
  typedef const char* pointer;
  typedef const char& reference;
  typedef const char& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  static const size_type npos;
  typedef const char* const_iterator;
  typedef const char* iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const { return const_reverse_iterator(ptr_); }
  // STL says return size_type, but Google says return int.
  int max_size() const { return length_; }
  int capacity() const { return length_; }

  int copy(char* buf, size_type n, size_type pos = 0) const;

  int find(const string_view& s, size_type pos = 0) const;
  int find(char c, size_type pos = 0) const;
  int rfind(const string_view& s, size_type pos = npos) const;
  int rfind(char c, size_type pos = npos) const;

  string_view substr(size_type pos, size_type n = npos) const;
};

bool operator==(const string_view& x, const string_view& y);

inline bool operator!=(const string_view& x, const string_view& y) {
  return !(x == y);
}

bool operator<(const string_view& x, const string_view& y);

inline bool operator>(const string_view& x, const string_view& y) {
  return y < x;
}

inline bool operator<=(const string_view& x, const string_view& y) {
  return !(x > y);
}

inline bool operator>=(const string_view& x, const string_view& y) {
  return !(x < y);
}

// Allow string_view to be logged.
extern std::ostream& operator<<(std::ostream& o, const string_view& piece);

}  // namespace operations_research

#endif  // OR_TOOLS_BASE_STRING_VIEW_H_
