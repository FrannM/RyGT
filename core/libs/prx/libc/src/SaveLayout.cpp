// GAME-005: each game's saves in user\savedata\<serial> (SaveLayout.hpp).
#include "prx/libc/include/SaveLayout.hpp"

#include <cstdint>
#include <fstream>
#include <map>
#include <system_error>

namespace fs = std::filesystem;

namespace Libc::SaveLayout {
namespace {

std::string Text(const fs::path& path) {
    try {
        const auto text = path.u8string();
        return std::string(text.begin(), text.end());
    } catch (...) {
        return "(a path that cannot be shown)";
    }
}

// Every file and folder under folder (relative, '/'-separated) with its size (-1 for a folder).
bool Snapshot(const fs::path& folder, std::map<std::string, std::int64_t>& out, std::string& failure) {
    std::error_code error;
    fs::recursive_directory_iterator it(folder, fs::directory_options::none, error), end;
    for (; !error && it != end; it.increment(error)) {
        std::error_code kind;
        const auto relative = it->path().lexically_relative(folder).generic_u8string();
        const std::string key(relative.begin(), relative.end());
        if (it->is_directory(kind)) {
            out[key] = -1;
        } else {
            const auto size = it->file_size(kind);
            if (kind) { failure = "could not read the size of " + key + ": " + kind.message(); return false; }
            out[key] = static_cast<std::int64_t>(size);
        }
    }
    if (error) { failure = "could not list " + Text(folder) + ": " + error.message(); return false; }
    return true;
}

bool HasFile(const fs::path& folder) {
    std::error_code error;
    if (!fs::is_directory(folder, error)) return false;
    fs::recursive_directory_iterator it(folder, fs::directory_options::none, error), end;
    for (; !error && it != end; it.increment(error)) {
        std::error_code kind;
        if (!it->is_directory(kind)) return true;
    }
    return error ? true : false; // unreadable: treat as holding saves (never skipped silently)
}

// A free name: base, else base-2, base-3, ...
fs::path FreeName(const fs::path& base) {
    std::error_code error;
    if (!fs::exists(base, error)) return base;
    for (int i = 2; i < 1000; ++i) {
        fs::path candidate = base;
        candidate += "-" + std::to_string(i);
        if (!fs::exists(candidate, error)) return candidate;
    }
    return {};
}

// Removes folder only if it holds nothing but empty folders (fs::remove refuses anything else).
bool RemoveEmptyTree(const fs::path& folder) {
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(folder, error)) {
        std::error_code kind;
        if (!entry.is_directory(kind) || kind || !RemoveEmptyTree(entry.path())) return false;
    }
    if (error) return false;
    return fs::remove(folder, error) && !error;
}

} // namespace

fs::path StoreFolder(const fs::path& root, const std::string& serial, const std::string& store) {
    return root / "user" / "savedata" / serial / store;
}

Use Migrate(const fs::path& root, const std::string& serial, const std::string& store, bool otherGameHere,
            const std::function<void(const std::string&)>& log, const std::string& date, const std::string& stamp) {
    const fs::path old = root / store;
    const fs::path mine = StoreFolder(root, serial, store);
    fs::path partial = mine, record = mine;
    partial += ".partial";
    record += ".moved-from";
    std::error_code error;
    const auto renameOld = [&]() {
        fs::path moved = old;
        moved += ".moved-" + date;
        moved = FreeName(moved);
        std::error_code renamed;
        if (!moved.empty()) fs::rename(old, moved, renamed);
        if (moved.empty() || renamed) {
            log("could not rename " + Text(old) + " to " + store + ".moved-" + date + " (tried again next start): " +
                (moved.empty() ? std::string("no free name") : renamed.message()));
            return;
        }
        log("renamed the old " + Text(old) + " to " + Text(moved.filename()) + " (kept; nothing was deleted)");
    };

    const bool oldHasData = HasFile(old);
    if (fs::exists(mine, error)) {
        const bool moved = fs::exists(record, error);
        if (moved || HasFile(mine) || !oldHasData) {
            // Copied before, the old folder not yet renamed (a run stopped there): finish it.
            if (moved && oldHasData) renameOld();
            return Use::New;
        }
        // Only empty folders (made by Set up before the first start): the old saves go there instead.
        if (!RemoveEmptyTree(mine)) {
            log("left " + Text(old) + " in place: " + Text(mine) + " could not be replaced; using the old folder");
            return Use::Old;
        }
    }
    if (!oldHasData) return Use::New; // a fresh install: nothing to move
    if (otherGameHere) {
        log("left " + Text(old) + " in place: both games' executables are in this folder, so whose saves it holds is "
            "unknown; using it as before. Move it to " + Text(mine) + " by hand to separate them");
        return Use::Old;
    }
    if (fs::exists(partial, error)) {
        fs::path stale = partial;
        stale += ".stale-" + stamp;
        stale = FreeName(stale);
        std::error_code renamed;
        if (!stale.empty()) fs::rename(partial, stale, renamed);
        if (stale.empty() || renamed) {
            log("left " + Text(old) + " in place: an earlier copy " + Text(partial) + " could not be set aside");
            return Use::Old;
        }
        log("set aside an unfinished copy from an earlier start as " + Text(stale.filename()) + " (kept); copying again");
    }
    fs::create_directories(mine.parent_path(), error);
    log("copying " + Text(old) + " to " + Text(partial));
    error.clear();
    fs::copy(old, partial, fs::copy_options::recursive | fs::copy_options::copy_symlinks, error);
    if (error) {
        log("left " + Text(old) + " in place: the copy failed (" + error.message() + "); using the old folder");
        return Use::Old;
    }
    std::map<std::string, std::int64_t> from, to;
    std::string failure;
    if (!Snapshot(old, from, failure) || !Snapshot(partial, to, failure) || from != to) {
        log("left " + Text(old) + " in place: the copy does not match it (" +
            (failure.empty() ? std::to_string(from.size()) + " entries against " + std::to_string(to.size()) : failure) +
            "); using the old folder");
        return Use::Old;
    }
    std::int64_t bytes = 0;
    std::size_t files = 0;
    for (const auto& [name, size] : from) if (size >= 0) { ++files; bytes += size; }
    log("checked the copy: " + std::to_string(files) + " files, " + std::to_string(bytes) + " bytes, the same as the old folder");
    {
        std::ofstream file(record, std::ios::binary | std::ios::trunc);
        file << Text(old) << "\n" << date << "\n" << files << " files, " << bytes << " bytes\n";
        if (!file) {
            log("left " + Text(old) + " in place: could not write " + Text(record));
            return Use::Old;
        }
    }
    fs::rename(partial, mine, error);
    if (error) {
        log("left " + Text(old) + " in place: could not rename the copy to " + Text(mine) + " (" + error.message() + ")");
        return Use::Old;
    }
    log("the copy is now " + Text(mine));
    renameOld();
    return Use::New;
}

} // namespace Libc::SaveLayout
