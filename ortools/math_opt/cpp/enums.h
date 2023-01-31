// Copyright 2010-2022 Google LLC
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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

// The MathOpt C++ API defines enums that are used in parameters and results and
// that corresponds to Proto generated enums.
//
// The tools in this header make sure the C++ enums provide the following
// features:
//  * enumerating all enum values
//  * bidirectional string conversion
//  * operator<< stream support
//  * bidirectional proto generated enum conversion
//
// Example declaration:
//
//  my_file.proto:
//    enum MyEnumProto {
//        MY_ENUM_UNSPECIFIED = 0;
//        MY_ENUM_FIRST_VALUE = 1;
//        MY_ENUM_SECOND_VALUE = 2;
//     }
//
//  my_file.h:
//    enum class MyEnum {
//      kFirstValue = MY_ENUM_FIRST_VALUE,
//      kSecondValue = MY_ENUM_SECOND_VALUE,
//    };
//
//    MATH_OPT_DEFINE_ENUM(MyEnum, MY_ENUM_UNSPECIFIED);
//
//  my_file.cc:
//    std::optional<absl::string_view>
//    Enum<MyEnum>::ToOptString(MyEnum value) {
//      switch (value) {
//        case MyEnum::kFirstValue:
//          return "first_value";
//        case MyEnum::kSecondValue:
//          return "second_value";
//      }
//      return std::nullopt;
//    }
//
//    absl::Span<const MyEnum> Enum<MyEnum>::AllValues() {
//      static constexpr MyEnum kMyEnumValues[] = {MyEnum::kFirstValue,
//                                                 MyEnum::kSecondValue};
//      return absl::MakeConstSpan(kMyEnumValues);
//    }
//
//  my_file_test.cc:
//    #include "ortools/math_opt/cpp/enums_testing.h"
//    ...
//    INSTANTIATE_TYPED_TEST_SUITE_P(MyEnum, EnumTest, MyEnum);
//
// Once this is done, the following functions are available:
//  * absl::Span<MyEnum> Enum<MyEnum>::AllValues()
//  * optional<MyEnum> EnumFromString<MyEnum>(string_view)
//  * string_view EnumToString(MyEnum)
//  * optional<string_view> EnumToOptString(MyEnum)
//  * optional<MyEnum> EnumFromProto(MyEnumProto)
//  * MyEnumProto EnumToProto(optional<MyEnum>)
//  * MyEnumProto EnumToProto(MyEnum)
//  * operator<<(MyEnum)
//  * operator<<(std::optional<MyEnum>)
//
// See examples of usage in the Enum struct documentation below.
#ifndef OR_TOOLS_MATH_OPT_CPP_ENUMS_H_
#define OR_TOOLS_MATH_OPT_CPP_ENUMS_H_

#include <optional>
#include <ostream>
#include <type_traits>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/log/check.h"

namespace operations_research::math_opt {

// This template is specialized for each enum in the C++ API.
//
// It provides a standard way to query properties of those enums and it is used
// by some global functions below to implement conversion from/to string or
// proto enum.
//
// Usage example:
//
//   // Iterating on all enum values.
//   for (const auto solver_type : Enum<SolverType>::AllValues()) {
//     ...
//   }
//
//   // Parsing a flag as an enum.
//   const std::optional<SolverType> solver_type =
//     EnumFromString(absl::GetFlag(FLAGS_solver_type));
//   if (!solver_type) {
//     return util::InvalidArgumentErrorBuilder()
//             _ << "failed to parse --solver_type value: "
//               << absl::GetFlag(FLAGS_solver_type);
//   }
//
//   // Conversion to string.
//   const SolverType solver_type = ...;
//   LOG(INFO) << "solver: " << solver_type;
//   absl::StrCat(EnumToString(solver_type), "_test");
//   absl::StrCat(EnumToOptString(solver_type).value(), "_test");
//
//   // Conversion to Proto.
//   const std::optional<SolverType> opt_solver_type = ...;
//   const SolverTypeProto solver_type_proto = EnumToProto(opt_solver_type);
//
//   // Conversion from Proto.
//   const SolverTypeProto solver_type_proto = ...;
//   const std::optional<SolverType> opt_solver_type =
//     EnumFromProto(solver_type_proto);
//
// Implementation note: don't specialize directly and instead use the
// MATH_OPT_DEFINE_ENUM macro.
template <typename E>
struct Enum {
  // Must be true in all implementation. This is used with std::enable_if to
  // condition the implementation of some overloads.
  static constexpr bool kIsImplemented = false;

  // The type of the Proto equivalent to this enum.
  //
  // (Here we use int as a placeholder so that the code compiles.)
  using Proto = int;

  // The value Proto enum that represents the unspecified case.
  static constexpr Proto kProtoUnspecifiedValue = {};

  // Returns a unique string that represent the enum. Returns nullopt if the
  // input value is not a valid value of the enum.
  //
  // The returned string should not include the enum name and should be in
  // snake_case (e.g. is the enum is kNoSolutionFound, this should return
  // "no_solution_found").
  //
  // Please prefer using the global functions EnumToString() (or
  // EnumToOptString() if support for invalid values is needed) instead to
  // benefit from automatic template type deduction.
  static std::optional<absl::string_view> ToOptString(E value);

  // Returns all possible values of the enum.
  static absl::Span<const E> AllValues();
};

using ProtoEnumIsValid = bool (*)(int);

// This template is specialized for each enum in the Proto API. It
// defines the correspondence with the C++ enum.
//
// Implementation note: don't specialize directly and instead use the
// MATH_OPT_DEFINE_ENUM macro.
template <typename P>
struct EnumProto {
  // The type of the C++ enum equivalent to the P proto enum.
  //
  // (Here we use void as a placeholder so that the code compiles.)
  using Cpp = void;

  // The smallest valid enum value.
  static constexpr P kMin = {};

  // The largest valid enum value.
  static constexpr P kMax = {};

  // Proto function returning the true if the input integer matches a valid
  // value (some values may be missing in range [kMin, kMax]).
  static constexpr ProtoEnumIsValid kIsValid = nullptr;
};

// Returns the Proto enum that matches the input C++ proto, returns
// Enum<E>::kProtoUnspecifiedValue if the input is std::nullopt.
template <typename E>
typename Enum<E>::Proto EnumToProto(const std::optional<E> value);

// Returns the Proto enum that matches the input C++ proto.
//
// Implementation note: this overload is necessary for EnumToProto(Xxx::kXxx)
// since C++ won't deduce E in std::optional<E> with the other overload.
template <typename E>
typename Enum<E>::Proto EnumToProto(const E value);

// Returns the C++ enum that matches the input Proto enum, returns
// std::nullopt if the input is kProtoUnspecifiedValue.
template <typename P>
std::optional<typename EnumProto<P>::Cpp> EnumFromProto(const P proto_value);

// Returns a unique string that represent the enum.
//
// It CHECKs that the input is a valid enum value. For most users this should
// always be the case since MathOpt don't generates invalid data.
//
// Prefer using operator<< when possible though. As a side benefice it does not
// CHECK but instead prints the integer value of the invalid input.
template <typename E>
absl::string_view EnumToString(const E value);

// Returns a unique string that represent the enum. Returns nullopt if the input
// value is not a valid value of the enum.
template <typename E>
std::optional<absl::string_view> EnumToOptString(const E value);

// Returns the enum value that corresponds to the input string or nullopt if no
// enum matches.
//
// The expected strings are the one returned by EnumToString().
//
// This is O(n) in complexity so use with care.
template <typename E>
std::optional<E> EnumFromString(const absl::string_view str);

// Overload of operator<< for enum types that implements Enum<E>.
//
// It calls EnumToOptString(), printing the returned value if not nullopt. When
// nullopt it prints the enum numeric value instead.
template <typename E,
          // We must use enable_if here to prevent this overload to be selected
          // for other types than ones that implement Enum<E>.
          typename = std::enable_if_t<Enum<E>::kIsImplemented>>
std::ostream& operator<<(std::ostream& out, const E value) {
  const std::optional<absl::string_view> opt_str = EnumToOptString(value);
  if (opt_str.has_value()) {
    out << *opt_str;
  } else {
    out << "<invalid enum (" << static_cast<std::underlying_type_t<E>>(value)
        << ")>";
  }
  return out;
}

// Overload of operator<< for std::optional<E> when Enum<E> is implemented.
//
// When the value is nullopt, it prints "<unspecified>", else it prints the enum
// value.
template <typename E,
          // We must use enable_if here to prevent this overload to be selected
          // for other types than ones that implement Enum<E>.
          typename = std::enable_if_t<Enum<E>::kIsImplemented>>
std::ostream& operator<<(std::ostream& out, const std::optional<E> opt_value) {
  if (opt_value.has_value()) {
    out << *opt_value;
  } else {
    out << "<unspecified>";
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
// Template functions implementations after this point.
////////////////////////////////////////////////////////////////////////////////

template <typename E>
typename Enum<E>::Proto EnumToProto(const std::optional<E> value) {
  return value ? static_cast<typename Enum<E>::Proto>(*value)
               : Enum<E>::kProtoUnspecifiedValue;
}

template <typename E>
typename Enum<E>::Proto EnumToProto(const E value) {
  return EnumToProto(std::make_optional(value));
}

template <typename P>
std::optional<typename EnumProto<P>::Cpp> EnumFromProto(const P proto_value) {
  if (proto_value == Enum<typename EnumProto<P>::Cpp>::kProtoUnspecifiedValue) {
    return std::nullopt;
  }
  return static_cast<typename EnumProto<P>::Cpp>(proto_value);
}

template <typename E>
absl::string_view EnumToString(const E value) {
  std::optional<absl::string_view> opt_str = Enum<E>::ToOptString(value);
  CHECK(opt_str.has_value())
      << "invalid value: " << static_cast<std::underlying_type_t<E>>(value);
  return *opt_str;
}

template <typename E>
std::optional<absl::string_view> EnumToOptString(const E value) {
  return Enum<E>::ToOptString(value);
}

template <typename E>
std::optional<E> EnumFromString(const absl::string_view str) {
  for (const E value : Enum<E>::AllValues()) {
    if (EnumToOptString(value) == str) {
      return value;
    }
  }
  return std::nullopt;
}

// Macros that defines the templates specializations for Enum and EnumProto.
//
// The CppEnum parameter is the name of the C++ enum class which values are the
// Proto enum values. The C++ enum must contain a value for each value of the
// Proto enum but the UNSPECIFIED one. The proto_unspecified_value is the
// UNSPECIFIED one.
//
// It leaves two functions to be implemented in the .cc file:
//
//   absl::string_view Enum<CppEnum>::ToOptString(CppEnum value) {
//   absl::Span<const CppEnum> Enum<CppEnum>::AllValues();
//
// See the comment at the top of this file for an example. See the comment on
// Enum struct for the functions that can then be used on enums.
#define MATH_OPT_DEFINE_ENUM(CppEnum, proto_unspecified_value)               \
  template <>                                                                \
  struct Enum<CppEnum> {                                                     \
    static constexpr bool kIsImplemented = true;                             \
    using Proto = CppEnum##Proto;                                            \
    static constexpr Proto kProtoUnspecifiedValue = proto_unspecified_value; \
    static std::optional<absl::string_view> ToOptString(CppEnum value);      \
    static absl::Span<const CppEnum> AllValues();                            \
  };                                                                         \
                                                                             \
  template <>                                                                \
  struct EnumProto<CppEnum##Proto> {                                         \
    using Cpp = CppEnum;                                                     \
    static constexpr CppEnum##Proto kMin = CppEnum##Proto##_MIN;             \
    static constexpr CppEnum##Proto kMax = CppEnum##Proto##_MAX;             \
    static constexpr ProtoEnumIsValid kIsValid = CppEnum##Proto##_IsValid;   \
  } /* missing semicolon to force adding it at the invocation site */

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_ENUMS_H_
