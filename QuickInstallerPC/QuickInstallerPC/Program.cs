using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.IO.Compression;
using System.Threading;

namespace QuickInstallerPC
{
    static class Program
    {
        //50MB
        const int FileSizeThreshold = 50 * 1024 * 1024;

        static string path_work = null;
        static string path_mp4 = null;
        static string path_vpk = null;

        static int data_count = 0;
        static int inst_count = 0;

        static List<string> meta_record = new List<string>();

        static void ProcessEachFile(string dir)
        {
            foreach (var d in Directory.GetDirectories(dir))            
                ProcessEachFile(d);

            foreach (var f in Directory.GetFiles(dir))
            {
                //Console.WriteLine("Processing: " + f.Substring(path_vpk.Length + 1));
                var info = new FileInfo(f);
                if(info.Length > FileSizeThreshold) {
                    var key = "qd_" + data_count.ToString("X4") + ".mp4";
                    var p = path_mp4 + Path.DirectorySeparatorChar + key;
                    File.Move(f, p);
                    meta_record.Add(key + " " + f.Substring(path_work.Length + 1));
                    ++data_count;
                }
            }
        }

        static void Process(string vpk)
        {
            var ext = vpk.Substring(vpk.Length - 3).ToLower();
            if (ext != "vpk") return;

            Console.WriteLine("Found vpk: " + vpk.Substring(vpk.LastIndexOf(Path.DirectorySeparatorChar) + 1));

            if (Directory.Exists(path_work))
            {
                Console.WriteLine("Removing temporary folder...");
                Directory.Delete(path_work, true);
                Thread.Sleep(4000);
            }
            Directory.CreateDirectory(path_work);
            Thread.Sleep(1000);

            Console.WriteLine("Extracting...");
            ZipFile.ExtractToDirectory(vpk, path_work, Encoding.UTF8);

            byte[] info = null;
            using (var fs = new FileStream(path_work + Path.DirectorySeparatorChar + "sce_sys" + Path.DirectorySeparatorChar + "param.sfo", FileMode.Open))
            {
                using (var buf = new MemoryStream())
                {
                    fs.Seek(-20, SeekOrigin.End);
                    fs.CopyTo(buf);
                    info = buf.ToArray();
                }
            }

            var appid = Encoding.ASCII.GetString(info, 0, 9);
            meta_record.Add("#" + appid);
            Console.WriteLine("App Id: " + appid);

            Console.WriteLine("Processing...");
            ProcessEachFile(path_work);

            Console.WriteLine("Compressing...");
            ZipFile.CreateFromDirectory(path_work, path_mp4 + Path.DirectorySeparatorChar + "qinst_" + inst_count.ToString("X2") + ".mp4",
                CompressionLevel.NoCompression, false, Encoding.UTF8);
                
            ++inst_count;
        }

        static void Main(string[] args)
        {
            path_work = Environment.CurrentDirectory + Path.DirectorySeparatorChar + "work";
            path_mp4 = Environment.CurrentDirectory + Path.DirectorySeparatorChar + "mp4";
            path_vpk = Environment.CurrentDirectory + Path.DirectorySeparatorChar + "vpk";

            if (!Directory.Exists(path_vpk))
            {
                Console.WriteLine("directory vpk is not found.");
                return;
            }

            if(Directory.Exists(path_mp4)) Directory.Delete(path_mp4, true);
            Directory.CreateDirectory(path_mp4);

            foreach(var t in Directory.GetFiles(path_vpk))
                Process(t);

            using (var fs = new FileStream(path_mp4 + Path.DirectorySeparatorChar + "qmeta.mp4", FileMode.CreateNew))
            {
                using (var writer = new StreamWriter(fs, Encoding.UTF8))
                {
                    foreach (var t in meta_record)
                    {
                        writer.WriteLine(t);
                    }
                }
            }

            Console.WriteLine("Done.");
            Console.ReadLine();
        }
    }
}
