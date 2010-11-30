#ifndef BASE_STRINGPRINTF_H
#define BASE_STRINGPRINTF_H

#include <string>

#include "base/stringpiece.h"
#include "base/util.h"

namespace operations_research {
string StringPrintf(const char* format, ...);
void SStringPrintf(string* dst, const char* format, ...);
void StringAppendF(string* dst, const char* format, ...);
string StrCat(const StringPiece& p1, const StringPiece& p2);
string StrCat(const StringPiece& p1,
              const StringPiece& p2,
              const StringPiece& p3);
string StrCat(const StringPiece& p1,
              const StringPiece& p2,
              const StringPiece& p3,
              const StringPiece& p4);
string StrCat(int64 a1, const StringPiece& p2);
string StrCat(const StringPiece& p1, int64 a2);
}  // namespace operations_research
#endif  // BASE_STRINGPRINTF_H
