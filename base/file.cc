// Copyright 2011 Google
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

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/file.h"
#include "base/logging.h"

namespace operations_research {

File::File(FILE* const f_des, const std::string& name)
  : f_(f_des), name_(name) {}

bool File::Delete(char* const name) {
  return remove(name) == 0;
}

bool File::Exists(char* const name) {
  return access(name, F_OK) == 0;
}

size_t File::Size() {
  struct stat f_stat;
  stat(name_.c_str(), &f_stat);
  return f_stat.st_size;
}

bool File::Flush() {
  return fflush(f_) == 0;
}

bool File::Close() {
  if (fclose(f_) == 0) {
    f_ = NULL;
    return true;
  } else {
    return false;
  }
}

void File::ReadOrDie(void* const buf, size_t size) {
  CHECK_EQ(fread(buf, 1, size, f_), size);
}

size_t File::Read(void* const buf, size_t size) {
  return fread(buf, 1, size, f_);
}

void File::WriteOrDie(const void* const buf, size_t size) {
  CHECK_EQ(fwrite(buf, 1, size, f_), size);
}
size_t File::Write(const void* const buf, size_t size) {
  return fwrite(buf, 1, size, f_);
}

File* File::OpenOrDie(const char* const name, const char* const flag) {
  FILE* const f_des = fopen(name, flag);
  if (f_des == NULL) {
    std::cerr << "Cannot open " << name;
    exit(1);
  }
  File* const f = new File(f_des, name);
  return f;
}

File* File::Open(const char* const name, const char* const flag) {
  FILE* const f_des = fopen(name, flag);
  if (f_des == NULL) return NULL;
  File* const f = new File(f_des, name);
  return f;
}

bool File::ReadLine(std::string* const line) {
  char buff[1000000];
  char* const result = fgets(buff, 1000000, f_);
  if (result == NULL) {
    return false;
  } else {
    *line = std::string(buff);
    return true;
  }
}

size_t File::WriteString(const std::string& line) {
  return Write(line.c_str(), line.size());
}

bool File::WriteLine(const std::string& line) {
  if (Write(line.c_str(), line.size()) != line.size()) return false;
  return Write("\n", 1) == 1;
}

std::string File::CreateFileName() const {
  return name_;
}

bool File::Open() const {
  return f_ != NULL;
}

void File::Init() {}
}  // namespace operations_research
