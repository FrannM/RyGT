#pragma once
// GAME-005: each game's saves in its own folder, keyed by its serial (GameSerial.hpp), as a PS4 keeps
// save data per title ID. Before, the guest's /_sd (save data, memory saves, the _aliases and
// _app_status records) and /download0 (download data: GT7's photos and Scapes in APP_DATA) were the
// run folder's _sd and download0, shared by whichever game ran there. Now:
//   /_sd       -> user\savedata\<serial>\_sd
//   /download0 -> user\savedata\<serial>\download0
// /temp0 (temporary data, emptied by the game) and /data are temp0\<serial> and data\<serial> since
// FEAT-GTS-291 (SharedNames.hpp).
//
// Moving a folder's saves (Migrate) is conservative, since they are the player's:
//   - The game's folder already there: nothing is moved (an interrupted move is finished, below).
//   - No old folder, or one without a file: nothing to move (a fresh install).
//   - The other game's executable in the same folder (GT7.exe beside GTSport.exe): whose saves the
//     old folder holds is unknown, so nothing is moved and the game keeps using the old folder.
//   - Else the old folder is COPIED to user\savedata\<serial>\<store>.partial, checked (every file and
//     folder, and each file's size, the same), a record <store>.moved-from written, the copy renamed
//     to <store>, and only then the old folder renamed to <store>.moved-<date> (never deleted).
//   - A copy left by an interrupted run (.partial) is renamed to .partial.stale-<time> (kept) and the
//     copy is made again from the untouched original. A run stopped after the copy was renamed but
//     before the old folder was finishes that rename the next time.
//   - Any failure leaves the old folder as it was and the game uses it for that run (UseOld); the
//     move is tried again at the next start.
// Every step is reported through log.
#include <filesystem>
#include <functional>
#include <string>

namespace Libc::SaveLayout {

/// The guest folders that hold a game's saves (their host folders are moved).
inline constexpr const char* kStores[] = {"_sd", "download0"};

enum class Use { New, Old };

/// root\user\savedata\<serial>\<store>.
std::filesystem::path StoreFolder(const std::filesystem::path& root, const std::string& serial, const std::string& store);

/// Moves root\<store> into the game's own folder as above; New when the game should use its own
/// folder (StoreFolder), Old when it keeps using root\<store> this time. date: "YYYYMMDD" for the
/// .moved- name; stamp: a unique time for a .partial.stale- name.
Use Migrate(const std::filesystem::path& root, const std::string& serial, const std::string& store, bool otherGameHere,
            const std::function<void(const std::string&)>& log, const std::string& date, const std::string& stamp);

} // namespace Libc::SaveLayout
