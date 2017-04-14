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

import com.google.operationsresearch.recordio.RecordWriter;
import com.google.operationsresearch.recordio.test.proto.TestOuterClass;

import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.util.ArrayList;

public class RecordIOTest {
    public static void main(String[] args) {

        ArrayList<TestOuterClass.Test> ds = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            ds.add(TestOuterClass.Test
                    .newBuilder()
                    .setName("uncompressed-" + i)
                    .setValue(String.valueOf(i))
                    .build());
        }


        // write uncompressed
        Path file = FileSystems.getDefault().getPath(".", "test.log");
        RecordWriter rw = new RecordWriter(file);
        rw.setUseCompression(false);
        for(TestOuterClass.Test d : ds) {
            rw.writeProtocolMessage(d); // uncompressed may have the wrong contract. Writer may be better off using .writeToOutputStream direclty on the message.
        }

        /* This doesn't work, because the Reader most likely has the generics wrong. */
        /*
        // read uncompressed
        File f = file.toFile();
        RecordReader rr = new RecordReader<>(f);
        ArrayList<TestOuterClass.Test> rs = new ArrayList<>();
        for (int i = 0; i < 10; i++) {

        } */
    }
}
