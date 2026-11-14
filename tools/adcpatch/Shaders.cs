using System.Security.Cryptography;
using GTToolsSharp;
using GTToolsSharp.Encryption;
using GTToolsSharp.Volumes;

/// Finds GT Sport's PS4 shader programs in the game's volumes. Each program ends with a
/// 32-byte footer that starts with "OrbShdr"; the renderer identifies a program by the hash in
/// that footer (offset 16) and the footer gives the code length (bits 8-31 of offset 8).
static class GameShaders
{
    static readonly byte[] Signature = "OrbShdr"u8.ToArray();

    /// (hash, footer offset, code length) of every program in the data.
    public static List<(ulong hash, long footer, int length)> Find(byte[] data)
    {
        var found = new List<(ulong, long, int)>();
        int i = 0;
        while (true)
        {
            int at = data.AsSpan(i).IndexOf(Signature);
            if (at < 0) return found;
            i += at;
            if (i + 32 <= data.Length)
            {
                uint bits = BitConverter.ToUInt32(data, i + 8);
                ulong hash = BitConverter.ToUInt64(data, i + 16);
                found.Add((hash, i, (int)(bits >> 8)));
            }
            i += Signature.Length;
        }
    }

    /// The bytes a program is identified by: its code (up to 64 KB before the footer) and the
    /// footer. The same in every copy of the game.
    public static byte[] ProgramBytes(byte[] data, long footer, int length)
    {
        int len = Math.Clamp(length, 0, 1 << 16);
        long start = Math.Max(0, footer - len);
        long end = Math.Min(data.Length, footer + 32);
        return data[(int)start..(int)end];
    }

    /// Where the program whose footer is at `footer` starts: its first instruction is
    /// s_mov_b32 vcc_hi, imm (0xBEEB03FF) with the footer (imm + 1) * 2 dwords after it, as the
    /// renderer finds the footer. -1 when there is no such instruction.
    public static int CodeStart(byte[] data, long footer)
    {
        for (long p = footer - 8; p >= 0 && footer - p <= (1 << 20); p -= 4)
        {
            if (BitConverter.ToUInt32(data, (int)p) == 0xBEEB03FF &&
                p + ((long)BitConverter.ToUInt32(data, (int)p + 4) + 1) * 8 == footer)
                return (int)p;
        }
        return -1;
    }

    static readonly HashSet<string> SkipExtensions = new(StringComparer.OrdinalIgnoreCase)
    {
        ".JXR", ".JPG", ".PNG", ".MP4", ".SXD", ".SXD2", ".SXD3", ".AST", ".GTESD",
        ".OTF", ".TTF", ".SVG", ".JSON", ".TXT", ".ADC", ".MPROJECT", ".ECP", ".CSS", ".JS",
    };

    /// FIX-GTS-281: the given files of the volumes (the update's first; a path is read once),
    /// unpacked on several threads, each with its own volume handle and temp file; visit runs on
    /// those threads. Unpacking (decryption, decompression) is single-threaded per file, and one
    /// track pack after another kept one core busy. threads is kept low: each holds a whole file
    /// (track packs are hundreds of MB). Returns how many failed.
    public static int ForEachWantedFile(IEnumerable<string> indexFiles, string tempDir,
                                        Func<string, bool> wanted, Action<string, byte[]> visit,
                                        int threads)
    {
        int failed = 0;
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        Directory.CreateDirectory(tempDir);
        foreach (var index in indexFiles)
        {
            if (!File.Exists(index)) continue;
            var keyset = KeysetStore.Default_Keysets.FirstOrDefault(k =>
            {
                using var probe = GTVolumePFS.Load(k, index, false, Syroot.BinaryData.Core.Endian.Little);
                return probe != null;
            });
            if (keyset == null) continue;
            var work = new List<string>();
            using (var vol = GTVolumePFS.Load(keyset, index, false, Syroot.BinaryData.Core.Endian.Little))
            {
                foreach (var file in vol.BTree.GetAllRegisteredFiles())
                {
                    string path = file.Key.Replace('\\', '/');
                    if (!seen.Add(path) || !wanted(path)) continue;
                    work.Add(file.Key);
                }
            }
            int next = -1, slot = 0;
            var workers = new List<Thread>();
            for (int t = 0; t < Math.Max(1, threads); t++)
            {
                string temp = Path.Combine(tempDir, $"part{Interlocked.Increment(ref slot)}.bin");
                var thread = new Thread(() =>
                {
                    using var vol = GTVolumePFS.Load(keyset, index, false, Syroot.BinaryData.Core.Endian.Little);
                    var unpacker = new PFSVolumeUnpacker(vol);
                    var infos = new Dictionary<uint, GTToolsSharp.BTree.FileInfoKey>();
                    foreach (var info in vol.BTree.FileInfos.Entries) infos[info.FileIndex] = info;
                    var files = vol.BTree.GetAllRegisteredFiles();
                    for (int i = Interlocked.Increment(ref next); i < work.Count; i = Interlocked.Increment(ref next))
                    {
                        string key = work[i];
                        string path = key.Replace('\\', '/');
                        try
                        {
                            if (File.Exists(temp)) File.Delete(temp);
                            if (!unpacker.UnpackFile(infos[files[key].EntryIndex], path, temp) || !File.Exists(temp)) continue;
                            visit(path, File.ReadAllBytes(temp));
                        }
                        catch (Exception e)
                        {
                            Interlocked.Increment(ref failed);
                            Console.Error.WriteLine($"skipped {path}: {e.GetType().Name}: {e.Message}");
                        }
                    }
                    if (File.Exists(temp)) File.Delete(temp);
                });
                thread.Start();
                workers.Add(thread);
            }
            foreach (var thread in workers) thread.Join();
        }
        return failed;
    }

    /// Walks every file of the volumes (the update's first; a path is read once) and calls
    /// visit(path, bytes) for each file that may hold shaders. FIX-GTS-281: a file that fails to
    /// unpack or to scan is reported and skipped (one bad file ended a 95 GB scan with nothing
    /// written); returns how many failed.
    public static int ForEachFile(IEnumerable<string> indexFiles, string tempDir,
                                  Action<string, byte[]> visit, Func<string, bool> wanted = null)
    {
        int failed = 0;
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        Directory.CreateDirectory(tempDir);
        string temp = Path.Combine(tempDir, "scan.bin");
        foreach (var index in indexFiles)
        {
            if (!File.Exists(index)) continue;
            GTVolumePFS vol = null;
            foreach (var keyset in KeysetStore.Default_Keysets)
            {
                vol = GTVolumePFS.Load(keyset, index, false, Syroot.BinaryData.Core.Endian.Little);
                if (vol != null) break;
            }
            if (vol == null) continue;
            using (vol)
            {
                var unpacker = new PFSVolumeUnpacker(vol);
                var infos = new Dictionary<uint, GTToolsSharp.BTree.FileInfoKey>();
                foreach (var info in vol.BTree.FileInfos.Entries) infos[info.FileIndex] = info;
                foreach (var file in vol.BTree.GetAllRegisteredFiles())
                {
                    string path = file.Key.Replace('\\', '/');
                    if (!seen.Add(path) || SkipExtensions.Contains(Path.GetExtension(path))) continue;
                    if (wanted != null && !wanted(path)) continue; // FIX-GTS-281: only these files
                    try
                    {
                        if (File.Exists(temp)) File.Delete(temp);
                        var quiet = Console.Out;
                        Console.SetOut(TextWriter.Null);
                        bool ok;
                        try { ok = unpacker.UnpackFile(infos[file.Value.EntryIndex], path, temp); }
                        finally { Console.SetOut(quiet); }
                        if (!ok || !File.Exists(temp)) continue;
                        visit(path, File.ReadAllBytes(temp));
                    }
                    catch (Exception e)
                    {
                        failed++;
                        Console.Error.WriteLine($"skipped {path}: {e.GetType().Name}: {e.Message}");
                    }
                }
            }
        }
        if (File.Exists(temp)) File.Delete(temp);
        return failed;
    }

}
