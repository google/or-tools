// Copyright 2010-2024 Google LLC
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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "ortools/base/status_macros.h"

// This file defines some IO interfaces for compatibility with Google
// IO specifications.
class File {
 public:
#ifndef SWIG  // no overloading
  // Opens file "name" with flags specified by "mode".
  // Flags are defined by fopen(), that is "r", "r+", "w", "w+". "a", and "a+".
  // The caller should free the File after closing it by passing the returned
  // pointer to delete.
  static File* Open(absl::string_view filename, absl::string_view mode);

  // Opens file "name" with flags specified by "mode".
  // If open failed, program will exit.
  // The caller should free the File after closing it by passing the returned
  // pointer to delete.
  static File* OpenOrDie(absl::string_view filename, absl::string_view mode);
#endif  // SWIG

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  size_t Read(void* buff, size_t size);

  // Reads "size" bytes to buff from file, buff should be pre-allocated.
  // If read failed, program will exit.
  void ReadOrDie(void* buff, size_t size);

  // Reads a line from file to a string.
  // Each line must be no more than max_length bytes.
  char* ReadLine(char* output, uint64_t max_length);

  // Reads the whole file to a string, with a maximum length of 'max_length'.
  // Returns the number of bytes read.
  int64_t ReadToString(std::string* line, uint64_t max_length);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  size_t Write(const void* buff, size_t size);

  // Writes "size" bytes of buff to file, buff should be pre-allocated.
  // If write failed, program will exit.
  void WriteOrDie(const void* buff, size_t size);

  // Writes a string to file.
  size_t WriteString(absl::string_view str);

  // Writes a string to file and append a "\n".
  bool WriteLine(absl::string_view line);

  // Closes the file.
  bool Close();
  absl::Status Close(int flags);

  // Flushes buffer.
  bool Flush();

  // Returns file size.
  size_t Size();

  // Inits internal data structures.
  static void Init();

  // Returns the file name.
  absl::string_view filename() const;

  // Deletes a file.
  static bool Delete(absl::string_view filename);

  // Tests if a file exists.
  static bool Exists(absl::string_view filename);

  bool Open() const;

 private:
  File(FILE* descriptor, absl::string_view name);

  FILE* f_;
  std::string name_;
};

namespace file {

using Options = int;

inline Options Defaults() { return 0xBABA; }

// As of 2016-01, these methods can only be used with flags = file::Defaults().

// The caller should free the File after closing it by passing *f to delete.
absl::Status Open(absl::string_view filename, absl::string_view mode, File** f,
                  Options options);
// The caller should free the File after closing it by passing the returned
// pointer to delete.
File* OpenOrDie(absl::string_view filename, absl::string_view mode,
                Options options);
absl::Status GetTextProto(absl::string_view filename,
                          google::protobuf::Message* proto, Options options);
template <typename T>
absl::StatusOr<T> GetTextProto(absl::string_view filename, Options options) {
  T proto;
  RETURN_IF_ERROR(GetTextProto(filename, &proto, options));
  return proto;
}
absl::Status SetTextProto(absl::string_view filename,
                          const google::protobuf::Message& proto,
                          Options options);
absl::Status GetBinaryProto(absl::string_view filename,
                            google::protobuf::Message* proto, Options options);
template <typename T>
absl::StatusOr<T> GetBinaryProto(absl::string_view filename, Options options) {
  T proto;
  RETURN_IF_ERROR(GetBinaryProto(filename, &proto, options));
  return proto;
}
absl::Status SetBinaryProto(absl::string_view filename,
                            const google::protobuf::Message& proto,
                            Options options);
absl::Status SetContents(absl::string_view filename, absl::string_view contents,
                         Options options);
absl::StatusOr<std::string> GetContents(absl::string_view path,
                                        Options options);
absl::Status GetContents(absl::string_view filename, std::string* output,
                         Options options);
absl::Status WriteString(File* file, absl::string_view contents,
                         Options options);

bool ReadFileToString(absl::string_view file_name, std::string* output);
bool WriteStringToFile(absl::string_view data, absl::string_view file_name);
bool ReadFileToProto(absl::string_view file_name,
                     google::protobuf::Message* proto);
void ReadFileToProtoOrDie(absl::string_view file_name,
                          google::protobuf::Message* proto);
bool WriteProtoToASCIIFile(const google::protobuf::Message& proto,
                           absl::string_view file_name);
void WriteProtoToASCIIFileOrDie(const google::protobuf::Message& proto,
                                absl::string_view file_name);
bool WriteProtoToFile(const google::protobuf::Message& proto,
                      absl::string_view file_name);
void WriteProtoToFileOrDie(const google::protobuf::Message& proto,
                           absl::string_view file_name);

absl::Status Delete(absl::string_view path, Options options);
absl::Status Exists(absl::string_view path, Options options);

}  // namespace file

#endif  // OR_TOOLS_BASE_FILE_H_
