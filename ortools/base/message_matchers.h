// Copyright 2010-2025 Google LLC
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

#include "absl/strings/string_view.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"

namespace testing {
namespace internal {
// Utilities.
// How to compare two fields (equal vs. equivalent).
typedef ::google::protobuf::util::MessageDifferencer::MessageFieldComparison
    ProtoFieldComparison;

// How to compare two floating-points (exact vs. approximate).
typedef ::google::protobuf::util::DefaultFieldComparator::FloatComparison
    ProtoFloatComparison;

// How to compare repeated fields (whether the order of elements matters).
typedef ::google::protobuf::util::MessageDifferencer::RepeatedFieldComparison
    RepeatedFieldComparison;

// Whether to compare all fields (full) or only fields present in the
// expected protobuf (partial).
typedef ::google::protobuf::util::MessageDifferencer::Scope
    ProtoComparisonScope;

const ProtoFieldComparison kProtoEqual =
    ::google::protobuf::util::MessageDifferencer::EQUAL;
const ProtoFieldComparison kProtoEquiv =
    ::google::protobuf::util::MessageDifferencer::EQUIVALENT;
const ProtoFloatComparison kProtoExact =
    ::google::protobuf::util::DefaultFieldComparator::EXACT;
const ProtoFloatComparison kProtoApproximate =
    ::google::protobuf::util::DefaultFieldComparator::APPROXIMATE;
const RepeatedFieldComparison kProtoCompareRepeatedFieldsRespectOrdering =
    ::google::protobuf::util::MessageDifferencer::AS_LIST;
const RepeatedFieldComparison kProtoCompareRepeatedFieldsIgnoringOrdering =
    ::google::protobuf::util::MessageDifferencer::AS_SET;
const ProtoComparisonScope kProtoFull =
    ::google::protobuf::util::MessageDifferencer::FULL;
const ProtoComparisonScope kProtoPartial =
    ::google::protobuf::util::MessageDifferencer::PARTIAL;

// Options for comparing two protobufs.
struct ProtoComparison {
  ProtoComparison()
      : field_comp(kProtoEqual),
        float_comp(kProtoExact),
        treating_nan_as_equal(false),
        has_custom_margin(false),
        has_custom_fraction(false),
        repeated_field_comp(kProtoCompareRepeatedFieldsRespectOrdering),
        scope(kProtoFull),
        float_margin(0.0),
        float_fraction(0.0),
        ignore_debug_string_format(false),
        fail_on_no_presence_default_values(false),
        verified_presence_in_string(false) {}

  ProtoFieldComparison field_comp;
  ProtoFloatComparison float_comp;
  bool treating_nan_as_equal;
  bool has_custom_margin;    // only used when float_comp = APPROXIMATE
  bool has_custom_fraction;  // only used when float_comp = APPROXIMATE
  RepeatedFieldComparison repeated_field_comp;
  ProtoComparisonScope scope;
  double float_margin;    // only used when has_custom_margin is set.
  double float_fraction;  // only used when has_custom_fraction is set.
  std::vector<std::string> ignore_fields;
  std::vector<std::string> ignore_field_paths;
  std::vector<std::string> unordered_fields;
  bool ignore_debug_string_format;
  bool fail_on_no_presence_default_values;
  bool verified_presence_in_string;
};

// Whether the protobuf must be initialized.
const bool kMustBeInitialized = true;
const bool kMayBeUninitialized = false;

class ProtoMatcher {
 public:
  using is_gtest_matcher = void;
  using MessageType = ::google::protobuf::Message;

  explicit ProtoMatcher(const MessageType& message)
      : message_(CloneMessage(message)) {}

  ProtoMatcher(const MessageType& message, bool, ProtoComparison&)
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

using PolymorphicProtoMatcher = PolymorphicMatcher<ProtoMatcher>;
}  // namespace internal

inline internal::ProtoMatcher EqualsProto(
    const ::google::protobuf::Message& message) {
  return internal::ProtoMatcher(message);
}

// for Pointwise
MATCHER(EqualsProto, "") {
  const auto& a = ::testing::get<0>(arg);
  const auto& b = ::testing::get<1>(arg);
  return ::testing::ExplainMatchResult(EqualsProto(b), a, result_listener);
}

// Constructs a matcher that matches the argument if
// argument.Equivalent(x) or argument->Equivalent(x) returns true.
inline internal::PolymorphicProtoMatcher EquivToProto(
    const ::google::protobuf::Message& x) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return MakePolymorphicMatcher(
      internal::ProtoMatcher(x, internal::kMayBeUninitialized, comp));
}

}  // namespace testing

#endif  // OR_TOOLS_BASE_MESSAGE_MATCHERS_H_
