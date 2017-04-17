// Copyright 2010-2017 Google
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
package com.google.operationsresearch.recordio;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public final class RecordIOUtils {

    public static int magicNumber = 0x3ed7230a;

    public static final byte[] i2b(int i) {
        return ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(i).array();
    }

    public static final int b2i(byte[] b) {
        return ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }

}
