#ifndef OR_TOOLS_BASE_FINGERPRINT2011_H_
#define OR_TOOLS_BASE_FINGERPRINT2011_H_

#include "base/integral_types.h"

uint64 FingerprintCat2011(uint64 fp1, uint64 fp2) {
  // Two big prime numbers.
  const uint64 kMul1 = 0xc6a4a7935bd1e995ULL;
  const uint64 kMul2 = 0x228876a7198b743ULL;
  uint64 a = fp1 * kMul1 + fp2 * kMul2;
  // Note: The following line also makes sure we never return 0 or 1, because we
  // will only add something to 'a' if there are any MSBs (the remaining bits
  // after the shift) being 0, in which case wrapping around would not happen.
  return a + (~a >> 47);
}

// This should be better (collision-wise) than the default hash<std::string>, without
// being much slower. It never returns 0 or 1.
uint64 Fingerprint2011(const char* bytes, size_t len) {
  // Some big prime numer.
  uint64 fp = 0xa5b85c5e198ed849ULL;
  const char* end = bytes + len;
  while (bytes + 8 <= end) {
    fp = FingerprintCat2011(fp, *(reinterpret_cast<const uint64*>(bytes)));
    bytes += 8;
  }
  // Note: we don't care about "consistency" (little or big endian) between
  // the bulk and the suffix of the message.
  uint64 last_bytes = 0;
  while (bytes < end) {
    last_bytes += *bytes;
    last_bytes <<= 8;
    bytes++;
  }
  return FingerprintCat2011(fp, last_bytes);
}

#endif  // OR_TOOLS_BASE_FINGERPRINT2011_H_
