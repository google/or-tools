#ifndef OR_TOOLS_BASE_ENCODINGUTILS_H_
#define OR_TOOLS_BASE_ENCODINGUTILS_H_

namespace operations_research {
namespace EncodingUtils {

// Returns the number of characters of a UTF8-encoded std::string.
inline int UTF8StrLen(const std::string& utf8_str) {
  if (utf8_str.empty()) return 0;
  const char* c = utf8_str.c_str();
  int count = 0;
  while (*c != '\0') {
    ++count;
    // See http://en.wikipedia.org/wiki/UTF-8#Description .
    const unsigned char x = *c;
    c += x < 0xC0 ? 1 : x < 0xE0 ? 2 : x < 0xF0 ? 3 : 4;
  }
  return count;
}

}  // namespace EncodingUtils
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_ENCODINGUTILS_H_
