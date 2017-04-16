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

import com.google.protobuf.Message;

import java.io.*;
import java.util.zip.Deflater;

import static com.google.operationsresearch.recordio.RecordIOUtils.i2b;
import static com.google.operationsresearch.recordio.RecordIOUtils.magicNumber;

public class RecordWriter {
    private File file;
    private boolean useCompression;
    private Deflater deflater;

    public RecordWriter(File file) {
        this.file = file;
        deflater = new Deflater();
    }

    public final boolean writeProtocolMessage(Message m) {
        byte[] messageBytes = m.toByteArray();
        try (FileOutputStream f = new FileOutputStream(file, true)) {
            f.write(i2b(magicNumber));
            f.write(i2b(m.getSerializedSize()));
            if(useCompression) {
                byte[] compressed = compress(messageBytes);
                f.write(i2b(compressed.length));
                f.write(compressed);
            } else {
                f.write(i2b(0));
                f.write(messageBytes);
            }
        } catch (FileNotFoundException e0) {
            e0.printStackTrace();
            return false;
        } catch (IOException e1) {
            e1.printStackTrace();
            return false;
        }
        return true;
    }

    public final void setUseCompression(boolean useCompression) {
        this.useCompression = useCompression;
    }

    private final byte[] compress(byte[] input) {
        deflater.reset();
        deflater.setInput(input);
        deflater.finish();
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        int sourceSize = input.length;
        int compressedSize = (int)(sourceSize + (sourceSize * 0.1f) + 16); // could be 1024, why not?
        byte[] output = new byte[compressedSize];

        int length = deflater.deflate(output);
        outputStream.write(output, 0, length);
        try {
            outputStream.close();
        } catch (IOException e0) {
            e0.printStackTrace();
        }

        byte[] retVal = outputStream.toByteArray();

        return retVal;
    }
}