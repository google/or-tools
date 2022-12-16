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

#include "ortools/math_opt/elemental/codegen/gen_python.h"

#include <string>

#include "gtest/gtest.h"
#include "ortools/math_opt/elemental/codegen/testing.h"

namespace operations_research::math_opt::codegen {
namespace {

TEST(GenPythonEnumsTest, EmitElements) {
  std::string code;
  PythonEnums()->EmitElements({"some_name", "other_name"}, &code);
  EXPECT_EQ(code,
            R"(class ElementType(enum.Enum):
  SOME_NAME = 0
  OTHER_NAME = 1

)");
}

TEST(GenPythonEnumsTest, EmitAttributes) {
  std::string code;
  PythonEnums()->EmitAttributes({GetTestDescriptor()}, &code);
  EXPECT_EQ(code,
            R"(
AttrValueType = TypeVar('AttrValueType', np.float64)

AttrPyValueType = TypeVar('AttrPyValueType', float)

class Attr(Generic[AttrValueType]):
  pass

class PyAttr(Generic[AttrPyValueType]):
  pass

class TestAttr2(Attr[np.float64], PyAttr[float], int, enum.Enum):
  A_NAME = 0
  B_NAME = 1

AnyAttr = Union[TestAttr2]
)");
}

}  // namespace

}  // namespace operations_research::math_opt::codegen
