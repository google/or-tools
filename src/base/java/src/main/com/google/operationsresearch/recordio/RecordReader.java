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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.zip.Inflater;

import static com.google.operationsresearch.recordio.RecordIOUtils.b2i;
import static com.google.operationsresearch.recordio.RecordIOUtils.magicNumber;

public class RecordReader<T extends com.google.protobuf.Message> {
    private FileInputStream file;
    private Inflater inflater;

    public RecordReader(File file) {
        try {
            this.file = new FileInputStream(file);
            inflater = new Inflater();
        } catch (FileNotFoundException e0) {
            e0.printStackTrace();
        }
    }

    public RecordReader() {
        inflater = new Inflater();
    }

    public void close() {
        try {
            file.close();
        } catch (IOException e0) {
            // swallow
        }
    }

    public final byte[] readProtocolMessage() {
        byte[] usizeb = new byte[4];
        byte[] csizeb = new byte[4];
        byte[] retVal = new byte[4];

        try {
            byte[] magic_number = new byte[4];
            file.read(magic_number, 0,4);

            try {
                if (b2i(magic_number) != magicNumber) {
                    throw new Exception("Unexpected magic number.");
                }
            } catch (Exception e0) {
                e0.printStackTrace();
            }

            file.read(usizeb, 0, 4);
            file.read(csizeb, 0, 4);
            int usize = b2i(usizeb);
            int csize = b2i(csizeb);

            if (csize != 0) {
                byte[] message = new byte[csize];
                file.read(message, 0, csize);
                try {
                    retVal = uncompress(csize, usize, message);
                    return retVal;
                } catch (Exception e0) {
                    e0.printStackTrace();
                }
            } else {
                retVal = new byte[usize];
                file.read(retVal, 0, usize);
                return retVal;
            }

        } catch (IOException e0) {
            e0.printStackTrace();
        }
        return retVal; // ugly
    }

    public final byte[] uncompress(int sourceSize, int outputSize, byte[] message) throws Exception{
        byte[] retVal = new byte[outputSize];
        inflater.reset();
        inflater.setInput(message,0, sourceSize);
        int resultLength = inflater.inflate(retVal);
        //inflater.end();
        if(resultLength != outputSize) {
            throw new Exception("Decompression failed: expected size (" + resultLength + ") and actual size (" + outputSize + ") don't match.");
        }

        return retVal;
    }
}
