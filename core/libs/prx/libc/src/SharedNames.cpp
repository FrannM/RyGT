// FEAT-GTS-291: the serial names of a runtime folder's settings file, temp0 and data (SharedNames.hpp).
#include "prx/libc/include/SharedNames.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace Libc::SharedNames {
namespace {

constexpr const char* kGt7Serial = "CUSA24769";

bool Exists(const fs::path& path) {
    std::error_code error;
    return fs::exists(path, error);
}

bool IsSerial(const std::string& name) {
    if (name.size() != 9) return false;
    for (std::size_t i = 0; i < 9; ++i) {
        const char c = name[i];
        if (i < 4 ? !(c >= 'A' && c <= 'Z') : !(c >= '0' && c <= '9')) return false;
    }
    return true;
}

std::string Name(const fs::path& path) { return path.filename().string(); }

// A file's lines, without their line ends (and a UTF-8 BOM).
std::vector<std::string> Lines(const fs::path& path) {
    std::vector<std::string> lines;
    std::ifstream file(path, std::ios::binary);
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (lines.empty() && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
            line.erase(0, 3);
        lines.push_back(line);
    }
    return lines;
}

// The key of a "key=value" line, trimmed; "" for any other line.
std::string Key(const std::string& line) {
    const auto equals = line.find('=');
    if (equals == std::string::npos) return {};
    std::string key = line.substr(0, equals);
    const auto first = key.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    key = key.substr(first, key.find_last_not_of(" \t") - first + 1);
    return key;
}

std::set<std::string> Keys(const fs::path& path) {
    std::set<std::string> keys;
    for (const auto& line : Lines(path))
        if (auto key = Key(line); !key.empty()) keys.insert(key);
    return keys;
}

// Every key of `from` is in `in`: `from` was merged into `in`.
bool KeysIn(const fs::path& from, const fs::path& in) {
    const auto have = Keys(in);
    for (const auto& key : Keys(from))
        if (have.count(key) == 0) return false;
    return true;
}

// Renames an old settings file to <name>.moved-<date> (or -<stamp> when that is taken).
void Retire(const fs::path& file, const fs::path& into, const Log& log, const std::string& date, const std::string& stamp) {
    fs::path moved = file;
    moved += ".moved-" + date;
    if (Exists(moved)) {
        moved = file;
        moved += ".moved-" + stamp;
    }
    if (Exists(moved)) {
        log("could not rename " + Name(file) + ": " + Name(moved) + " already exists; it was left as it is.");
        return;
    }
    std::error_code error;
    fs::rename(file, moved, error);
    if (error) log("could not rename " + Name(file) + " to " + Name(moved) + ": " + error.message());
    else log("renamed " + Name(file) + " to " + Name(moved) + " (its settings are in " + Name(into) + ").");
}

// The names in a folder that are not a game's own folder (<name>\<serial>): what was there before.
std::vector<std::string> LooseEntries(const fs::path& folder) {
    std::vector<std::string> names;
    std::error_code error;
    if (!fs::is_directory(folder, error)) return names;
    for (const auto& entry : fs::directory_iterator(folder, error)) {
        const auto name = Name(entry.path());
        std::error_code kind;
        if (IsSerial(name) && entry.is_directory(kind)) continue;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string Join(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ", ") + name;
    return text;
}

} // namespace

fs::path IniFile(const fs::path& root, const std::string& serial) { return root / (serial + ".RyGT.ini"); }

fs::path Folder(const fs::path& root, const std::string& name, const std::string& serial) { return root / name / serial; }

bool OwnsOldFiles(const fs::path& root, const std::string& serial) {
    const bool gt7 = serial == kGt7Serial;
    const bool gtsExe = Exists(root / "GTSport.exe"), gt7Exe = Exists(root / "GT7.exe");
    const bool gt7Release = Exists(root / "RyGT7.ini") || Exists(root / "RyGT7.exe") || Exists(root / "RyGT7.exe.old");
    if (gt7) return !gtsExe && (gt7Exe || gt7Release);
    return !gt7Exe && !gt7Release;
}

void MoveIni(const fs::path& root, const std::string& serial, bool owns, const Log& log, const std::string& date,
             const std::string& stamp) {
    if (!owns) return;
    const bool gt7 = serial == kGt7Serial;
    const auto target = IniFile(root, serial), old = root / "RyGT.ini", old7 = root / "RyGT7.ini";
    const bool hasOld = Exists(old), hasOld7 = gt7 && Exists(old7);
    if (!hasOld && !hasOld7) return;
    std::error_code error;
    if (!Exists(target)) {
        if (!gt7) {
            fs::rename(old, target, error);
            if (error) log("could not rename RyGT.ini to " + Name(target) + ": " + error.message() + " The next start tries again.");
            else log("renamed RyGT.ini to " + Name(target) + ".");
            return;
        }
        // GT7: RyGT7.ini's lines, then RyGT.ini's other keys (online play's), in a new file.
        std::vector<std::string> lines;
        std::set<std::string> keys;
        if (hasOld7) {
            lines = Lines(old7);
            for (const auto& line : lines)
                if (auto key = Key(line); !key.empty()) keys.insert(key);
        }
        if (hasOld) {
            for (const auto& line : Lines(old)) {
                const auto key = Key(line);
                if (key.empty() || !keys.insert(key).second) continue;
                lines.push_back(line);
            }
        }
        fs::path partial = target;
        partial += ".partial";
        {
            std::ofstream file(partial, std::ios::binary | std::ios::trunc);
            for (const auto& line : lines) file << line << "\r\n";
            if (!file) {
                log("could not write " + Name(partial) + "; the old settings files were left as they are.");
                return;
            }
        }
        fs::rename(partial, target, error);
        if (error) {
            log("could not rename " + Name(partial) + " to " + Name(target) + ": " + error.message());
            return;
        }
        log(std::string("made ") + Name(target) + " from " + (hasOld7 && hasOld ? "RyGT7.ini and RyGT.ini" : hasOld7 ? "RyGT7.ini" : "RyGT.ini") + ".");
    }
    if (!gt7) {
        log(Name(target) + " exists, so RyGT.ini beside it was left as it is.");
        return;
    }
    for (const auto& file : {old7, old}) {
        if (!Exists(file)) continue;
        if (KeysIn(file, target)) Retire(file, target, log, date, stamp);
        else log(Name(file) + " has settings that " + Name(target) + " has not; it was left as it is.");
    }
}

Use MoveFolder(const fs::path& root, const std::string& name, const std::string& serial, bool owns, const Log& log,
               const std::string& stamp) {
    const auto folder = root / name, own = Folder(root, name, serial);
    const std::string shownOwn = name + "\\" + serial, prefix = name + ".moving-" + serial + "-";
    std::error_code error;

    // A move cut short: <name>.moving-<serial>-<time> is the old folder.
    std::vector<fs::path> moving;
    for (const auto& entry : fs::directory_iterator(root, error)) {
        const auto entryName = Name(entry.path());
        std::error_code kind;
        if (entryName.compare(0, prefix.size(), prefix) == 0 && entry.is_directory(kind)) moving.push_back(entry.path());
    }
    std::sort(moving.begin(), moving.end());
    for (const auto& staged : moving) {
        if (Exists(own)) {
            log("could not finish moving " + Name(staged) + " to " + shownOwn + ": " + shownOwn +
                " already exists. Nothing was overwritten; " + Name(staged) + " is kept.");
            continue;
        }
        std::error_code step;
        if (!Exists(folder)) {
            fs::create_directory(folder, step);
            if (!step) log("made a new " + name + " folder (finishing an earlier move).");
        }
        if (!step) fs::rename(staged, own, step);
        if (step) log("could not finish moving " + Name(staged) + " to " + shownOwn + ": " + step.message());
        else log("moved " + Name(staged) + " to " + shownOwn + " (finished an earlier move).");
    }

    const auto loose = LooseEntries(folder);
    if (loose.empty()) return Use::New;
    if (Exists(own)) {
        log(shownOwn + " exists beside older files in " + name + " (" + Join(loose) + "); they were left where they are.");
        return Use::New;
    }
    if (!owns) return Use::Old;
    for (const auto& entry : fs::directory_iterator(folder, error)) {
        std::error_code kind;
        if (!IsSerial(Name(entry.path())) || !entry.is_directory(kind)) continue;
        log(name + " holds another game's folder (" + Name(entry.path()) + ") beside older files, so they were left where they are.");
        return Use::Old;
    }
    const auto staged = root / (prefix + stamp);
    fs::rename(folder, staged, error);
    if (error) {
        log("moving " + name + " to " + shownOwn + " stopped: " + error.message() + " The next start carries on.");
        return Use::Old;
    }
    log("step 1 of 3, renamed " + name + " to " + Name(staged) + ".");
    fs::create_directory(folder, error);
    if (!error) {
        log("step 2 of 3, made a new " + name + " folder.");
        fs::rename(staged, own, error);
    }
    if (error) {
        log("moving " + name + " to " + shownOwn + " stopped: " + error.message() + " The next start carries on.");
        return Use::Old;
    }
    log("step 3 of 3, moved " + Name(staged) + " to " + shownOwn + ".");
    return Use::New;
}

} // namespace Libc::SharedNames
