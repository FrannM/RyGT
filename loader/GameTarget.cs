// FEAT-GTS-290: one launcher for Gran Turismo Sport and Gran Turismo 7. What differs between the two
// games' folders and launches, and the launcher's own small settings file (RyGT-Launcher.ini beside
// RyGTLauncher.exe): the game picked last and each game's folder.
// SPDX-License-Identifier: GPL-2.0-only

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;

namespace RyGT
{
    /// The order is the home view's game list.
    enum GameKind { GTSport = 0, GT7 = 1 }

    /// A game's names, files and colours. GT Sport's folder is a RyGTS release (GTSport.exe), GT7's a ryGT7
    /// release (GT7.exe, patches\CUSA24769.xml), and since FEAT-GTS-291 both may be one folder: every file a
    /// game writes there is named after its serial (TitleId): its settings (user\custom_configs\<TitleId>.json,
    /// GAME-004), shaders (shaders\<TitleId>*), saves (user\savedata\<TitleId>\_sd and \download0, GAME-005),
    /// logs (logs\<TitleId>), app0 (app0\<TitleId>, App0Layout), the launcher's settings (<TitleId>.RyGT.ini,
    /// where GT Sport had RyGT.ini and GT7 RyGT7.ini) and its Set up, temp0 and data folders
    /// (convert\<TitleId>, temp0\<TitleId>, data\<TitleId>; SharedNames). The runtime and renderer libraries
    /// are one build for both games (libs\libSceMbus.prx included), and user\input_config\default.ini is
    /// the bindings file both share.
    sealed class GameInfo
    {
        public GameKind Kind;
        public string Key;          // "gts" or "gt7" (RyGT-Launcher.ini, --game)
        public string Name;         // "Gran Turismo Sport"
        public string Short;        // "GT Sport"
        public string TitleId;      // "CUSA03220"
        public string ExeName;      // "GTSport.exe"
        public string ProcessName;  // "GTSport"
        public string IniName;      // "CUSA03220.RyGT.ini" (FEAT-GTS-291: <TitleId>.RyGT.ini)
        public string Product;      // "RyGTS": the release whose folder the game is in
        public string TitleAccent;  // "SPORT": the home view's title is GRAN TURISMO <accent>
        public string WindowTitle;
        /// The launcher's accent while this game is picked: red for GT Sport, blue for GT7.
        public Color Accent, AccentHover, AccentText;

        public static readonly GameInfo GTSport = new GameInfo
        {
            Kind = GameKind.GTSport, Key = "gts", Name = "Gran Turismo Sport", Short = "GT Sport", TitleId = "CUSA03220",
            ExeName = "GTSport.exe", ProcessName = "GTSport", IniName = "CUSA03220.RyGT.ini", Product = "RyGTS", TitleAccent = "SPORT",
            WindowTitle = "RyGT Launcher - Gran Turismo Sport",
            Accent = Color.FromArgb(178, 24, 36), AccentHover = Color.FromArgb(206, 40, 52), AccentText = Color.FromArgb(240, 92, 76)
        };

        public static readonly GameInfo GT7 = new GameInfo
        {
            Kind = GameKind.GT7, Key = "gt7", Name = "Gran Turismo 7", Short = "GT7", TitleId = "CUSA24769",
            ExeName = "GT7.exe", ProcessName = "GT7", IniName = "CUSA24769.RyGT.ini", Product = "ryGT7", TitleAccent = "7",
            WindowTitle = "RyGT Launcher - Gran Turismo 7",
            Accent = Color.FromArgb(24, 96, 196), AccentHover = Color.FromArgb(44, 120, 224), AccentText = Color.FromArgb(92, 162, 255)
        };

        public static GameInfo Of(GameKind kind) { return kind == GameKind.GT7 ? GT7 : GTSport; }

        /// GAME-004: the game's settings in a folder, user\custom_configs\<TitleId>.json, as the runtime and
        /// the renderer read them (CUSA03220.json for GT Sport, CUSA24769.json for GT7).
        public string ConfigPath(string folder) { return Path.Combine(folder, "user", "custom_configs", TitleId + ".json"); }

        /// FEAT-GTS-291: the launcher's settings for the game in a folder, <TitleId>.RyGT.ini, which the runtime
        /// reads too (game=, update=, stdout_level=, online play's keys; core/libs/GameSerial.hpp).
        public string IniPath(string folder) { return Path.Combine(folder, IniName); }

        /// FEAT-GTS-291: the game's own folder in a runtime folder's convert (Set up's work folder), temp0 or
        /// data: convert\<TitleId>, temp0\<TitleId>, data\<TitleId>.
        public string OwnFolder(string folder, string name) { return Path.Combine(folder, name, TitleId); }

        public static GameKind Other(GameKind kind) { return kind == GameKind.GT7 ? GameKind.GTSport : GameKind.GT7; }

        /// "gts", "gt7", "gtsport", "sport" or "7" (any case); null for anything else.
        public static GameKind? Parse(string text)
        {
            string key = (text ?? "").Trim().ToLowerInvariant();
            if (key == "gts" || key == "gtsport" || key == "sport" || key == "gt sport") return GameKind.GTSport;
            if (key == "gt7" || key == "7") return GameKind.GT7;
            return null;
        }
    }

    /// GAME-004: GT7's files follow its own serial, CUSA24769, where ryGT7 named them after GT Sport's
    /// (CUSA03220). In a GT7 folder the launcher renames the old names to the new ones: the settings
    /// (user\custom_configs\CUSA03220.json), every shaders\CUSA03220* file and folder (the manifest, the
    /// direct sets, the pipeline caches) and user\input_config\CUSA03220*. Only when the new name is free:
    /// nothing is overwritten, and a second run finds nothing to do. GT Sport's folders are never touched.
    static class SerialMigration
    {
        public const string OldSerial = "CUSA03220";

        /// Renames GT7's old-serial files in folder; a line for the log for each one moved or that could
        /// not be (a file in use stays, and is tried again next time). Nothing for a folder that is not
        /// GT7's (LauncherState.LooksLikeGt7) or has nothing to rename.
        public static List<string> MigrateGt7(string folder)
        {
            var notes = new List<string>();
            if (string.IsNullOrEmpty(folder) || !Directory.Exists(folder) || !LauncherState.LooksLikeGt7(folder)) return notes;
            // FEAT-GTS-291: GT Sport may share GT7's folder now, and there CUSA03220 files are GT Sport's own:
            // only a folder whose old files are GT7's (no GTSport.exe) and that GT Sport has not used (no
            // CUSA03220.RyGT.ini, logs\CUSA03220 or app0\CUSA03220) is renamed.
            if (!SharedNames.OwnsOldFiles(folder, GameKind.GT7) || File.Exists(GameInfo.GTSport.IniPath(folder)) ||
                Directory.Exists(Path.Combine(folder, "logs", OldSerial)) || Directory.Exists(App0Layout.Folder(folder, OldSerial))) return notes;
            string serial = GameInfo.GT7.TitleId;
            string configs = Path.Combine(folder, "user", "custom_configs");
            Move(notes, folder, Path.Combine(configs, OldSerial + ".json"), Path.Combine(configs, serial + ".json"));
            foreach (string sub in new[] { "shaders", Path.Combine("user", "input_config") })
            {
                string dir = Path.Combine(folder, sub);
                string[] entries;
                try { entries = Directory.Exists(dir) ? Directory.GetFileSystemEntries(dir, OldSerial + "*") : new string[0]; }
                catch (Exception e) { notes.Add("Could not list " + sub + ": " + e.Message); continue; }
                Array.Sort(entries, StringComparer.OrdinalIgnoreCase);
                foreach (string entry in entries)
                {
                    string name = Path.GetFileName(entry);
                    if (!name.StartsWith(OldSerial, StringComparison.OrdinalIgnoreCase)) continue;
                    Move(notes, folder, entry, Path.Combine(dir, serial + name.Substring(OldSerial.Length)));
                }
            }
            return notes;
        }

        static void Move(List<string> notes, string folder, string from, string to)
        {
            string shown = from.StartsWith(folder, StringComparison.OrdinalIgnoreCase) ? from.Substring(folder.Length).TrimStart('\\') : from;
            string shownTo = Path.GetFileName(to);
            try
            {
                bool file = File.Exists(from), dir = !file && Directory.Exists(from);
                if (!file && !dir) return;
                if (File.Exists(to) || Directory.Exists(to)) return; // never overwrite: the new name wins
                if (file) File.Move(from, to); else Directory.Move(from, to);
                notes.Add("GAME-004: renamed " + shown + " to " + shownTo + " (GT7's serial is " + GameInfo.GT7.TitleId + ").");
            }
            catch (Exception e) { notes.Add("GAME-004: could not rename " + shown + " to " + shownTo + ": " + e.Message); }
        }
    }

    /// FEAT-GTS-291: RyGTLauncher.exe replaced the two launchers before it, RyGTS.exe (GT Sport) and
    /// RyGT7.exe (GT7). A folder updated by one of them still holds it, so at start the launcher renames
    /// an older launcher in its own folder to <name>.old: players keep one launcher, and a shortcut to
    /// the old name fails instead of starting an outdated one. Only a file with that name whose version
    /// information names the old launcher's product is renamed, never a game or anything else.
    static class OldLaunchers
    {
        /// Each older launcher's file name and the ProductName in its version information.
        static readonly string[][] Known = { new[] { "RyGTS.exe", "RyGTS" }, new[] { "RyGT7.exe", "ryGT7" } };

        /// Renames the older launchers in folder (never self, the running launcher) to <name>.old. Best
        /// effort: returns a line for the log for each one renamed or that could not be.
        public static List<string> Retire(string folder, string self)
        {
            var notes = new List<string>();
            foreach (var known in Known)
            {
                string exe = Path.Combine(folder, known[0]), old = exe + ".old";
                try
                {
                    if (!File.Exists(exe)) continue;
                    if (!string.IsNullOrEmpty(self) && string.Equals(Path.GetFullPath(exe), Path.GetFullPath(self), StringComparison.OrdinalIgnoreCase)) continue;
                    if (!string.Equals(System.Diagnostics.FileVersionInfo.GetVersionInfo(exe).ProductName, known[1], StringComparison.Ordinal)) continue;
                    if (File.Exists(old)) File.Delete(old); // an older launcher renamed before
                    File.Move(exe, old);
                    notes.Add("Renamed the older launcher " + known[0] + " to " + known[0] + ".old (" + Updater.LauncherExe + " replaces it).");
                }
                catch (Exception e) { notes.Add("Could not rename the older launcher " + known[0] + " to " + known[0] + ".old: " + e.Message); }
            }
            return notes;
        }

        /// After a roll back to a release from before RyGTLauncher.exe: the older launcher Retire
        /// renamed, back under its name, to start instead (or the one still there). Null if none.
        public static string PutBack(string folder)
        {
            foreach (var known in Known)
            {
                string exe = Path.Combine(folder, known[0]);
                try
                {
                    if (!File.Exists(exe) && File.Exists(exe + ".old")) File.Move(exe + ".old", exe);
                    if (File.Exists(exe)) return exe;
                }
                catch { }
            }
            return null;
        }
    }

    /// RyGT-Launcher.ini beside the launcher: "game=gts|gt7" (the game picked last) and "gts_root=" /
    /// "gt7_root=" (each game's folder; empty: found as below). Other lines are kept as they are. Each
    /// game's own settings stay in its folder's <TitleId>.RyGT.ini (FEAT-GTS-291; RyGT.ini and RyGT7.ini before).
    sealed class LauncherState
    {
        public const string FileName = "RyGT-Launcher.ini";
        readonly string launcherRoot, path;
        readonly List<string> otherLines = new List<string>();
        readonly Dictionary<GameKind, string> roots = new Dictionary<GameKind, string> { { GameKind.GTSport, "" }, { GameKind.GT7, "" } };
        public GameKind Last;
        /// Set for the tests and --setup --game: nothing is written back.
        public bool Transient;

        public LauncherState(string launcherRoot)
        {
            this.launcherRoot = launcherRoot;
            path = Path.Combine(launcherRoot, FileName);
            // Without a file: the game whose folder the launcher is in (GT Sport unless it is a ryGT7 folder).
            Last = LooksLikeGt7(launcherRoot) ? GameKind.GT7 : GameKind.GTSport;
            if (!File.Exists(path)) return;
            try
            {
                foreach (var line in File.ReadAllLines(path))
                {
                    int eq = line.IndexOf('=');
                    string key = eq < 0 ? "" : line.Substring(0, eq).Trim(), value = eq < 0 ? "" : line.Substring(eq + 1).Trim();
                    if (key == "game") { var game = GameInfo.Parse(value); if (game.HasValue) Last = game.Value; }
                    else if (key == "gts_root") roots[GameKind.GTSport] = value;
                    else if (key == "gt7_root") roots[GameKind.GT7] = value;
                    else otherLines.Add(line);
                }
            }
            catch { }
        }

        public void Save()
        {
            if (Transient) return;
            var lines = new List<string> { "game=" + GameInfo.Of(Last).Key, "gts_root=" + roots[GameKind.GTSport], "gt7_root=" + roots[GameKind.GT7] };
            lines.AddRange(otherLines);
            File.WriteAllLines(path, lines);
        }

        /// The folder as set (empty: the default below).
        public string Setting(GameKind game) { return roots[game]; }

        /// Sets a game's folder; the default folder (or nothing) is kept as empty, so it follows the launcher.
        public void SetRoot(GameKind game, string folder)
        {
            // GT Sport's folder is the launcher's own (it has no folder field); only a transient session
            // (the tests, --setup --game gts with a folder) may point it elsewhere.
            if (game == GameKind.GTSport && !Transient) return;
            folder = (folder ?? "").Trim().TrimEnd('\\');
            roots[game] = string.Equals(folder, DefaultRoot(game), StringComparison.OrdinalIgnoreCase) ? "" : folder;
        }

        /// The game's folder: the one set, else the launcher's own folder when the game can use it (GT Sport
        /// always: it has no folder field; GT7 in a ryGT7 folder), else empty.
        public string RootOf(GameKind game)
        {
            string set = game == GameKind.GTSport && !Transient ? "" : roots[game];
            if (set.Length == 0) return DefaultRoot(game);
            try { return Path.GetFullPath(set).TrimEnd('\\'); }
            catch { return ""; } // not a path: as if none were set (the Game page shows what was typed)
        }

        string DefaultRoot(GameKind game)
        {
            // FEAT-GTS-291: GT Sport's folder is always the launcher's, a ryGT7 folder included (both games
            // may share one folder now).
            if (game == GameKind.GTSport) return launcherRoot;
            return LooksLikeGt7(launcherRoot) ? launcherRoot : "";
        }

        /// Whether the game can be set up and played from its folder: it exists, and for GT7 it is a ryGT7
        /// release (GT7's patch and the like; GT Sport takes any folder). FEAT-GTS-291: both games may use
        /// ONE folder, a ryGT7 folder for GT Sport too: nothing either game writes there collides any more
        /// (each name follows the game's serial, see GameInfo, and libs\libSceMbus.prx is one file for both).
        public bool Ready(GameKind game)
        {
            string folder = RootOf(game);
            if (folder.Length == 0 || !Directory.Exists(folder)) return false;
            return game != GameKind.GT7 || LooksLikeGt7(folder);
        }

        /// A ryGT7 release folder: its older launcher (RyGT7.exe, or RyGT7.exe.old once OldLaunchers renamed
        /// it), its settings (RyGT7.ini, CUSA24769.RyGT.ini since FEAT-GTS-291), its game, or GT7's patch (a
        /// package that ships RyGTLauncher.exe instead).
        public static bool LooksLikeGt7(string folder)
        {
            if (string.IsNullOrEmpty(folder)) return false;
            try
            {
                return File.Exists(Path.Combine(folder, "RyGT7.exe")) || File.Exists(Path.Combine(folder, "RyGT7.exe.old")) ||
                       File.Exists(Path.Combine(folder, "RyGT7.ini")) || File.Exists(GameInfo.GT7.IniPath(folder)) ||
                       File.Exists(Path.Combine(folder, "GT7.exe")) || File.Exists(Path.Combine(folder, "patches", "CUSA24769.xml"));
            }
            catch { return false; }
        }
    }

    /// FEAT-GTS-291: each game's own app0 in a runtime folder is app0\<serial> (app0\CUSA03220 for GT Sport,
    /// app0\CUSA24769 for GT7), so two games can share one folder. It is the first layer of the game's /app0
    /// (the update and the base game folders come after it; core/libs/prx/libc/include/AppOverlay.hpp) and
    /// holds what Set up made: the converted sce_module.
    ///
    /// The converted executable loads its modules from the path the converter wrote into it
    /// (app0\sce_module\*.guest.prx before, app0\<serial>\sce_module\*.guest.prx with --app0), before the
    /// runtime starts. So a folder set up before keeps its app0 as it is until Set up runs again, and
    /// Set up then moves app0 into app0\<serial> (Migrate) before it writes the new modules there. The
    /// new layout needs a converter with --app0 and runtime libraries that read app0\<serial>
    /// (Supported); with older ones Set up writes app0 as before.
    static class App0Layout
    {
        /// In libs\libc.prx when the runtime maps /app0 to app0\<serial> (core/libs/prx/libc/src/General.cpp).
        public const string RuntimeMarker = "RYGT_APP0_PER_GAME/1";
        /// In converter\ryty.exe (its usage text) when it takes --app0.
        public const string ConverterMarker = "[--app0 <folder>]";
        const string MovingPrefix = "app0.moving-";

        /// The game's own app0 folder: app0\<serial>.
        public static string Folder(string root, string serial) { return Path.Combine(root, "app0", serial); }

        /// The folder the game's /app0 starts at today, as the runtime picks it (Libc::App0Top): app0\<serial>
        /// when it exists, else app0 (a folder set up before).
        public static string Top(string root, string serial)
        {
            string own = Folder(root, serial);
            return Directory.Exists(own) ? own : Path.Combine(root, "app0");
        }

        /// Whether this folder's converter and runtime libraries know app0\<serial>.
        public static bool Supported(string root)
        {
            return FileContains(Path.Combine(root, "libs", "libc.prx"), RuntimeMarker) &&
                   FileContains(Path.Combine(root, "converter", "ryty.exe"), ConverterMarker);
        }

        /// The folder Set up writes the converted modules' folder (sce_module) into, and the converter's --app0
        /// value for it ("" for the layout before: no option, app0).
        public static string SetupFolder(string root, string serial, bool supported, out string converterOption)
        {
            converterOption = supported ? " --app0 app0/" + serial : "";
            return supported ? Folder(root, serial) : Path.Combine(root, "app0");
        }

        /// A serial folder name: four capital letters and five digits (CUSA03220).
        public static bool IsSerial(string name)
        {
            if (name == null || name.Length != 9) return false;
            for (int i = 0; i < 9; i++)
                if (i < 4 ? !(name[i] >= 'A' && name[i] <= 'Z') : !(name[i] >= '0' && name[i] <= '9')) return false;
            return true;
        }

        /// Whether a converted executable loads its modules from app0\sce_module (converted before --app0).
        public static bool LoadsFlatApp0(string exe)
        {
            return FileContains(exe, "app0\\sce_module\\") || FileContains(exe, "app0/sce_module/");
        }

        internal static bool FileContains(string path, string text)
        {
            try
            {
                if (!File.Exists(path)) return false;
                byte[] data = File.ReadAllBytes(path), name = System.Text.Encoding.ASCII.GetBytes(text);
                for (int i = 0; i + name.Length <= data.Length; i++)
                {
                    if (data[i] != name[0]) continue;
                    int j = 1;
                    while (j < name.Length && data[i + j] == name[j]) j++;
                    if (j == name.Length) return true;
                }
            }
            catch { }
            return false;
        }

        /// Moves a folder's app0 from the layout before (app0\sce_module and the like directly in it) into
        /// app0\<serial> for `game`, and finishes a move that was cut short. A line for the setup log for
        /// each step, nothing when there is nothing to do. Never deletes or overwrites anything:
        ///   - only in a folder that is clearly `game`'s: not both GTSport.exe and GT7.exe, and not only the
        ///     other game's exe;
        ///   - app0 itself is moved only when the game's executable will not load app0\sce_module any more:
        ///     `exeReplaced` (Set up, about to install one converted with --app0) or an executable that
        ///     does not name app0\sce_module;
        ///   - not when app0 already holds a serial folder beside the older files (that is left to the player).
        /// A folder can't be moved into its own child, so: app0 is renamed app0.moving-<serial>-<time>, a new
        /// app0 is made, and app0.moving-... becomes app0\<serial>. Cut short after the first or second step,
        /// the next call (the next Play or Set up) does the rest.
        public static List<string> Migrate(string root, GameInfo game, bool exeReplaced)
        {
            var notes = new List<string>();
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return notes;
            string serial = game.TitleId, app0 = Path.Combine(root, "app0"), own = Folder(root, serial);
            bool gts = File.Exists(Path.Combine(root, GameInfo.GTSport.ExeName)), gt7 = File.Exists(Path.Combine(root, GameInfo.GT7.ExeName));
            string[] moving;
            try { moving = Directory.GetDirectories(root, MovingPrefix + "*"); }
            catch (Exception e) { notes.Add("app0: could not list " + root + ": " + e.Message); return notes; }
            Array.Sort(moving, StringComparer.OrdinalIgnoreCase);
            List<string> loose = LooseEntries(app0);
            if (moving.Length == 0 && loose.Count == 0) return notes;
            if (gts && gt7)
            {
                notes.Add("app0: GTSport.exe and GT7.exe are both in this folder, so it is not clear whose app0 this is; it was left as it is.");
                return notes;
            }
            if (game.Kind == GameKind.GT7 ? gts : gt7) return notes; // the other game's folder

            // A move cut short: app0.moving-<serial>-<time> is the old app0.
            foreach (string dir in moving)
            {
                string name = Path.GetFileName(dir);
                string movedSerial = name.Length >= MovingPrefix.Length + 9 ? name.Substring(MovingPrefix.Length, 9) : "";
                if (movedSerial != serial) continue;
                if (Directory.Exists(own) || File.Exists(own))
                {
                    notes.Add("app0: could not finish moving " + name + " to app0\\" + serial + ": app0\\" + serial + " already exists. Nothing was overwritten; " + name + " is kept.");
                    continue;
                }
                try
                {
                    if (!Directory.Exists(app0))
                    {
                        Directory.CreateDirectory(app0);
                        notes.Add("app0: made a new app0 folder (finishing an earlier move).");
                    }
                    Directory.Move(dir, own);
                    notes.Add("app0: moved " + name + " to app0\\" + serial + " (finished an earlier move).");
                }
                catch (Exception e) { notes.Add("app0: could not finish moving " + name + " to app0\\" + serial + ": " + e.Message); }
            }

            loose = LooseEntries(app0);
            if (loose.Count == 0) return notes;
            string exe = Path.Combine(root, game.ExeName);
            if (!exeReplaced && File.Exists(exe) && LoadsFlatApp0(exe)) return notes; // until Set up runs again
            if (Directory.Exists(own) || File.Exists(own))
            {
                notes.Add("app0: app0\\" + serial + " exists beside older files in app0 (" + string.Join(", ", loose.ToArray()) + "); they were left where they are.");
                return notes;
            }
            foreach (string entry in Directory.GetDirectories(app0))
            {
                if (!IsSerial(Path.GetFileName(entry))) continue;
                notes.Add("app0: app0 holds another game's folder (" + Path.GetFileName(entry) + ") beside older files, so they were left where they are.");
                return notes;
            }
            string stamp = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff", System.Globalization.CultureInfo.InvariantCulture);
            string staged = Path.Combine(root, MovingPrefix + serial + "-" + stamp);
            try
            {
                Directory.Move(app0, staged);
                notes.Add("app0: step 1 of 3, renamed app0 to " + Path.GetFileName(staged) + ".");
                Directory.CreateDirectory(app0);
                notes.Add("app0: step 2 of 3, made a new app0 folder.");
                Directory.Move(staged, own);
                notes.Add("app0: step 3 of 3, moved " + Path.GetFileName(staged) + " to app0\\" + serial + ": " + game.Short + "'s app0 is app0\\" + serial + " now.");
            }
            catch (Exception e) { notes.Add("app0: moving app0 to app0\\" + serial + " stopped: " + e.Message + " The next Play or Set up carries on."); }
            return notes;
        }

        /// The names in app0 that are not a game's own folder (app0\<serial>): what Set up wrote into app0
        /// before FEAT-GTS-291.
        static List<string> LooseEntries(string app0)
        {
            var names = new List<string>();
            try
            {
                if (!Directory.Exists(app0)) return names;
                foreach (string entry in Directory.GetFileSystemEntries(app0))
                {
                    string name = Path.GetFileName(entry);
                    if (IsSerial(name) && Directory.Exists(entry)) continue;
                    names.Add(name);
                }
            }
            catch { }
            names.Sort(StringComparer.OrdinalIgnoreCase);
            return names;
        }
    }

    /// FEAT-GTS-291: the last names two games would share in one runtime folder get the game's serial
    /// (TitleId), so both games can use one folder:
    ///   RyGT.ini (GT Sport), RyGT7.ini and RyGT.ini (GT7)  -> <TitleId>.RyGT.ini (GameInfo.IniPath)
    ///   convert -> convert\<TitleId>  (Set up's work folder: eboot.elf, sce_module, port-report.json)
    ///   temp0   -> temp0\<TitleId>    (the game's /temp0)
    ///   data    -> data\<TitleId>     (the game's /data: GT7's offline catalog data\CUSA24769\gt7offline)
    /// GT7's launcher kept its settings in RyGT7.ini and mirrored the runtime's values into RyGT.ini, where
    /// online play kept its keys too: both go into one file, RyGT7.ini's lines and then RyGT.ini's other keys.
    ///
    /// Migrate moves a folder's old names, at the launcher's start and before Set up and Play; libc does the
    /// same at load for the settings file, temp0 and data, for a game started without the launcher
    /// (core/libs/prx/libc/include/SharedNames.hpp; keep the two in step). The rules are the save and app0
    /// moves':
    ///   - only in a folder whose old files are clearly this game's (OwnsOldFiles): not beside the other
    ///     game's exe, and for GT Sport not in a ryGT7 folder (RyGT7.ini, RyGT7.exe, RyGT7.exe.old);
    ///   - nothing is deleted or overwritten: the old settings files are renamed <name>.moved-<date> once
    ///     all their keys are in the new file; a folder moves into its own child in three steps
    ///     (<name> -> <name>.moving-<serial>-<time>, a new <name>, the staged folder -> <name>\<serial>), and
    ///     a move cut short is finished by the next call; a second call finds nothing to do;
    ///   - a line for the game's setup log (logs\<TitleId>\setup.log) for each step.
    static class SharedNames
    {
        public const string OldIni = "RyGT.ini", OldGt7Ini = "RyGT7.ini";
        /// The folders that get a <TitleId> folder; the runtime moves the last two itself too.
        public static readonly string[] Folders = { "convert", "temp0", "data" };

        /// Whether the files from before the serial names in `root` are `game`'s.
        public static bool OwnsOldFiles(string root, GameKind game)
        {
            bool gts = File.Exists(Path.Combine(root, GameInfo.GTSport.ExeName)), gt7 = File.Exists(Path.Combine(root, GameInfo.GT7.ExeName));
            bool gt7Release = File.Exists(Path.Combine(root, OldGt7Ini)) || File.Exists(Path.Combine(root, "RyGT7.exe")) ||
                              File.Exists(Path.Combine(root, "RyGT7.exe.old"));
            if (game == GameKind.GT7) return !gts && (gt7 || gt7Release);
            return !gt7 && !gt7Release;
        }

        /// Moves `game`'s old names in `root` (above); a line for the setup log for each step.
        public static List<string> Migrate(string root, GameInfo game)
        {
            var notes = new List<string>();
            if (string.IsNullOrEmpty(root) || !Directory.Exists(root)) return notes;
            bool owns;
            try { owns = OwnsOldFiles(root, game.Kind); }
            catch (Exception e) { notes.Add("Could not look at " + root + ": " + e.Message); return notes; }
            string stamp = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fff", System.Globalization.CultureInfo.InvariantCulture);
            string date = DateTime.Now.ToString("yyyyMMdd", System.Globalization.CultureInfo.InvariantCulture);
            try { MoveIni(root, game, owns, notes, date, stamp); }
            catch (Exception e) { notes.Add("Settings file: " + e.Message); }
            foreach (string name in Folders)
            {
                try { MoveFolder(root, name, game, owns, notes, stamp); }
                catch (Exception e) { notes.Add(name + ": " + e.Message); }
            }
            return notes;
        }

        /// Renames (GT Sport) or merges (GT7) the old settings files into <TitleId>.RyGT.ini.
        public static void MoveIni(string root, GameInfo game, bool owns, List<string> notes, string date, string stamp)
        {
            if (!owns) return;
            bool gt7 = game.Kind == GameKind.GT7;
            string target = game.IniPath(root), old = Path.Combine(root, OldIni), old7 = Path.Combine(root, OldGt7Ini);
            bool hasOld = File.Exists(old), hasOld7 = gt7 && File.Exists(old7);
            if (!hasOld && !hasOld7) return;
            string name = game.IniName;
            if (!File.Exists(target))
            {
                if (!gt7)
                {
                    try { File.Move(old, target); notes.Add("Settings: renamed RyGT.ini to " + name + "."); }
                    catch (Exception e) { notes.Add("Settings: could not rename RyGT.ini to " + name + ": " + e.Message + " The next start tries again."); }
                    return;
                }
                var lines = new List<string>();
                var keys = new HashSet<string>(StringComparer.Ordinal);
                if (hasOld7)
                    foreach (string line in File.ReadAllLines(old7))
                    {
                        lines.Add(line);
                        string key = Key(line);
                        if (key.Length > 0) keys.Add(key);
                    }
                if (hasOld)
                    foreach (string line in File.ReadAllLines(old))
                    {
                        string key = Key(line);
                        if (key.Length == 0 || !keys.Add(key)) continue;
                        lines.Add(line);
                    }
                string partial = target + ".partial";
                File.WriteAllLines(partial, lines);
                File.Move(partial, target);
                notes.Add("Settings: made " + name + " from " + (hasOld7 && hasOld ? "RyGT7.ini and RyGT.ini" : hasOld7 ? "RyGT7.ini" : "RyGT.ini") + ".");
            }
            if (!gt7)
            {
                notes.Add("Settings: " + name + " exists, so RyGT.ini beside it was left as it is.");
                return;
            }
            foreach (string file in new[] { old7, old })
            {
                if (!File.Exists(file)) continue;
                if (KeysIn(file, target)) Retire(file, name, notes, date, stamp);
                else notes.Add("Settings: " + Path.GetFileName(file) + " has settings that " + name + " has not; it was left as it is.");
            }
        }

        /// The key of a "key=value" line, trimmed; "" for any other line.
        static string Key(string line)
        {
            int eq = line.IndexOf('=');
            return eq < 0 ? "" : line.Substring(0, eq).Trim(' ', '\t', '﻿');
        }

        static bool KeysIn(string from, string into)
        {
            var have = new HashSet<string>(File.ReadAllLines(into).Select(Key), StringComparer.Ordinal);
            return File.ReadAllLines(from).Select(Key).Where(k => k.Length > 0).All(have.Contains);
        }

        static void Retire(string file, string into, List<string> notes, string date, string stamp)
        {
            string moved = file + ".moved-" + date;
            if (File.Exists(moved) || Directory.Exists(moved)) moved = file + ".moved-" + stamp;
            string shown = Path.GetFileName(file), shownMoved = Path.GetFileName(moved);
            if (File.Exists(moved) || Directory.Exists(moved)) { notes.Add("Settings: could not rename " + shown + ": " + shownMoved + " already exists; it was left as it is."); return; }
            try { File.Move(file, moved); notes.Add("Settings: renamed " + shown + " to " + shownMoved + " (its settings are in " + into + ")."); }
            catch (Exception e) { notes.Add("Settings: could not rename " + shown + " to " + shownMoved + ": " + e.Message); }
        }

        /// Moves root\<name> into root\<name>\<TitleId> (above). True when the game's own folder is the one
        /// to use (it exists, or nothing older is in the way).
        public static bool MoveFolder(string root, string name, GameInfo game, bool owns, List<string> notes, string stamp)
        {
            string serial = game.TitleId, folder = Path.Combine(root, name), own = game.OwnFolder(root, name);
            string shownOwn = name + "\\" + serial, prefix = name + ".moving-" + serial + "-";
            // A move cut short: <name>.moving-<serial>-<time> is the old folder.
            string[] moving = Directory.GetDirectories(root, prefix + "*");
            Array.Sort(moving, StringComparer.OrdinalIgnoreCase);
            foreach (string staged in moving)
            {
                string stagedName = Path.GetFileName(staged);
                if (Directory.Exists(own) || File.Exists(own))
                {
                    notes.Add(name + ": could not finish moving " + stagedName + " to " + shownOwn + ": " + shownOwn + " already exists. Nothing was overwritten; " + stagedName + " is kept.");
                    continue;
                }
                try
                {
                    if (!Directory.Exists(folder))
                    {
                        Directory.CreateDirectory(folder);
                        notes.Add(name + ": made a new " + name + " folder (finishing an earlier move).");
                    }
                    Directory.Move(staged, own);
                    notes.Add(name + ": moved " + stagedName + " to " + shownOwn + " (finished an earlier move).");
                }
                catch (Exception e) { notes.Add(name + ": could not finish moving " + stagedName + " to " + shownOwn + ": " + e.Message); }
            }

            List<string> loose = LooseEntries(folder);
            if (loose.Count == 0) return true;
            if (Directory.Exists(own))
            {
                notes.Add(name + ": " + shownOwn + " exists beside older files in " + name + " (" + string.Join(", ", loose.ToArray()) + "); they were left where they are.");
                return true;
            }
            if (!owns) return false;
            foreach (string entry in Directory.GetDirectories(folder))
            {
                if (!App0Layout.IsSerial(Path.GetFileName(entry))) continue;
                notes.Add(name + ": " + name + " holds another game's folder (" + Path.GetFileName(entry) + ") beside older files, so they were left where they are.");
                return false;
            }
            string movingTo = Path.Combine(root, prefix + stamp);
            try
            {
                Directory.Move(folder, movingTo);
                notes.Add(name + ": step 1 of 3, renamed " + name + " to " + Path.GetFileName(movingTo) + ".");
                Directory.CreateDirectory(folder);
                notes.Add(name + ": step 2 of 3, made a new " + name + " folder.");
                Directory.Move(movingTo, own);
                notes.Add(name + ": step 3 of 3, moved " + Path.GetFileName(movingTo) + " to " + shownOwn + ": " + game.Short + "'s " + name + " is " + shownOwn + " now.");
                return true;
            }
            catch (Exception e)
            {
                notes.Add(name + ": moving " + name + " to " + shownOwn + " stopped: " + e.Message + " The next Play or Set up carries on.");
                return false;
            }
        }

        /// The names in a folder that are not a game's own folder (<name>\<serial>).
        static List<string> LooseEntries(string folder)
        {
            var names = new List<string>();
            if (!Directory.Exists(folder)) return names;
            foreach (string entry in Directory.GetFileSystemEntries(folder))
            {
                string entryName = Path.GetFileName(entry);
                if (App0Layout.IsSerial(entryName) && Directory.Exists(entry)) continue;
                names.Add(entryName);
            }
            names.Sort(StringComparer.OrdinalIgnoreCase);
            return names;
        }
    }
}
