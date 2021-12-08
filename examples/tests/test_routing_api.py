#!/usr/bin/env python3
# Various calls to CP api from python to verify they work.
'''Test routing API'''

from functools import partial

import unittest

from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp


def Distance(node_i, node_j):
    return node_i + node_j


def TransitDistance(manager, i, j):
    return Distance(manager.IndexToNode(i), manager.IndexToNode(j))


def One(unused_i, unused_j):
    return 1


def Two(unused_i, unused_j):
    return 1


def Three(unused_i, unused_j):
    return 1


class Callback(object):

    def __init__(self, model):
      self.model = model
      self.costs = []

    def __call__(self):
      self.costs.append(self.model.CostVar().Max())


class TestRoutingIndexManager(unittest.TestCase):

    def test_ctor(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        if __debug__:
            print(f'class RoutingIndexManager: {dir(manager)}')
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42+3*2-1, manager.GetNumberOfIndices())
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(7, manager.IndexToNode(manager.GetStartIndex(i)))
            self.assertEqual(7, manager.IndexToNode(manager.GetEndIndex(i)))

    def test_ctor_multi_depot_same(self):
        manager = pywrapcp.RoutingIndexManager(
            42, 3, [0, 0, 0], [0, 0, 0])
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42+3*2-1, manager.GetNumberOfIndices())
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(0, manager.IndexToNode(manager.GetStartIndex(i)))
            self.assertEqual(0, manager.IndexToNode(manager.GetEndIndex(i)))

    def test_ctor_multi_depot_all_diff(self):
        manager = pywrapcp.RoutingIndexManager(
            42, 3, [1, 2, 3], [4, 5, 6])
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42, manager.GetNumberOfIndices())
        for i in range(manager.GetNumberOfVehicles()):
            self.assertEqual(i+1, manager.IndexToNode(manager.GetStartIndex(i)))
            self.assertEqual(i+4, manager.IndexToNode(manager.GetEndIndex(i)))


class TestRoutingModel(unittest.TestCase):

    def test_ctor(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        if __debug__:
            print(f'class RoutingModel: {dir(routing)}')

    def test_solve(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(0, assignment.ObjectiveValue())

    def test_solve_multi_depot(self):
        manager = pywrapcp.RoutingIndexManager(
            42, 3, [1, 2, 3], [4, 5, 6])
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(0, assignment.ObjectiveValue())


    def test_unary_transit_vector(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        vector = [i+1 for i in range(5)]
        transit_id = routing.RegisterUnaryTransitVector(vector)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(15, assignment.ObjectiveValue())

    def test_unary_transit_callback(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        transit_id = routing.RegisterUnaryTransitCallback(lambda from_index: 1)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(5, assignment.ObjectiveValue())

    def test_transit_matrix(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        matrix = [[i+1 for i in range(5)] for j in range(5)]
        transit_id = routing.RegisterTransitMatrix(matrix)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(15, assignment.ObjectiveValue())

    def test_transit_callback(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        transit_id = routing.RegisterTransitCallback(lambda from_index, to_indx: 1)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(5, assignment.ObjectiveValue())

    def test_vector_dimension(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        transit_id, success = routing.AddVectorDimension(
                [i+1 for i in range(5)],
                10, # capacity
                True, # fix_start_cumul_to_zero
                "DimensionName")
        self.assertTrue(success)
        self.assertEqual(transit_id, 1)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(16, assignment.ObjectiveValue())

    def test_matrix_dimension(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        matrix = [[i+1 for i in range(5)] for j in range(5)]
        transit_id, success = routing.AddMatrixDimension(
                matrix,
                10, # capacity
                True, # fix_start_cumul_to_zero
                "DimensionName")
        self.assertTrue(success)
        self.assertEqual(transit_id, 1)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(16, assignment.ObjectiveValue())

    def test_matrix_dimension_2(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        matrix = [[i+1 for i in range(5)] for j in range(5)]
        transit_id, success = routing.AddMatrixDimension(
                matrix,
                10, # capacity
                True, # fix_start_cumul_to_zero
                "DimensionName")
        self.assertTrue(success)
        self.assertEqual(transit_id, 1)
        transit2_id = routing.RegisterTransitMatrix(matrix)
        self.assertEqual(transit2_id, 2)
        routing.SetArcCostEvaluatorOfAllVehicles(transit2_id)
        self.assertEqual(routing.ROUTING_NOT_SOLVED, routing.status())
        assignment = routing.Solve()
        self.assertEqual(routing.ROUTING_SUCCESS, routing.status())
        self.assertIsNotNone(assignment)
        self.assertEqual(16, assignment.ObjectiveValue())

if __name__ == '__main__':
    unittest.main(verbosity=2)
