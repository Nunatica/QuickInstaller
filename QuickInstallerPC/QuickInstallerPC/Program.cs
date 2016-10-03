using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using LocalLib;

namespace QuickInstallerPC
{
    static class Program
    {
        const int FileSizeThreshold = 5 * 1024 * 1024;

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

        static void ProcessSafeMode(string path)
        {
            using(var fs = new FileStream(path, FileMode.Open))
            {
                fs.Position = 0x80;

                if (fs.ReadByte() == 1)
                {
                    fs.Position--;
                    fs.WriteByte(2);
                    Console.WriteLine("Converted as safe mode: " + path);
                }
            }
        }

        static void ProcessEachFile(string dir)
        {
            //Console.WriteLine("Working directory: " + dir.Substring(Environment.CurrentDirectory.Length));
            int remaining_entry = 0;

            foreach (var d in Directory.GetDirectories(dir))
            {
                ProcessEachFile(d);
                remaining_entry += 1;
            }

            foreach (var f in Directory.GetFiles(dir))
            {
                if (f.Contains("eboot.bin"))
                {
                    ProcessSafeMode(f);
                    return;
                }

                var info = new FileInfo(f);
                if (info.Length > FileSizeThreshold)
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
                else
                    remaining_entry += 1;
            }

            if(remaining_entry == 0)
            {
                string path = null;
                for (int u = 0; u < 10; ++u) {
                    path = dir + Path.DirectorySeparatorChar + "qinstph" + u;
                    if (File.Exists(path))
                        path = null;
                    else
                        break;
                }

                if (path == null) throw new Exception("Failed to create placeholder.");

                using (var fs = new FileStream(path, FileMode.CreateNew))
                {
                    fs.Write(header_dummy, 0, 4);
                }
            }
        }

        static void ProcessWork()
        {
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
            Directory.Delete(path_work + Path.DirectorySeparatorChar + "sce_sys" + Path.DirectorySeparatorChar + "manual", true);
            foreach (var d in Directory.GetDirectories(path_work))
            {
                if (d.Contains("sce_module")) continue;
                if (d.Contains("sce_sys")) continue;
                ProcessEachFile(d);
            }

            Console.WriteLine("Packing...");
            ZipUtil.CreateFromDirectory(path_work, path_mp4 + Path.DirectorySeparatorChar + "qinst_" + inst_count.ToString("X2") + ".mp4");

            ++inst_count;
        }

        static void ProcessDirectory(string dir)
        {
            if (Directory.Exists(path_work))
            {
                Console.WriteLine("Removing temporary folder...");
                Directory.Delete(path_work, true);
                while (Directory.Exists(path_work)) { Thread.Sleep(100); }
            }

            new DirectoryInfo(dir).MoveTo(path_work);
            while (!Directory.Exists(path_work)) { Thread.Sleep(100); }

            ProcessWork();
        }

        static void ProcessVPK(string vpk)
        {
            var ext = vpk.Substring(vpk.Length - 3).ToLower();
            if (ext != "vpk") return;

            Console.WriteLine("Found vpk: " + vpk.Substring(vpk.LastIndexOf(Path.DirectorySeparatorChar) + 1));

            if (Directory.Exists(path_work))
            {
                Console.WriteLine("Removing temporary folder...");
                Directory.Delete(path_work, true);
                while(Directory.Exists(path_work)) { Thread.Sleep(100);  }
            }

            Directory.CreateDirectory(path_work);
            while (!Directory.Exists(path_work)) { Thread.Sleep(100); }

            Console.WriteLine("Extracting...");
            ZipUtil.ExtractToDirectory(vpk, path_work);

            ProcessWork();
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
                ProcessVPK(t);
            foreach (var t in Directory.GetDirectories(path_vpk))
                ProcessDirectory(t);
            
            Console.WriteLine("Removing temporary folder...");
            Directory.Delete(path_work, true);
            while (Directory.Exists(path_work)) { Thread.Sleep(100); }

            using (var fs = new FileStream(path_mp4 + Path.DirectorySeparatorChar + "qmeta.mp4", FileMode.CreateNew))
            {
                using (var writer = new StreamWriter(fs, new UTF8Encoding(false)))
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
