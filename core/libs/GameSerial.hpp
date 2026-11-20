#pragma once
// Which game this process runs and the files named after its serial (GAME-004), for every library:
// GT7 is CUSA24769, GT Sport CUSA03220. The renderer tells the games apart the same way.
//   - The game: SHADGT_GAME=gt7 or gts (gtsport) as the launcher sets it; another value, or none,
//     falls back to the executable's name (GT7.exe is GT7, anything else GT Sport).
//   - The settings: user\custom_configs\<serial>.json next to the game's exe. A GT7 folder from
//     before the serial change (started without the launcher, which renames the file) still has
//     CUSA03220.json; it is read while CUSA24769.json is missing, unless GTSport.exe is in the folder
//     (FIX-RT-007: there CUSA03220.json is GT Sport's own). Nothing here writes either file.
//   - FEAT-GTS-291: the launcher's settings (game=, update=, stdout_level=, online...): <serial>.RyGT.ini
//     next to the game's exe (SettingsIniPath), where both games had RyGT.ini.
#include <cstddef>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
// Include after the file's own <windows.h> (with its WIN32_LEAN_AND_MEAN / NOMINMAX choices).
#include <windows.h>
#else
#include <cstdio>
#endif

namespace GameSerial {

inline constexpr const char* kGtSport = "CUSA03220";
inline constexpr const char* kGt7 = "CUSA24769";

namespace detail {

inline bool SameText(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        const char x = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
        const char y = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b - 'A' + 'a') : *b;
        if (x != y) return false;
    }
    return *a == *b;
}

inline bool Detect() {
    if (const char* game = std::getenv("SHADGT_GAME"); game != nullptr) {
        if (SameText(game, "gt7")) return true;
        if (SameText(game, "gts") || SameText(game, "gtsport")) return false;
    }
#ifdef _WIN32
    wchar_t path[MAX_PATH * 2]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
    if (length == 0 || length >= MAX_PATH * 2) return false;
    const wchar_t* name = path;
    for (const wchar_t* c = path; *c; ++c) if (*c == L'\\' || *c == L'/') name = c + 1;
    return _wcsicmp(name, L"GT7.exe") == 0;
#else
    return false;
#endif
}

#ifdef _WIN32
inline constexpr char kSeparator = '\\';
#else
inline constexpr char kSeparator = '/';
#endif

// Whether a file exists: utf8 for a UTF-8 path, else the system code page (as fopen takes it).
inline bool Exists(const std::string& path, bool utf8) {
#ifdef _WIN32
    if (!utf8) return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    const int length = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (length <= 0) return false;
    std::wstring wide(static_cast<std::size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), length);
    return GetFileAttributesW(wide.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    (void)utf8;
    if (std::FILE* file = std::fopen(path.c_str(), "rb")) {
        std::fclose(file);
        return true;
    }
    return false;
#endif
}

} // namespace detail

/// Whether this process runs GT7 (read once; it never changes while the game runs).
inline bool RunningGt7() {
    static const bool gt7 = detail::Detect();
    return gt7;
}

/// The running game's serial: "CUSA24769" for GT7, "CUSA03220" for GT Sport.
inline const char* GameSerial() {
    return RunningGt7() ? kGt7 : kGtSport;
}

/// The settings file in a game folder (dir, with or without a trailing separator; "" is the
/// current folder): user\custom_configs\<serial>.json, or under GT7 the old CUSA03220.json while
/// only that exists and GTSport.exe is not in the folder (FIX-RT-007: then it is GT Sport's).
/// utf8: dir is UTF-8 (else the system code page, as fopen and the A APIs take).
inline std::string GameConfigPath(std::string dir, bool utf8 = false) {
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/') dir += detail::kSeparator;
    const std::string folder = dir + "user" + detail::kSeparator + "custom_configs" + detail::kSeparator;
    std::string path = folder + GameSerial() + ".json";
    if (RunningGt7() && !detail::Exists(path, utf8) && !detail::Exists(dir + "GTSport.exe", utf8)) {
        std::string old = folder + kGtSport + ".json";
        if (detail::Exists(old, utf8)) return old;
    }
    return path;
}

/// FEAT-GTS-291: the launcher's and the runtime's settings in a game folder: <serial>.RyGT.ini
/// (CUSA03220.RyGT.ini for GT Sport, CUSA24769.RyGT.ini for GT7), so two games can share one folder.
/// Before, both games named it RyGT.ini (GT7's launcher kept its own RyGT7.ini and mirrored the
/// runtime's values into RyGT.ini). The launcher, and libc at load, rename the old file
/// (prx/libc/include/SharedNames.hpp); while it is still there (it could not be renamed) the old
/// RyGT.ini is read instead, unless it may be the other game's: the other game's exe is in the
/// folder, or (for GT Sport) GT7's RyGT7.ini. dir empty: the current folder (a relative path).
inline std::filesystem::path SettingsIniPath(const std::filesystem::path& dir, const char* serial) {
    std::error_code error;
    const auto path = dir / (std::string(serial) + ".RyGT.ini");
    if (std::filesystem::exists(path, error)) return path;
    const bool gt7 = detail::SameText(serial, kGt7);
    const auto old = dir / "RyGT.ini";
    if (!std::filesystem::exists(old, error)) return path;
    if (std::filesystem::exists(dir / (gt7 ? "GTSport.exe" : "GT7.exe"), error)) return path;
    if (!gt7 && std::filesystem::exists(dir / "RyGT7.ini", error)) return path;
    return old;
}

#ifdef _WIN32
/// GameConfigPath for a wide (UTF-16) folder.
inline std::wstring GameConfigPath(std::wstring dir) {
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') dir += L'\\';
    const std::wstring folder = dir + L"user\\custom_configs\\";
    std::wstring path = folder + (RunningGt7() ? L"CUSA24769" : L"CUSA03220") + L".json";
    if (RunningGt7() && GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((dir + L"GTSport.exe").c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wstring old = folder + L"CUSA03220.json";
        if (GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES) return old;
    }
    return path;
}
#endif

} // namespace GameSerial
