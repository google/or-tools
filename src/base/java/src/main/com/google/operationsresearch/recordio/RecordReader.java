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

import com.google.common.primitives.Ints;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.zip.Inflater;

import static com.google.operationsresearch.recordio.RecordIOUtils.b2i;
import static com.google.operationsresearch.recordio.RecordIOUtils.magicNumber;

public class RecordReader<T extends com.google.protobuf.Message> implements AutoCloseable {
    private FileInputStream file;
    private Inflater inflater;

    public RecordReader(File file) throws FileNotFoundException {
        this.file = new FileInputStream(file);
        this.inflater = new Inflater();
    }

    @Override
    public void close() throws IOException{
            file.close();
    }

    public final byte[] readProtocolMessage() throws Exception{
        byte[] usizeb = new byte[Ints.BYTES];
        byte[] csizeb = new byte[Ints.BYTES];
        byte[] magic_number = new byte[Ints.BYTES];
        byte[] retVal;

        file.read(magic_number, 0,Ints.BYTES);

        if (b2i(magic_number) != magicNumber) {
            throw new Exception("Unexpected magic number.");
        }
            file.read(usizeb, 0, Ints.BYTES);
            file.read(csizeb, 0, Ints.BYTES);
            int usize = b2i(usizeb);
            int csize = b2i(csizeb);

            if (csize != 0) {
                byte[] message = new byte[csize];
                file.read(message, 0, csize);
                retVal = uncompress(message, csize, usize);
                return retVal;
            } else {
                retVal = new byte[usize];
                file.read(retVal, 0, usize);
                return retVal;
            }
    }

    private final byte[] uncompress(byte[] message, int sourceSize, int outputSize) throws Exception{
        byte[] retVal = new byte[outputSize];
        inflater.reset();
        inflater.setInput(message,0, sourceSize);
        int resultLength = inflater.inflate(retVal);

        if(resultLength != outputSize) {
            throw new Exception("Decompression failed: expected size (" + resultLength + ") and actual size (" + outputSize + ") don't match.");
        }

        return retVal;
    }
}
