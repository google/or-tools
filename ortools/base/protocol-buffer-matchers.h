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

// emulates g3/testing/base/public/gmock_utils/protocol-buffer-matchers.h
#ifndef ORTOOLS_BASE_PROTOCOL_BUFFER_MATCHERS_H_
#define ORTOOLS_BASE_PROTOCOL_BUFFER_MATCHERS_H_

// gMock matchers used to validate protocol buffer arguments.

// WHAT THIS IS
// ============
//
// This library defines the following matchers in the ::protobuf_matchers
// namespace:
//
//   EqualsProto(pb)              The argument equals pb.
//   EquivToProto(pb)             The argument is equivalent to pb.
//
// where:
//
//   - pb can be either a protobuf value or a human-readable string
//     representation of it.
//   - When pb is a string, the matcher can optionally accept a
//     template argument for the type of the protobuf,
//     e.g. EqualsProto<Foo>("foo: 1").
//   - "equals" is defined as the argument's Equals(pb) method returns true.
//   - "equivalent to" is defined as the argument's Equivalent(pb) method
//     returns true.
//
// These matchers can match either a protobuf value or a pointer to
// it.  They make a copy of pb, and thus can out-live pb.  When the
// match fails, the matchers print a detailed message (the value of
// the actual protobuf, the value of the expected protobuf, and which
// fields are different).
//
// EXAMPLES
// ========
//
//   using ::protobuf_matchers::EqualsProto;
//   using ::protobuf_matchers::EquivToProto;
//
//   // my_pb.Equals(expected_pb).
//   EXPECT_THAT(my_pb, EqualsProto(expected_pb));
//
//   // my_pb is equivalent to a protobuf whose foo field is 1 and
//   // whose bar field is "x".
//   EXPECT_THAT(my_pb, EquivToProto("foo: 1 "
//                                   "bar: 'x'"));

#include <cstddef>
#include <iterator>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"

namespace testing {
using DifferencerConfigFunction =
    std::function<void(::google::protobuf::util::DefaultFieldComparator*,
                       ::google::protobuf::util::MessageDifferencer*)>;
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
        float_fraction(0.0) {}

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
  DifferencerConfigFunction differencer_config_function;
};

// Whether the protobuf must be initialized.
const bool kMustBeInitialized = true;
const bool kMayBeUninitialized = false;

// Parses the TextFormat representation of a protobuf, allowing required fields
// to be missing.  Returns true if successful.
bool ParsePartialFromAscii(const std::string& pb_ascii,
                           ::google::protobuf::Message* proto,
                           std::string* error_text);

// Returns true iff p and q can be compared (i.e. have the same descriptor).
bool ProtoComparable(const ::google::protobuf::Message& p,
                     const ::google::protobuf::Message& q);

// Returns true iff actual and expected are comparable and match.  The
// comp argument specifies how the two are compared.
bool ProtoCompare(const ProtoComparison& comp,
                  const ::google::protobuf::Message& actual,
                  const ::google::protobuf::Message& expected);

// Describes the types of the expected and the actual protocol buffer.
std::string DescribeTypes(const ::google::protobuf::Message& expected,
                          const ::google::protobuf::Message& actual);
// Prints the protocol buffer pointed to by proto.
std::string PrintProtoPointee(const ::google::protobuf::Message* proto);

// Describes the differences between the two protocol buffers.
std::string DescribeDiff(const ProtoComparison& comp,
                         const ::google::protobuf::Message& actual,
                         const ::google::protobuf::Message& expected);

// Common code for implementing EqualsProto and EquivToProto.
class ProtoMatcherBase {
 public:
  ProtoMatcherBase(
      bool must_be_initialized,     // Must the argument be fully initialized?
      const ProtoComparison& comp)  // How to compare the two protobufs.
      : must_be_initialized_(must_be_initialized), comp_(new auto(comp)) {}

  ProtoMatcherBase(const ProtoMatcherBase& other)
      : must_be_initialized_(other.must_be_initialized_),
        comp_(new auto(*other.comp_)) {}

  ProtoMatcherBase(ProtoMatcherBase&& other) = default;

  virtual ~ProtoMatcherBase() {}

  // Prints the expected protocol buffer.
  virtual void PrintExpectedTo(::std::ostream* os) const = 0;

  // Returns the expected value as a protobuf object; if the object
  // cannot be created (e.g. in ProtoStringMatcher), explains why to
  // 'listener' and returns NULL.  The caller must call
  // DeleteExpectedProto() on the returned value later.
  virtual const ::google::protobuf::Message* CreateExpectedProto(
      const ::google::protobuf::Message& arg,  // For determining the type of
                                               // the expected protobuf.
      ::testing::MatchResultListener* listener) const = 0;

  // Deletes the given expected protobuf, which must be obtained from
  // a call to CreateExpectedProto() earlier.
  virtual void DeleteExpectedProto(
      const ::google::protobuf::Message* expected) const = 0;

  bool MatchAndExplain(const ::google::protobuf::Message& arg,
                       ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(arg, false, listener);
  }

  bool MatchAndExplain(const ::google::protobuf::Message* arg,
                       ::testing::MatchResultListener* listener) const {
    return (arg != NULL) && MatchAndExplain(*arg, true, listener);
  }

  // Describes the expected relation between the actual protobuf and
  // the expected one.
  void DescribeRelationToExpectedProto(::std::ostream* os) const {
    if (comp_->repeated_field_comp ==
        kProtoCompareRepeatedFieldsIgnoringOrdering) {
      *os << "(ignoring repeated field ordering) ";
    }
    if (!comp_->ignore_fields.empty()) {
      *os << "(ignoring fields: ";
      const char* sep = "";
      for (size_t i = 0; i < comp_->ignore_fields.size(); ++i, sep = ", ")
        *os << sep << comp_->ignore_fields[i];
      *os << ") ";
    }
    if (comp_->float_comp == kProtoApproximate) {
      *os << "approximately ";
      if (comp_->has_custom_margin || comp_->has_custom_fraction) {
        *os << "(";
        if (comp_->has_custom_margin) {
          std::stringstream ss;
          ss << std::setprecision(std::numeric_limits<double>::digits10 + 2)
             << comp_->float_margin;
          *os << "absolute error of float or double fields <= " << ss.str();
        }
        if (comp_->has_custom_margin && comp_->has_custom_fraction) {
          *os << " or ";
        }
        if (comp_->has_custom_fraction) {
          std::stringstream ss;
          ss << std::setprecision(std::numeric_limits<double>::digits10 + 2)
             << comp_->float_fraction;
          *os << "relative error of float or double fields <= " << ss.str();
        }
        *os << ") ";
      }
    }

    if (comp_->differencer_config_function) {
      *os << "(with custom differencer config) ";
    }

    *os << (comp_->scope == kProtoPartial ? "partially " : "")
        << (comp_->field_comp == kProtoEqual ? "equal" : "equivalent")
        << (comp_->treating_nan_as_equal ? " (treating NaNs as equal)" : "")
        << " to ";
    PrintExpectedTo(os);
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "is " << (must_be_initialized_ ? "fully initialized and " : "");
    DescribeRelationToExpectedProto(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is " << (must_be_initialized_ ? "not fully initialized or " : "")
        << "not ";
    DescribeRelationToExpectedProto(os);
  }

  bool must_be_initialized() const { return must_be_initialized_; }

  const ProtoComparison& comp() const { return *comp_; }

 private:
  bool MatchAndExplain(const ::google::protobuf::Message& arg,
                       bool is_matcher_for_pointer,
                       ::testing::MatchResultListener* listener) const;

  const bool must_be_initialized_;
  std::unique_ptr<ProtoComparison> comp_;
};

// Returns a copy of the given proto2 message.
inline ::google::protobuf::Message* CloneProto2(
    const ::google::protobuf::Message& src) {
  ::google::protobuf::Message* clone = src.New();
  clone->CopyFrom(src);
  return clone;
}

// Implements EqualsProto and EquivToProto where the matcher parameter is a
// protobuf.
class ProtoMatcher : public ProtoMatcherBase {
 public:
  using MessageType = ::google::protobuf::Message;

  ProtoMatcher(
      const MessageType& expected,  // The expected protobuf.
      bool must_be_initialized,     // Must the argument be fully initialized?
      const ProtoComparison& comp)  // How to compare the two protobufs.
      : ProtoMatcherBase(must_be_initialized, comp),
        expected_(CloneProto2(expected)) {
    if (must_be_initialized) {
      CHECK(expected.IsInitialized())
          << "The protocol buffer given to *InitializedProto() "
          << "must itself be initialized, but the following required fields "
          << "are missing: " << expected.InitializationErrorString() << ".";
    }
  }

  virtual void PrintExpectedTo(::std::ostream* os) const {
    *os << expected_->GetDescriptor()->full_name() << " ";
    ::testing::internal::UniversalPrint(*expected_, os);
  }

  virtual const ::google::protobuf::Message* CreateExpectedProto(
      const ::google::protobuf::Message& /* arg */,
      ::testing::MatchResultListener* /* listener */) const {
    return expected_.get();
  }

  virtual void DeleteExpectedProto(
      const ::google::protobuf::Message* /* expected */) const {}

 private:
  const std::shared_ptr<const MessageType> expected_;
};

using PolymorphicProtoMatcher = ::testing::PolymorphicMatcher<ProtoMatcher>;

// Implements EqualsProto and EquivToProto where the matcher parameter is a
// string.
class ProtoStringMatcher : public ProtoMatcherBase {
 public:
  using MessageType = ::google::protobuf::Message;

  ProtoStringMatcher(
      absl::string_view
          expected,              // The text representing the expected protobuf.
      bool must_be_initialized,  // Must the argument be fully initialized?
      const ProtoComparison comp)  // How to compare the two protobufs.
      : ProtoMatcherBase(must_be_initialized, comp), expected_(std::string(expected)) {}

  // Parses the expected string as a protobuf of the same type as arg,
  // and returns the parsed protobuf (or NULL when the parse fails).
  // The caller must call DeleteExpectedProto() on the return value
  // later.
  virtual const MessageType* CreateExpectedProto(
      const MessageType& arg,
      ::testing::MatchResultListener* listener) const {
    ::google::protobuf::Message* expected_proto = arg.New();
    // We don't insist that the expected string parses as an
    // *initialized* protobuf.  Otherwise EqualsProto("...") may
    // wrongfully fail when the actual protobuf is not fully
    // initialized.
    std::string error_text;
    if (ParsePartialFromAscii(expected_, expected_proto, &error_text)) {
      return expected_proto;
    } else {
      delete expected_proto;
      if (listener->IsInterested()) {
        *listener << "where ";
        PrintExpectedTo(listener->stream());
        *listener << " doesn't parse as a " << arg.GetDescriptor()->full_name()
                  << ":\n"
                  << error_text;
      }
      return NULL;
    }
  }

  virtual void DeleteExpectedProto(
      const ::google::protobuf::Message* expected) const {
    delete expected;
  }

  virtual void PrintExpectedTo(::std::ostream* os) const {
    *os << "<" << expected_ << ">";
  }

 private:
  const std::string expected_;
};

}  // namespace internal

// Constructs a matcher that matches the argument if
// argument.Equals(m) or argument->Equals(m) returns true.
inline internal::PolymorphicProtoMatcher EqualsProto(
    const ::google::protobuf::Message& m) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return ::testing::MakePolymorphicMatcher(
      internal::ProtoMatcher(m, internal::kMayBeUninitialized, comp));
}

inline PolymorphicMatcher<internal::ProtoStringMatcher> EqualsProto(
    absl::string_view m) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEqual;
  return MakePolymorphicMatcher(
      internal::ProtoStringMatcher(m, internal::kMayBeUninitialized, comp));
}

// for Pointwise
MATCHER(EqualsProto, "") {
  const auto& a = ::testing::get<0>(arg);
  const auto& b = ::testing::get<1>(arg);
  return ::testing::ExplainMatchResult(EqualsProto(b), a, result_listener);
}

// Constructs a matcher that matches the argument if
// argument.Equivalent(m) or argument->Equivalent(m) returns true.
inline internal::PolymorphicProtoMatcher EquivToProto(
    const ::google::protobuf::Message& m) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return MakePolymorphicMatcher(
      internal::ProtoMatcher(m, internal::kMayBeUninitialized, comp));
}

inline PolymorphicMatcher<internal::ProtoStringMatcher> EquivToProto(
    absl::string_view m) {
  internal::ProtoComparison comp;
  comp.field_comp = internal::kProtoEquiv;
  return MakePolymorphicMatcher(
      internal::ProtoStringMatcher(m, internal::kMayBeUninitialized, comp));
}

// Returns a matcher that is the same as a given inner matcher, but applies a
// given function to the message differencer before using it for the
// comparison between the expected and actual protobufs.
//
// Prefer more specific transformer functions if possible; they result in
// better error messages and more readable test code.
//
// By default, the differencer is configured to use the field comparator which
// is also passed to the config function. It's possible to modify that
// comparator, although it's preferable to customize it through other
// transformers, e.g. Approximately.
//
// It's also possible to replace the comparator entirely, by passing it to
// set_field_comparator() method of the provided differencer. The user retains
// the ownership over the comparator and must guarantee that its lifetime
// exceeds the lifetime of the matcher.
//
// The config function will be applied after any configuration settings
// specified by other transformers. Overwriting these settings may result in
// misleading test failure messages; in particular, a config function that
// provides its own field comparator should not be used with transformers that
// rely on the default comparator, i.e. Approximately and TreatingNaNsAsEqual.
template <class InnerProtoMatcher>
inline InnerProtoMatcher WithDifferencerConfig(
    DifferencerConfigFunction differencer_config_function,
    InnerProtoMatcher inner_proto_matcher) {
  inner_proto_matcher.mutable_impl().SetDifferencerConfigFunction(
      differencer_config_function);
  return inner_proto_matcher;
}

}  // namespace testing

#endif  // ORTOOLS_BASE_PROTOCOL_BUFFER_MATCHERS_H_
