// FEAT-GTS-290: one launcher for Gran Turismo Sport and Gran Turismo 7. What differs between the two
// games' folders and launches, and the launcher's own small settings file (RyGT-Launcher.ini beside
// RyGTS.exe): the game picked last and each game's folder.
// SPDX-License-Identifier: GPL-2.0-only

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;

namespace RyGT
{
    /// The order is the home view's game list.
    enum GameKind { GTSport = 0, GT7 = 1 }

    /// A game's names, files and colours. Each game keeps its own folder: GT Sport's is a RyGTS release
    /// (GTSport.exe, RyGT.ini), GT7's a ryGT7 release (GT7.exe, RyGT7.ini). The runtimes and renderers
    /// are different builds and both use user\custom_configs\CUSA03220.json, so the two never share one.
    sealed class GameInfo
    {
        public GameKind Kind;
        public string Key;          // "gts" or "gt7" (RyGT-Launcher.ini, --game)
        public string Name;         // "Gran Turismo Sport"
        public string Short;        // "GT Sport"
        public string TitleId;      // "CUSA03220"
        public string ExeName;      // "GTSport.exe"
        public string ProcessName;  // "GTSport"
        public string IniName;      // "RyGT.ini"
        public string Product;      // "RyGTS": the release whose folder the game is in
        public string TitleAccent;  // "SPORT": the home view's title is GRAN TURISMO <accent>
        public string WindowTitle;
        /// The launcher's accent while this game is picked: red for GT Sport, blue for GT7.
        public Color Accent, AccentHover, AccentText;

        public static readonly GameInfo GTSport = new GameInfo
        {
            Kind = GameKind.GTSport, Key = "gts", Name = "Gran Turismo Sport", Short = "GT Sport", TitleId = "CUSA03220",
            ExeName = "GTSport.exe", ProcessName = "GTSport", IniName = "RyGT.ini", Product = "RyGTS", TitleAccent = "SPORT",
            WindowTitle = "RyGTS - Gran Turismo Sport",
            Accent = Color.FromArgb(178, 24, 36), AccentHover = Color.FromArgb(206, 40, 52), AccentText = Color.FromArgb(240, 92, 76)
        };

        public static readonly GameInfo GT7 = new GameInfo
        {
            Kind = GameKind.GT7, Key = "gt7", Name = "Gran Turismo 7", Short = "GT7", TitleId = "CUSA24769",
            ExeName = "GT7.exe", ProcessName = "GT7", IniName = "RyGT7.ini", Product = "ryGT7", TitleAccent = "7",
            WindowTitle = "ryGT7 - Gran Turismo 7",
            Accent = Color.FromArgb(24, 96, 196), AccentHover = Color.FromArgb(44, 120, 224), AccentText = Color.FromArgb(92, 162, 255)
        };

        public static GameInfo Of(GameKind kind) { return kind == GameKind.GT7 ? GT7 : GTSport; }

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

    /// RyGT-Launcher.ini beside the launcher: "game=gts|gt7" (the game picked last) and "gts_root=" /
    /// "gt7_root=" (each game's folder; empty: found as below). Other lines are kept as they are. Each
    /// game's own settings stay in its folder's RyGT.ini or RyGT7.ini, read and written as before.
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
            folder = (folder ?? "").Trim().TrimEnd('\\');
            roots[game] = string.Equals(folder, DefaultRoot(game), StringComparison.OrdinalIgnoreCase) ? "" : folder;
        }

        /// The game's folder: the one set, else the launcher's own folder when it is that game's
        /// (a RyGTS folder for GT Sport, a ryGT7 folder for GT7), else empty.
        public string RootOf(GameKind game)
        {
            string set = roots[game];
            if (set.Length == 0) return DefaultRoot(game);
            try { return Path.GetFullPath(set).TrimEnd('\\'); }
            catch { return ""; } // not a path: as if none were set (the Game page shows what was typed)
        }

        string DefaultRoot(GameKind game)
        {
            bool gt7Folder = LooksLikeGt7(launcherRoot);
            return (game == GameKind.GT7) == gt7Folder ? launcherRoot : "";
        }

        /// Whether the game can be set up and played from its folder: it exists, and it is that game's
        /// kind of folder (GT7 needs a ryGT7 release; GT Sport any folder that is not one).
        public bool Ready(GameKind game)
        {
            string folder = RootOf(game);
            if (folder.Length == 0 || !Directory.Exists(folder)) return false;
            if (game == GameKind.GT7) return LooksLikeGt7(folder);
            if (LooksLikeGt7(folder)) return false;
            // Not the same folder as GT7's: both games' configs are user\custom_configs\CUSA03220.json.
            return !string.Equals(folder, RootOf(GameKind.GT7), StringComparison.OrdinalIgnoreCase);
        }

        /// A ryGT7 release folder: its launcher, its settings or its game.
        public static bool LooksLikeGt7(string folder)
        {
            if (string.IsNullOrEmpty(folder)) return false;
            try
            {
                return File.Exists(Path.Combine(folder, "RyGT7.exe")) || File.Exists(Path.Combine(folder, "RyGT7.ini")) ||
                       File.Exists(Path.Combine(folder, "GT7.exe"));
            }
            catch { return false; }
        }
    }
}
