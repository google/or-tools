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

#ifndef OR_TOOLS_BASE_MESSAGE_MATCHERS_H_
#define OR_TOOLS_BASE_MESSAGE_MATCHERS_H_

#include <memory>

#include "gmock/gmock.h"
#include "google/protobuf/message.h"

namespace testing {

class ProtoMatcher {
 public:
  using is_gtest_matcher = void;
  using MessageType = ::google::protobuf::Message;

  explicit ProtoMatcher(const MessageType& message)
      : message_(CloneMessage(message)) {}

  bool MatchAndExplain(const MessageType& m,
                       testing::MatchResultListener*) const {
    return EqualsMessage(*message_, m);
  }
  bool MatchAndExplain(const MessageType* m,
                       testing::MatchResultListener*) const {
    return EqualsMessage(*message_, *m);
  }

  void DescribeTo(std::ostream* os) const {
    *os << "has the same serialization as " << ExpectedMessageDescription();
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "does not have the same serialization as "
        << ExpectedMessageDescription();
  }

 private:
  std::unique_ptr<MessageType> CloneMessage(const MessageType& message) {
    std::unique_ptr<MessageType> clone(message.New());
    clone->CheckTypeAndMergeFrom(message);
    return clone;
  }

  bool EqualsMessage(const ::google::protobuf::Message& m_1,
                     const ::google::protobuf::Message& m_2) const {
    std::string s_1, s_2;
    m_1.SerializeToString(&s_1);
    m_2.SerializeToString(&s_2);
    return s_1 == s_2;
  }

  std::string ExpectedMessageDescription() const {
    return message_->DebugString();
  }

  const std::shared_ptr<MessageType> message_;
};

inline ProtoMatcher EqualsProto(const ::google::protobuf::Message& message) {
  return ProtoMatcher(message);
}

// for Pointwise
MATCHER(EqualsProto, "") {
  const auto& a = ::testing::get<0>(arg);
  const auto& b = ::testing::get<1>(arg);
  return ::testing::ExplainMatchResult(EqualsProto(b), a, result_listener);
}

}  // namespace testing

#endif  // OR_TOOLS_BASE_MESSAGE_MATCHERS_H_
