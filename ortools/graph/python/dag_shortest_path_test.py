#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""High-level tests for dag_shortest_path.

Do not test edge cases here, as they are tested in the C++ unit tests. Here,
we just test the interface of the Python module.
"""

from absl.testing import absltest

from ortools.graph.python import dag_shortest_path


class ArcWithLengthTest(absltest.TestCase):

    def test_arc_with_length(self) -> None:
        arc = dag_shortest_path.ArcWithLength(from_node=1, to_node=2, length=3.5)
        self.assertEqual(arc.from_node, 1)
        self.assertEqual(arc.to_node, 2)
        self.assertEqual(arc.length, 3.5)

        r = repr(arc)
        self.assertTrue(r.startswith("ArcWithLength("))
        self.assertTrue(r.endswith(")"))
        self.assertIn("from_node=1", r)
        self.assertIn("to_node=2", r)
        self.assertIn("length=", r)


class PathWithLengthTest(absltest.TestCase):

    def test_path_with_length(self) -> None:
        path = dag_shortest_path.PathWithLength(
            length=5.0, arc_path=[1, 2], node_path=[0, 1, 3]
        )
        self.assertEqual(path.length, 5.0)
        self.assertEqual(path.arc_path, [1, 2])
        self.assertEqual(path.node_path, [0, 1, 3])

        r = repr(path)
        self.assertTrue(r.startswith("PathWithLength("))
        self.assertTrue(r.endswith(")"))
        self.assertIn("length=", r)
        self.assertIn("arc_path=[1, 2]", r)
        self.assertIn("node_path=[0, 1, 3]", r)


class DagShortestPathTest(absltest.TestCase):

    def test_shortest_paths_on_dag(self) -> None:
        # Set up a simple DAG:
        # 0 -> 1 (length 1.0)
        # 1 -> 3 (length 2.0)
        # 0 -> 2 (length 1.5)
        # 2 -> 3 (length 1.0)
        arcs = [
            dag_shortest_path.ArcWithLength(from_node=0, to_node=1, length=1.0),
            dag_shortest_path.ArcWithLength(from_node=1, to_node=3, length=2.0),
            dag_shortest_path.ArcWithLength(from_node=0, to_node=2, length=1.5),
            dag_shortest_path.ArcWithLength(from_node=2, to_node=3, length=1.0),
        ]

        # Shortest path should be 0 -> 2 -> 3 with length 2.5
        path = dag_shortest_path.shortest_paths_on_dag(
            num_nodes=4, arcs_with_length=arcs, source=0, destination=3
        )

        self.assertEqual(path.length, 2.5)
        self.assertEqual(path.node_path, [0, 2, 3])
        self.assertEqual(path.arc_path, [2, 3])


class StaticGraphTest(absltest.TestCase):
    """Tests the StaticGraph and StaticGraphBuilder classes.

    They should be moved to a new module when wrapping the StaticGraph class
    properly.
    """

    def test_static_graph_builder(self) -> None:
        builder = dag_shortest_path.StaticGraphBuilder(4, 4)
        builder.add_arc(0, 1)
        builder.add_arc(1, 3)
        builder.add_arc(0, 2)
        builder.add_arc(2, 3)
        graph, permutation = builder.build()
        del permutation

        self.assertEqual(graph.num_nodes(), 4)
        self.assertEqual(graph.num_arcs(), 4)


class DagKShortestPathTest(absltest.TestCase):

    def test_k_shortest_paths_on_dag(self) -> None:
        arcs = [
            dag_shortest_path.ArcWithLength(from_node=0, to_node=1, length=1.0),
            dag_shortest_path.ArcWithLength(from_node=1, to_node=3, length=2.0),
            dag_shortest_path.ArcWithLength(from_node=0, to_node=2, length=1.5),
            dag_shortest_path.ArcWithLength(from_node=2, to_node=3, length=1.0),
        ]

        paths = dag_shortest_path.k_shortest_paths_on_dag(
            num_nodes=4,
            arcs_with_length=arcs,
            source=0,
            destination=3,
            path_count=2,
        )

        # There must be two paths, iterated by increasing length.
        self.assertLen(paths, 2)

        # 1st path must be 0 -> 2 -> 3 (length 2.5)
        self.assertEqual(paths[0].length, 2.5)
        self.assertEqual(paths[0].node_path, [0, 2, 3])
        self.assertEqual(paths[0].arc_path, [2, 3])

        # 2nd path must be 0 -> 1 -> 3 (length 3.0)
        self.assertEqual(paths[1].length, 3.0)
        self.assertEqual(paths[1].node_path, [0, 1, 3])
        self.assertEqual(paths[1].arc_path, [0, 1])

    def test_shortest_paths_on_dag_wrapper(self) -> None:
        builder = dag_shortest_path.StaticGraphBuilder(4, 4)
        builder.add_arc(0, 1)
        builder.add_arc(1, 3)
        builder.add_arc(0, 2)
        builder.add_arc(2, 3)
        graph, permutation = builder.build()
        raw_lengths = [1.0, 2.0, 1.5, 1.0]
        arc_lengths = [raw_lengths[p] for p in permutation]
        inv_perm = [0] * len(permutation)
        for i, p in enumerate(permutation):
            inv_perm[p] = i

        self.assertEqual(graph.num_nodes(), 4)
        self.assertEqual(graph.num_arcs(), 4)

        wrapper = dag_shortest_path.ShortestPathsOnDagWrapper(
            graph=graph,
            arc_lengths=arc_lengths,
            topological_order=[0, 1, 2, 3],
        )
        wrapper.run_shortest_path_on_dag(sources=[0])

        self.assertTrue(wrapper.is_reachable(3))
        self.assertCountEqual(wrapper.reached_nodes(), [0, 1, 2, 3])
        self.assertEqual(wrapper.length_to(3), 2.5)
        self.assertEqual(wrapper.node_path_to(3), [0, 2, 3])
        self.assertEqual([inv_perm[a] for a in wrapper.arc_path_to(3)], [2, 3])

    def test_shortest_paths_on_dag_wrapper_fails_without_topological_order(
        self,
    ) -> None:
        builder = dag_shortest_path.StaticGraphBuilder(4, 4)
        builder.add_arc(0, 1)
        builder.add_arc(1, 3)
        builder.add_arc(0, 2)
        builder.add_arc(2, 3)
        graph, permutation = builder.build()
        raw_lengths = [1.0, 2.0, 1.5, 1.0]
        arc_lengths = [raw_lengths[p] for p in permutation]

        self.assertEqual(graph.num_nodes(), 4)
        self.assertEqual(graph.num_arcs(), 4)

        with self.assertRaises(ValueError):
            # std::invalid_argument is mapped to ValueError.
            dag_shortest_path.ShortestPathsOnDagWrapper(
                graph=graph,
                arc_lengths=arc_lengths,
                topological_order=[],
            )

    def test_k_shortest_paths_on_dag_wrapper(self) -> None:
        builder = dag_shortest_path.StaticGraphBuilder(4, 4)
        builder.add_arc(0, 1)
        builder.add_arc(1, 3)
        builder.add_arc(0, 2)
        builder.add_arc(2, 3)
        graph, permutation = builder.build()
        raw_lengths = [1.0, 2.0, 1.5, 1.0]
        arc_lengths = [raw_lengths[p] for p in permutation]
        inv_perm = [0] * len(permutation)
        for i, p in enumerate(permutation):
            inv_perm[p] = i

        wrapper = dag_shortest_path.KShortestPathsOnDagWrapper(
            graph=graph,
            arc_lengths=arc_lengths,
            topological_order=[0, 1, 2, 3],
            path_count=2,
        )
        self.assertEqual(wrapper.path_count(), 2)
        wrapper.run_k_shortest_path_on_dag(sources=[0])

        self.assertTrue(wrapper.is_reachable(3))
        self.assertCountEqual(wrapper.reached_nodes(), [0, 1, 2, 3])
        self.assertEqual(wrapper.lengths_to(3), [2.5, 3.0])
        self.assertEqual(wrapper.node_paths_to(3), [[0, 2, 3], [0, 1, 3]])
        self.assertEqual(
            [[inv_perm[a] for a in path] for path in wrapper.arc_paths_to(3)],
            [[2, 3], [0, 1]],
        )

    def test_k_shortest_paths_on_dag_wrapper_fails_without_topological_order(
        self,
    ) -> None:
        builder = dag_shortest_path.StaticGraphBuilder(4, 4)
        builder.add_arc(0, 1)
        builder.add_arc(1, 3)
        builder.add_arc(0, 2)
        builder.add_arc(2, 3)
        graph, permutation = builder.build()
        raw_lengths = [1.0, 2.0, 1.5, 1.0]
        arc_lengths = [raw_lengths[p] for p in permutation]

        with self.assertRaises(ValueError):
            # std::invalid_argument is mapped to ValueError.
            dag_shortest_path.KShortestPathsOnDagWrapper(
                graph=graph,
                arc_lengths=arc_lengths,
                topological_order=[],
                path_count=2,
            )


if __name__ == "__main__":
    absltest.main()
