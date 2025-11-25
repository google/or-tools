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

import re

from absl.testing import absltest
from tools.testing import bintest_matchers


def check(log: str, *args: str):
    return bintest_matchers.extract(log, *args, check_matcher_specs=True)


def extract(log: str, *args: str):
    return bintest_matchers.extract(log, *args, check_matcher_specs=False)


class BintestMatchersTest(absltest.TestCase):

    def test_string_regex(self):
        pattern = bintest_matchers.Re.STRING
        self.assertRegex('""', pattern)
        self.assertRegex("''", pattern)
        self.assertRegex('"foo"', pattern)
        self.assertRegex("'foo'", pattern)
        self.assertRegex("'fo\no'", pattern)
        self.assertNotRegex("'foo", pattern)

    def test_keyword_regex(self):
        pattern = bintest_matchers.Re._KEYWORD_EXPR
        self.assertRegex("@num()", pattern)
        self.assertRegex("@num(foo)", pattern)
        self.assertRegex("@num(123)", pattern)
        self.assertNotRegex("@num", pattern)
        self.assertNotRegex("@numb(foo)", pattern)
        self.assertNotRegex("@numb)foo)", pattern)

    def test_number_regex(self):
        pattern = bintest_matchers.Re.NUMBER
        self.assertRegex("123", pattern)
        self.assertRegex("123.456", pattern)
        self.assertRegex("123e-06", pattern)
        self.assertRegex("123e6", pattern)
        self.assertRegex("123e+6", pattern)
        self.assertRegex("1.23e10", pattern)
        self.assertRegex("123.456e-7", pattern)
        self.assertRegex(".1", pattern)
        self.assertRegex("+123", pattern)
        self.assertRegex("+123.456", pattern)
        self.assertRegex("+123e-06", pattern)
        self.assertRegex("+123e6", pattern)
        self.assertRegex("+123e+6", pattern)
        self.assertRegex("+1.23e10", pattern)
        self.assertRegex("+123.456e-7", pattern)
        self.assertRegex("-123", pattern)
        self.assertRegex("-123.456", pattern)
        self.assertRegex("-123e-06", pattern)
        self.assertRegex("-123e6", pattern)
        self.assertRegex("-123e+6", pattern)
        self.assertRegex("-1.23e10", pattern)
        self.assertRegex("-123.456e-7", pattern)
        self.assertRegex("123E-06", pattern)
        self.assertRegex("123E6", pattern)
        self.assertRegex("123E+6", pattern)
        self.assertRegex("1.23E10", pattern)
        self.assertRegex("123.456E-7", pattern)
        self.assertIsNone(re.fullmatch(pattern, " "))
        self.assertIsNone(re.fullmatch(pattern, "123.456e"))
        self.assertIsNone(re.fullmatch(pattern, "123e"))
        self.assertIsNone(re.fullmatch(pattern, "123e-"))
        self.assertIsNone(re.fullmatch(pattern, "."))

    def test_check(self):
        # text
        check("Hello world!", "world")
        # approx float
        check("Hello 1!", "Hello @num(1)")
        check("0.9999999", "@num(1)")
        check("1.0000001", "@num(1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("0.9", "@num(1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("1.1", "@num(1)")
        # approx float with specific tolerance
        check("0.95", "@num(1~0.1)")
        check("1.05", "@num(1~0.1)")
        # greater
        check("2", "@num(>1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("1", "@num(>1)")
        # less
        check("0", "@num(<1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("1", "@num(<1)")
        # greater or equal
        check("2", "@num(>=1)")
        check("1", "@num(>=1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("0", "@num(>=1)")
        # less or equal
        check("0", "@num(<=1)")
        check("1", "@num(<=1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("2", "@num(<=1)")
        # within
        check("0.5", "@num(>0, <1)")
        check("0.5", "@num(>=0.5, <=0.5)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("-1", "@num(>0, <1)")
        with self.assertRaises(bintest_matchers.MatchError):
            check("2", "@num(>0, <1)")

    def test_extract_return(self):
        v1, v2, v3 = extract("2+4=6", "@num()+@num()=@num()")
        self.assertEqual(v1, 2)
        self.assertEqual(v2, 4)
        self.assertEqual(v3, 6)

    def test_extract_failure(self):
        with self.assertRaises(ValueError):
            extract("123", "@num(123)")


if __name__ == "__main__":
    absltest.main()
