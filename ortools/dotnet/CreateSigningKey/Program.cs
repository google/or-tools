// Copyright 2010-2021 Google LLC
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

using System;
using System.IO;
using System.Security.Cryptography;

namespace CreateSigningKey
{
class Program
{
    static void Main(string[] args)
    {
        if (args == null || args.Length == 0)
        {
            Console.WriteLine("Key filename not specified.");
            return;
        }
        string path = Directory.GetCurrentDirectory() + args[0];
        Console.WriteLine("Key filename:" + path);
        if (Console.Out != null)
            Console.Out.Flush();
        File.WriteAllBytes(path, GenerateStrongNameKeyPair());
    }

    public static byte[] GenerateStrongNameKeyPair()
    {
        using (var provider = new RSACryptoServiceProvider(4096))
        {
            return provider.ExportCspBlob(!provider.PublicOnly);
        }
    }
}
}
