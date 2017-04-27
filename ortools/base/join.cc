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
#include "ortools/base/basictypes.h"
#include "ortools/base/join.h"
#include "ortools/base/stringpiece.h"
#include "ortools/base/stringprintf.h"

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

void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h,
               const AlphaNum& i) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e, f, g, h, i);
}

void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h,
               const AlphaNum& i, const AlphaNum& j) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e, f, g, h, i, j);
}

void StrAppend(std::string* s, const AlphaNum& a, const AlphaNum& b,
               const AlphaNum& c, const AlphaNum& d, const AlphaNum& e,
               const AlphaNum& f, const AlphaNum& g, const AlphaNum& h,
               const AlphaNum& i, const AlphaNum& j, const AlphaNum& k) {
  StrAppend(s, a, b, c, d);
  StrAppend(s, e, f, g, h, i, j, k);
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
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h, const AlphaNum& i) {
  std::string out;
  StrAppend(&out, a, b, c, d, e, f, g, h, i);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h, const AlphaNum& i,
              const AlphaNum& j) {
  std::string out;
  StrAppend(&out, a, b, c, d, e, f, g, h, i, j);
  return out;
}
std::string StrCat(const AlphaNum& a, const AlphaNum& b, const AlphaNum& c,
              const AlphaNum& d, const AlphaNum& e, const AlphaNum& f,
              const AlphaNum& g, const AlphaNum& h, const AlphaNum& i,
              const AlphaNum& j, const AlphaNum& k) {
  std::string out;
  StrAppend(&out, a, b, c, d, e, f, g, h, i, j, k);
  return out;
}

