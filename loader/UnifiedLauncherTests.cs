// FEAT-GTS-290: checks of the launcher for both games: RyGT-Launcher.ini (the game picked last and each
// game's folder), each game's settings file read and written as its own launcher did, the accent
// colours, and the pages built for each game and after switching. Built with test-launcher.cmd.
// SPDX-License-Identifier: GPL-2.0-only
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;

namespace RyGT
{
    static class UnifiedLauncherTests
    {
        static int checks;
        static void Check(bool value, string message)
        { checks++; if (!value) throw new Exception(message); }

        static string Folder(string root, string name)
        {
            string path = Path.Combine(root, name);
            Directory.CreateDirectory(path);
            return path;
        }

        static void StateChecks(string root)
        {
            // A RyGTS folder: GT Sport is the default and its folder is the launcher's; GT7 has none yet.
            string gts = Folder(root, "RyGTS");
            var state = new LauncherState(gts);
            Check(state.Last == GameKind.GTSport, "A RyGTS folder starts with GT Sport");
            Check(state.RootOf(GameKind.GTSport) == gts && state.RootOf(GameKind.GT7) == "", "GT Sport's folder is the launcher's, GT7 has none");
            Check(state.Ready(GameKind.GTSport) && !state.Ready(GameKind.GT7), "Only GT Sport is ready");
            Check(!File.Exists(Path.Combine(gts, LauncherState.FileName)), "Reading writes nothing");
            // A ryGT7 folder picked for GT7 and kept with the last game; other lines survive.
            string gt7 = Folder(root, "ryGT7");
            File.WriteAllText(Path.Combine(gt7, "RyGT7.ini"), "game=\n");
            File.WriteAllText(Path.Combine(gts, LauncherState.FileName), "# a comment\nfuture=1\n");
            state = new LauncherState(gts);
            state.SetRoot(GameKind.GT7, gt7 + "\\");
            state.Last = GameKind.GT7;
            state.Save();
            var text = File.ReadAllText(Path.Combine(gts, LauncherState.FileName));
            Check(text.Contains("game=gt7") && text.Contains("gt7_root=" + gt7) && text.Contains("gts_root=\r\n"), "Saved: game and GT7's folder, GT Sport's default left empty");
            Check(text.Contains("# a comment") && text.Contains("future=1"), "Unknown lines kept");
            var again = new LauncherState(gts);
            Check(again.Last == GameKind.GT7 && again.RootOf(GameKind.GT7) == gt7 && again.Ready(GameKind.GT7), "Read back");
            // The default folder is not written out (it follows the launcher).
            again.SetRoot(GameKind.GTSport, gts);
            Check(again.Setting(GameKind.GTSport) == "", "The default folder is kept as empty");
            // GT Sport can't use the ryGT7 folder (both configs are user\custom_configs\CUSA03220.json).
            again.SetRoot(GameKind.GTSport, gt7);
            Check(!again.Ready(GameKind.GTSport), "GT Sport in a ryGT7 folder is refused");
            // The launcher inside a ryGT7 folder: GT7 is the default and its folder the launcher's.
            var inside = new LauncherState(gt7);
            Check(inside.Last == GameKind.GT7 && inside.RootOf(GameKind.GT7) == gt7 && inside.RootOf(GameKind.GTSport) == "", "A ryGT7 folder starts with GT7");
            // Transient (tests, --setup --game): never written.
            var transient = new LauncherState(root) { Transient = true };
            transient.Save();
            Check(!File.Exists(Path.Combine(root, LauncherState.FileName)), "Transient state is not saved");
            Check(GameInfo.Parse("GTS") == GameKind.GTSport && GameInfo.Parse("gt7") == GameKind.GT7 && GameInfo.Parse("gt6") == null, "--game values");
        }

        static void SettingsChecks(string root)
        {
            // GT Sport: an existing RyGT.ini (as RyGTS 0.2.2 wrote it) comes back byte for byte.
            string gts = Folder(root, "settings-gts");
            string ini = Path.Combine(gts, "RyGT.ini");
            string old = "game=D:\\GTS\\CUSA03220\r\nupdate=D:\\GTS\\CUSA03220-patch\r\nmodules=D:\\fw\\sys_modules\r\nfonts=\r\n" +
                         "stdout_level=Info\r\nhdd_streaming=1\r\nshow_advanced=1\r\nprompt_obsolete=0\r\nenv=1|SHADGT_PARALLEL_WORKERS=4\r\n";
            File.WriteAllText(ini, old);
            var settings = new Settings(ini);
            Check(settings.Game == "D:\\GTS\\CUSA03220" && settings.HddStreaming && settings.ShowAdvanced && !settings.PromptObsolete &&
                  settings.Env.Count == 1 && settings.StdoutLevel == "Info", "GT Sport's RyGT.ini read");
            settings.Save();
            Check(File.ReadAllText(ini) == old, "GT Sport's RyGT.ini written as before");
            Check(!settings.IsGt7, "RyGT.ini is GT Sport's");
            // GT7: RyGT7.ini as ryGT7 0.1.6 wrote it, and the runtime's values mirrored into RyGT.ini.
            string gt7 = Folder(root, "settings-gt7");
            string ini7 = Path.Combine(gt7, "RyGT7.ini");
            string old7 = "game=E:\\GT7\\CUSA24769\r\nupdate=\r\nmodules=E:\\fw\r\nfonts=E:\\fw\\fonts\r\nstdout_level=Warning\r\nshow_advanced=0\r\nenv=0|RYGT_NEO=1\r\n";
            File.WriteAllText(ini7, old7);
            File.WriteAllText(Path.Combine(gt7, "RyGT.ini"), "keep=this\nstdout_level=Error\n");
            var settings7 = new Settings(ini7);
            Check(settings7.IsGt7 && settings7.Game == "E:\\GT7\\CUSA24769" && settings7.Env.Count == 1, "GT7's RyGT7.ini read");
            settings7.Save();
            Check(File.ReadAllText(ini7) == old7, "GT7's RyGT7.ini written as before (no GT Sport lines)");
            string runtime = File.ReadAllText(Path.Combine(gt7, "RyGT.ini"));
            Check(runtime.Contains("keep=this") && runtime.Contains("stdout_level=Warning") && !runtime.Contains("stdout_level=Error") &&
                  runtime.Contains("game=E:\\GT7\\CUSA24769"), "GT7's runtime values mirrored into RyGT.ini");
            // A game without a folder keeps its values in memory and writes nothing.
            var none = new Settings(null);
            none.Game = "x";
            none.Save();
            Check(none.Game == "x", "A folderless game's settings stay in memory");
        }

        static void AccentChecks()
        {
            Color red = GameInfo.GTSport.Accent, blue = GameInfo.GT7.Accent;
            Check(red.R > red.B * 3 && GameInfo.GTSport.AccentText.R > GameInfo.GTSport.AccentText.B, "GT Sport is red");
            Check(blue.B > blue.R * 3 && GameInfo.GT7.AccentText.B > GameInfo.GT7.AccentText.R, "GT7 is blue");
            var half = Anim.Mix(red, blue, 0.5f);
            Check(half.R == (red.R + blue.R) / 2 && Math.Abs(half.B - (red.B + blue.B) / 2) <= 1, "The fade passes half way");
            Check(Anim.Mix(red, blue, 0) == Color.FromArgb(red.ToArgb()) && Anim.Mix(red, blue, 1).ToArgb() == blue.ToArgb(), "The fade ends on the colours");
        }

        static List<string> Pages(LoaderForm form)
        {
            var pages = (Dictionary<string, System.Windows.Forms.Panel>)typeof(LoaderForm)
                .GetField("pages", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(form);
            return pages.Keys.ToList();
        }

        static void FormChecks(string root)
        {
            string gts = Folder(root, "form-gts"), gt7 = Folder(root, "form-gt7");
            File.WriteAllText(Path.Combine(gt7, "RyGT7.ini"), "");
            using (var form = new LoaderForm(GameKind.GTSport, gts))
            {
                var pages = Pages(form);
                Check(pages.Contains("Display") && pages.Contains("Graphics") && pages.Contains("Shaders") && pages.Contains("Controls") &&
                      pages.Contains("Advanced") && !pages.Contains("Online") && !pages.Contains("Account"), "GT Sport's pages");
                Check(Dark.Accent.ToArgb() == GameInfo.GTSport.Accent.ToArgb(), "GT Sport's accent");
                Check(form.Text == GameInfo.GTSport.WindowTitle, "GT Sport's window title");
                // GT7 has no folder here: only the Game and About pages, and Play is off.
                form.SelectGame(GameKind.GT7);
                pages = Pages(form);
                Check(pages.SequenceEqual(new[] { "Game", "About" }), "A folderless GT7 gets the Game and About pages");
                Check(Dark.Accent.ToArgb() == GameInfo.GT7.Accent.ToArgb(), "GT7's accent after the switch");
            }
            using (var form = new LoaderForm(GameKind.GT7, gt7))
            {
                var pages = Pages(form);
                Check(pages.Contains("Online") && pages.Contains("Account") && pages.Contains("Controls") && pages.Contains("Graphics"), "GT7's pages");
                // ryGT7's defaults are written into GT7's own config, as RyGT7.exe did.
                string config = File.ReadAllText(Path.Combine(gt7, "user", "custom_configs", "CUSA03220.json"));
                Check(config.Contains("\"readbacks_mode\":2") && config.Contains("\"native_renderer\":true"), "GT7's defaults written");
                Check(!File.Exists(Path.Combine(gts, "user", "custom_configs", "CUSA03220.json")) ||
                      !File.ReadAllText(Path.Combine(gts, "user", "custom_configs", "CUSA03220.json")).Contains("readbacks_mode"), "GT Sport's config untouched by GT7");
                Check(form.Text == GameInfo.GT7.WindowTitle, "GT7's window title");
            }
        }

        static int Main()
        {
            string root = Path.Combine(Path.GetTempPath(), "RyGT-unified-test-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            try
            {
                StateChecks(root);
                SettingsChecks(root);
                AccentChecks();
                FormChecks(root);
            }
            catch (Exception e) { Console.Error.WriteLine("FAIL: " + e); return 1; }
            finally { try { Directory.Delete(root, true); } catch { } }
            Console.WriteLine("PASS: " + checks + " unified launcher checks");
            return 0;
        }
    }
}
