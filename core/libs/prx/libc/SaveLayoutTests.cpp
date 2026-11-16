// GAME-005: each game's saves in user\savedata\<serial> and the move of a folder's saves (SaveLayout.hpp).
// Standalone: g++ -std=c++20 -I<core/libs> SaveLayoutTests.cpp src/SaveLayout.cpp
#include "prx/libc/include/SaveLayout.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Libc::SaveLayout::Migrate;
using Libc::SaveLayout::StoreFolder;
using Libc::SaveLayout::Use;

static int failures = 0;
static int checks = 0;
static std::vector<std::string> lines;

static void Check(bool condition, const char* what) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
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

static Use Run(const fs::path& root, const char* serial, bool otherHere = false, const char* store = "_sd") {
    lines.clear();
    return Migrate(root, serial, store, otherHere, [](const std::string& line) { lines.push_back(line); }, "20261115",
                   "20261115T120000");
}

static bool Logged(const std::string& text) {
    for (const auto& line : lines) if (line.find(text) != std::string::npos) return true;
    return false;
}

// A GT Sport-style save folder: two saves, a memory save and the records.
static void OldSaves(const fs::path& root) {
    Put(root / "_sd" / "local-data" / "sce_sys" / "param.bin", std::string(1200, 'p'));
    Put(root / "_sd" / "local-data" / "save.dat", std::string(65536, 's'));
    Put(root / "_sd" / "sce_sdmemory" / "1" / "memory.dat", "memory");
    Put(root / "_sd" / "_aliases", "");
    fs::create_directories(root / "_sd" / "_app_status");
}

int main() {
    const auto base = fs::temp_directory_path() / "rygt-save-layout-tests";
    fs::remove_all(base);
    const char* gts = "CUSA03220";
    const char* gt7 = "CUSA24769";

    Check(StoreFolder("R", gt7, "_sd") == fs::path("R") / "user" / "savedata" / gt7 / "_sd", "the store folder is user/savedata/<serial>/<store>");

    // A fresh install: nothing to move, the game's own folder is used.
    {
        const auto root = base / "fresh";
        fs::create_directories(root);
        Check(Run(root, gts) == Use::New && lines.empty(), "fresh: new layout, nothing logged");
        Check(!fs::exists(root / "_sd") && !fs::exists(root / "user"), "fresh: nothing made");
        fs::create_directories(root / "_sd" / "local-data"); // only empty folders: no saves
        Check(Run(root, gts) == Use::New && fs::exists(root / "_sd" / "local-data"), "empty old folders: not moved, left alone");
    }

    // Existing saves: copied, checked, the copy put in place, the old folder renamed and kept.
    {
        const auto root = base / "existing";
        OldSaves(root);
        const auto mine = StoreFolder(root, gts, "_sd");
        Check(Run(root, gts) == Use::New, "existing: new layout");
        Check(Read(mine / "local-data" / "save.dat") == std::string(65536, 's') && fs::exists(mine / "sce_sdmemory" / "1" / "memory.dat") &&
                  fs::exists(mine / "_aliases") && fs::is_directory(mine / "_app_status"), "existing: every file and folder copied");
        Check(!fs::exists(root / "_sd") && Read(root / "_sd.moved-20261115" / "local-data" / "save.dat").size() == 65536,
              "existing: the old folder renamed, its files kept");
        Check(fs::exists(mine.string() + ".moved-from") && !fs::exists(mine.string() + ".partial"), "existing: the record written, no copy left over");
        Check(Logged("copying") && Logged("checked the copy: 4 files") && Logged("the copy is now") && Logged("renamed the old"), "existing: each step logged");
        // Already moved: a second start does nothing.
        Check(Run(root, gts) == Use::New && lines.empty() && fs::exists(root / "_sd.moved-20261115"), "already moved: nothing done");
        // The game's own folder is never overwritten, even if an old folder turns up again.
        Put(root / "_sd" / "local-data" / "save.dat", "older");
        Put(mine / "local-data" / "save.dat", "newer");
        Check(Run(root, gts) == Use::New && Read(mine / "local-data" / "save.dat") == "newer", "never overwrites the game's folder");
        Check(Read(root / "_sd.moved-20261115-2" / "local-data" / "save.dat") == "older", "a reappearing old folder is set aside, not deleted");
    }

    // An interrupted copy: the half copy is set aside (kept) and the copy made again from the original.
    {
        const auto root = base / "interrupted";
        OldSaves(root);
        const auto mine = StoreFolder(root, gt7, "_sd");
        Put(mine.string() + ".partial/local-data/save.dat", "half");
        Check(Run(root, gt7) == Use::New, "interrupted: new layout");
        Check(Read(mine / "local-data" / "save.dat") == std::string(65536, 's'), "interrupted: redone from the intact original");
        Check(Read(mine.string() + ".partial.stale-20261115T120000/local-data/save.dat") == "half", "interrupted: the half copy kept aside");
        Check(Logged("unfinished copy"), "interrupted: logged");
    }

    // Stopped after the copy was in place but before the old folder was renamed: finished next time.
    {
        const auto root = base / "rename-left";
        OldSaves(root);
        const auto mine = StoreFolder(root, gts, "_sd");
        fs::create_directories(mine.parent_path());
        fs::copy(root / "_sd", mine, fs::copy_options::recursive);
        Put(mine.string() + ".moved-from", "x");
        Check(Run(root, gts) == Use::New && !fs::exists(root / "_sd") && fs::exists(root / "_sd.moved-20261115"), "the old folder's rename finished");
    }

    // Set up made the game's folders (empty) before the first start: the old saves still move there.
    {
        const auto root = base / "premade";
        OldSaves(root);
        const auto mine = StoreFolder(root, gt7, "_sd");
        fs::create_directories(mine / "local-data");
        fs::create_directories(mine / "sce_sdmemory");
        Check(Run(root, gt7) == Use::New && Read(mine / "local-data" / "save.dat").size() == 65536, "empty premade folders take the old saves");
    }

    // Download data (GT7's photos) moves the same way.
    {
        const auto root = base / "download";
        Put(root / "download0" / "APP_DATA" / "photo" / "1.bin", "photo");
        Check(Run(root, gt7, false, "download0") == Use::New && Read(StoreFolder(root, gt7, "download0") / "APP_DATA" / "photo" / "1.bin") == "photo" &&
                  fs::exists(root / "download0.moved-20261115"), "download0 moved");
    }

    // Both games in one folder.
    {
        // Old shared saves with both executables there: whose they are is unknown, so nothing moves.
        const auto root = base / "both";
        OldSaves(root);
        Check(Run(root, gts, true) == Use::Old && fs::exists(root / "_sd" / "local-data" / "save.dat") && !fs::exists(root / "user"),
              "both games, old saves: left in place and used as before");
        Check(Logged("both games"), "both games: logged");
        // No old saves: each game gets its own folder, side by side.
        const auto shared = base / "shared";
        fs::create_directories(shared);
        Check(Run(shared, gts, true) == Use::New && Run(shared, gt7, true) == Use::New, "shared fresh folder: both use their own");
        Put(StoreFolder(shared, gts, "_sd") / "local-data" / "save.dat", "gts");
        Put(StoreFolder(shared, gt7, "_sd") / "local-data" / "save.dat", "gt7");
        Check(Read(StoreFolder(shared, gts, "_sd") / "local-data" / "save.dat") == "gts" &&
                  Read(StoreFolder(shared, gt7, "_sd") / "local-data" / "save.dat") == "gt7", "shared folder: the games' saves are apart");
        // A RyGTS folder moved first; GT7 added later finds no old saves and starts its own.
        const auto later = base / "later";
        OldSaves(later);
        Check(Run(later, gts) == Use::New && Run(later, gt7, true) == Use::New && !fs::exists(StoreFolder(later, gt7, "_sd")) &&
                  fs::exists(StoreFolder(later, gts, "_sd") / "local-data" / "save.dat"), "GT Sport's moved saves never go to GT7");
    }

    fs::remove_all(base);
    if (failures == 0) std::printf("PASS: %d save layout checks\n", checks);
    return failures == 0 ? 0 : 1;
}
