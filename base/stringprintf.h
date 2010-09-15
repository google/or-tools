#ifndef BASE_STRINGPRINTF_H
#define BASE_STRINGPRINTF_H

#include "base/util.h"

namespace operations_research {
string StringPrintf(const char* format, ...);
void SStringPrintf(string* dst, const char* format, ...);
void StringAppendF(string* dst, const char* format, ...);
}  // namespace operations_research
#endif  // BASE_STRINGPRINTF_H
