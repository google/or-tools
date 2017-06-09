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

#ifndef OR_TOOLS_BASE_FILE_H_
#define OR_TOOLS_BASE_FILE_H_

#include <cstdlib>
#include <cstdio>
#include <string>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/string_view.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/io/tokenizer.h"
#include "ortools/base/status.h"

// This file defines some IO interfaces for compatibility with Google
// IO specifications.
class File {
 public:
  // Opens file "name" with flags specified by "flag".
  // Flags are defined by fopen(), that is "r", "r+", "w", "w+". "a", and "a+".
  static File* Open(const char* const name, const char* const flag);

#ifndef SWIG  // no overloading
  inline static File* Open(const operations_research::string_view& name, const char* const mode) {
    return Open(name.data(), mode);
  }
#endif  // SWIG

  // Opens file "name" with flags specified by "flag".
  // If open failed, program will exit.
  static File* OpenOrDie(const char* const name, const char* const flag);

#ifndef SWIG  // no overloading
  inline static File* OpenOrDie(const operations_research::string_view& name, const char* const flag) {
    return OpenOrDie(name.data(), flag);
  }
#endif  // SWIG

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  size_t Read(void* const buff, size_t size);

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  // If read failed, program will exit.
  void ReadOrDie(void* const buff, size_t size);

  // Reads a line from file to a std::string.
  // Each line must be no more than max_length bytes.
  char* ReadLine(char* const output, uint64 max_length);

  // Reads the whole file to a std::string, with a maximum length of 'max_length'.
  // Returns the number of bytes read.
  int64 ReadToString(std::string* const line, uint64 max_length);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  size_t Write(const void* const buff, size_t size);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  // If write failed, program will exit.
  void WriteOrDie(const void* const buff, size_t size);

  // Writes a std::string to file.
  size_t WriteString(const std::string& line);

  // Writes a std::string to file and append a "\n".
  bool WriteLine(const std::string& line);

  // Closes the file.
  bool Close();
  util::Status Close(int flags);

  // Flushes buffer.
  bool Flush();

  // Returns file size.
  size_t Size();

  // Inits internal data structures.
  static void Init();

  // Returns the file name.
  operations_research::string_view filename() const;

  // Deletes a file.
  static bool Delete(const char* const name);
  static bool Delete(const operations_research::string_view& name) {
    return Delete(name.data());
  }

  // Tests if a file exists.
  static bool Exists(const char* const name);

  bool Open() const;

 private:
  File(FILE* const descriptor, const operations_research::string_view& name);

  FILE* f_;
  const operations_research::string_view name_;
};

namespace file {
inline int Defaults() { return 0xBABA; }

// As of 2016-01, these methods can only be used with flags = file::Defaults().
util::Status Open(const operations_research::string_view& filename, const operations_research::string_view& mode,
                  File** f, int flags);
util::Status SetTextProto(const operations_research::string_view& filename, const google::protobuf::Message& proto,
                          int flags);
util::Status SetBinaryProto(const operations_research::string_view& filename,
                            const google::protobuf::Message& proto, int flags);
util::Status SetContents(const operations_research::string_view& filename, const std::string& contents,
                         int flags);
util::Status GetContents(const operations_research::string_view& filename, std::string* output, int flags);
util::Status WriteString(File* file, const std::string& contents, int flags);

bool ReadFileToString(const operations_research::string_view& file_name, std::string* output);
bool WriteStringToFile(const std::string& data, const operations_research::string_view& file_name);
bool ReadFileToProto(const operations_research::string_view& file_name, google::protobuf::Message* proto);
void ReadFileToProtoOrDie(const operations_research::string_view& file_name, google::protobuf::Message* proto);
bool WriteProtoToASCIIFile(const google::protobuf::Message& proto,
                           const operations_research::string_view& file_name);
void WriteProtoToASCIIFileOrDie(const google::protobuf::Message& proto,
                                const operations_research::string_view& file_name);
bool WriteProtoToFile(const google::protobuf::Message& proto, const operations_research::string_view& file_name);
void WriteProtoToFileOrDie(const google::protobuf::Message& proto,
                           const operations_research::string_view& file_name);

util::Status Delete(const operations_research::string_view& path, int flags);

}  // namespace file

#endif  // OR_TOOLS_BASE_FILE_H_
