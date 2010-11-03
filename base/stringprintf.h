#ifndef BASE_STRINGPRINTF_H
#define BASE_STRINGPRINTF_H

#include "base/util.h"

namespace operations_research {
string StringPrintf(const char* format, ...);
void SStringPrintf(string* dst, const char* format, ...);
void StringAppendF(string* dst, const char* format, ...);
string StrCat(const string& s1, const string& s2);
string StrCat(const string& s1, const char* const s2);
string StrCat(const char* const s1, const char* const s2);
string StrCat(const char* const s1, const string& s2);
string StrCat(int64 a1, const char* const s2);
string StrCat(const char* const s1, int64 a2);
}  // namespace operations_research
#endif  // BASE_STRINGPRINTF_H
