# Various calls to CP api from python to verify they work.
'''Test routing API'''
import types
import unittest

from ortools.constraint_solver import pywrapcp

class TestIntVarContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Contains'),
            dir(pywrapcp.IntVarContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Element'),
            dir(pywrapcp.IntVarContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Size'), dir(pywrapcp.IntVarContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Store'), dir(pywrapcp.IntVarContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.IntVarContainer, 'Restore'),
            dir(pywrapcp.IntVarContainer))


class TestIntervalVarContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Contains'),
            dir(pywrapcp.IntervalVarContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Element'),
            dir(pywrapcp.IntervalVarContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Size'),
            dir(pywrapcp.IntervalVarContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Store'),
            dir(pywrapcp.IntervalVarContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalVarContainer, 'Restore'),
            dir(pywrapcp.IntervalVarContainer))


class TestSequenceVarContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Contains'),
            dir(pywrapcp.SequenceVarContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Element'),
            dir(pywrapcp.SequenceVarContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Size'),
            dir(pywrapcp.SequenceVarContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Store'),
            dir(pywrapcp.SequenceVarContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceVarContainer, 'Restore'),
            dir(pywrapcp.SequenceVarContainer))


if __name__ == '__main__':
    unittest.main()
