// Copyright 2010-2021 Google LLC
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

// Solve the Slitherlink problem:
//    see https://en.wikipedia.org/wiki/Slitherlink

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"

const std::vector<std::vector<int> > tiny = {{3, 3, 1}};

const std::vector<std::vector<int> > small = {
    {3, 2, -1, 3}, {-1, -1, -1, 2}, {3, -1, -1, -1}, {3, -1, 3, 1}};

const std::vector<std::vector<int> > medium = {
    {-1, 0, -1, 1, -1, -1, 1, -1},  {-1, 3, -1, -1, 2, 3, -1, 2},
    {-1, -1, 0, -1, -1, -1, -1, 0}, {-1, 3, -1, -1, 0, -1, -1, -1},
    {-1, -1, -1, 3, -1, -1, 0, -1}, {1, -1, -1, -1, -1, 3, -1, -1},
    {3, -1, 1, 3, -1, -1, 3, -1},   {-1, 0, -1, -1, 3, -1, 3, -1}};

const std::vector<std::vector<int> > big = {
    {3, -1, -1, -1, 2, -1, 1, -1, 1, 2},
    {1, -1, 0, -1, 3, -1, 2, 0, -1, -1},
    {-1, 3, -1, -1, -1, -1, -1, -1, 3, -1},
    {2, 0, -1, 3, -1, 2, 3, -1, -1, -1},
    {-1, -1, -1, 1, 1, 1, -1, -1, 3, 3},
    {2, 3, -1, -1, 2, 2, 3, -1, -1, -1},
    {-1, -1, -1, 1, 2, -1, 2, -1, 3, 3},
    {-1, 2, -1, -1, -1, -1, -1, -1, 2, -1},
    {-1, -1, 1, 1, -1, 2, -1, 1, -1, 3},
    {3, 3, -1, 1, -1, 2, -1, -1, -1, 2}};

namespace operations_research {
namespace sat {

void PrintSolution(const std::vector<std::vector<int> >& data,
                   const std::vector<std::vector<bool> >& h_arcs,
                   const std::vector<std::vector<bool> >& v_arcs) {
  const int num_rows = data.size();
  const int num_columns = data[0].size();

  for (int i = 0; i < num_rows; ++i) {
    std::string first_line;
    std::string second_line;
    std::string third_line;
    for (int j = 0; j < num_columns; ++j) {
      const bool h_arc = h_arcs[i][j];
      const bool v_arc = v_arcs[j][i];
      const int sum = data[i][j];
      first_line += h_arc ? " -----" : "      ";
      second_line += v_arc ? "|" : " ";
      second_line +=
          sum == -1 ? "     " : absl::StrFormat("  %d  ", sum).c_str();
      third_line += v_arc ? "|     " : "      ";
    }
    const bool termination = v_arcs[num_columns][i];
    second_line += termination == 1 ? "|" : " ";
    third_line += termination == 1 ? "|" : " ";
    std::cout << first_line << std::endl;
    std::cout << third_line << std::endl;
    std::cout << second_line << std::endl;
    std::cout << third_line << std::endl;
  }
  std::string last_line;
  for (int j = 0; j < num_columns; ++j) {
    const bool h_arc = h_arcs[num_rows][j];
    last_line += h_arc ? " -----" : "      ";
  }
  std::cout << last_line << std::endl;
}

void SlitherLink(const std::vector<std::vector<int> >& data) {
  const int num_rows = data.size();
  const int num_columns = data[0].size();

  const int num_nodes = (num_rows + 1) * (num_columns + 1);
  const int num_horizontal_arcs = num_columns * (num_rows + 1);
  const int num_vertical_arcs = (num_rows) * (num_columns + 1);

  auto undirected_horizontal_arc = [=](int x, int y) {
    CHECK_LT(x, num_columns);
    CHECK_LT(y, num_rows + 1);
    return x + (num_columns * y);
  };

  auto undirected_vertical_arc = [=](int x, int y) {
    CHECK_LT(x, num_columns + 1);
    CHECK_LT(y, num_rows);
    return x + (num_columns + 1) * y;
  };

  auto node_index = [=](int x, int y) {
    CHECK_LT(x, num_columns + 1);
    CHECK_LT(y, num_rows + 1);
    return x + y * (num_columns + 1);
  };

  CpModelBuilder builder;

  std::vector<BoolVar> horizontal_arcs;
  for (int arc = 0; arc < 2 * num_horizontal_arcs; ++arc) {
    horizontal_arcs.push_back(builder.NewBoolVar());
  }
  std::vector<BoolVar> vertical_arcs;
  for (int arc = 0; arc < 2 * num_vertical_arcs; ++arc) {
    vertical_arcs.push_back(builder.NewBoolVar());
  }

  CircuitConstraint circuit = builder.AddCircuitConstraint();
  // Horizontal arcs.
  for (int x = 0; x < num_columns; ++x) {
    for (int y = 0; y < num_rows + 1; ++y) {
      const int arc = undirected_horizontal_arc(x, y);
      circuit.AddArc(node_index(x, y), node_index(x + 1, y),
                     horizontal_arcs[2 * arc]);
      circuit.AddArc(node_index(x + 1, y), node_index(x, y),
                     horizontal_arcs[2 * arc + 1]);
    }
  }
  // Vertical arcs.
  for (int x = 0; x < num_columns + 1; ++x) {
    for (int y = 0; y < num_rows; ++y) {
      const int arc = undirected_vertical_arc(x, y);
      circuit.AddArc(node_index(x, y), node_index(x, y + 1),
                     vertical_arcs[2 * arc]);
      circuit.AddArc(node_index(x, y + 1), node_index(x, y),
                     vertical_arcs[2 * arc + 1]);
    }
  }
  // Self loops.
  std::vector<BoolVar> self_nodes(num_nodes);
  for (int x = 0; x < num_columns + 1; ++x) {
    for (int y = 0; y < num_rows + 1; ++y) {
      const int node = node_index(x, y);
      const BoolVar self_node = builder.NewBoolVar();
      circuit.AddArc(node, node, self_node);
      self_nodes[node] = self_node;
    }
  }

  for (int x = 0; x < num_columns; ++x) {
    for (int y = 0; y < num_rows; ++y) {
      if (data[y][x] == -1) continue;
      std::vector<BoolVar> neighbors;
      const int top_arc = undirected_horizontal_arc(x, y);
      neighbors.push_back(horizontal_arcs[2 * top_arc]);
      neighbors.push_back(horizontal_arcs[2 * top_arc + 1]);
      const int bottom_arc = undirected_horizontal_arc(x, y + 1);
      neighbors.push_back(horizontal_arcs[2 * bottom_arc]);
      neighbors.push_back(horizontal_arcs[2 * bottom_arc + 1]);
      const int left_arc = undirected_vertical_arc(x, y);
      neighbors.push_back(vertical_arcs[2 * left_arc]);
      neighbors.push_back(vertical_arcs[2 * left_arc + 1]);
      const int right_arc = undirected_vertical_arc(x + 1, y);
      neighbors.push_back(vertical_arcs[2 * right_arc]);
      neighbors.push_back(vertical_arcs[2 * right_arc + 1]);
      builder.AddEquality(LinearExpr::Sum(neighbors), data[y][x]);
    }
  }

  // Special rule on corners: value == 3 implies 2 corner arcs used.
  if (data[0][0] == 3) {
    const int h_arc = undirected_horizontal_arc(0, 0);
    builder.AddBoolOr(
        {horizontal_arcs[2 * h_arc], horizontal_arcs[2 * h_arc + 1]});
    const int v_arc = undirected_vertical_arc(0, 0);
    builder.AddBoolOr({vertical_arcs[2 * v_arc], vertical_arcs[2 * v_arc + 1]});
  }
  if (data[0][num_columns - 1] == 3) {
    const int h_arc = undirected_horizontal_arc(num_columns - 1, 0);
    builder.AddBoolOr(
        {horizontal_arcs[2 * h_arc], horizontal_arcs[2 * h_arc + 1]});
    const int v_arc = undirected_vertical_arc(num_columns, 0);
    builder.AddBoolOr({vertical_arcs[2 * v_arc], vertical_arcs[2 * v_arc + 1]});
  }
  if (data[num_rows - 1][0] == 3) {
    const int h_arc = undirected_horizontal_arc(0, num_rows);
    builder.AddBoolOr(
        {horizontal_arcs[2 * h_arc], horizontal_arcs[2 * h_arc + 1]});
    const int v_arc = undirected_vertical_arc(0, num_rows - 1);
    builder.AddBoolOr({vertical_arcs[2 * v_arc], vertical_arcs[2 * v_arc + 1]});
  }
  if (data[num_rows - 1][num_columns - 1] == 3) {
    const int h_arc = undirected_horizontal_arc(num_columns - 1, num_rows);
    builder.AddBoolOr(
        {horizontal_arcs[2 * h_arc], horizontal_arcs[2 * h_arc + 1]});
    const int v_arc = undirected_vertical_arc(num_columns, num_rows - 1);
    builder.AddBoolOr({vertical_arcs[2 * v_arc], vertical_arcs[2 * v_arc + 1]});
  }

  // Topology rule: Border arcs are oriented in one direction.
  for (int x = 0; x < num_columns; ++x) {
    const int top_arc = undirected_horizontal_arc(x, 0);
    builder.AddEquality(horizontal_arcs[2 * top_arc + 1], 0);
    const int bottom_arc = undirected_horizontal_arc(x, num_rows);
    builder.AddEquality(horizontal_arcs[2 * bottom_arc], 0);
  }

  for (int y = 0; y < num_rows; ++y) {
    const int left_arc = undirected_vertical_arc(0, y);
    builder.AddEquality(vertical_arcs[2 * left_arc], 0);
    const int right_arc = undirected_vertical_arc(num_columns, y);
    builder.AddEquality(vertical_arcs[2 * right_arc + 1], 0);
  }

  const CpSolverResponse response = Solve(builder.Build());

  std::vector<std::vector<bool> > h_arcs(num_rows + 1);
  for (int y = 0; y < num_rows + 1; ++y) {
    for (int x = 0; x < num_columns; ++x) {
      const int arc = undirected_horizontal_arc(x, y);
      h_arcs[y].push_back(
          SolutionBooleanValue(response, horizontal_arcs[2 * arc]) ||
          SolutionBooleanValue(response, horizontal_arcs[2 * arc + 1]));
    }
  }

  std::vector<std::vector<bool> > v_arcs(num_columns + 1);
  for (int y = 0; y < num_rows; ++y) {
    for (int x = 0; x < num_columns + 1; ++x) {
      const int arc = undirected_vertical_arc(x, y);
      v_arcs[x].push_back(
          SolutionBooleanValue(response, vertical_arcs[2 * arc]) ||
          SolutionBooleanValue(response, vertical_arcs[2 * arc + 1]));
    }
  }

  PrintSolution(data, h_arcs, v_arcs);
  LOG(INFO) << CpSolverResponseStats(response);
}

}  // namespace sat
}  // namespace operations_research

int main() {
  std::cout << "Tiny problem" << std::endl;
  operations_research::sat::SlitherLink(tiny);
  std::cout << "Small problem" << std::endl;
  operations_research::sat::SlitherLink(small);
  std::cout << "Medium problem" << std::endl;
  operations_research::sat::SlitherLink(medium);
  std::cout << "Big problem" << std::endl;
  operations_research::sat::SlitherLink(big);
  return EXIT_SUCCESS;
}
