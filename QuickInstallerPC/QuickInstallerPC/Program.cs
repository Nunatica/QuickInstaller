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
        const int FileSizeThreshold = 20 * 1024 * 1024;

        static byte[] header_buf = new byte[4];
        static byte[] header_dummy = new byte[] { 0, 0, 0, 0 };

        static string path_work = null;
        static string path_mp4 = null;
        static string path_vpk = null;
        static string app_id = null;

        static int data_count = 0;
        static int inst_count = 0;

        static List<string> meta_record = new List<string>();

        static string ConvertBinaryToHex(byte[] arr)
        {
            var sb = new StringBuilder();
            foreach (var t in arr)
                sb.AppendFormat("{0:X2}", t);
            return sb.ToString();
        }

        static void ProcessEachFile(string dir)
        {
            foreach (var d in Directory.GetDirectories(dir))            
                ProcessEachFile(d);

            foreach (var f in Directory.GetFiles(dir))
            {
                var info = new FileInfo(f);

                if(info.Length > FileSizeThreshold)
                {
                    using (var fs = new FileStream(f, FileMode.Open))
                    {
                        fs.Seek(0, SeekOrigin.Begin);
                        fs.Read(header_buf, 0, 4);
                        fs.Seek(0, SeekOrigin.Begin);
                        fs.Write(header_dummy, 0, 4);
                    }

                    //Console.WriteLine("Processing: " + f.Substring(path_vpk.Length + 1));
                    var key = "qd_" + data_count.ToString("X4") + ".mp4";
                    var p = path_mp4 + Path.DirectorySeparatorChar + key;
                    File.Move(f, p);
                    var record = "ux0:app/" + app_id + "/" + f.Substring(path_work.Length + 1).Replace('\\', '/') + " " + ConvertBinaryToHex(header_buf);
                    meta_record.Add(record);
                    ++data_count;
                }
            }
        }

        static void Process(string vpk)
        {
            var ext = vpk.Substring(vpk.Length - 3).ToLower();
            if (ext != "vpk") return;

            Console.WriteLine("Found vpk: " + vpk.Substring(vpk.LastIndexOf(Path.DirectorySeparatorChar) + 1));

            if (new FileInfo(vpk).Length < FileSizeThreshold)
            {
                File.Copy(vpk, path_mp4 + Path.DirectorySeparatorChar + "qinst_" + inst_count.ToString("X2") + ".mp4");
                ++inst_count;
                return;
            }

            if (Directory.Exists(path_work))
            {
                Console.WriteLine("Removing temporary folder...");
                Directory.Delete(path_work, true);
                while(Directory.Exists(path_work)) { Thread.Sleep(100);  }
            }
            Directory.CreateDirectory(path_work);
            while (!Directory.Exists(path_work)) { Thread.Sleep(100); }

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

            app_id = Encoding.ASCII.GetString(info, 0, 9);
            Console.WriteLine("App Id: " + app_id);
            
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
                Console.WriteLine("Directory vpk is not found.");
                return;
            }

            if (Directory.Exists(path_mp4))
            {
                Directory.Delete(path_mp4, true);
                while (Directory.Exists(path_mp4)) { Thread.Sleep(100); }
            }

            Directory.CreateDirectory(path_mp4);
            while (!Directory.Exists(path_mp4)) { Thread.Sleep(100); }

            foreach (var t in Directory.GetFiles(path_vpk))
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
        }
    }
}
