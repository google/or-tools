// Copyright 2010 Google
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

#include "base/stringpiece.h"
#include "base/util.h"

namespace operations_research {

static void StringAppendV(string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer
  char space[1024];

  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
#if defined(_MSC_VER)
  backup_ap = ap;
#else
  va_copy(backup_ap, ap);
#endif
  int result = vsnprintf(space, sizeof(space), format, backup_ap);
  va_end(backup_ap);

  if ((result >= 0) && (result < sizeof(space))) {
    // It fit
    dst->append(space, result);
    return;
  }

  // Repeatedly increase buffer size until it fits
  int length = sizeof(space);
  while (true) {
    if (result < 0) {
      // Older behavior: just try doubling the buffer size
      length *= 2;
    } else {
      // We need exactly "result+1" characters
      length = result+1;
    }
    char* buf = new char[length];

    // Restore the va_list before we use it again
#if defined(_MSC_VER)
  backup_ap = ap;
#else
    va_copy(backup_ap, ap);
#endif
    result = vsnprintf(buf, length, format, backup_ap);
    va_end(backup_ap);

    if ((result >= 0) && (result < length)) {
      // It fit
      dst->append(buf, result);
      delete[] buf;
      return;
    }
    delete[] buf;
  }
}

string StringPrintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

void SStringPrintf(string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  dst->clear();
  StringAppendV(dst, format, ap);
  va_end(ap);
}

void StringAppendF(string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(dst, format, ap);
  va_end(ap);
}

// ----- StrCat -----

string StrCat(const StringPiece& p1, const StringPiece& p2) {
  string result = p1.ToString();
  result += p2.ToString();
  return result;
}

string StrCat(const StringPiece& p1,
              const StringPiece& p2,
              const StringPiece& p3) {
  string result = p1.ToString();
  result += p2.ToString();
  result += p3.ToString();
  return result;
}

string StrCat(const StringPiece& p1,
              const StringPiece& p2,
              const StringPiece& p3,
              const StringPiece& p4) {
  string result = p1.ToString();
  result += p2.ToString();
  result += p3.ToString();
  result += p4.ToString();
  return result;
}

string StrCat(int64 a1, const StringPiece& p2) {
  return StringPrintf("%lld%s", a1, p2.ToString().c_str());
}

string StrCat(const StringPiece& p1, int64 a2) {
  return StringPrintf("%s%lld", p1.ToString().c_str(), a2);
}

}  // namespace operations_research
