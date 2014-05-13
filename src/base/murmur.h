#ifndef OR_TOOLS_BASE_MURMUR_H_
#define OR_TOOLS_BASE_MURMUR_H_

#include "base/fingerprint2011.h"

namespace util_hash {
// In the or-tools project, MurmurHash64 is just a redirection towards
// Fingerprint2011. Ideally, it is meant to be using the murmurhash
// algorithm described in http://murmurhash.googlepages.com.
inline uint64 MurmurHash64(const char *buf, const size_t len) {
  return Fingerprint2011(buf, len);
}
}

#endif  // OR_TOOLS_BASE_MURMUR_H_
