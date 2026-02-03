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

from absl.testing import absltest
from ortools.util.python import wrappers_test_extension


class WrappersTest(absltest.TestCase):

    def test_wrappers(self):
        msg = wrappers_test_extension.WrappersTestMessage()
        msg.int32_field = 12
        self.assertEqual(msg.int32_field, 12)

        msg.int64_field = 123456789012345
        self.assertEqual(msg.int64_field, 123456789012345)

        msg.float_field = 3.14
        self.assertAlmostEqual(msg.float_field, 3.14, delta=1e-5)

        msg.string_field = "test"
        self.assertEqual(msg.string_field, "test")

        msg.enum_field = wrappers_test_extension.WrappersTestMessage.TEST_ENUM_VALUE1
        self.assertEqual(
            msg.enum_field,
            wrappers_test_extension.WrappersTestMessage.TEST_ENUM_VALUE1,
        )

        msg.nested_message_field.nested_int_field = 42
        self.assertEqual(msg.nested_message_field.nested_int_field, 42)

        msg.value_enum_field = (
            wrappers_test_extension.WrappersTestMessage.WrappersTestMessage_Value.VALUE_A
        )
        self.assertEqual(
            msg.value_enum_field,
            wrappers_test_extension.WrappersTestMessage.WrappersTestMessage_Value.VALUE_A,
        )

        msg.repeated_int32_field.append(1)
        msg.repeated_int32_field.append(2)
        self.assertLen(msg.repeated_int32_field, 2)
        self.assertEqual(msg.repeated_int32_field[0], 1)
        self.assertEqual(msg.repeated_int32_field[1], 2)

        msg.repeated_string_field.append("a")
        msg.repeated_string_field.append("b")
        self.assertLen(msg.repeated_string_field, 2)
        self.assertEqual(msg.repeated_string_field[0], "a")

        nested = msg.repeated_nested_message_field.add()
        nested.nested_int_field = 100
        self.assertEqual(msg.repeated_nested_message_field[0].nested_int_field, 100)

    def test_copy_from(self):
        msg1 = wrappers_test_extension.WrappersTestMessage()
        msg1.int32_field = 10
        msg2 = wrappers_test_extension.WrappersTestMessage()
        msg2.copy_from(msg1)
        self.assertEqual(msg2.int32_field, 10)

    def test_parse_string(self):
        msg = wrappers_test_extension.WrappersTestMessage()
        msg.int32_field = 123
        text = str(msg)
        msg2 = wrappers_test_extension.WrappersTestMessage()
        # The C++ wrapper uses TextFormat::ParseFromString
        msg2.parse_text_format(text)
        self.assertEqual(msg2.int32_field, 123)


if __name__ == "__main__":
    absltest.main()
