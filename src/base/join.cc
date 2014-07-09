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

#include <string>
#include "base/basictypes.h"
#include "base/join.h"
#include "base/stringpiece.h"
#include "base/stringprintf.h"

namespace operations_research {
// ----- StrCat -----

static char* Append1(char* out, const AlphaNum& x) {
  memcpy(out, x.data(), x.size());
  return out + x.size();
}

static char* Append2(char* out, const AlphaNum& x1, const AlphaNum& x2) {
  memcpy(out, x1.data(), x1.size());
  out += x1.size();

  memcpy(out, x2.data(), x2.size());
  return out + x2.size();
}

static char* Append4(char* out, const AlphaNum& x1, const AlphaNum& x2,
                     const AlphaNum& x3, const AlphaNum& x4) {
  memcpy(out, x1.data(), x1.size());
  out += x1.size();

  memcpy(out, x2.data(), x2.size());
  out += x2.size();

  memcpy(out, x3.data(), x3.size());
  out += x3.size();

  memcpy(out, x4.data(), x4.size());
  return out + x4.size();
}

void StrAppend(std::string* s, const AlphaNum& a) { s->append(a.data(), a.size()); }
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b) {
  s->reserve(s->size() + a.size() + b.size());
  s->append(a.data(), a.size());
  s->append(b.data(), b.size());
}
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c) {
  s->reserve(s->size() + a.size() + b.size() + c.size());
  s->append(a.data(), a.size());
  s->append(b.data(), b.size());
  s->append(c.data(), c.size());
}
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d) {
  s->reserve(s->size() + a.size() + b.size() + c.size() + d.size());
  s->append(a.data(), a.size());
  s->append(b.data(), b.size());
  s->append(c.data(), c.size());
  s->append(d.data(), d.size());
}
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e);
}
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e, f);
}
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e, f, g);
}
void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e, f, g, h);
}

std::string StrCat(const AlphaNum& a) { return std::string(a.data(), a.size()); }
std::string StrCat(const AlphaNum& a, const AlphaNum& b) {
  std::string out;
  StrAppend(&out, a, b);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c) {
  std::string out;
  StrAppend(&out, a, b, c);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d) {
  std::string out;
  StrAppend(&out, a, b, c, d);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e) {
  std::string out;
  StrAppend(&out, a, b, c, d, e);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f) {
  std::string out;
  StrAppend(&out, a, b, c, d, e, f);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g) {
  std::string out;
  StrAppend(&out, a, b, c, d, e, f, g);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h) {
  std::string out;
  StrAppend(&out, a, b, c, d, e, f, g, h);
  return out;
}

}  // namespace operations_research
