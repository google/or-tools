// Copyright 2011-2014 Google
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
//
//
//  Some helpers for Input/Ouput.

#ifndef OR_TOOLS_TUTORIALS_CPLUSPLUS_IO_HELPERS_H
#define OR_TOOLS_TUTORIALS_CPLUSPLUS_IO_HELPERS_H

namespace operations_research {

//  Generic class to write an object of class T to a file thanks
//  to one of its member with MemberSignature signature.
//  We restrict ourself to this signature as all checking are done with
//  asserts.
//  You must pass a FULLY qualified method name to SetMember()!
template <typename T>
class WriteToFile {
public:
  typedef void (T::*MemberSignature)(std::ostream &) const;
  WriteToFile(const T * t, const std::string & filename) : t_(t), filename_(filename), member_(NULL) {};
  void SetMember(MemberSignature m) {
    member_ = m;
  }
  void Run() {
    std::ofstream write_stream(filename_.c_str());
    CHECK_EQ(write_stream.is_open(), true) << "Unable to open file: " << filename_;
    CHECK_NE(member_, NULL) << "Object method is not set!";
    (t_->*member_)(write_stream);
    write_stream.close();
  }
private:
  const T * t_;
  const std::string & filename_;
  MemberSignature member_;
};

//  Same but with an additional parameter.
template <typename T, typename P1>
class WriteToFileP1 {
public:
  typedef void (T::*MemberSignature)(std::ostream &, const P1 &) const;
  WriteToFileP1(const T * t, const std::string & filename) : t_(t), filename_(filename), member_(NULL) {};
  void SetMember(MemberSignature m) {
    member_ = m;
  }
  void Run(const P1 & p) {
    std::ofstream write_stream(filename_.c_str());
    CHECK_EQ(write_stream.is_open(), true) << "Unable to open file: " << filename_;
    CHECK_NE(member_, NULL) << "Object method is not set!";
    (t_->*member_)(write_stream, p);
    write_stream.close();
  }
private:
  const T * t_;
  const std::string & filename_;
  MemberSignature member_;
};

//  One entry class to fatally log to different std::ostreams if needed.
class FatalInstanceLoadingLog {
public:
  FatalInstanceLoadingLog() {}
  void AddOutputStream(std::ostream * out) {
    streams_.push_back(out);
  }
  void Write(const char * msg, const  std::string & wrong_keyword = "", int line_number = -1) {
    std::stringstream line;
    line << msg;
    if (wrong_keyword != "") {
      line << ": \"" << wrong_keyword << "\"";
    }
    if (line_number != -1) {
      line << " on line " << line_number;
    }
    line << std::endl;
    for (int i = 0; i < streams_.size(); ++i) {
      (*streams_[i]) << line.str();
    }
    LOG(FATAL) << msg << ": \"" << wrong_keyword << "\" on line " << line_number;
  }
private:
  std::vector<std::ostream *> streams_;
};

}  //  namespace operations_research

#endif //  OR_TOOLS_TUTORIALS_CPLUSPLUS_IO_HELPERS_H