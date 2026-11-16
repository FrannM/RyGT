// FEAT-GTS-291: <serial>.RyGT.ini, temp0\<serial> and data\<serial>, and the moves into them (SharedNames.hpp).
// Standalone: g++ -std=c++20 -I<core/libs> SharedNamesTests.cpp src/SharedNames.cpp
#include "prx/libc/include/SharedNames.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace Libc::SharedNames;

static int failures = 0;
static int checks = 0;
static std::vector<std::string> lines;

static void Check(bool condition, const char* what) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        for (const auto& line : lines) std::fprintf(stderr, "    log: %s\n", line.c_str());
        ++failures;
    }
}

static void Put(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

static std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static const Log Collect = [](const std::string& line) { lines.push_back(line); };

static void Ini(const fs::path& root, const char* serial) {
    lines.clear();
    MoveIni(root, serial, OwnsOldFiles(root, serial), Collect, "20261115", "20261115T120000");
}

static Use Move(const fs::path& root, const char* name, const char* serial, const char* stamp = "20261115T120000") {
    lines.clear();
    return MoveFolder(root, name, serial, OwnsOldFiles(root, serial), Collect, stamp);
}

static std::size_t Count(const fs::path& folder) {
    std::size_t count = 0;
    for (const auto& entry : fs::directory_iterator(folder)) { (void)entry; ++count; }
    return count;
}

int main() {
    const fs::path root = fs::temp_directory_path() / "rygt-shared-names-tests";
    fs::remove_all(root);
    const char* gts = "CUSA03220";
    const char* gt7 = "CUSA24769";

    Check(IniFile("D:\\RyGT", gts) == fs::path("D:\\RyGT") / "CUSA03220.RyGT.ini", "the settings file is named after the serial");
    Check(Folder("D:\\RyGT", "data", gt7) == fs::path("D:\\RyGT") / "data" / "CUSA24769", "data\\<serial>");

    // Whose old files they are.
    {
        const auto folder = root / "owner";
        fs::create_directories(folder);
        Check(OwnsOldFiles(folder, gts) && !OwnsOldFiles(folder, gt7), "a folder with no game: GT Sport's (a RyGTS folder before Set up)");
        Put(folder / "RyGT7.ini", "");
        Check(!OwnsOldFiles(folder, gts) && OwnsOldFiles(folder, gt7), "RyGT7.ini: a ryGT7 folder");
        fs::remove(folder / "RyGT7.ini");
        Put(folder / "GT7.exe", "MZ");
        Check(!OwnsOldFiles(folder, gts) && OwnsOldFiles(folder, gt7), "GT7.exe alone: GT7's");
        Put(folder / "GTSport.exe", "MZ");
        Check(!OwnsOldFiles(folder, gts) && !OwnsOldFiles(folder, gt7), "both exes: nobody's");
        fs::remove(folder / "GT7.exe");
        Check(OwnsOldFiles(folder, gts) && !OwnsOldFiles(folder, gt7), "GTSport.exe alone: GT Sport's");
    }

    // GT Sport: RyGT.ini is renamed, byte for byte; a second run does nothing.
    {
        const auto folder = root / "gts-ini";
        const std::string text = "game=D:\\GTS\\CUSA03220\r\nupdate=\r\nstdout_level=Info\r\n";
        Put(folder / "GTSport.exe", "MZ");
        Put(folder / "RyGT.ini", text);
        Ini(folder, gts);
        Check(Read(folder / "CUSA03220.RyGT.ini") == text && !fs::exists(folder / "RyGT.ini") && lines.size() == 1, "GT Sport's RyGT.ini renamed");
        Ini(folder, gts);
        Check(lines.empty(), "a second run does nothing");
        // An old RyGT.ini beside the new one is never merged or overwritten.
        Put(folder / "RyGT.ini", "game=other\r\n");
        Ini(folder, gts);
        Check(Read(folder / "CUSA03220.RyGT.ini") == text && Read(folder / "RyGT.ini") == "game=other\r\n" && lines.size() == 1,
              "never overwritten");
        // GT7 never takes GT Sport's file.
        Ini(folder, gt7);
        Check(!fs::exists(folder / "CUSA24769.RyGT.ini") && lines.empty(), "GT7 leaves GT Sport's folder alone");
    }

    // GT7: RyGT7.ini and the RyGT.ini it mirrored into (with online play's keys) become one file.
    {
        const auto folder = root / "gt7-ini";
        Put(folder / "GT7.exe", "MZ");
        Put(folder / "RyGT7.ini", "game=E:\\GT7\\CUSA24769\r\nupdate=\r\nstdout_level=Warning\r\nshow_advanced=0\r\nenv=0|RYGT_NEO=1\r\n");
        Put(folder / "RyGT.ini", "online=1\r\nonline_id=Racer\r\nstdout_level=Warning\r\ngame=E:\\GT7\\CUSA24769\r\nupdate=\r\n");
        Ini(folder, gt7);
        const auto merged = Read(folder / "CUSA24769.RyGT.ini");
        Check(merged == "game=E:\\GT7\\CUSA24769\r\nupdate=\r\nstdout_level=Warning\r\nshow_advanced=0\r\nenv=0|RYGT_NEO=1\r\nonline=1\r\nonline_id=Racer\r\n",
              "RyGT7.ini's lines, then RyGT.ini's other keys");
        Check(!fs::exists(folder / "RyGT7.ini") && !fs::exists(folder / "RyGT.ini") &&
              fs::exists(folder / "RyGT7.ini.moved-20261115") && fs::exists(folder / "RyGT.ini.moved-20261115") && lines.size() == 3,
              "both old files kept as .moved-<date>");
        Check(!fs::exists(folder / "CUSA24769.RyGT.ini.partial"), "no .partial left");
        Ini(folder, gt7);
        Check(lines.empty(), "a second run does nothing");
        // Cut short after the new file was made: the next run renames the old files.
        Put(folder / "RyGT7.ini", "game=E:\\GT7\\CUSA24769\r\n");
        Put(folder / "RyGT.ini", "online_id=Racer\r\n");
        Ini(folder, gt7);
        Check(Read(folder / "CUSA24769.RyGT.ini") == merged && fs::exists(folder / "RyGT7.ini.moved-20261115T120000") &&
              fs::exists(folder / "RyGT.ini.moved-20261115T120000"), "an interrupted move is finished (a taken .moved- name gets the time)");
        // An old file with a key the new one lacks stays.
        Put(folder / "RyGT.ini", "account_key=abc\r\n");
        Ini(folder, gt7);
        Check(Read(folder / "RyGT.ini") == "account_key=abc\r\n" && Read(folder / "CUSA24769.RyGT.ini") == merged && lines.size() == 1,
              "settings the new file lacks are never dropped");
        fs::remove(folder / "RyGT.ini");
        // Only RyGT7.ini (GT7 never started), or only RyGT.ini (GT7.exe started without its launcher).
        const auto only7 = root / "gt7-only7";
        Put(only7 / "RyGT7.ini", "game=X\r\n");
        Ini(only7, gt7);
        Check(Read(only7 / "CUSA24769.RyGT.ini") == "game=X\r\n" && fs::exists(only7 / "RyGT7.ini.moved-20261115"), "RyGT7.ini alone");
        const auto only = root / "gt7-only";
        Put(only / "GT7.exe", "MZ");
        Put(only / "RyGT.ini", "game=Y\r\nonline=0\r\n");
        Ini(only, gt7);
        Check(Read(only / "CUSA24769.RyGT.ini") == "game=Y\r\nonline=0\r\n" && fs::exists(only / "RyGT.ini.moved-20261115"), "RyGT.ini alone");
        // GT Sport never takes GT7's RyGT.ini (beside RyGT7.ini).
        const auto mirror = root / "gt7-mirror";
        Put(mirror / "RyGT7.ini", "game=Z\r\n");
        Put(mirror / "RyGT.ini", "game=Z\r\n");
        Ini(mirror, gts);
        Check(!fs::exists(mirror / "CUSA03220.RyGT.ini") && fs::exists(mirror / "RyGT.ini") && lines.empty(), "GT Sport leaves GT7's files alone");
    }

    // Both exes: whose settings they are is not clear, nothing moves.
    {
        const auto folder = root / "both-ini";
        Put(folder / "GTSport.exe", "MZ");
        Put(folder / "GT7.exe", "MZ");
        Put(folder / "RyGT.ini", "game=?\r\n");
        Ini(folder, gts);
        Ini(folder, gt7);
        Check(fs::exists(folder / "RyGT.ini") && !fs::exists(folder / "CUSA03220.RyGT.ini") && !fs::exists(folder / "CUSA24769.RyGT.ini"),
              "both exes: RyGT.ini left as it is");
    }

    // data: GT7's gt7offline moves into data\CUSA24769 in three steps.
    {
        const auto folder = root / "gt7-data";
        Put(folder / "GT7.exe", "MZ");
        Check(Move(folder, "data", gt7) == Use::New && lines.empty() && !fs::exists(folder / "data"), "no data folder: nothing to move");
        Put(folder / "data" / "gt7offline" / "catalog.bin", "cars");
        Check(Move(folder, "data", gt7) == Use::New && lines.size() == 3 &&
              Read(folder / "data" / "CUSA24769" / "gt7offline" / "catalog.bin") == "cars" && Count(folder / "data") == 1 &&
              Count(folder) == 2, "data\\gt7offline -> data\\CUSA24769\\gt7offline");
        Check(Move(folder, "data", gt7) == Use::New && lines.empty(), "a second run does nothing");
        // GT Sport in the same folder (GT7.exe still there): its own folder, nothing of GT7's touched.
        Check(Move(folder, "data", gts) == Use::New && lines.empty() && fs::exists(folder / "data" / "CUSA24769" / "gt7offline"),
              "GT Sport beside GT7's data\\CUSA24769");
    }

    // Interrupted after step 1 (no data folder) and after step 2 (an empty one).
    for (const bool afterStep2 : {false, true}) {
        const auto folder = root / (afterStep2 ? "cut2" : "cut1");
        Put(folder / "GT7.exe", "MZ");
        Put(folder / "data.moving-CUSA24769-1" / "gt7offline" / "catalog.bin", "cars");
        if (afterStep2) fs::create_directories(folder / "data");
        Check(Move(folder, "data", gt7) == Use::New && lines.size() == (afterStep2 ? 1u : 2u) &&
              Read(folder / "data" / "CUSA24769" / "gt7offline" / "catalog.bin") == "cars" && !fs::exists(folder / "data.moving-CUSA24769-1"),
              afterStep2 ? "finished after step 2" : "finished after step 1");
        Check(Move(folder, "data", gt7) == Use::New && lines.empty(), "and not twice");
    }

    // An interrupted move whose target exists meanwhile: nothing overwritten, both kept.
    {
        const auto folder = root / "clash";
        Put(folder / "GT7.exe", "MZ");
        Put(folder / "temp0.moving-CUSA24769-1" / "a.tmp", "older");
        Put(folder / "temp0" / "CUSA24769" / "a.tmp", "newer");
        Check(Move(folder, "temp0", gt7) == Use::New && lines.size() == 1 && lines[0].find("already exists") != std::string::npos &&
              Read(folder / "temp0" / "CUSA24769" / "a.tmp") == "newer" && Read(folder / "temp0.moving-CUSA24769-1" / "a.tmp") == "older",
              "never overwritten");
    }

    // Older files beside the game's own folder stay where they are.
    {
        const auto folder = root / "beside";
        Put(folder / "GTSport.exe", "MZ");
        Put(folder / "temp0" / "CUSA03220" / "a.tmp", "own");
        Put(folder / "temp0" / "b.tmp", "older");
        Check(Move(folder, "temp0", gts) == Use::New && lines.size() == 1 && fs::exists(folder / "temp0" / "b.tmp"), "older files left beside");
    }

    // Not this game's folder, or another game's folder beside older files: the game keeps the old folder.
    {
        const auto folder = root / "unclear";
        Put(folder / "GTSport.exe", "MZ");
        Put(folder / "GT7.exe", "MZ");
        Put(folder / "data" / "gt7offline" / "catalog.bin", "cars");
        Check(Move(folder, "data", gt7) == Use::Old && Move(folder, "data", gts) == Use::Old &&
              fs::exists(folder / "data" / "gt7offline" / "catalog.bin"), "both exes: data left, the old folder used");
        fs::remove(folder / "GT7.exe");
        fs::create_directories(folder / "data" / "CUSA24769");
        Check(Move(folder, "data", gts) == Use::Old && lines.size() == 1 && fs::exists(folder / "data" / "gt7offline"),
              "another game's data\\<serial> beside older files: left alone");
    }

    fs::remove_all(root);
    std::printf("%d shared names checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
