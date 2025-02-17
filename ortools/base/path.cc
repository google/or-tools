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

#include "ortools/base/path.h"

#include <cstring>
#include <string>

#include "absl/strings/str_cat.h"

namespace file {

// 40% of the time in JoinPath() is from calls with 2 arguments, so we
// specialize that case.
std::string JoinPath(absl::string_view path1, absl::string_view path2) {
  if (path1.empty()) return std::string(path2);
  if (path2.empty()) return std::string(path1);
  if (path1.back() == '/') {
    if (path2.front() == '/')
      return absl::StrCat(path1, absl::ClippedSubstr(path2, 1));
  } else {
    if (path2.front() != '/') return absl::StrCat(path1, "/", path2);
  }
  return absl::StrCat(path1, path2);
}

namespace internal {

// Given a collection of file paths, append them all together,
// ensuring that the proper path separators are inserted between them.
std::string JoinPathImpl(bool honor_abs,
                         std::initializer_list<absl::string_view> paths) {
  std::string result;

  if (paths.size() != 0) {
    // This size calculation is worst-case: it assumes one extra "/" for every
    // path other than the first.
    size_t total_size = paths.size() - 1;
    for (const absl::string_view path : paths) total_size += path.size();
    result.resize(total_size);

    auto begin = result.begin();
    auto out = begin;
    bool trailing_slash = false;
    for (absl::string_view path : paths) {
      if (path.empty()) continue;
      if (path.front() == '/') {
        if (honor_abs) {
          out = begin;  // wipe out whatever we've built up so far.
        } else if (trailing_slash) {
          path.remove_prefix(1);
        }
      } else {
        if (!trailing_slash && out != begin) *out++ = '/';
      }
      const size_t this_size = path.size();
      memcpy(&*out, path.data(), this_size);
      out += this_size;
      trailing_slash = out[-1] == '/';
    }
    result.erase(out - begin);
  }
  return result;
}

// Return the parts of the basename of path, split on the final ".".
// If there is no "." in the basename or "." is the final character in the
// basename, the second value will be empty.
std::pair<absl::string_view, absl::string_view> SplitBasename(
    absl::string_view path) {
  path = Basename(path);

  size_t pos = path.find_last_of('.');
  if (pos == absl::string_view::npos)
    return std::make_pair(path, absl::ClippedSubstr(path, path.size(), 0));
  return std::make_pair(path.substr(0, pos),
                        absl::ClippedSubstr(path, pos + 1));
}

}  // namespace internal

bool IsAbsolutePath(absl::string_view path) {
  return !path.empty() && path[0] == '/';
}

std::string AddSlash(absl::string_view path) {
  size_t length = path.size();
  if (length && path[length - 1] != '/') {
    return absl::StrCat(path, "/");
  } else {
    return std::string(path);
  }
}

absl::string_view Dirname(absl::string_view path) {
  return SplitPath(path).first;
}

absl::string_view Basename(absl::string_view path) {
  return SplitPath(path).second;
}

std::pair<absl::string_view, absl::string_view> SplitPath(
    absl::string_view path) {
  size_t pos = path.find_last_of('/');

  // Handle the case with no '/' in 'path'.
  if (pos == absl::string_view::npos)
    return std::make_pair(path.substr(0, 0), path);

  // Handle the case with a single leading '/' in 'path'.
  if (pos == 0)
    return std::make_pair(path.substr(0, 1), absl::ClippedSubstr(path, 1));

  return std::make_pair(path.substr(0, pos),
                        absl::ClippedSubstr(path, pos + 1));
}

absl::string_view Stem(absl::string_view path) {
  return internal::SplitBasename(path).first;
}

absl::string_view Extension(absl::string_view path) {
  return internal::SplitBasename(path).second;
}

std::string CleanPath(const absl::string_view unclean_path) {
  std::string path = std::string(unclean_path);
  const char* src = path.c_str();
  std::string::iterator dst = path.begin();

  // Check for absolute path and determine initial backtrack limit.
  const bool is_absolute_path = *src == '/';
  if (is_absolute_path) {
    *dst++ = *src++;
    while (*src == '/') ++src;
  }
  std::string::const_iterator backtrack_limit = dst;

  // Process all parts
  while (*src) {
    bool parsed = false;

    if (src[0] == '.') {
      //  1dot ".<whateverisnext>", check for END or SEP.
      if (src[1] == '/' || !src[1]) {
        if (*++src) {
          ++src;
        }
        parsed = true;
      } else if (src[1] == '.' && (src[2] == '/' || !src[2])) {
        // 2dot END or SEP (".." | "../<whateverisnext>").
        src += 2;
        if (dst != backtrack_limit) {
          // We can backtrack the previous part
          for (--dst; dst != backtrack_limit && dst[-1] != '/'; --dst) {
            // Empty.
          }
        } else if (!is_absolute_path) {
          // Failed to backtrack and we can't skip it either. Rewind and copy.
          src -= 2;
          *dst++ = *src++;
          *dst++ = *src++;
          if (*src) {
            *dst++ = *src;
          }
          // We can never backtrack over a copied "../" part so set new limit.
          backtrack_limit = dst;
        }
        if (*src) {
          ++src;
        }
        parsed = true;
      }
    }

    // If not parsed, copy entire part until the next SEP or EOS.
    if (!parsed) {
      while (*src && *src != '/') {
        *dst++ = *src++;
      }
      if (*src) {
        *dst++ = *src++;
      }
    }

    // Skip consecutive SEP occurrences
    while (*src == '/') {
      ++src;
    }
  }

  // Calculate and check the length of the cleaned path.
  int path_length = dst - path.begin();
  if (path_length != 0) {
    // Remove trailing '/' except if it is root path ("/" ==> path_length := 1)
    if (path_length > 1 && path[path_length - 1] == '/') {
      --path_length;
    }
    path.resize(path_length);
  } else {
    // The cleaned path is empty; assign "." as per the spec.
    path.assign(1, '.');
  }
  return path;
}

std::string CollapseSlashes(absl::string_view path) {
  std::string ret;
  ret.reserve(path.size());
  bool prev_was_slash = false;
  for (char c : path) {
    if (c == '/') {
      if (prev_was_slash) {
        continue;
      }
      prev_was_slash = true;
    } else {
      prev_was_slash = false;
    }
    ret.push_back(c);
  }
  return ret;
}

}  // namespace file
