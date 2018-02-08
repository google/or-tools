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

            File.WriteAllBytes(args[0], GenerateStrongNameKeyPair());
        }

        public static byte[] GenerateStrongNameKeyPair()
        {
            using (var provider = new RSACryptoServiceProvider(1024, new CspParameters() { KeyNumber = 2 }))
            {
                return provider.ExportCspBlob(!provider.PublicOnly);
            }
        }
    }
}
