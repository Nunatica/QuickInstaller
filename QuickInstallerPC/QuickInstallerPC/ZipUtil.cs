using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using ICSharpCode.SharpZipLib.Zip;

namespace LocalLib
{
    static class ZipUtil
    {
        static readonly FastZip handle;

        static ZipUtil ()
        {
            handle = new FastZip();
            handle.CompressionMethod = CompressionMethod.Stored;
        }

#if false
        public static void ExtractToDirectory(string zip, string dir)
        {
            ZipFile.ExtractToDirectory(zip, dir, Encoding.UTF8);
        }

        public static void CreateFromDirectory(string dir, string zip)
        {        
            ZipFile.CreateFromDirectory(dir, zip, CompressionLevel.Fastest, false, Encoding.UTF8);
        }        
#endif

        public static void ExtractToDirectory(string zip, string dir)
        {
            handle.ExtractZip(zip, dir, null);
        }

        public static void CreateFromDirectory(string dir, string zip)
        {
            handle.CreateZip(zip, dir, true, null);            
        }
    }
}