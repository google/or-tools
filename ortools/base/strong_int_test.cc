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

// Unit test cases for StrongInt.
#include "ortools/base/strong_int.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

#include "absl/container/node_hash_map.h"
#include "absl/flags/marshalling.h"
#include "absl/hash/hash_testing.h"
#include "absl/log/log.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace util_intops {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::HasSubstr;

DEFINE_STRONG_INT_TYPE(StrongInt8, int8_t);
DEFINE_STRONG_INT_TYPE(StrongUInt8, uint8_t);
DEFINE_STRONG_INT_TYPE(StrongInt16, int16_t);
DEFINE_STRONG_INT_TYPE(StrongUInt16, uint16_t);
DEFINE_STRONG_INT_TYPE(StrongInt32, int32_t);
DEFINE_STRONG_INT_TYPE(StrongInt64, int64_t);
DEFINE_STRONG_INT_TYPE(StrongUInt32, uint32_t);
DEFINE_STRONG_INT_TYPE(StrongUInt64, uint64_t);
DEFINE_STRONG_INT_TYPE(StrongLong, long);  // NOLINT
DEFINE_STRONG_INT_TYPE(StrongUInt128, absl::uint128);
DEFINE_STRONG_INT_TYPE(StrongInt128, absl::int128);

TEST(StrongIntTypeIdTest, TypeIdIsAsExpected) {
  EXPECT_EQ("StrongInt8", StrongInt8::TypeName());
  EXPECT_EQ("StrongLong", StrongLong::TypeName());
}

template <typename StrongIntTypeUnderTest>
class StrongIntTest : public ::testing::Test {
 public:
  using T = StrongIntTypeUnderTest;
  using V = typename T::ValueType;
};

// All tests will be executed on the following StrongInt<> types.
using SupportedStrongIntTypes =
    ::testing::Types<StrongInt8, StrongUInt8, StrongInt16, StrongUInt16,
                     StrongInt32, StrongInt64, StrongUInt64, StrongLong,
                     StrongUInt128, StrongInt128>;

TYPED_TEST_SUITE(StrongIntTest, SupportedStrongIntTypes);

// NOTE: On all tests, we use the accessor value() as to not invoke the
// comparison operators which must themselves be tested.

TYPED_TEST(StrongIntTest, TestTraits) {
  using T = typename TestFixture::T;

  EXPECT_TRUE(std::is_standard_layout<T>::value);
  EXPECT_TRUE(std::is_trivially_copy_constructible<T>::value);
  EXPECT_TRUE(std::is_trivially_copy_assignable<T>::value);
  EXPECT_TRUE(std::is_trivially_destructible<T>::value);
}

TYPED_TEST(StrongIntTest, TestCtors) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  {  // Test default construction.
    T x;
    EXPECT_EQ(V(), x.value());
  }

  {  // Test construction from a value.
    T x(93);
    EXPECT_EQ(V(93), x.value());
  }

  {  // Test construction from a negative value.
    T x(-1);
    EXPECT_EQ(V(-1), x.value());
  }

  {  // Test copy construction.
    T x(76);
    T y(x);
    EXPECT_EQ(V(76), y.value());
  }

  {  // Test construction from int8_t.
    constexpr int8_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());
    static_assert(static_cast<int8_t>(T(i).value()) == 93,
                  "value() is not constexpr");

    int8_t j = -76;
    T y(j);
    EXPECT_EQ(V(-76), y.value());
  }

  {  // Test construction from uint8_t.
    uint8_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());
  }

  {  // Test construction from int16_t.
    int16_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());

    int16_t j = -76;
    T y(j);
    EXPECT_EQ(V(-76), y.value());
  }

  {  // Test construction from uint16_t.
    uint16_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());
  }

  {  // Test construction from int32_t.
    int32_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());

    int32_t j = -76;
    T y(j);
    EXPECT_EQ(V(-76), y.value());
  }

  {  // Test construction from uint32_t.
    uint32_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());
  }

  {  // Test construction from int64_t.
    int64_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());

    int64_t j = -76;
    T y(j);
    EXPECT_EQ(V(-76), y.value());
  }

  {  // Test construction from uint64_t.
    uint64_t i = 93;
    T x(i);
    EXPECT_EQ(V(93), x.value());
  }

  if constexpr (std::is_fundamental_v<V>) {
    {  // Test construction from float.
      float i = 93.1;
      T x(i);
      EXPECT_EQ(V(93), x.value());

      // It is undefined to init an unsigned int from a negative float.
      if constexpr (std::numeric_limits<V>::is_signed) {
        float j = -76.1;
        T y(j);
        EXPECT_EQ(V(-76.1), y.value());
      }
    }

    {  // Test construction from double.
      double i = 93.1;
      T x(i);
      EXPECT_EQ(V(93), x.value());

      // It is undefined to init an unsigned int from a negative double.
      if constexpr (std::numeric_limits<V>::is_signed) {
        double j = -76.1;
        T y(j);
        EXPECT_EQ(V(-76.1), y.value());
      }
    }

    {  // Test construction from long double.
      long double i = 93.1;
      T x(i);
      EXPECT_EQ(V(93), x.value());

      // It is undefined to init an unsigned int from a negative long double.
      if constexpr (std::numeric_limits<V>::is_signed) {
        long double j = -76.1;
        T y(j);
        EXPECT_EQ(V(-76.1), y.value());
      }
    }
  }

  {  // Test constexpr assignment
    constexpr T x(123);
    EXPECT_EQ(V(123), x.value());
  }
}

TYPED_TEST(StrongIntTest, TestAbslParseFlag) {
  using T = typename TestFixture::T;

  T t;
  std::string error;
  EXPECT_TRUE(absl::ParseFlag("123", &t, &error));
  EXPECT_EQ(t, T(123));
  EXPECT_EQ(absl::UnparseFlag(t), "123");
}

TYPED_TEST(StrongIntTest, TestAbslParseFlagNotAnInt) {
  using T = typename TestFixture::T;

  T t;
  std::string error;
  EXPECT_FALSE(absl::ParseFlag("not_an_int", &t, &error));
  EXPECT_THAT(error,
              AllOf(HasSubstr("'not_an_int'"), HasSubstr(T::TypeName())));
}

TYPED_TEST(StrongIntTest, TestAbslParseFlagEmptyString) {
  using T = typename TestFixture::T;

  T t;
  std::string error;
  EXPECT_FALSE(absl::ParseFlag("", &t, &error));
  EXPECT_THAT(error, AllOf(HasSubstr("''"), HasSubstr(T::TypeName())));
}

TYPED_TEST(StrongIntTest, TestAbslParseFlagRangeLimits) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  const V max_int = std::numeric_limits<V>::max();
  const std::string max = absl::StrCat(max_int);
  const V min_int = std::numeric_limits<V>::min();
  const std::string min = absl::StrCat(min_int);

  std::string max_plus_one;
  std::string min_minus_one;
  if constexpr (std::is_same_v<V, absl::int128>) {
    EXPECT_THAT(max, Eq("170141183460469231731687303715884105727"));
    max_plus_one = "170141183460469231731687303715884105728";

    EXPECT_THAT(min, Eq("-170141183460469231731687303715884105728"));
    min_minus_one = "-170141183460469231731687303715884105729";
  } else if constexpr (std::is_same_v<V, absl::uint128>) {
    EXPECT_THAT(max, Eq("340282366920938463463374607431768211455"));
    max_plus_one = "340282366920938463463374607431768211456";

    EXPECT_THAT(min, Eq("0"));
    min_minus_one = "-1";
  } else {
    max_plus_one = absl::StrCat(absl::int128(max_int) + 1);
    min_minus_one = absl::StrCat(absl::int128(min_int) - 1);
  }

  T t;
  std::string error;
  EXPECT_TRUE(absl::ParseFlag(max, &t, &error));
  EXPECT_EQ(t, T(max_int));
  EXPECT_EQ(absl::UnparseFlag(t), max);
  EXPECT_TRUE(absl::ParseFlag(min, &t, &error));
  EXPECT_EQ(t, T(min_int));
  EXPECT_EQ(absl::UnparseFlag(t), min);

  EXPECT_FALSE(absl::ParseFlag(max_plus_one, &t, &error));
  EXPECT_THAT(error, AllOf(HasSubstr(max_plus_one), HasSubstr(T::TypeName())));
  EXPECT_FALSE(absl::ParseFlag(min_minus_one, &t, &error));
  EXPECT_THAT(error, AllOf(HasSubstr(min_minus_one), HasSubstr(T::TypeName())));
}

namespace {
struct PositiveValidator {
  template <class T, class U>
  static bool ValidateInit(U i) {
    if (i < 0) LOG(FATAL) << "PositiveValidator";
    return true;
  }
};
}  // namespace

TYPED_TEST(StrongIntTest, TestAbslParseFlagInvalid) {
  using V = typename TestFixture::V;
  struct CustomTag {
    static constexpr absl::string_view TypeName() { return "CustomTag"; }
  };
  using T = StrongInt<CustomTag, V, PositiveValidator>;

  T t;
  std::string error;
  if constexpr (std::numeric_limits<V>::is_signed) {
    EXPECT_DEATH(absl::ParseFlag("-123", &t, &error), "PositiveValidator");
  } else {
    EXPECT_FALSE(absl::ParseFlag("-123", &t, &error));
    EXPECT_THAT(error, AllOf(HasSubstr("'-123'"), HasSubstr("CustomTag")));
  }
}

TYPED_TEST(StrongIntTest, TestCtorDeath) {
  using V = typename TestFixture::V;

  if constexpr (std::numeric_limits<V>::is_signed) {
    struct CustomTag {};
    using T = StrongInt<CustomTag, V, PositiveValidator>;
    EXPECT_DEATH(T(static_cast<V>(-123)), "PositiveValidator");
  }
}

TYPED_TEST(StrongIntTest, TestMetadata) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  T t;
  EXPECT_EQ(std::numeric_limits<V>::max(), t.Max().value());
  EXPECT_EQ(std::numeric_limits<V>::min(), t.Min().value());
}

TYPED_TEST(StrongIntTest, TestUnaryOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  {  // Test unary plus and minus of positive values.
    T x(123);
    EXPECT_EQ(V(123), (+x).value());
    EXPECT_EQ(V(-123), (-x).value());
  }
  {  // Test unary plus and minus of negative values.
    T x(-123);
    EXPECT_EQ(V(-123), (+x).value());
    EXPECT_EQ(V(123), (-x).value());
  }
  {  // Test logical not of positive values.
    T x(123);
    EXPECT_EQ(false, !x);
    EXPECT_EQ(true, !!x);
  }
  {  // Test logical not of negative values.
    T x(-123);
    EXPECT_EQ(false, !x);
    EXPECT_EQ(true, !!x);
  }
  {  // Test logical not of zero.
    T x(0);
    EXPECT_EQ(true, !x);
    EXPECT_EQ(false, !!x);
  }
  {  // Test bitwise not of positive values.
    T x(123);
    EXPECT_EQ(V(~(x.value())), (~x).value());
    EXPECT_EQ(x.value(), (~~x).value());
  }
  {  // Test bitwise not of zero.
    T x(0x00);
    EXPECT_EQ(V(~(x.value())), (~x).value());
    EXPECT_EQ(x.value(), (~~x).value());
  }
}

TYPED_TEST(StrongIntTest, TestIncrementDecrementOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test simple increments and decrements.
  T x(0);
  EXPECT_EQ(V(0), x.value());
  EXPECT_EQ(V(0), (x++).value());
  EXPECT_EQ(V(1), x.value());
  EXPECT_EQ(V(2), (++x).value());
  EXPECT_EQ(V(2), x.value());
  EXPECT_EQ(V(2), (x--).value());
  EXPECT_EQ(V(1), x.value());
  EXPECT_EQ(V(0), (--x).value());
  EXPECT_EQ(V(0), x.value());
}

TYPED_TEST(StrongIntTest, TestAssignmentOperator) {
  using T = typename TestFixture::T;

  {  // Test simple assignment from the same type.
    T x(12);
    T y(34);
    EXPECT_EQ(y.value(), (x = y).value());
    EXPECT_EQ(y.value(), x.value());
  }
}

#define TEST_T_OP_T(xval, op, yval)                                           \
  {                                                                           \
    T x(xval);                                                                \
    T y(yval);                                                                \
    V expected = x.value() op y.value();                                      \
    EXPECT_EQ(expected, (x op y).value());                                    \
    EXPECT_EQ(expected, (x op## = y).value());                                \
    EXPECT_EQ(expected, x.value());                                           \
    constexpr T cx_x(xval);                                                   \
    constexpr T cx_y(yval);                                                   \
    if constexpr (std::is_fundamental_v<V>) {                                 \
      constexpr V cx_expected = static_cast<V>(cx_x.value() op cx_y.value()); \
      static_assert((cx_x op cx_y) == T(cx_expected),                         \
                    #xval " " #op " " #yval);                                 \
    } else {                                                                  \
      V cx_expected = static_cast<V>(cx_x.value() op cx_y.value());           \
      EXPECT_EQ((cx_x op cx_y), T(cx_expected)) << #xval " " #op " " #yval;   \
    }                                                                         \
  }

TYPED_TEST(StrongIntTest, TestPlusOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test positive vs. positive addition.
  TEST_T_OP_T(9, +, 3)
  // Test negative vs. positive addition.
  TEST_T_OP_T(-9, +, 3)
  // Test positive vs. negative addition.
  TEST_T_OP_T(9, +, -3)
  // Test negative vs. negative addition.
  TEST_T_OP_T(-9, +, -3)
  // Test addition by zero.
  TEST_T_OP_T(93, +, 0);
}

TYPED_TEST(StrongIntTest, TestMinusOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test positive vs. positive subtraction.
  TEST_T_OP_T(9, -, 3)
  // Test negative vs. positive subtraction.
  TEST_T_OP_T(-9, -, 3)
  // Test positive vs. negative subtraction.
  TEST_T_OP_T(9, -, -3)
  // Test negative vs. negative subtraction.
  TEST_T_OP_T(-9, -, -3)
  // Test positive vs. positive subtraction resulting in negative.
  TEST_T_OP_T(3, -, 9);
  // Test subtraction of zero.
  TEST_T_OP_T(93, -, 0);
  // Test subtraction from zero.
  TEST_T_OP_T(0, -, 93);
}

#define TEST_T_OP_NUM(xval, op, numtype, yval)                              \
  {                                                                         \
    T x(xval);                                                              \
    numtype y = yval;                                                       \
    V expected = x.value() op y;                                            \
    EXPECT_EQ(expected, (x op y).value());                                  \
    EXPECT_EQ(expected, (x op## = y).value());                              \
    EXPECT_EQ(expected, x.value());                                         \
    constexpr T cx_x(xval);                                                 \
    if constexpr (std::is_fundamental_v<V>) {                               \
      constexpr V cx_expected = static_cast<V>(cx_x.value() op yval);       \
      static_assert((cx_x op yval) == T(cx_expected),                       \
                    #xval " " #op " " #yval);                               \
    } else {                                                                \
      V cx_expected =                                                       \
          static_cast<V>(static_cast<numtype>(cx_x.value()) op yval);       \
      EXPECT_EQ((cx_x op yval), T(cx_expected)) << #xval " " #op " " #yval; \
    }                                                                       \
  }
#define TEST_NUM_OP_T(numtype, xval, op, yval)                              \
  {                                                                         \
    numtype x = xval;                                                       \
    T y(yval);                                                              \
    V expected = x op y.value();                                            \
    EXPECT_EQ(expected, (x op y).value());                                  \
    constexpr T cx_y(yval);                                                 \
    if constexpr (std::is_fundamental_v<V>) {                               \
      constexpr V cx_expected = static_cast<V>(xval op cx_y.value());       \
      static_assert((xval op cx_y) == T(cx_expected),                       \
                    #xval " " #op " " #yval);                               \
    } else {                                                                \
      V cx_expected =                                                       \
          static_cast<V>(xval op static_cast<numtype>(cx_y.value()));       \
      EXPECT_EQ((xval op cx_y), T(cx_expected)) << #xval " " #op " " #yval; \
    }                                                                       \
  }

// NOLINTNEXTLINE: google-readability-function-size
TYPED_TEST(StrongIntTest, TestMultiplyOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test positive vs. positive multiplication.
  TEST_T_OP_NUM(9, *, V, 3);
  TEST_NUM_OP_T(V, 9, *, 3);
  if constexpr (std::is_signed<V>::value) {
    // Test negative vs. positive multiplication.
    TEST_T_OP_NUM(-9, *, V, 3);
    TEST_NUM_OP_T(V, -9, *, 3);
    // Test positive vs. negative multiplication.
    TEST_T_OP_NUM(9, *, V, -3);
    TEST_NUM_OP_T(V, 9, *, -3);
    // Test negative vs. negative multiplication.
    TEST_T_OP_NUM(-9, *, V, -3);
    TEST_NUM_OP_T(V, -9, *, -3);
  }
  // Test multiplication by one.
  TEST_T_OP_NUM(93, *, V, 1);
  TEST_NUM_OP_T(V, 93, *, 1);
  // Test multiplication by zero.
  TEST_T_OP_NUM(93, *, V, 0);
  TEST_NUM_OP_T(V, 93, *, 0);
  if constexpr (std::is_signed<V>::value) {
    // Test multiplication by a negative.
    TEST_T_OP_NUM(93, *, V, -1);
    TEST_NUM_OP_T(V, 93, *, -1);
  }
  // Test multiplication by int8_t.
  TEST_T_OP_NUM(39, *, int8_t, 2);
  TEST_NUM_OP_T(int8_t, 39, *, 2);
  // Test multiplication by uint8_t.
  TEST_T_OP_NUM(39, *, uint8_t, 2);
  TEST_NUM_OP_T(uint8_t, 39, *, 2);
  // Test multiplication by int16_t.
  TEST_T_OP_NUM(39, *, int16_t, 2);
  TEST_NUM_OP_T(int16_t, 39, *, 2);
  // Test multiplication by uint16_t.
  TEST_T_OP_NUM(39, *, uint16_t, 2);
  TEST_NUM_OP_T(uint16_t, 39, *, 2);
  // Test multiplication by int32_t.
  TEST_T_OP_NUM(39, *, int32_t, 2);
  TEST_NUM_OP_T(int32_t, 39, *, 2);
  // Test multiplication by uint32_t.
  TEST_T_OP_NUM(39, *, uint32_t, 2);
  TEST_NUM_OP_T(uint32_t, 39, *, 2);
  // Test multiplication by int64_t.
  TEST_T_OP_NUM(39, *, int64_t, 2);
  TEST_NUM_OP_T(int64_t, 39, *, 2);
  // Test multiplication by uint64_t.
  TEST_T_OP_NUM(39, *, uint64_t, 2);
  TEST_NUM_OP_T(uint64_t, 39, *, 2);
  if constexpr (std::is_fundamental_v<V>) {
    // Test multiplication by float.
    TEST_T_OP_NUM(39, *, float, 2.1);
    TEST_NUM_OP_T(float, 39, *, 2.1);
    // Test multiplication by double.
    TEST_T_OP_NUM(39, *, double, 2.1);
    TEST_NUM_OP_T(double, 39, *, 2.1);
    // Test multiplication by long double.
    TEST_T_OP_NUM(39, *, long double, 2.1);
    TEST_NUM_OP_T(long double, 39, *, 2.1);
  }
}

TYPED_TEST(StrongIntTest, TestDivideOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test positive vs. positive division.
  TEST_T_OP_NUM(9, /, V, 3);
  if constexpr (std::is_signed<V>::value) {
    // Test negative vs. positive division.
    TEST_T_OP_NUM(-9, /, V, 3);
    // Test positive vs. negative division.
    TEST_T_OP_NUM(9, /, V, -3);
    // Test negative vs. negative division.
    TEST_T_OP_NUM(-9, /, V, -3);
  }
  // Test division by one.
  TEST_T_OP_NUM(93, /, V, 1);
  if constexpr (std::is_signed<V>::value) {
    // Test division by a negative.
    TEST_T_OP_NUM(93, /, V, -1);
  }
  // Test division by int8_t.
  TEST_T_OP_NUM(93, /, int8_t, 2);
  // Test division by uint8_t.
  TEST_T_OP_NUM(93, /, uint8_t, 2);
  // Test division by int16_t.
  TEST_T_OP_NUM(93, /, int16_t, 2);
  // Test division by uint16_t.
  TEST_T_OP_NUM(93, /, uint16_t, 2);
  // Test division by int32_t.
  TEST_T_OP_NUM(93, /, int32_t, 2);
  // Test division by uint32_t.
  TEST_T_OP_NUM(93, /, uint32_t, 2);
  // Test division by int64_t.
  TEST_T_OP_NUM(93, /, int64_t, 2);
  // Test division by uint64_t.
  TEST_T_OP_NUM(93, /, uint64_t, 2);
  if constexpr (std::is_fundamental_v<V>) {
    // Test division by float.
    TEST_T_OP_NUM(93, /, float, 2.1);
    // Test division by double.
    TEST_T_OP_NUM(93, /, double, 2.1);
    // Test division by long double.
    TEST_T_OP_NUM(93, /, long double, 2.1);
  }
}

TYPED_TEST(StrongIntTest, TestModuloOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test positive vs. positive modulo.
  TEST_T_OP_NUM(7, %, V, 6);
  // Test negative vs. positive modulo.
  TEST_T_OP_NUM(-7, %, V, 6);
  // Test positive vs. negative modulo.
  TEST_T_OP_NUM(7, %, V, -6);
  // Test negative vs. negative modulo.
  TEST_T_OP_NUM(-7, %, V, -6);
  // Test modulo by one.
  TEST_T_OP_NUM(93, %, V, 1);
  // Test modulo by a negative.
  TEST_T_OP_NUM(93, %, V, -5);
  // Test modulo by int8_t.
  TEST_T_OP_NUM(93, %, int8_t, 5);
  // Test modulo by uint8_t.
  TEST_T_OP_NUM(93, %, uint8_t, 5);
  // Test modulo by int16_t.
  TEST_T_OP_NUM(93, %, int16_t, 5);
  // Test modulo by uint16_t.
  TEST_T_OP_NUM(93, %, uint16_t, 5);
  // Test modulo by int32_t.
  TEST_T_OP_NUM(93, %, int32_t, 5);
  // Test modulo by uint32_t.
  TEST_T_OP_NUM(93, %, uint32_t, 5);
  // Test modulo by int64_t.
  TEST_T_OP_NUM(93, %, int64_t, 5);
  // Test modulo by uint64_t.
  TEST_T_OP_NUM(93, %, uint64_t, 5);
  // Test modulo by a larger value.
  TEST_T_OP_NUM(93, %, V, 100);
}

TYPED_TEST(StrongIntTest, TestLeftShiftOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test basic shift.
  TEST_T_OP_NUM(0x09, <<, int, 3);
  // Test shift by zero.
  TEST_T_OP_NUM(0x09, <<, int, 0);
}

TYPED_TEST(StrongIntTest, TestRightShiftOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test basic shift.
  TEST_T_OP_NUM(0x09, >>, int, 3);
  // Test shift by zero.
  TEST_T_OP_NUM(0x09, >>, int, 0);
}

TYPED_TEST(StrongIntTest, TestBitAndOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test basic bit-and.
  TEST_T_OP_T(0x09, &, 0x03);
  // Test bit-and by zero.
  TEST_T_OP_T(0x09, &, 0x00);
}

TYPED_TEST(StrongIntTest, TestBitOrOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test basic bit-or.
  TEST_T_OP_T(0x09, |, 0x03);
  // Test bit-or by zero.
  TEST_T_OP_T(0x09, |, 0x00);
}

TYPED_TEST(StrongIntTest, TestBitXorOperators) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  // Test basic bit-xor.
  TEST_T_OP_T(0x09, ^, 0x03);
  // Test bit-xor by zero.
  TEST_T_OP_T(0x09, ^, 0x00);
}

TYPED_TEST(StrongIntTest, TestComparisonOperators) {
  using T = typename TestFixture::T;

  T x(93);

  EXPECT_TRUE(x == T(93));
  EXPECT_TRUE(T(93) == x);
  EXPECT_FALSE(x == T(76));
  EXPECT_FALSE(T(76) == x);

  EXPECT_TRUE(x != T(76));
  EXPECT_TRUE(T(76) != x);
  EXPECT_FALSE(x != T(93));
  EXPECT_FALSE(T(93) != x);

  EXPECT_TRUE(x < T(94));
  EXPECT_FALSE(T(94) < x);
  EXPECT_FALSE(x < T(76));
  EXPECT_TRUE(T(76) < x);

  EXPECT_TRUE(x <= T(94));
  EXPECT_FALSE(T(94) <= x);
  EXPECT_FALSE(x <= T(76));
  EXPECT_TRUE(T(76) <= x);
  EXPECT_TRUE(x <= T(93));
  EXPECT_TRUE(T(93) <= x);

  EXPECT_TRUE(x > T(76));
  EXPECT_FALSE(T(76) > x);
  EXPECT_FALSE(x > T(94));
  EXPECT_TRUE(T(94) > x);

  EXPECT_TRUE(x >= T(76));
  EXPECT_FALSE(T(76) >= x);
  EXPECT_FALSE(x >= T(94));
  EXPECT_TRUE(T(94) >= x);
  EXPECT_TRUE(x >= T(93));
  EXPECT_TRUE(T(93) >= x);
}

TYPED_TEST(StrongIntTest, TestStreamOutputOperator) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  T x(93);
  std::ostringstream out;
  out << x;
  EXPECT_EQ("93", out.str());

  for (const T t :
       {T(std::numeric_limits<V>::min()), T(std::numeric_limits<V>::min() + 1),
        T(std::numeric_limits<V>::min() + 10),
        // Note that we use types with at least 8 bits, so
        // adding/subtracting 100 gives us valid values.
        T(std::numeric_limits<V>::min() + 100),
        T(std::numeric_limits<V>::max() - 100),
        T(std::numeric_limits<V>::max() - 10),
        T(std::numeric_limits<V>::max() - 1),
        T(std::numeric_limits<V>::max())}) {
    // Printing `t` should produce the same result as printing `t.value()`.
    std::ostringstream out;
    out << t;
    if constexpr (std::is_same_v<V, int8_t> || std::is_same_v<V, uint8_t>) {
      EXPECT_EQ(absl::StrFormat("%v", static_cast<int>(t.value())), out.str());
    } else {
      EXPECT_EQ(absl::StrFormat("%v", t.value()), out.str());
    }
  }
}

TYPED_TEST(StrongIntTest, AbslStringify) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  T x(93);
  EXPECT_EQ("93", absl::StrCat(x));
  EXPECT_EQ("93", absl::StrFormat("%v", x));
  EXPECT_EQ("93", absl::Substitute("$0", x));

  for (const T t :
       {T(std::numeric_limits<V>::min()), T(std::numeric_limits<V>::min() + 1),
        T(std::numeric_limits<V>::min() + 10),
        // Note that we use types with at least 8 bits, so
        // adding/subtracting 100 gives us valid values.
        T(std::numeric_limits<V>::min() + 100),
        T(std::numeric_limits<V>::max() - 100),
        T(std::numeric_limits<V>::max() - 10),
        T(std::numeric_limits<V>::max() - 1),
        T(std::numeric_limits<V>::max())}) {
    // Printing `t` should produce the same result as printing `t.value()`.
    if constexpr (std::is_same_v<V, int8_t> || std::is_same_v<V, uint8_t>) {
      EXPECT_EQ(absl::StrFormat("%v", static_cast<int>(t.value())),
                absl::StrCat(t));
    } else {
      EXPECT_EQ(absl::StrFormat("%v", t.value()), absl::StrCat(t));
    }
  }
}

TYPED_TEST(StrongIntTest, TestHasher) {
  using T = typename TestFixture::T;
  using Hasher = typename T::Hasher;

  EXPECT_EQ(Hasher()(T(0)), Hasher()(T(0)));
  EXPECT_NE(Hasher()(T(1)), Hasher()(T(2)));
}

TYPED_TEST(StrongIntTest, TestHashFunctor) {
  using T = typename TestFixture::T;
  using Hasher = typename T::Hasher;

  absl::node_hash_map<T, char, Hasher> map;
  T a(0);
  map[a] = 'c';
  EXPECT_EQ('c', map[a]);
  map[++a] = 'o';
  EXPECT_EQ('o', map[a]);
}

TYPED_TEST(StrongIntTest, TestHash) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      T(std::numeric_limits<V>::min()),
      T(std::numeric_limits<V>::min() + 1),
      T(std::numeric_limits<V>::min() + 10),
      // Note that we use types with at least 8 bits, so adding/subtracting 100
      // gives us valid values.
      T(std::numeric_limits<V>::min() + 100),
      T(std::numeric_limits<V>::max() - 100),
      T(std::numeric_limits<V>::max() - 10),
      T(std::numeric_limits<V>::max() - 1),
      T(std::numeric_limits<V>::max()),
  }));
}

TYPED_TEST(StrongIntTest, TestStrongIntRange) {
  using T = typename TestFixture::T;

  const int64_t kMaxOuterIterations = 100;
  for (int64_t to = 0; to < kMaxOuterIterations; ++to) {
    int count = 0;
    absl::uint128 sum = 0;
    for (const auto x : MakeStrongIntRange(T(to))) {
      ++count;
      sum += x.value();
    }
    EXPECT_EQ(to, count);
    EXPECT_EQ(to * (to - 1) / 2, sum);
  }
  for (int64_t to = 0; to < kMaxOuterIterations; ++to) {
    for (int64_t from = 0; from <= to; ++from) {
      int count = 0;
      absl::uint128 sum = 0;
      for (const auto x : MakeStrongIntRange(T(from), T(to))) {
        ++count;
        sum += x.value();
      }
      EXPECT_EQ(to - from, count);
      EXPECT_EQ((to * (to - 1) / 2) - (from * (from - 1) / 2), sum);
    }
  }
}

// Test Min() and Max() can be used in constexpr.
TYPED_TEST(StrongIntTest, ConstexprMinMax) {
  using T = typename TestFixture::T;
  using V = typename TestFixture::V;

  constexpr V max = T::Max().value();
  constexpr V min = T::Min().value();
  (void)max;
  (void)min;
}

template <typename Ttest, typename Tbig>
bool ExhaustiveTest() {
  using V = typename Ttest::ValueType;

  Tbig v_min = std::numeric_limits<V>::min();
  Tbig v_max = std::numeric_limits<V>::max();
  for (Tbig lhs = v_min; lhs <= v_max; ++lhs) {
    for (Tbig rhs = v_min; rhs <= v_max; ++rhs) {
      {
        Ttest t_lhs(lhs);
        Ttest t_rhs(rhs);
        EXPECT_EQ(Ttest(lhs + rhs), t_lhs + t_rhs);
      }
      {
        Ttest t_lhs(lhs);
        Ttest t_rhs(rhs);
        EXPECT_EQ(Ttest(lhs - rhs), t_lhs - t_rhs);
      }
      {
        Ttest t_lhs(lhs);
        EXPECT_EQ(Ttest(lhs * rhs), t_lhs * rhs);
      }
      {
        Ttest t_lhs(lhs);
        if (rhs != 0) {
          EXPECT_EQ(Ttest(lhs / rhs), t_lhs / rhs);
        }
      }
      {
        Ttest t_lhs(lhs);
        if (rhs != 0) {
          EXPECT_EQ(Ttest(lhs % rhs), t_lhs % rhs);
        }
      }
    }
  }
  return true;
}

TEST(StrongIntTest, Exhaustive) {
  EXPECT_TRUE((ExhaustiveTest<StrongInt8, int>()));
  EXPECT_TRUE((ExhaustiveTest<StrongUInt8, int>()));
}

TEST(StrongIntTest, ExplicitCasting) {
  StrongInt8 x(8);
  EXPECT_THAT(static_cast<int8_t>(x), Eq(x.value()));
  EXPECT_THAT(static_cast<size_t>(x), Eq(x.value()));
}

// Create some types outside the util_intops:: namespace to prove that
// conversions work.
namespace other_namespace {

DEFINE_STRONG_INT_TYPE(Inches, int64_t);
DEFINE_STRONG_INT_TYPE(Feet, int64_t);
DEFINE_STRONG_INT_TYPE(Centimeters, int32_t);

constexpr Feet StrongIntConvert(const Inches& arg, Feet* /* unused */) {
  return Feet(arg.value() / 12);
}
constexpr Centimeters StrongIntConvert(const Inches& arg,
                                       Centimeters* /* unused */) {
  return Centimeters(arg.value() * 2.54);
}

TEST(StrongIntTest, TestConversion) {
  {  // Test simple copy construction.
    Inches in1(12);
    Inches in2(in1);
    EXPECT_EQ(12, in2.value());
  }
  {  // Test conversion from Inches to Feet.
    Inches in(60);
    Feet ft(in);
    EXPECT_EQ(5, ft.value());

    constexpr Inches kIn(60);
    constexpr Feet kFt(kIn);
    EXPECT_EQ(kFt, ft);
  }
  {  // Test conversion from Inches to Centimeters.
    Inches in(10);
    Centimeters cm(in);
    EXPECT_EQ(25, cm.value());

    constexpr Inches kIn(10);
    constexpr Centimeters kCm(kIn);
    EXPECT_EQ(kCm, cm);
  }
}

// Test SFINAE on template<T> constexpr StrongInt(T init_value) constructor.
// Without it, the non-convertible case in the assertions below would become a
// hard compilation failure because of the compile-time evaluation of
// static_cast<ValueType>(init_value) in the _constexpr_ constructor body.
template <typename T>
struct StrongIntTestHelper {
  template <typename U, typename = typename std::enable_if<
                            std::is_constructible<StrongInt<void, T>, U>::value,
                            void>::type>
  StrongIntTestHelper(U /*x*/) {}  // NOLINT
};

static_assert(!std::is_convertible<void, StrongIntTestHelper<int>>::value, "");
static_assert(std::is_convertible<int, StrongIntTestHelper<int>>::value, "");

// Test the IsStrongInt type trait.
static_assert(IsStrongInt<StrongInt8>::value, "");
static_assert(IsStrongInt<StrongUInt16>::value, "");
static_assert(!IsStrongInt<int8_t>::value, "");
static_assert(!IsStrongInt<long>::value, "");  // NOLINT
static_assert(!IsStrongInt<void>::value, "");

// Test we can define our own AbslStringify.
DEFINE_STRONG_INT_TYPE(CustomPrint, uint32_t);

template <typename Sink>
void AbslStringify(Sink& sink, CustomPrint arg) {
  absl::Format(&sink, "CustomPrint(%v)", arg.value());
}

TEST(StrongIntTest, CustomPrint) {
  CustomPrint val(42);
  EXPECT_EQ(absl::StrCat(val), "CustomPrint(42)");
}

}  // namespace other_namespace

}  // namespace
}  // namespace util_intops
