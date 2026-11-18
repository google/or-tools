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

#include "ortools/flatzinc/parser.h"

#include <array>
#include <string>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/flatzinc/model.h"

namespace operations_research::fz {
namespace {

using ::testing::DoubleEq;
using ::testing::ElementsAre;

TEST(ParserTest, VariableDeclaration) {
  const std::string kDeclaration =
      "var 2..10: a :: output_var;\n"
      "solve satisfy;\n";
  Model model("VariableDeclaration");
  ASSERT_TRUE(ParseFlatzincString(kDeclaration, &model));
  ASSERT_EQ(model.variables().size(), 1);
  Variable* const var = model.variables().back();
  const Domain& domain = var->domain;
  EXPECT_TRUE(domain.is_interval);
  EXPECT_THAT(domain.values, ElementsAre(2, 10));
  EXPECT_EQ(var->name, "a");
  EXPECT_FALSE(var->temporary);
  ASSERT_EQ(model.output().size(), 1);
  EXPECT_EQ(model.output()[0].name, "a");
}

TEST(ParserTest, VariableDeclarationFromInteger) {
  const std::string kDeclaration =
      "var 2..10: a :: output_var = 3;\n"
      "solve satisfy;\n";
  Model model("VariableDeclarationFromInteger");
  ASSERT_TRUE(ParseFlatzincString(kDeclaration, &model));
  ASSERT_EQ(model.variables().size(), 1);
  Variable* const var = model.variables().back();
  EXPECT_FALSE(var->domain.is_interval);
  EXPECT_THAT(var->domain.values, ElementsAre(3));
  EXPECT_EQ(var->name, "a");
}

TEST(ParserTest, VariableDeclarationFromNamedConstantAndNestedArrays) {
  const std::string kDeclaration =
      "int: base = 5;\n"
      "array [1..5] of int: a = [11, 12, base, 14, 15];\n"
      "array [1..3] of int: b = [a[1], a[2], a[3]];\n"
      "int: c = b[3];\n"
      "var int: x :: output_var = c;\n"
      "solve satisfy;\n";
  Model model("NestedConstants");
  ASSERT_TRUE(ParseFlatzincString(kDeclaration, &model));
  ASSERT_EQ(model.variables().size(), 1);
  Variable* const var = model.variables().back();
  EXPECT_FALSE(var->domain.is_interval);
  EXPECT_THAT(var->domain.values, ElementsAre(5));
  EXPECT_EQ(var->name, "x");
}

TEST(ParserTest, VariableMergingAndIntroducedFlags) {
  const std::string kInput =
      "var 1..10: X_INTRODUCED_0_;\n"
      "var 5..20: x :: output_var = X_INTRODUCED_0_;\n"
      "var bool: y :: var_is_introduced;\n"
      "solve satisfy;\n";
  Model model("Merging");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.variables().size(), 2);

  Variable* const merged = model.variables()[0];
  EXPECT_EQ(merged->name, "x");
  EXPECT_FALSE(merged->temporary);
  EXPECT_TRUE(merged->domain.is_interval);
  EXPECT_THAT(merged->domain.values, ElementsAre(5, 10));

  Variable* const intro = model.variables()[1];
  EXPECT_EQ(intro->name, "y");
  EXPECT_TRUE(intro->temporary);
  EXPECT_TRUE(intro->domain.display_as_boolean);
}

TEST(ParserTest, PredicatesAndMultiDimensionalOutputArray) {
  const std::string kInput =
      "predicate table_int(array [int, int] of int: t, "
      "array [1..2] of var int: x);\n"
      "predicate zero_arg();\n"
      "var 0..1: x0;\n"
      "var 0..1: x1;\n"
      "array [1..2] of var bool: grid :: output_array([0..0, 1..2]) = "
      "[x0, true];\n"
      "array [1..0] of int: empty_ints = [];\n"
      "array [1..0] of float: empty_floats = [];\n"
      "array [1..0] of set of int: empty_sets = [];\n"
      "array [1..0] of var int: empty_vars = [];\n"
      "solve satisfy;\n";
  Model model("PredicatesAndArrays");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.output().size(), 1);
  const SolutionOutputSpecs& out = model.output()[0];
  EXPECT_EQ(out.name, "grid");
  EXPECT_TRUE(out.display_as_boolean);
  ASSERT_EQ(out.bounds.size(), 2);
  EXPECT_EQ(out.bounds[0].min_value, 0);
  EXPECT_EQ(out.bounds[0].max_value, 0);
  EXPECT_EQ(out.bounds[1].min_value, 1);
  EXPECT_EQ(out.bounds[1].max_value, 2);
  EXPECT_EQ(out.flat_variables.size(), 2);
}

TEST(ParserTest, FloatConstantsArraysAndArguments) {
  const std::string kInput =
      "float: c = 1.5;\n"
      "array [1..2] of -2.0..2.0: coeffs = [c, -0.5];\n"
      "var -1.0..3.0: x = coeffs[1];\n"
      "var float: y;\n"
      "constraint float_lin_le(coeffs, [x, y], coeffs[2]) "
      ":: defines_var(y) :: domain;\n"
      "solve minimize 2.5;\n";
  Model model("Floats");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.constraints().size(), 1);
  const Constraint* const ct = model.constraints()[0];
  EXPECT_EQ(ct->type, "float_lin_le");
  EXPECT_TRUE(ct->strong_propagation);
  ASSERT_EQ(ct->arguments.size(), 3);
  EXPECT_EQ(ct->arguments[0].type, Argument::FLOAT_LIST);
  EXPECT_THAT(ct->arguments[0].floats,
              ElementsAre(DoubleEq(1.5), DoubleEq(-0.5)));
  EXPECT_EQ(ct->arguments[1].type, Argument::VAR_REF_ARRAY);
  EXPECT_EQ(ct->arguments[2].type, Argument::FLOAT_VALUE);
  EXPECT_THAT(ct->arguments[2].floats, ElementsAre(DoubleEq(-0.5)));

  ASSERT_NE(model.objective(), nullptr);
  EXPECT_FALSE(model.maximize());
  EXPECT_TRUE(model.objective()->domain.is_float);
  EXPECT_THAT(model.objective()->domain.float_values,
              ElementsAre(DoubleEq(2.5)));
}

TEST(ParserTest, SetConstantsEmptySetAndHexOctalLiterals) {
  const std::string kInput =
      "set of int: single = {0x10};\n"
      "set of int: empty_s = {};\n"
      "array [1..2] of set of 0..32: s_arr = [single, 0o10..0o12];\n"
      "var set of {0, 16}: sv1 = single;\n"
      "var set of 1..10: sv2 = {};\n"
      "var set of 1..10: sv3;\n"
      "constraint set_eq({}, sv3) :: symmetry_breaking :: redundant;\n"
      "constraint array_set_element(1, s_arr, sv1);\n"
      "constraint set_in(8, s_arr[2]);\n"
      "solve maximize 0x2a;\n";
  Model model("SetsAndRadix");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.constraints().size(), 3);

  const Constraint* const eq_ct = model.constraints()[0];
  EXPECT_EQ(eq_ct->type, "set_eq");
  EXPECT_TRUE(eq_ct->is_symmetric_breaking);
  EXPECT_TRUE(eq_ct->is_redundant);
  ASSERT_EQ(eq_ct->arguments.size(), 2);
  EXPECT_EQ(eq_ct->arguments[0].type, Argument::INT_LIST);
  EXPECT_TRUE(eq_ct->arguments[0].values.empty());

  const Constraint* const in_ct = model.constraints()[2];
  ASSERT_EQ(in_ct->arguments.size(), 2);
  EXPECT_EQ(in_ct->arguments[1].type, Argument::INT_INTERVAL);
  EXPECT_THAT(in_ct->arguments[1].values, ElementsAre(8, 10));

  ASSERT_NE(model.objective(), nullptr);
  EXPECT_TRUE(model.maximize());
  EXPECT_EQ(model.objective()->domain.Value(), 42);
}

TEST(ParserTest, AnnotationsStringsAndSearchSpecs) {
  const std::string kInput =
      "int: k = 2;\n"
      "array [1..2] of var 1..5: xs;\n"
      "constraint all_different_int(xs) :: note(\"hello \\\"world\\\"\");\n"
      "solve :: seq_search([\n"
      "  int_search(xs, first_fail, indomain_min, complete),\n"
      "  int_search([xs[1], xs[2]], input_order, indomain_max, complete)\n"
      "]) :: custom_flag() :: label(\"run_1\") minimize xs[k];\n";
  Model model("Annotations");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.search_annotations().size(), 3);
  EXPECT_EQ(model.search_annotations()[0].type, Annotation::FUNCTION_CALL);
  EXPECT_EQ(model.search_annotations()[0].id, "seq_search");
  EXPECT_EQ(model.search_annotations()[1].type, Annotation::FUNCTION_CALL);
  EXPECT_EQ(model.search_annotations()[1].id, "custom_flag");
  EXPECT_TRUE(model.search_annotations()[1].annotations.empty());
  EXPECT_EQ(model.search_annotations()[2].DebugString(), "label(\"run_1\")");
  ASSERT_NE(model.objective(), nullptr);
  EXPECT_EQ(model.objective()->name, "xs[2]");
}

TEST(ParserTest, MalformedInputsReturnFalseGracefully) {
  static constexpr auto kBadInputs = std::to_array<absl::string_view>({
      "",
      // Missing semicolon.
      "var int: x\nsolve satisfy;",
      // Trailing tokens after solve.
      "solve satisfy; var int: x;",
      // Non-1-based array declaration.
      "array [0..2] of int: a = [1, 2, 3]; solve satisfy;",
      // Array size mismatch.
      "array [1..3] of int: a = [1, 2]; solve satisfy;",
      // 1-based index out of bounds (0 and n+1).
      "array [1..2] of int: a = [10, 20];\n"
      "int: b = a[0];\n"
      "solve satisfy;",
      "array [1..2] of int: a = [10, 20];\n"
      "int: b = a[3];\n"
      "solve satisfy;",
      // Unknown identifier in declaration and constraint.
      "var int: x = missing; solve satisfy;",
      "var int: x; constraint int_eq(x, missing); solve satisfy;",
      // Value outside declared domain.
      "int: c = 5; var 1..3: x = c; solve satisfy;",
      "1..3: c = 9; solve satisfy;",
      // Incompatible variable alias (int aliased to float).
      "var float: f;\n"
      "var int: i = f;\n"
      "solve satisfy;",
      // Non-scalar objective.
      "array [1..2] of var 1..3: xs;\n"
      "solve minimize xs;",
      // Integer literal overflow.
      "int: x = 99999999999999999999999999999; solve satisfy;",
      // Excessive array upper bound (guards against OOM / std::length_error).
      "array [1..9223372036854775807] of var int: x; solve satisfy;",
      "array [1..30000001] of var int: x; solve satisfy;",
  });
  for (absl::string_view bad : kBadInputs) {
    Model model("BadInput");
    EXPECT_FALSE(ParseFlatzincString(bad, &model)) << "Accepted bad input:\n"
                                                   << bad;
  }
}

TEST(ParserTest, DeeplyNestedAnnotationsReturnFalseGracefully) {
  std::string nested_brackets = "solve :: ";
  nested_brackets.append(200, '[');
  nested_brackets.append(200, ']');
  nested_brackets.append(" satisfy;");
  Model brackets_model("NestedBrackets");
  EXPECT_FALSE(ParseFlatzincString(nested_brackets, &brackets_model));

  std::string nested_calls = "solve :: ";
  for (int i = 0; i < 200; ++i) nested_calls.append("f(");
  nested_calls.append("1");
  nested_calls.append(200, ')');
  nested_calls.append(" satisfy;");
  Model calls_model("NestedCalls");
  EXPECT_FALSE(ParseFlatzincString(nested_calls, &calls_model));
}

TEST(ParserTest, AllPredicateParameterTypesFromSpec) {
  const std::string kInput =
      "predicate p_par(bool: a, int: b, float: c, set of int: d,\n"
      "  0.0..1.0: e, 1..5: f, {1, 3, 5}: g,\n"
      "  set of 1..5: h, set of {2, 4}: i,\n"
      "  array [1..2] of 0.0..1.0: j, array [int] of 1..5: k,\n"
      "  array [int] of set of 1..5: l, array [int] of set of {1, 2}: m);\n"
      "predicate p_var(var bool: a, var float: b, var 0.0..1.0: c,\n"
      "  var int: d, var 1..5: e, var {1, 3}: f,\n"
      "  var set of 1..5: g, var set of {1, 2}: h, var set of int: i,\n"
      "  array [int] of var bool: j, array [1..2] of var float: k,\n"
      "  array [int] of var 0.0..1.0: l, array [int] of var int: m,\n"
      "  array [int] of var 1..5: n, array [int] of var set of 1..5: o,\n"
      "  array [int] of var set of {1, 2}: p,\n"
      "  array [int] of var set of int: q);\n"
      "solve satisfy;\n";
  Model model("AllPredicates");
  EXPECT_TRUE(ParseFlatzincString(kInput, &model));
}

TEST(ParserTest, AllParameterAndVariableTypesFromSpec) {
  const std::string kInput =
      "bool: pb = true;\n"
      "int: pi = -42;\n"
      "float: pf = 3.14;\n"
      "set of int: ps = 1..10;\n"
      "1..3: pr = 2;\n"
      "0.0..1.0: prf = 0.5;\n"
      "{1, 3}: pds = 1;\n"
      "array [1..2] of bool: pba = [pb, false];\n"
      "array [1..2] of int: pia = [pi, 1];\n"
      "array [1..2] of float: pfa = [pf, 1.0];\n"
      "array [1..2] of set of int: psa = [ps, {1, 2}];\n"
      "var bool: vb :: output_var;\n"
      "var float: vf;\n"
      "var 0.0..1.0: vrf;\n"
      "var int: vi :: output_var;\n"
      "var 1..5: vr;\n"
      "var {2, 4, 6}: vds;\n"
      "var set of 1..3: vsr;\n"
      "var set of {1, 3, 5}: vsds;\n"
      "var int: vdef :: is_defined_var;\n"
      "constraint int_plus(vi, vr, vdef) :: defines_var(vdef);\n"
      "solve satisfy;\n";
  Model model("AllTypes");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.output().size(), 2);
  EXPECT_EQ(model.output()[0].name, "vb");
  EXPECT_TRUE(model.output()[0].display_as_boolean);
  EXPECT_EQ(model.output()[1].name, "vi");
  EXPECT_FALSE(model.output()[1].display_as_boolean);
}

TEST(ParserTest, ZeroArgConstraintFloatIntervalAndFloatAnnotations) {
  const std::string kInput =
      "array [1..2] of var 0.0..1.0: xs;\n"
      "constraint noop();\n"
      "constraint in_range(0.0..1.0);\n"
      "solve :: float_search(xs, 0.01, input_order, indomain_split, complete)\n"
      "      :: bounds(0.0..1.0) satisfy;\n";
  Model model("EdgeCases");
  ASSERT_TRUE(ParseFlatzincString(kInput, &model));
  ASSERT_EQ(model.constraints().size(), 2);
  EXPECT_EQ(model.constraints()[0]->type, "noop");
  EXPECT_TRUE(model.constraints()[0]->arguments.empty());
  EXPECT_EQ(model.constraints()[1]->type, "in_range");
  ASSERT_EQ(model.constraints()[1]->arguments.size(), 1);
  EXPECT_EQ(model.constraints()[1]->arguments[0].type,
            Argument::FLOAT_INTERVAL);
  EXPECT_THAT(model.constraints()[1]->arguments[0].floats,
              ElementsAre(DoubleEq(0.0), DoubleEq(1.0)));
  ASSERT_EQ(model.search_annotations().size(), 2);
}

TEST(ParserTest, TopLevelOrderingAndDomainErrorsReturnFalse) {
  static constexpr auto kBadOrderAndDomains = std::to_array<absl::string_view>({
      // Missing solve item.
      "var int: x;",
      // Predicate after variable declaration.
      "var int: x; predicate p(); solve satisfy;",
      // Variable declaration after constraint.
      "var int: x; constraint int_eq(x, 1); var int: y; solve satisfy;",
      // Constraint after solve.
      "var int: x; solve satisfy; constraint int_eq(x, 1);",
      // Multiple solve items.
      "solve satisfy; solve satisfy;",
      // Bool constant outside bool domain.
      "bool: b = 2; solve satisfy;",
      // Float constant outside float domain.
      "0.0..1.0: f = 1.5; solve satisfy;",
      // Non-interval bounds in output_array.
      "array [1..1] of var int: xs :: output_array([1]) = [1]; solve satisfy;",
  });
  for (absl::string_view bad : kBadOrderAndDomains) {
    Model model("BadOrder");
    EXPECT_FALSE(ParseFlatzincString(bad, &model)) << "Accepted bad input:\n"
                                                   << bad;
  }
}

}  // namespace
}  // namespace operations_research::fz
