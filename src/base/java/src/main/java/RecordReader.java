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

import com.google.protobuf.Extension;
import com.google.protobuf.Parser;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.math.BigInteger;
import java.nio.charset.Charset;
import java.util.zip.Inflater;

public class RecordReader<T extends Parser> {
    private FileInputStream file;
    private Charset charset = Charset.forName("UTF-8");
    private Parser parser;
    private Inflater inflater;

    public int kMagicNumber = 0x3ed7230a;


    public RecordReader(File file) {
        try {
            this.file = new FileInputStream(file);
            inflater = new Inflater();
        } catch (FileNotFoundException e0) {
            e0.printStackTrace();
        }
    }

    public void close() {
        try {
            file.close();
        } catch (IOException e0) {
            // swallow
        }
    }

    public final boolean readProtocolMessage(T m) {
        byte[] usizeb = new byte[4];
        byte[] csizeb = new byte[4];

        try {
            byte[] magic_number = new byte[4];
            file.read(magic_number, 0,4);

            if (b2i(magic_number) != kMagicNumber) {
                return false;
            }

            file.read(usizeb, 0, 4);
            file.read(csizeb, 0, 4);
            int usize = b2i(usizeb);
            int csize = b2i(csizeb);

            if (csize != 0) {
                byte[] message = new byte[csize];
                file.read(message, 0, csize);
                try {
                    m.parseFrom(uncompress(csize, usize, message));
                } catch (Exception e0) {
                    e0.printStackTrace();
                }
            } else {
                byte[] message = new byte[usize];
                file.read(message, 0, usize);
                m.parseFrom(message);
            }

        } catch (IOException e0) {
            e0.printStackTrace();
            return false;
        }
        return true;
    }

    private final int b2i(byte[] b) {
        return new BigInteger(b).intValue();
    }

    private final byte[] uncompress(int sourceSize, int outputSize, byte[] message) throws Exception{
        byte[] retVal = new byte[outputSize];
        inflater.setInput(message,0, sourceSize);
        int resultLength = inflater.inflate(retVal);
        inflater.end();
        if(resultLength != outputSize) {
            throw new Exception("Decompression failed: expected size (" + resultLength + ") and actual size (" + outputSize + ") don't match.");
        }

        return retVal;
    }
}
