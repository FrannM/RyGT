// GAME-005: at libc's load, before any library reads or writes a save, the run folder's saves move
// into the running game's own folder (SaveLayout.hpp) and /_sd and /download0 resolve there. Here in
// the runtime, so a game started without the launcher (a shortcut, Steam) gets the same layout.
// Only for the games themselves (GTSport.exe or GT7.exe, or SHADGT_GAME set by the launcher): any
// other program that loads libc keeps the run folder's _sd and download0 untouched.
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

#include "GameSerial.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/SaveLayout.hpp"
#include "prx/libc/include/SharedNames.hpp"

namespace {

// The game's folder: the executable's (CrashReport.cpp makes it the working directory, which the
// path resolver takes as the run folder; at load that may not have happened yet, so not asked here).
std::filesystem::path ExeFolder() {
#ifdef _WIN32
    wchar_t path[MAX_PATH * 2]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
    if (length == 0 || length >= MAX_PATH * 2) return {};
    return std::filesystem::weakly_canonical(std::filesystem::path(path).parent_path());
#else
    return std::filesystem::current_path();
#endif
}

bool RunningAGame() {
    if (const char* game = std::getenv("SHADGT_GAME"); game != nullptr && *game != '\0') return true;
#ifdef _WIN32
    wchar_t path[MAX_PATH * 2]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
    if (length == 0 || length >= MAX_PATH * 2) return false;
    const wchar_t* name = path;
    for (const wchar_t* c = path; *c; ++c) if (*c == L'\\' || *c == L'/') name = c + 1;
    return _wcsicmp(name, L"GTSport.exe") == 0 || _wcsicmp(name, L"GT7.exe") == 0;
#else
    return false;
#endif
}

std::string Now(const char* format) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char text[32] = {};
    std::strftime(text, sizeof(text), format, &local);
    return text;
}

// FEAT-GTS-291: the game's settings file, /temp0 and /data take its serial (SharedNames.hpp), before
// any library reads <serial>.RyGT.ini. Each step goes to logs\<serial>\setup.log, as the launcher's.
void SharedFolders() {
    try {
        const std::filesystem::path root = ExeFolder();
        if (root.empty()) return;
        const std::string serial = GameSerial::GameSerial();
        const std::string date = Now("%Y%m%d"), stamp = Now("%Y%m%dT%H%M%S");
        const auto log = [&](const std::string& line) {
            std::fprintf(stderr, "[layout] %s: %s\n", serial.c_str(), line.c_str());
            std::error_code made;
            std::filesystem::create_directories(root / "logs" / serial, made);
#ifdef _WIN32
            std::FILE* file = _wfopen((root / "logs" / serial / "setup.log").c_str(), L"ab");
#else
            std::FILE* file = std::fopen((root / "logs" / serial / "setup.log").c_str(), "ab");
#endif
            if (file != nullptr) {
                std::fprintf(file, "%s %s (at start): %s\n", Now("%Y-%m-%d %H:%M:%S").c_str(), serial.c_str(), line.c_str());
                std::fclose(file);
            }
        };
        const bool owns = Libc::SharedNames::OwnsOldFiles(root, serial);
        Libc::SharedNames::MoveIni(root, serial, owns, log, date, stamp);
        for (const char* name : Libc::SharedNames::kGuestFolders) {
            const std::string host = Libc::SharedNames::Folder(root, name, serial).string();
            if (Libc::SharedNames::MoveFolder(root, name, serial, owns, log, stamp) == Libc::SharedNames::Use::New) {
                AddPathAlias_nid_no_patch(name, host.c_str());
            } else {
                log(std::string("this run uses the run folder's ") + name);
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[layout] temp0, data and the settings file kept their old names (%s)\n", e.what());
    }
}

void Start() {
    if (!RunningAGame()) return;
    try {
        const std::filesystem::path root = ExeFolder();
        if (root.empty()) return;
        const std::string serial = GameSerial::GameSerial();
        const bool otherHere = std::filesystem::exists(root / (GameSerial::RunningGt7() ? "GTSport.exe" : "GT7.exe"));
        const std::string date = Now("%Y%m%d"), stamp = Now("%Y%m%dT%H%M%S");
        for (const char* store : Libc::SaveLayout::kStores) {
            const auto folder = Libc::SaveLayout::StoreFolder(root, serial, store);
            const std::string host = folder.string(); // as the resolver reads it back (throws if it can't)
            // In stderr.log and in logs\savedata.log (stderr may go nowhere without the launcher).
            const auto log = [&](const std::string& line) {
                std::fprintf(stderr, "[savedata] %s %s: %s\n", serial.c_str(), store, line.c_str());
                std::error_code made;
                std::filesystem::create_directories(root / "logs" / GameSerial::GameSerial(), made);
#ifdef _WIN32
                std::FILE* file = _wfopen((root / "logs" / GameSerial::GameSerial() / "savedata.log").c_str(), L"ab");
#else
                std::FILE* file = std::fopen((root / "logs" / GameSerial::GameSerial() / "savedata.log").c_str(), "ab");
#endif
                if (file != nullptr) {
                    std::fprintf(file, "%s %s %s: %s\n", Now("%Y-%m-%d %H:%M:%S").c_str(), serial.c_str(), store, line.c_str());
                    std::fclose(file);
                }
            };
            if (Libc::SaveLayout::Migrate(root, serial, store, otherHere, log, date, stamp) == Libc::SaveLayout::Use::New) {
                AddPathAlias_nid_no_patch(store, host.c_str());
            } else {
                log("this run uses the run folder's " + std::string(store));
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[savedata] the per-game save folders were not set up (%s); the run folder's are used\n", e.what());
    }
    SharedFolders();
}

// At libc's load, as LaunchDefaults, before the other libraries load and their games start.
const int started_at_load = (Start(), 0);

} // namespace
