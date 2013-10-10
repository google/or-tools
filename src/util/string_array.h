// Copyright 2010-2013 Google
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

#ifndef OR_TOOLS_UTIL_STRING_ARRAY_H_
#define OR_TOOLS_UTIL_STRING_ARRAY_H_

#include <string>

#include "base/integral_types.h"
#include "base/stringprintf.h"

using std::string;

namespace operations_research {
// ---------- Pretty Print Helpers ----------

// Creates a string from an array of object pointers supporting the
// DebugString() method, and a separator.
template <class T>
string DebugStringArray(T* const* array, int size, const string& separator) {
  string out;
  for (int i = 0; i < size; ++i) {
    if (i > 0) {
      out.append(separator);
    }
    out.append(array[i]->DebugString());
  }
  return out;
}

// TODO(user): rename DebugStringVector to PtrVectorDebugString and
// DebugStringRefVector to VectorDebugString

// Creates a string from an vector of object pointers supporting the
// DebugString() method, and a separator.
template <class T>
string DebugStringVector(const std::vector<T*>& array, const string& separator) {
  return DebugStringArray(array.data(), array.size(), separator);
}

// Creates a string from an vector of objects supporting the
// DebugString() method, and a separator.
template <class T>
string DebugStringRefVector(const std::vector<T>& array, const string& separator) {
  string out;
  for (int i = 0; i < array.size(); ++i) {
    if (i > 0) {
      out.append(separator);
    }
    out.append(array[i].DebugString());
  }
  return out;
}

// Creates a string from an array of objects supporting the
// name() method, and a separator.
template <class T>
string NameArray(T* const* array, int size, const string& separator) {
  string out;
  for (int i = 0; i < size; ++i) {
    if (i > 0) {
      out.append(separator);
    }
    out.append(array[i]->name());
  }
  return out;
}

// Creates a string from an vector of objects supporting the
// name() method, and a separator.
template <class T>
string NameVector(const std::vector<T*>& array, const string& separator) {
  return NameArray(array.data(), array.size(), separator);
}

// Creates a string from  vector of objects having a string field "name" (i.e.
// "object.name" is a string), and a separator.
template <class T>
string NameFieldVector(const std::vector<T*>& array, const string& separator) {
  string out;
  for (int i = 0; i < array.size(); ++i) {
    if (i > 0) {
      out.append(separator);
    }
    out.append(array[i]->name);
  }
  return out;
}

// Creates a string from an array of int64, and a separator.
inline string Int64ArrayToString(const int64* const array, int size,
                                 const string& separator) {
  string out;
  for (int i = 0; i < size; ++i) {
    if (i > 0) {
      out.append(separator);
    }
    StringAppendF(&out, "%" GG_LL_FORMAT "d", array[i]);
  }
  return out;
}

// Creates a string from an array of int, and a separator.
inline string IntArrayToString(const int* const array, int size,
                               const string& separator) {
  string out;
  for (int i = 0; i < size; ++i) {
    if (i > 0) {
      out.append(separator);
    }
    StringAppendF(&out, "%d", array[i]);
  }
  return out;
}

// Creates a string from an vector of int64, and a separator.
inline string IntVectorToString(const std::vector<int64>& array,
                                const string& separator) {
  return Int64ArrayToString(array.data(), array.size(), separator);
}

// Creates a string from an vector of int, and a separator.
inline string IntVectorToString(const std::vector<int>& array,
                                const string& separator) {
  return IntArrayToString(array.data(), array.size(), separator);
}

}  // namespace operations_research
#endif  // OR_TOOLS_UTIL_STRING_ARRAY_H_
