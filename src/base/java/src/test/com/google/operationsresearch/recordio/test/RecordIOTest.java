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
import com.google.operationsresearch.recordio.test.TestProtos.TestProto;
import org.junit.Test;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;

import static org.junit.Assert.assertEquals;

public class RecordIOTest {

    @Test
    public final void testUncompressed() {
        ArrayList<TestProto> protos = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            protos.add(TestProto
                    .newBuilder()
                    .setName("uncompressed-" + i)
                    .setValue(String.valueOf(i))
                    .build());
        }

        // write uncompressed
        File file = new File("uncompressed.log");
        try(RecordWriter rw = new RecordWriter(file)) {
            rw.setUseCompression(false);
            for (TestProto p : protos) {
                rw.write(p.toByteArray());
            }
        } catch (FileNotFoundException e0) {
            e0.printStackTrace();
        } catch (Exception e1) {
            e1.printStackTrace();
        }

        // read uncompressed
        try(RecordReader rr = new RecordReader(file)) {
            ArrayList<TestProto> results = new ArrayList<>();

            for (int i = 0; i < 10; i++) {
                byte[] r = rr.read();
                    results.add(TestProto.parseFrom(r));
            }
            assertEquals(protos, results);

        } catch (FileNotFoundException e0) {
            e0.printStackTrace();
        } catch (IOException e1) {
            e1.printStackTrace();
        } catch (Exception e2) {
            e2.printStackTrace();
        } finally {
            file.deleteOnExit();
        }

    }

    @Test
    public  final void testCompressed() {
        ArrayList<TestProto> protos = new ArrayList<>();

        for(int i = 0; i < 10; i++) {
            protos.add(TestProto
                    .newBuilder()
                    .setName("compressed-" + i)
                    .setValue(String.valueOf(i))
                    .build());
        }

        // write compressed
        File file = new File("compressed.log");
        try(RecordWriter rw = new RecordWriter(file); RecordReader rr = new RecordReader(file)) {
            rw.setUseCompression(true);
            for (TestProto p : protos) {
                rw.write(p.toByteArray());
            }

            // read compressed
            ArrayList<TestProto> results = new ArrayList<>();

            for (int i = 0; i < 10; i++) {
                byte[] r = rr.read();
                results.add(TestProto.parseFrom(r));
            }

            assertEquals(protos, results);

        } catch (FileNotFoundException e0) {
            e0.printStackTrace();
        } catch (Exception e1) {
            e1.printStackTrace();
        } finally {
            file.deleteOnExit();
        }
    }
}
