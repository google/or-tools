# Various calls to CP api from python to verify they work.
'''Test routing API'''
from __future__ import print_function
import types
import unittest

from ortools.constraint_solver import pywrapcp

class TestIntContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.IntContainer, 'Contains'),
            dir(pywrapcp.IntContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.IntContainer, 'Element'),
            dir(pywrapcp.IntContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.IntContainer, 'Size'), dir(pywrapcp.IntContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.IntContainer, 'Store'), dir(pywrapcp.IntContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.IntContainer, 'Restore'),
            dir(pywrapcp.IntContainer))


class TestIntervalContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalContainer, 'Contains'),
            dir(pywrapcp.IntervalContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalContainer, 'Element'),
            dir(pywrapcp.IntervalContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalContainer, 'Size'),
            dir(pywrapcp.IntervalContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalContainer, 'Store'),
            dir(pywrapcp.IntervalContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.IntervalContainer, 'Restore'),
            dir(pywrapcp.IntervalContainer))


class TestSequenceContainerAPI(unittest.TestCase):

    def test_contains(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceContainer, 'Contains'),
            dir(pywrapcp.SequenceContainer))

    def test_element(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceContainer, 'Element'),
            dir(pywrapcp.SequenceContainer))

    def test_size(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceContainer, 'Size'),
            dir(pywrapcp.SequenceContainer))

    def test_store(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceContainer, 'Store'),
            dir(pywrapcp.SequenceContainer))

    def test_restore(self):
        self.assertTrue(
            hasattr(pywrapcp.SequenceContainer, 'Restore'),
            dir(pywrapcp.SequenceContainer))


if __name__ == '__main__':
    unittest.main()
