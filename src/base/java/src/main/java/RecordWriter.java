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

import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.zip.Deflater;

public class RecordWriter {
    // Magic number when reading and writing protocol buffers
    private Path file;
    private Charset charset = Charset.forName("UTF-8");
    public int kMagicNumber = 0x3ed7230a;
    private boolean useCompression;
    private Deflater deflater;

    public RecordWriter(Path file) {
        this.file = file;
        deflater = new Deflater();
    }

    public final void writeProtocolMessage(Message m) {
        try (BufferedWriter writer = Files.newBufferedWriter(file, charset)) {
            writer.write(kMagicNumber);
            writer.write(m.toString().length());
            if(useCompression) {
                String compressed = compress(m.toString());
                writer.write(compressed.length());
                writer.write(compressed);
            } else {
                writer.write(0);
                writer.write(m.toString());
            }
        } catch (IOException e0) {
            e0.printStackTrace();
        }
    }

    public final void setUseCompression(boolean useCompression) {
        this.useCompression = useCompression;
    }

    private final String compress(String s) {
        byte[] b = s.getBytes();
        int sourceSize = b.length;
        int dsize = (int)(sourceSize + (sourceSize * 0.1f) + 16); // NOLINT
        byte[] output = new byte[dsize];

        deflater.setInput(b);
        deflater.finish();
        int compressLength = deflater.deflate(output, 0, output.length);

        byte[] trimmed = new byte[compressLength];

        System.arraycopy(output,0, trimmed,0,compressLength);

        String retVal = trimmed.toString();
        return retVal;
    }
}