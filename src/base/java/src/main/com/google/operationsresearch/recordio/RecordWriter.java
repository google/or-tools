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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
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
        try (FileOutputStream f = new FileOutputStream(file, true)) {
            f.write(i2b(magicNumber));
            f.write(i2b(m.getSerializedSize()));
            if(useCompression) {
                byte[] compressed = compress(m.toByteArray());
                f.write(i2b(compressed.length));
                f.write(compressed);
            } else {
                f.write(i2b(0));
                m.writeTo(f);
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
        int sourceSize = input.length;
        int compressedSize = (int)(sourceSize + (sourceSize * 0.1f) + 16); // could be 1024, why not?
        byte[] output = new byte[compressedSize];

        deflater.setInput(input);
        deflater.finish();
        int compressLength = deflater.deflate(output, 0, output.length);

        byte[] retVal = new byte[compressLength];

        System.arraycopy(output,0, retVal,0,compressLength);

        return retVal;
    }
}