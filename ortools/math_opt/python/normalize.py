# Copyright 2010-2024 Google LLC
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

# A fork of net/proto2/contrib/pyutil/normalize.py. A lot of the code can be
# deleted because we do not support proto2 (no groups, no extension). Further,
# the code has been changed to not clear:
#   * optional scalar fields at their default value.
#   * durations
#   * messages in a oneof


"""Utility functions for normalizing proto3 message objects in Python."""
from google.protobuf import duration_pb2
from google.protobuf import descriptor
from google.protobuf import message


def math_opt_normalize_proto(protobuf_message: message.Message) -> None:
    """Clears all non-duration submessages that are not in one_ofs.

    A message is considered `empty` if:
      * every non-optional scalar fields has its default value,
      * every optional scalar field is unset,
      * every repeated/map fields is empty
      * every oneof is unset,
      * every duration field is unset
      * all other message fields (singular, not oneof, not duration) are `empty`.
    This function clears all `empty` fields from `message`.

    This is useful for testing.

    Args:
      protobuf_message: The Message object to clear.
    """
    for field, value in protobuf_message.ListFields():
        if field.type != field.TYPE_MESSAGE:
            continue
        if field.label == field.LABEL_REPEATED:
            # Now the repeated case, recursively normalize each member. Note that
            # there is no field presence for repeated fields, so we don't need to call
            # ClearField().
            #
            # Maps need to be handled specially.
            if (
                field.message_type.has_options
                and field.message_type.GetOptions().map_entry
            ):
                if (
                    field.message_type.fields_by_number[2].type
                    == descriptor.FieldDescriptor.TYPE_MESSAGE
                ):
                    for item in value.values():
                        math_opt_normalize_proto(item)
            # The remaining case is a regular repeated field (a list).
            else:
                for item in value:
                    math_opt_normalize_proto(item)
            continue
        # Last case, the non-repeated sub-message
        math_opt_normalize_proto(value)
        # If field value is empty, not a Duration, and not in a oneof, clear it.
        if (
            not value.ListFields()
            and field.message_type != duration_pb2.Duration.DESCRIPTOR
            and field.containing_oneof is None
        ):
            protobuf_message.ClearField(field.name)
