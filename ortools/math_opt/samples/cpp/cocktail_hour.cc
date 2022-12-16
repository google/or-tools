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

// Pick ingredients to buy to make the maximum number of cocktails.
//
// Given a list of cocktails, each of which is made from a list of ingredients,
// and a budget of how many ingredients you can buy, solve a MIP to pick a
// subset of the ingredients so that you can make the largest number of
// cocktails.
//
// This program can be run in three modes:
//   text: Outputs the optimal set of ingredients and cocktails that can be
//     produced as plain text to standard out.
//   latex: Outputs a menu of the cocktails that can be made as LaTeX code to
//     standard out.
//   analysis: Computes the number of cocktails that can be made as a function
//     of the number of ingredients for all values.
//
// In latex mode, the output can be piped directly to pdflatex, e.g.
//   blaze run -c opt \
//     ortools/math_opt/examples/cpp/cocktail_hour \
//     -- --num_ingredients 10 --mode latex | pdflatex -output-directory /tmp
// will create a PDF in /tmp.
#include <cstddef>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/status_macros.h"

ABSL_FLAG(std::string, mode, "text",
          "One of \"text\", \"latex\", or \"analysis\".");
ABSL_FLAG(int, num_ingredients, 10,
          "How many ingredients to buy (ignored in analysis mode).");
ABSL_FLAG(std::vector<std::string>, existing_ingredients, {},
          "Ingredients you already have (ignored in analysis mode).");
ABSL_FLAG(std::vector<std::string>, unavailable_ingredients, {},
          "Ingredients you cannot get (ignored in analysis mode).");
ABSL_FLAG(std::vector<std::string>, required_cocktails, {},
          "Cocktails you must be able to make (ignored in analysis mode).");
ABSL_FLAG(std::vector<std::string>, blocked_cocktails, {},
          "Cocktails to exclude from the menu (ignored in analysis mode).");

namespace {

namespace math_opt = ::operations_research::math_opt;

constexpr absl::string_view kIngredients[] = {"Amaro Nonino",
                                              "All Spice Dram",
                                              "Aperol",
                                              "Bitters",
                                              "Bourbon",
                                              "Brandy",
                                              "Campari",
                                              "Cinnamon",
                                              "Chambord",
                                              "Cherry",
                                              "Cloves",
                                              "Cointreau",
                                              "Coke",
                                              "Cranberry",
                                              "Creme de Cacao",
                                              "Creme de Violette",
                                              "Cucumber",
                                              "Egg",
                                              "Gin",
                                              "Green Chartreuse",
                                              "Heavy Cream",
                                              "Lemon",
                                              "Lillet Blanc",
                                              "Lime",
                                              "Luxardo",
                                              "Mint",
                                              "Orange",
                                              "Orange Flower Water Extract",
                                              "Orgeat",
                                              "Pickle",
                                              "Pineapple Juice",
                                              "Pisco",
                                              "Prosecco",
                                              "Raspberry Vodka",
                                              "Ruby Port",
                                              "Rum",
                                              "Seltzer",
                                              "Simple Syrup",
                                              "Sugar",
                                              "Sweet Vermouth",
                                              "Tequila",
                                              "Tonic Water",
                                              "Vodka"};

constexpr std::size_t kIngredientsSize =
    sizeof(kIngredients) / sizeof(kIngredients[0]);

struct Cocktail {
  std::string name;
  std::vector<std::string> ingredients;
};

std::vector<Cocktail> AllCocktails() {
  return {
      // Aperitifs
      {.name = "Prosecco glass", .ingredients = {"Prosecco"}},
      {.name = "Aperol Spritz", .ingredients = {"Prosecco", "Aperol"}},
      {.name = "Chambord Spritz", .ingredients = {"Prosecco", "Chambord"}},
      {.name = "Improved French 75",
       .ingredients = {"Prosecco", "Vodka", "Lemon", "Simple Syrup"}},
      // Quick and Simple
      {.name = "Gin and Tonic", .ingredients = {"Gin", "Tonic Water", "Lime"}},
      {.name = "Rum and Coke", .ingredients = {"Rum", "Coke"}},
      {.name = "Improved Manhattan",
       .ingredients = {"Bourbon", "Sweet Vermouth", "Bitters"}},
      // Vodka

      // Serve with a sugared rim
      {.name = "Lemon Drop",
       .ingredients = {"Vodka", "Cointreau", "Lemon", "Simple Syrup"}},
      // Shake, then float 2oz Prosecco after pouring
      {.name = "Big Crush",
       .ingredients = {"Raspberry Vodka", "Cointreau", "Lemon", "Chambord",
                       "Prosecco"}},
      {.name = "Cosmopolitan",
       .ingredients = {"Vodka", "Cranberry", "Cointreau", "Lime"}},
      // A shot, chase with 1/3 of pickle spear
      {.name = "Vodka/Pickle", .ingredients = {"Vodka", "Pickle"}},

      // Gin
      {.name = "Last Word",
       .ingredients = {"Gin", "Green Chartreuse", "Luxardo", "Lime"}},
      {.name = "Corpse Reviver #2 (Lite)",
       .ingredients = {"Gin", "Cointreau", "Lillet Blanc", "Lemon"}},
      {.name = "Negroni", .ingredients = {"Gin", "Sweet Vermouth", "Campari"}},
      // "Float" Creme de Violette (it will sink)
      {.name = "Aviation",
       .ingredients = {"Gin", "Luxardo", "Lemon", "Creme de Violette"}},

      // Bourbon
      {.name = "Paper Plane",
       .ingredients = {"Bourbon", "Aperol", "Amaro Nonino", "Lemon"}},
      {.name = "Derby",
       .ingredients = {"Bourbon", "Sweet Vermouth", "Lime", "Cointreau"}},
      // Muddle sugar, water, bitters, and orange peel. Garnish with a Luxardo
      // cherry (do not cheap out), spill cherry syrup generously in drink
      {.name = "Old Fashioned",
       .ingredients = {"Bourbon", "Sugar", "Bitters", "Orange", "Cherry"}},
      {.name = "Boulevardier",
       .ingredients = {"Bourbon", "Sweet Vermouth", "Campari"}},

      // Tequila
      {.name = "Margarita", .ingredients = {"Tequila", "Cointreau", "Lime"}},
      // Shake with chopped cucumber and strain. Garnish with cucumber.
      {.name = "Midnight Cruiser",
       .ingredients = {"Tequila", "Aperol", "Lime", "Pineapple Juice",
                       "Cucumber", "Simple Syrup"}},

      {.name = "Tequila shot", .ingredients = {"Tequila"}},
      // Rum

      // Shake with light rum, float a dark rum on top.
      {.name = "Pineapple Mai Tai",
       .ingredients = {"Rum", "Lime", "Orgeat", "Cointreau",
                       "Pineapple Juice"}},
      {.name = "Daiquiri", .ingredients = {"Rum", "Lime", "Simple Syrup"}},
      {.name = "Mojito",
       .ingredients = {"Rum", "Lime", "Simple Syrup", "Mint", "Seltzer"}},
      // Add bitters generously. Invert half lime to form a cup, fill with
      // Green Chartreuse and cloves. Float lime cup on drink and ignite.
      {.name = "Kennedy",
       .ingredients = {"Rum", "All Spice Dram", "Bitters", "Lime",
                       "Simple Syrup", "Cloves", "Green Chartreuse"}},

      // Egg

      {.name = "Pisco Sour",
       .ingredients = {"Pisco", "Lime", "Simple Syrup", "Egg", "Bitters"}},
      {.name = "Viana",
       .ingredients = {"Ruby Port", "Brandy", "Creme de Cacao", "Sugar", "Egg",
                       "Cinnamon"}},
      // Add cream last before shaking (and seltzer after shaking). Shake for 10
      // minutes, no less.
      {.name = "Ramos gin fizz",
       .ingredients = {"Gin", "Seltzer", "Heavy Cream",
                       "Orange Flower Water Extract", "Egg", "Lemon", "Lime",
                       "Simple Syrup"}}};
}

struct Menu {
  std::vector<std::string> ingredients;
  std::vector<Cocktail> cocktails;
};

absl::StatusOr<Menu> SolveForMenu(
    const int max_new_ingredients, const bool enable_solver_output,
    const absl::flat_hash_set<std::string>& existing_ingredients,
    const absl::flat_hash_set<std::string>& unavailable_ingredients,
    const absl::flat_hash_set<std::string>& required_cocktails,
    const absl::flat_hash_set<std::string>& blocked_cocktails) {
  const std::vector<Cocktail> all_cocktails = AllCocktails();
  math_opt::Model model("Cocktail hour");
  absl::flat_hash_map<std::string, math_opt::Variable> ingredient_vars;
  for (const absl::string_view ingredient : kIngredients) {
    const double lb = existing_ingredients.contains(ingredient) ? 1.0 : 0.0;
    const double ub = unavailable_ingredients.contains(ingredient) ? 0.0 : 1.0;
    const math_opt::Variable v = model.AddIntegerVariable(lb, ub, ingredient);
    gtl::InsertOrDie(&ingredient_vars, std::string(ingredient), v);
  }
  math_opt::LinearExpression ingredients_used;
  for (const auto& [name, ingredient_var] : ingredient_vars) {
    ingredients_used += ingredient_var;
  }
  model.AddLinearConstraint(ingredients_used <=
                            max_new_ingredients + existing_ingredients.size());

  absl::flat_hash_map<std::string, math_opt::Variable> cocktail_vars;
  for (const Cocktail& cocktail : all_cocktails) {
    const double lb = required_cocktails.contains(cocktail.name) ? 1.0 : 0.0;
    const double ub = blocked_cocktails.contains(cocktail.name) ? 0.0 : 1.0;
    const math_opt::Variable v =
        model.AddIntegerVariable(lb, ub, cocktail.name);
    for (const std::string& ingredient : cocktail.ingredients) {
      model.AddLinearConstraint(v <=
                                gtl::FindOrDie(ingredient_vars, ingredient));
    }
    gtl::InsertOrDie(&cocktail_vars, cocktail.name, v);
  }
  math_opt::LinearExpression cocktails_made;
  for (const auto& [name, cocktail_var] : cocktail_vars) {
    cocktails_made += cocktail_var;
  }
  model.Maximize(cocktails_made);
  const math_opt::SolveArguments args = {
      .parameters = {.enable_output = enable_solver_output}};
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   math_opt::Solve(model, math_opt::SolverType::kGscip, args));

  RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());
  Menu menu;
  for (const absl::string_view ingredient : kIngredients) {
    if (result.variable_values().at(ingredient_vars.at(ingredient)) > 0.5) {
      menu.ingredients.push_back(std::string(ingredient));
    }
  }
  for (const Cocktail& cocktail : all_cocktails) {
    if (result.variable_values().at(cocktail_vars.at(cocktail.name)) > 0.5) {
      menu.cocktails.push_back(cocktail);
    }
  }
  return menu;
}

absl::flat_hash_set<std::string> SetFromVec(absl::Span<const std::string> vec) {
  return {vec.begin(), vec.end()};
}

absl::Status AnalysisMode() {
  std::cout << "Considering " << AllCocktails().size() << " cocktails and "
            << kIngredientsSize << " ingredients." << std::endl;
  std::cout << "Solving for number of cocktails that can be made as a function "
               "of number of ingredients"
            << std::endl;

  std::cout << "ingredients | cocktails" << std::endl;
  for (int i = 1; i <= kIngredientsSize; ++i) {
    const absl::StatusOr<Menu> menu = SolveForMenu(
        i, false, /*existing_ingredients=*/{}, /*unavailable_ingredients=*/{},
        /*required_cocktails=*/{}, /*blocked_cocktails=*/{});
    RETURN_IF_ERROR(menu.status())
        << "Failure when solving for " << i << " ingredients";
    std::cout << i << " | " << menu->cocktails.size() << std::endl;
  }
  return absl::OkStatus();
}

std::string ExportToLaTeX(absl::Span<const Cocktail> cocktails,
                          absl::string_view title = "Cocktail Hour") {
  std::vector<std::string> lines;
  lines.push_back("\\documentclass{article}");
  lines.push_back("\\usepackage{fullpage}");
  lines.push_back("\\linespread{2}");
  lines.push_back("\\begin{document}");
  lines.push_back("\\begin{center}");
  lines.push_back(absl::StrCat("\\begin{Huge}", title, "\\end{Huge}"));
  lines.push_back("");
  for (const Cocktail& cocktail : cocktails) {
    lines.push_back(absl::StrCat(cocktail.name, "---{\\em ",
                                 absl::StrJoin(cocktail.ingredients, ", "),
                                 "}"));
    lines.push_back("");
  }
  lines.push_back("\\end{center}");
  lines.push_back("\\end{document}");

  return absl::StrReplaceAll(absl::StrJoin(lines, "\n"), {{"#", "\\#"}});
}

absl::Status Main() {
  const std::string mode = absl::GetFlag(FLAGS_mode);
  CHECK(absl::flat_hash_set<std::string>({"text", "latex", "analysis"})
            .contains(mode))
      << "Unexpected mode: " << mode;

  // We are in analysis mode.
  if (mode == "analysis") {
    return AnalysisMode();
  }

  OR_ASSIGN_OR_RETURN3(
      Menu menu,
      SolveForMenu(absl::GetFlag(FLAGS_num_ingredients), mode == "text",
                   SetFromVec(absl::GetFlag(FLAGS_existing_ingredients)),
                   SetFromVec(absl::GetFlag(FLAGS_unavailable_ingredients)),
                   SetFromVec(absl::GetFlag(FLAGS_required_cocktails)),
                   SetFromVec(absl::GetFlag(FLAGS_blocked_cocktails))),
      _ << "error when solving for optimal set of ingredients");

  // We are in latex mode.
  if (mode == "latex") {
    std::cout << ExportToLaTeX(menu.cocktails) << std::endl;
    return absl::OkStatus();
  }

  // We are in text mode
  std::cout << "Considered " << AllCocktails().size() << " cocktails and "
            << kIngredientsSize << " ingredients." << std::endl;
  std::cout << "Solution has " << menu.ingredients.size()
            << " ingredients to make " << menu.cocktails.size() << " cocktails."
            << std::endl
            << std::endl;

  std::cout << "Ingredients:" << std::endl;
  for (const std::string& ingredient : menu.ingredients) {
    std::cout << "  " << ingredient << std::endl;
  }
  std::cout << "Cocktails:" << std::endl;
  for (const Cocktail& cocktail : menu.cocktails) {
    std::cout << "  " << cocktail.name << std::endl;
  }
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
