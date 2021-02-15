# Various calls to CP api from python to verify they work.
'''Test routing API'''

import unittest
from ortools.constraint_solver import routing_enums_pb2
from ortools.constraint_solver import pywrapcp


class TestRoutingIndexManager(unittest.TestCase):

    def test_ctor(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        if __debug__:
            print(f"class RoutingIndexManager: {dir(manager)}")
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42+3*2-1, manager.GetNumberOfIndices())

    def test_ctor_multi_same(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, [0,0,0], [0,0,0])
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42+3*2-1, manager.GetNumberOfIndices())

    def test_ctor_multi_all_diff(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, [1,2,3], [4,5,6])
        self.assertIsNotNone(manager)
        self.assertEqual(42, manager.GetNumberOfNodes())
        self.assertEqual(3, manager.GetNumberOfVehicles())
        self.assertEqual(42, manager.GetNumberOfIndices())


class TestRoutingModel(unittest.TestCase):

    def test_ctor(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, 7)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        if __debug__:
            print(f"class RoutingModel: {dir(routing)}")

    def test_solve(self):
        manager = pywrapcp.RoutingIndexManager(42, 3, [1,2,3], [4,5,6])
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        self.assertTrue(routing.Solve())

    def test_unary_transit_vector(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        transit_id = routing.RegisterUnaryTransitVector([i+1 for i in range(5)])
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertTrue(routing.Solve())

    def test_unary_transit_callback(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        transit_id = routing.RegisterUnaryTransitCallback(lambda from_index: 1)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertTrue(routing.Solve())

    def test_transit_matrix(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        matrix = [[i+1 for i in range(5)] for j in range(5)]
        transit_id = routing.RegisterTransitMatrix(matrix)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertTrue(routing.Solve())

    def test_transit_callback(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        transit_id = routing.RegisterTransitCallback(lambda from_index, to_indx: 1)
        self.assertEqual(1, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertTrue(routing.Solve())

    def test_pair(self):
        p = pywrapcp.PairIntBool(5, True)
        if __debug__:
            print(f"class PairIntBool: {dir(p)}")
        self.assertEqual(5, p[0])
        self.assertEqual(True, p[1])

    def test_vector_dimension(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        res = routing.AddVectorDimension(
                [i+1 for i in range(5)],
                10, # capacity
                True, # fix_start_cumul_to_zero
                "DimensionName")
        if __debug__:
            print(f"class PairIntBool: {dir(res)}")
            print(f"result: {res}")
        self.assertEqual(res[0], 1)
        self.assertEqual(res[1], True)
        routing.SetArcCostEvaluatorOfAllVehicles(res[0])
        self.assertTrue(routing.Solve())

    def test_matrix_dimension(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        matrix = [[i+1 for i in range(5)] for j in range(5)]
        res = routing.AddMatrixDimension(
                matrix,
                10, # capacity
                True, # fix_start_cumul_to_zero
                "DimensionName")
        if __debug__:
            print(f"class PairIntBool: {dir(res)}")
            print(f"result: {res}")
        self.assertEqual(res[0], 1)
        self.assertEqual(res[1], True)
        routing.SetArcCostEvaluatorOfAllVehicles(res[0])
        self.assertTrue(routing.Solve())

    def test_matrix_dimension_2(self):
        manager = pywrapcp.RoutingIndexManager(5, 2, 0)
        self.assertIsNotNone(manager)
        routing = pywrapcp.RoutingModel(manager)
        self.assertIsNotNone(routing)
        matrix = [[i+1 for i in range(5)] for j in range(5)]
        routing.AddMatrixDimension(
                matrix,
                10, # capacity
                True, # fix_start_cumul_to_zero
                "DimensionName")
        transit_id = routing.RegisterTransitMatrix(matrix)
        self.assertEqual(2, transit_id)
        routing.SetArcCostEvaluatorOfAllVehicles(transit_id)
        self.assertTrue(routing.Solve())

if __name__ == '__main__':
    unittest.main(verbosity=2)
