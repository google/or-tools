using System;
using System.IO;
using System.Security.Cryptography;

namespace CreateSigningKey {
  class Program {
    static void Main(string[] args) {
      if (args == null || args.Length == 0) {
        Console.WriteLine("Key filename not specified.");
        return;
      }
      string path = Directory.GetCurrentDirectory() + args[0];
      Console.WriteLine("Key filename:" + path);
      if (Console.Out!=null) Console.Out.Flush();
      File.WriteAllBytes(path, GenerateStrongNameKeyPair());
    }

    public static byte[] GenerateStrongNameKeyPair() {
      using (var provider = new RSACryptoServiceProvider(1024)) {
        return provider.ExportCspBlob(!provider.PublicOnly);
      }
    }
  }
}
