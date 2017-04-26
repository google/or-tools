// Copyright 2010-2014 Google
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

namespace Google.OrTools {

using System;
using Google.Protobuf;

public static class ProtoHelper
{
  public static byte[] ProtoToByteArray(IMessage message)
  {
    int size = message.CalculateSize();
    byte[] buffer = new byte[size];
    CodedOutputStream output = new CodedOutputStream(buffer);
    message.WriteTo(output);
    return buffer;
  }
}
}  // namespace Google.OrTools
