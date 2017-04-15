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
package com.google.operationsresearch.recordio.test;

import com.google.operationsresearch.recordio.RecordReader;
import com.google.operationsresearch.recordio.RecordWriter;
import com.google.operationsresearch.recordio.test.proto.TestOuterClass.Test;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.util.ArrayList;

public class RecordIOTest {

    public static void main(String[] args) {

        testUncompressed();
//        testCompressed();




        /* This doesn't work, because the Reader most likely has the generics wrong. */
        /*
        // read uncompressed
        File f = file.toFile();
        RecordReader rr = new RecordReader<>(f);
        ArrayList<TestOuterClass.Test> rs = new ArrayList<>();
        for (int i = 0; i < 10; i++) {

        } */
    }

    private static final void testUncompressed() {
        ArrayList<Test> protos = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            protos.add(Test
                    .newBuilder()
                    .setName("uncompressed-" + i)
                    .setValue(String.valueOf(i))
                    .build());
        }

        // write uncompressed
        File file = new File("uncompressed.log");
        RecordWriter rw = new RecordWriter(file);
        rw.setUseCompression(false);
        for(Test p : protos) {
            rw.writeProtocolMessage(p);
        }

        // read uncompressed
        RecordReader rr = new RecordReader(file);
        ArrayList<Test> results = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            byte[] r = rr.readProtocolMessage();
            try {
                results.add(Test.parseFrom(r));
            } catch(InvalidProtocolBufferException e0) {
                e0.printStackTrace();
            }
        }

        assert(protos.containsAll(results));
        assert(results.containsAll(protos));
    }

    private static final void testCompressed() {
        ArrayList<Test> protos = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            protos.add(Test
                    .newBuilder()
                    .setName("compressed-" + i)
                    .setValue(String.valueOf(i))
                    .build());
        }

        // write compressed
        File file = new File("compressed.log");
        RecordWriter rw = new RecordWriter(file);
        rw.setUseCompression(true);
        for(Test p : protos) {
            rw.writeProtocolMessage(p);
        }

        // read compressed
        RecordReader rr = new RecordReader(file);
        ArrayList<Test> results = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            byte[] r = rr.readProtocolMessage();
            try {
                results.add(Test.parseFrom(r));
            } catch(InvalidProtocolBufferException e0) {
                e0.printStackTrace();
            }
        }

        assert(protos.containsAll(results));
        assert(results.containsAll(protos));
    }
}
