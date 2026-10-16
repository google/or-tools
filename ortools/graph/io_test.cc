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

#include "ortools/graph/io.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/options.h"
#include "ortools/base/path.h"
#include "ortools/base/strong_int.h"
#include "ortools/graph/graph.h"

namespace util {
namespace {

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::HasSubstr;
using testing::UnorderedElementsAre;

typedef StaticGraph<> SomeGraph;

TEST(GraphToStringTest, EmptyGraph) {
  const auto graph = SomeGraph::Builder(0, 0).Build(nullptr);
  EXPECT_EQ("", GraphToString(*graph, PRINT_GRAPH_ARCS));
  EXPECT_EQ("", GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS));
  EXPECT_EQ("", GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
  EXPECT_EQ(R"(digraph {
}
)",
            GraphToString(*graph, PRINT_GRAPH_DOT));
}

TEST(GraphToStringTest, SmallGraphWithMultipleAndSelfArcs) {
  const auto graph = GraphFromArcs<StaticGraph<>>(6, {{1, 3},
                                                      {3, 1},
                                                      {3, 1},
                                                      {2, 4},
                                                      {5, 5},
                                                      {5, 5},
                                                      {1, 3},
                                                      {5, 5},
                                                      {5, 2},
                                                      {5, 0}});
  EXPECT_EQ("1->3\n1->3\n2->4\n3->1\n3->1\n5->5\n5->5\n5->5\n5->2\n5->0",
            GraphToString(graph, PRINT_GRAPH_ARCS));
  EXPECT_EQ("0: \n1: 3 3\n2: 4\n3: 1 1\n4: \n5: 5 5 5 2 0",
            GraphToString(graph, PRINT_GRAPH_ADJACENCY_LISTS));
  EXPECT_EQ("0: \n1: 3 3\n2: 4\n3: 1 1\n4: \n5: 0 2 5 5 5",
            GraphToString(graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
  EXPECT_EQ(R"(digraph {
  1->3;
  1->3;
  2->4;
  3->1;
  3->1;
  5->5;
  5->5;
  5->5;
  5->2;
  5->0;
}
)",
            GraphToString(graph, PRINT_GRAPH_DOT));
}

DEFINE_STRONG_INT_TYPE(NodeIndex, int16_t);
DEFINE_STRONG_INT_TYPE(ArcIndex, int32_t);

TEST(GraphToStringTest, TypedIndices) {
  StaticGraph<NodeIndex, ArcIndex>::Builder builder(NodeIndex(2), ArcIndex(1));
  builder.AddArc(NodeIndex(0), NodeIndex(1));
  const auto graph = std::move(builder).Build(nullptr);
  EXPECT_EQ("0->1", GraphToString(*graph, PRINT_GRAPH_ARCS));
  EXPECT_EQ("0: 1\n1: ", GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS));
  EXPECT_EQ("0: 1\n1: ",
            GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
  EXPECT_EQ(R"(digraph {
  0->1;
}
)",
            GraphToString(*graph, PRINT_GRAPH_DOT));
}

class ReadGraphFileTest : public ::testing::Test {
 protected:
  std::string GraphFilename() const {
    return file::JoinPath(testing::TempDir(), "graph.g");
  }

  void SetUp() override {
    const char kGraph[] =
        "5 4 3 1 3\n"
        "1 2\n"
        "2 1\n"
        "2 0\n"
        "1 1";
    ASSERT_OK(file::SetContents(GraphFilename(), kGraph, file::Defaults()));
    num_nodes_with_color_ = {12343, -324, -1};  // Some junk.
  }

  std::string ExpectReadGraphFileFailureAndGetErrorMessage(
      absl::string_view graph_file_contents) {
    if (!file::SetContents(GraphFilename(), graph_file_contents,
                           file::Defaults())
             .ok()) {
      ADD_FAILURE() << "Unit test internal failure: Could not write graph to '"
                    << GraphFilename() << "'.";
      return "";
    }
    absl::StatusOr<SomeGraph*> error_or_graph = ReadGraphFile<SomeGraph>(
        this->GraphFilename(), /*directed=*/false, &num_nodes_with_color_);
    if (error_or_graph.ok()) {
      ADD_FAILURE() << "Expected failure when reading graph file:\n"
                    << graph_file_contents;
      delete error_or_graph.value();
      return "";
    }
    return std::string(error_or_graph.status().message());
  }

  std::vector<int> num_nodes_with_color_;
};

template <typename GraphType>
class ReadGraphFileTypedTest : public ReadGraphFileTest {};

typedef ::testing::Types<
    ListGraph<int16_t, int16_t>, ListGraph<int16_t, int32_t>,
    ListGraph<int32_t, int32_t>, ListGraph<uint32_t, uint32_t>,
    ReverseArcListGraph<int16_t, int32_t>,
    ReverseArcListGraph<int16_t, int32_t>,
    ReverseArcListGraph<int32_t, int32_t>,
    ReverseArcListGraph<uint32_t, int32_t>,
    ReverseArcStaticGraph<int16_t, int32_t>,
    ReverseArcStaticGraph<int16_t, int32_t>,
    ReverseArcStaticGraph<int32_t, int32_t>,
    ReverseArcStaticGraph<uint32_t, int32_t>, StaticGraph<int16_t, int32_t>,
    StaticGraph<int16_t, int32_t>, StaticGraph<int32_t, int32_t>,
    StaticGraph<uint32_t, uint32_t>>
    TestedGraphTypes;

TYPED_TEST_SUITE(ReadGraphFileTypedTest, TestedGraphTypes);

TYPED_TEST(ReadGraphFileTypedTest, Directed) {
  typedef TypeParam Graph;
  absl::StatusOr<Graph*> error_or_graph =
      ReadGraphFile<Graph>(this->GraphFilename(), /*directed=*/true,
                           /*num_nodes_with_color_or_null=*/nullptr);
  ASSERT_OK(error_or_graph.status());
  std::unique_ptr<Graph> graph(error_or_graph.value());
  ASSERT_EQ(5, graph->num_nodes());
  ASSERT_EQ(4, graph->num_arcs());
  EXPECT_EQ(
      "0: \n"
      "1: 1 2\n"
      "2: 0 1\n"
      "3: \n"
      "4: ",
      GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
}

TYPED_TEST(ReadGraphFileTypedTest, NonDirectedWithColors) {
  typedef TypeParam Graph;
  absl::StatusOr<Graph*> error_or_graph = ReadGraphFile<Graph>(
      this->GraphFilename(), /*directed=*/false, &this->num_nodes_with_color_);
  ASSERT_OK(error_or_graph.status());
  std::unique_ptr<Graph> graph(error_or_graph.value());
  ASSERT_EQ(5, graph->num_nodes());
  ASSERT_EQ(7, graph->num_arcs());
  EXPECT_EQ(
      "0: 2\n"
      "1: 1 2 2\n"
      "2: 0 1 1\n"
      "3: \n"
      "4: ",
      GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
  EXPECT_THAT(this->num_nodes_with_color_, ElementsAre(1, 3, 1));
}

TEST_F(ReadGraphFileTest, EmptyGraph) {
  ASSERT_OK(file::SetContents(GraphFilename(), "0 0", file::Defaults()));
  for (const bool directed : {false, true}) {
    absl::StatusOr<SomeGraph*> error_or_graph = ReadGraphFile<SomeGraph>(
        GraphFilename(), directed, &num_nodes_with_color_);
    SCOPED_TRACE(absl::StrFormat("Directed = %s", directed ? "true" : "false"));
    ASSERT_OK(error_or_graph.status());
    std::unique_ptr<SomeGraph> graph(error_or_graph.value());
    EXPECT_EQ(0, graph->num_nodes());
    EXPECT_EQ(0, graph->num_arcs());
    EXPECT_THAT(num_nodes_with_color_, ElementsAre(0));
  }
}

TEST_F(ReadGraphFileTest, InexistentFile) {
  absl::StatusOr<SomeGraph*> error_or_graph = ReadGraphFile<SomeGraph>(
      "Some unknown file path", /*directed=*/false, &num_nodes_with_color_);
  ASSERT_FALSE(error_or_graph.ok());
  EXPECT_THAT(error_or_graph.status().message(),
              HasSubstr("Unknown or empty file"));
}

TEST_F(ReadGraphFileTest, EmptyFile) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage(""),
              HasSubstr("Unknown or empty file"));
}

TEST_F(ReadGraphFileTest, JunkFile) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("Some junk."),
              HasSubstr("First line"));
}

TEST_F(ReadGraphFileTest, EmptyHeader) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("\n1 2\n0 0"),
              HasSubstr("First line"));
}

TEST_F(ReadGraphFileTest, NegativeNumNodes) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("-1 2\n0 0\n0 0"),
              HasSubstr("First line"));
}

TEST_F(ReadGraphFileTest, NegativeNumArcs) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 -1"),
              HasSubstr("First line"));
}

TEST_F(ReadGraphFileTest, NegativeSrcNodeInArc) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 1\n-1 0"),
              HasSubstr("line 2:"));
}

TEST_F(ReadGraphFileTest, NegativeDstNodeInArc) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 1\n0 -1"),
              HasSubstr("line 2:"));
}

TEST_F(ReadGraphFileTest, SrcNodeTooLargeInArc) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 1\n2 0"),
              HasSubstr("line 2:"));
}

TEST_F(ReadGraphFileTest, DstNodeTooLargeInArc) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 1\n0 2"),
              HasSubstr("line 2:"));
}

TEST_F(ReadGraphFileTest, NotEnoughArcs) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 2\n0 1"),
              HasSubstr("number of arcs/edges"));
}

TEST_F(ReadGraphFileTest, TooManyArcs) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 1\n0 1\n1 1"),
              HasSubstr("number of arcs/edges"));
}

TEST_F(ReadGraphFileTest, InvalidSuperfluousArc) {
  // Verify that superflous arcs are also parsed and checked.
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("2 1\n0 1\n1 2"),
              HasSubstr("line 3:"));
}

TEST_F(ReadGraphFileTest, ZeroColors) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 0"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, TooManyColorsGiven) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 2 4 3"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, NotEnoughColorsGiven) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 3 4"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, ColorCardinalityIsZero) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 3 1 0"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, LastColorCardinalityIsZero) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 3 2 1"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, NegativeColorCardinality) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 3 -1 2"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, LastColorCardinalityIsNegative) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 3 2 2"),
              HasSubstr("color"));
}

TEST_F(ReadGraphFileTest, ColorCardinalityDoesNotParse) {
  EXPECT_THAT(ExpectReadGraphFileFailureAndGetErrorMessage("3 0 3 junk 2"),
              HasSubstr("integer"));
}

template <typename GraphType>
class WriteGraphToFileTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(WriteGraphToFileTypedTest, TestedGraphTypes);

TYPED_TEST(WriteGraphToFileTypedTest, Directed) {
  typedef TypeParam Graph;
  const std::string output_file =
      file::JoinPath(testing::TempDir(), "write_graph.g");
  const auto graph =
      GraphFromArcs<Graph>(/*num_nodes=*/4, /*arcs=*/{{1, 2}, {2, 1}, {1, 1}});
  ASSERT_OK(WriteGraphToFile(graph, output_file, /*directed=*/true,
                             /*num_nodes_with_color=*/{}));
  std::string contents;
  ASSERT_OK(file::GetContents(output_file, &contents, file::Defaults()));
  std::vector<std::string> lines =
      absl::StrSplit(contents, '\n', absl::SkipEmpty());
  ASSERT_EQ("4 3", lines[0]);
  lines.erase(lines.begin());
  EXPECT_THAT(lines, UnorderedElementsAre("1 2", "1 1", "2 1"));
}

TYPED_TEST(WriteGraphToFileTypedTest, NonDirectedWithColors) {
  typedef TypeParam Graph;
  const std::string output_file =
      file::JoinPath(testing::TempDir(), "write_graph.g");
  const auto graph = GraphFromArcs<Graph>(
      /*num_nodes=*/3, /*arcs=*/{{1, 1}, {1, 2}, {2, 1}, {1, 1}, {1, 1}});
  ASSERT_OK(WriteGraphToFile(graph, output_file, /*directed=*/false, {2, 1}));
  std::string contents;
  ASSERT_OK(file::GetContents(output_file, &contents, file::Defaults()));
  std::vector<std::string> lines =
      absl::StrSplit(contents, '\n', absl::SkipEmpty());
  ASSERT_EQ("3 4 2 2", lines[0]);
  lines.erase(lines.begin());
  EXPECT_THAT(lines, UnorderedElementsAre("1 1", "1 2", "1 1", "1 1"));
}

TEST(WriteGraphToFileTest, InvalidPath) {
  const auto graph = GraphFromArcs<ListGraph<>>(
      /*num_nodes=*/3, /*arcs=*/{{2, 1}, {1, 0}});
  const absl::Status status = WriteGraphToFile(graph, "/tmp", /*directed=*/true,
                                               /*num_nodes_with_color=*/{});
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.ToString(), HasSubstr("Could not open"));
  EXPECT_THAT(status.ToString(), HasSubstr("'/tmp'"));
}

TEST(WriteGraphToFileTest, NonDirectedWithOddNumberOfNonSelfArcs) {
  const auto graph = GraphFromArcs<ListGraph<>>(
      /*num_nodes=*/3,
      /*arcs=*/{{2, 1}, {1, 2}, {1, 0}, {1, 1}});
  const std::string filename =
      file::JoinPath(testing::TempDir(), "odd_graph.g");
  ASSERT_OK(WriteGraphToFile(graph, filename, /*directed=*/true,
                             /*num_nodes_with_color=*/{}));
  const absl::Status status =
      WriteGraphToFile(graph, filename, /*directed=*/false,
                       /*num_nodes_with_color=*/{});
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.ToString(), HasSubstr("odd"));
}

TEST(WriteGraphToFileTest, NegativeColorCardinality) {
  StaticGraph<>::Builder builder(/*num_nodes=*/2, /*arc_capacity=*/0);
  const auto graph = std::move(builder).BuildGraph(nullptr);
  const std::string filename =
      file::JoinPath(testing::TempDir(), "bad_color0.g");
  const absl::Status status =
      WriteGraphToFile(graph, filename, /*directed=*/false, {3, -1});
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.ToString(), HasSubstr("color"));
}

TEST(WriteGraphToFileTest, ColorCardinalitiesDoNotAddUp) {
  StaticGraph<>::Builder builder(/*num_nodes=*/3, /*arc_capacity=*/0);
  const auto graph = std::move(builder).BuildGraph(nullptr);
  const std::string filename =
      file::JoinPath(testing::TempDir(), "bad_color1.g");
  const absl::Status status =
      WriteGraphToFile(graph, filename, /*directed=*/false, {1, 1});
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.ToString(), HasSubstr("color"));
}

TEST(WriteGraphToFileTest, ColorCardinalityIsZero) {
  StaticGraph<>::Builder builder(/*num_nodes=*/3, /*arc_capacity=*/0);
  const auto graph = std::move(builder).BuildGraph(nullptr);
  const std::string filename =
      file::JoinPath(testing::TempDir(), "bad_color1.g");
  const absl::Status status =
      WriteGraphToFile(graph, filename, /*directed=*/false, {1, 2, 0});
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.ToString(), HasSubstr("color"));
}

template <typename GraphType>
class ReadAndWriteGraphTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(ReadAndWriteGraphTypedTest, TestedGraphTypes);

TYPED_TEST(ReadAndWriteGraphTypedTest, RandomizedStressTest) {
  typedef TypeParam Graph;
  // Timing as of 2014-04, on forge: <5s in fastbuild; <5s in opt.
  const int kNumTests = DEBUG_MODE ? 300 : 2000;
  std::mt19937 random(1234);
  for (int i = 0; i < kNumTests; ++i) {
    // Generate a random graph.
    const bool directed = absl::Bernoulli(random, 1.0 / 2);
    const int num_nodes = absl::Uniform(random, 0, 100) + 1;
    const int num_arcs = absl::Uniform(random, 0, 100);
    SCOPED_TRACE(
        absl::StrFormat("With graph #%d: %sdirected, %d nodes, %d arcs", i,
                        directed ? "" : "un", num_nodes, num_arcs));
    typename Graph::Builder builder(num_nodes,
                                    directed ? num_arcs : 2 * num_arcs);
    for (int j = 0; j < num_arcs; ++j) {
      const int from = absl::Uniform(random, 0, num_nodes);
      const int to = absl::Uniform(random, 0, num_nodes);
      builder.AddArc(from, to);
      if (!directed && from != to) builder.AddArc(to, from);
    }
    const auto graph = std::move(builder).Build(nullptr);
    // Generate random colors.
    std::vector<int> num_nodes_with_color;
    if (absl::Bernoulli(random, 1.0 / 2)) {
      const int num_colors = absl::Uniform(random, 0, num_nodes) + 1;
      int nodes_left = num_nodes;
      for (int i = 0; i < num_colors - 1; ++i) {
        const int num_other_colors_left = num_colors - i - 1;
        const int max_cardinality = nodes_left - num_other_colors_left;
        const int cardinality = absl::Uniform(random, 0, max_cardinality) + 1;
        num_nodes_with_color.push_back(cardinality);
        nodes_left -= cardinality;
      }
      num_nodes_with_color.push_back(nodes_left);
    }

    // Write it to a file, read it back, and verify that it remained identical.
    // Use a different file for each case. They should be on a local
    // filesystem, and be quickly garbage collected anyway.
    const std::string filename =
        file::JoinPath(testing::TempDir(), absl::StrFormat("io_%d.g", i));
    ASSERT_OK(
        WriteGraphToFile(*graph, filename, directed, num_nodes_with_color));
    std::vector<int> read_num_nodes_with_color;
    absl::StatusOr<Graph*> status_or_graph =
        ReadGraphFile<Graph>(filename, directed, &read_num_nodes_with_color);
    ASSERT_OK(status_or_graph.status());
    std::unique_ptr<Graph> copied_graph(status_or_graph.value());
    ASSERT_EQ(GraphToString(*graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED),
              GraphToString(*copied_graph, PRINT_GRAPH_ADJACENCY_LISTS_SORTED));
    ASSERT_THAT(read_num_nodes_with_color,
                ElementsAreArray(num_nodes_with_color.empty()
                                     ? std::vector<int>(1, num_nodes)
                                     : num_nodes_with_color));
  }
}

}  // namespace
}  // namespace util
