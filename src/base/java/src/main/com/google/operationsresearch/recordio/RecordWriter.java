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

public class RecordWriter implements AutoCloseable{
    private File file;
    private boolean useCompression;
    private Deflater deflater;
    private FileOutputStream outputStream;

    public RecordWriter(File file) throws FileNotFoundException{
        this.file = file;
        useCompression = false;
        deflater = new Deflater();
        outputStream = new FileOutputStream(file, true);
    }

    @Override
    public void close() throws Exception {
        deflater.end();
        outputStream.close();
    }

    public final void setUseCompression(boolean useCompression) {
        this.useCompression = useCompression;
    }

    public final void writeProtocolMessage(Message m) throws IOException {
        byte[] messageBytes = m.toByteArray();

        outputStream.write(i2b(magicNumber));
        outputStream.write(i2b(m.getSerializedSize()));
        if (useCompression) {
            byte[] compressed = compress(messageBytes);
            outputStream.write(i2b(compressed.length));
            outputStream.write(compressed);
        } else {
            outputStream.write(i2b(0));
            outputStream.write(messageBytes);
        }
    }

    private final byte[] compress(byte[] input){
        deflater.reset();
        deflater.setInput(input);
        deflater.finish();
        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        int sourceSize = input.length;
        int compressedSize = (int)(sourceSize + (sourceSize * 0.1f) + 16);
        byte[] output = new byte[compressedSize];

        int length = deflater.deflate(output);
        outputStream.write(output, 0, length);
        try {
            outputStream.close();
        } catch (Exception e0) {
            // swallow, it'd mean we couldn't write to an in-memory byte array.
        }

        byte[] retVal = outputStream.toByteArray();

        return retVal;
    }

}