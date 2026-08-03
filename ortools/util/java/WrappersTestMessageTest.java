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

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.ortools.Loader;
import com.google.ortools.util.WrappersTestMessage;
import java.util.List;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

/** Tests for Java SWIG proto wrappers. */
public class WrappersTestMessageTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testWrappers() {
    WrappersTestMessage msg = new WrappersTestMessage();
    msg.setInt32Field(12);
    assertEquals(12, msg.getInt32Field());

    msg.setInt64Field(123456789012345L);
    assertEquals(123456789012345L, msg.getInt64Field());

    msg.setFloatField(3.14f);
    assertEquals(msg.getFloatField(), 3.14f, 1e-5f);

    msg.setStringField("test");
    assertEquals("test", msg.getStringField());

    // Enums are wrapped as nested classes by default in SWIG 4.0.
    msg.setEnumField(WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1);
    assertEquals(WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1, msg.getEnumField());
    assertEquals("TEST_ENUM_VALUE1", WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1.name());

    msg.mutableNestedMessageField().setNestedIntField(42);
    assertEquals(42, msg.mutableNestedMessageField().getNestedIntField());

    msg.setOneofString("test");
    assertEquals("test", msg.getOneofString());
    msg.clearOneofString();
    assertEquals("", msg.getOneofString());

    msg.getRepeatedInt32FieldList().append(1);
    msg.getRepeatedInt32FieldList().append(2);
    assertEquals(2, msg.getRepeatedInt32FieldList().size());
    assertEquals(1, (int) msg.getRepeatedInt32FieldList().get(0));
    assertEquals(2, (int) msg.getRepeatedInt32FieldList().get(1));

    msg.getRepeatedStringFieldList().append("a");
    msg.getRepeatedStringFieldList().append("b");
    assertEquals(2, msg.getRepeatedStringFieldList().size());
    assertEquals("a", msg.getRepeatedStringFieldList().get(0));

    WrappersTestMessage.NestedMessage nested = msg.getRepeatedNestedMessageFieldList().add();
    nested.setNestedIntField(100);
    assertEquals(100, msg.getRepeatedNestedMessageFieldList().get(0).getNestedIntField());
  }

  @Test
  public void testListInterface() {
    WrappersTestMessage msg = new WrappersTestMessage();
    List<Integer> intList = msg.getRepeatedInt32FieldList();
    intList.add(10);
    intList.add(20);
    assertEquals(2, intList.size());
    assertEquals(Integer.valueOf(10), intList.get(0));
    assertEquals(Integer.valueOf(20), intList.get(1));

    intList.set(0, 15);
    assertEquals(Integer.valueOf(15), intList.get(0));

    intList.remove(0);
    assertEquals(1, intList.size());
    assertEquals(Integer.valueOf(20), intList.get(0));

    List<String> stringList = msg.getRepeatedStringFieldList();
    stringList.add("first");
    stringList.add("second");
    assertEquals("first", stringList.get(0));
    stringList.remove(0);
    assertEquals("second", stringList.get(0));

    List<WrappersTestMessage.NestedMessage> msgList = msg.getRepeatedNestedMessageFieldList();
    WrappersTestMessage.NestedMessage n = new WrappersTestMessage.NestedMessage();
    n.setNestedIntField(5);
    msgList.add(n);
    assertEquals(1, msgList.size());
    assertEquals(5, msgList.get(0).getNestedIntField());
  }

  @Test
  public void testCopyFrom() {
    WrappersTestMessage msg1 = new WrappersTestMessage();
    msg1.setInt32Field(10);
    WrappersTestMessage msg2 = new WrappersTestMessage();
    msg2.copyFrom(msg1);
    assertEquals(10, msg2.getInt32Field());
  }

  @Test
  public void testParseString() {
    WrappersTestMessage msg = new WrappersTestMessage();
    msg.setInt32Field(123);
    String text = msg.toString();
    WrappersTestMessage msg2 = new WrappersTestMessage();
    assertTrue(msg2.parseTextFormat(text));
    assertEquals(123, msg2.getInt32Field());
  }

  @Test
  public void testEnum() {
    WrappersTestMessage msg = new WrappersTestMessage();
    msg.setEnumField(WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1);
    assertEquals("TEST_ENUM_VALUE1", msg.getEnumField().name());
    assertEquals(WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1, msg.getEnumField());
    switch (msg.getEnumField()) {
      case WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1 ->
        assertEquals(WrappersTestMessage.TestEnum.TEST_ENUM_VALUE1, msg.getEnumField());
      case WrappersTestMessage.TestEnum.TEST_ENUM_VALUE2 ->
        assertEquals(WrappersTestMessage.TestEnum.TEST_ENUM_VALUE2, msg.getEnumField());
      default ->
        assertEquals(WrappersTestMessage.TestEnum.TEST_ENUM_UNSPECIFIED, msg.getEnumField());
    }
  }
}
